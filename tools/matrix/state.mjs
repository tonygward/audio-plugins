import fs from 'node:fs';
import path from 'node:path';

export function atomic(file, value) {
  const tmp = `${file}.${process.pid}.tmp`;
  const fd = fs.openSync(tmp, 'w', 0o600);
  try { fs.writeFileSync(fd, JSON.stringify(value, null, 2) + '\n'); fs.fsyncSync(fd); } finally { fs.closeSync(fd); }
  fs.renameSync(tmp, file);
}

export function locked(directory, fn) {
  fs.mkdirSync(directory, { recursive: true, mode: 0o700 });
  const file = path.join(directory, 'worker.lock');
  // Never automatically steal a lock by age or PID: PID reuse and suspended workers are real.
  const fd = fs.openSync(file, 'wx', 0o600);
  try { fs.writeFileSync(fd, JSON.stringify({ pid: process.pid, started: new Date().toISOString() })); return fn(); }
  finally { fs.closeSync(fd); fs.unlinkSync(file); }
}

export function readState(directory) {
  const file = path.join(directory, 'state.json');
  if (!fs.existsSync(file)) return { schema: 1, tasks: {}, events: [], history: [] };
  const state = JSON.parse(fs.readFileSync(file, 'utf8'));
  if (state.schema !== 1 || !state.tasks || !Array.isArray(state.events) || !Array.isArray(state.history)) throw new Error('Invalid state; restore verified backup');
  return state;
}

export async function lockedAsync(directory, fn) {
  fs.mkdirSync(directory, { recursive: true, mode: 0o700 });
  const file = path.join(directory, 'worker.lock');
  const fd = fs.openSync(file, 'wx', 0o600);
  try { fs.writeFileSync(fd, JSON.stringify({ pid: process.pid, started: new Date().toISOString() })); return await fn(); }
  finally { fs.closeSync(fd); fs.unlinkSync(file); }
}

export function reconcile(state, prompts, revision, now = new Date().toISOString()) {
  const ids = new Set(prompts.map(p => p.id));
  for (const task of Object.values(state.tasks)) {
    if (task.state === 'claimed') { task.state = 'interrupted'; task.reason = 'Worker restarted; inspect retained work before retry'; }
    if (!ids.has(task.id) && task.state === 'pending') { task.state = 'withdrawn'; task.reason = 'Missing from current main'; }
  }
  for (const p of prompts) {
    let task = state.tasks[p.id];
    if (!task) task = state.tasks[p.id] = { id: p.id, state: 'pending', attempts: 0, created: now };
    if (p.complete) task.state = 'complete';
    else if (['pending', 'withdrawn'].includes(task.state)) task.state = p.deferred || p.excluded || p.blocked ? 'withdrawn' : 'pending';
    task.file = p.file; task.hash = p.hash; task.execution = p.execution; task.revision = revision; task.updated = now;
  }
}

export function claim(state, prompt, revision, maximum = 2) {
  const task = state.tasks[prompt.id];
  if (!task || task.state !== 'pending' || task.hash !== prompt.hash || task.revision !== revision) throw new Error('Claim rejected');
  if (task.attempts >= maximum) { task.state = 'dead-letter'; return false; }
  task.state = 'claimed'; task.attempts++; return true;
}

export function finish(state, id, result, details = {}) {
  const task = state.tasks[id];
  if (!task || task.state !== 'claimed' || !['awaiting-review', 'interrupted', 'dead-letter'].includes(result)) throw new Error('Invalid finish transition');
  Object.assign(task, details, { state: result });
  // Only reconciliation of the merged canonical done folder may mark complete.
}

export function record(state, event) {
  state.history.push({ time: new Date().toISOString(), ...event });
  state.history = state.history.slice(-1000);
}
