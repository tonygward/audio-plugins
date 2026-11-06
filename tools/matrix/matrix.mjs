import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { scan, select } from './backlog.mjs';
import { atomic, locked, readState, reconcile, record } from './state.mjs';
import { usageGate, dailyDue, londonDay, claudePlan } from './policy.mjs';

export function command(executable, args, cwd) {
  return execFileSync(executable, args, { cwd, encoding: 'utf8', timeout: 60000, maxBuffer: 16 * 1024 * 1024, windowsHide: true, stdio: ['ignore', 'pipe', 'pipe'] }).trimEnd();
}

export function snapshot(repo, { local = false, run = command } = {}) {
  if (!local) run('git', ['fetch', '--no-tags', 'origin', 'main'], repo);
  const revision = run('git', ['rev-parse', '--verify', local ? 'HEAD' : 'refs/remotes/origin/main'], repo);
  if (!/^[a-f0-9]{40,64}$/.test(revision)) throw new Error('Invalid main revision');
  // Read immutable tracked blobs. Never execute code, hooks or config from fetched main.
  const tree = run('git', ['ls-tree', '-r', '--name-only', revision], repo).split('\n');
  const files = new Map();
  for (const file of tree) {
    if (/^(eq|analog-compressor|maximiser|shared)\/prompts\/.+\.md$/.test(file)) files.set(file, run('git', ['show', `${revision}:${file}`], repo));
  }
  return { revision, files };
}

export function readConfig(file) {
  const config = JSON.parse(fs.readFileSync(file, 'utf8'));
  if (!path.isAbsolute(config.repository ?? '') || !path.isAbsolute(config.stateDirectory ?? '')) throw new Error('Absolute repository and stateDirectory required');
  if (config.liveExecution !== false || config.autoMerge !== false || config.releasePublication !== false) throw new Error('This deployment supports dry-run only; live/merge/release must be false');
  if (!Number.isInteger(config.batchLimit) || config.batchLimit < 1 || config.batchLimit > 6) throw new Error('batchLimit must be 1–6');
  if (typeof config.enabled !== 'boolean') throw new Error('enabled must be a boolean');
  dailyDue(null, config.dailyTime);
  return config;
}

export function tick(config, { trigger = 'daily', local = false, loadSnapshot = snapshot, now = new Date() } = {}) {
  const dir = config.stateDirectory;
  return locked(dir, () => {
    if (!config.enabled || fs.existsSync(path.join(dir, 'PAUSED'))) return { paused: true, reason: 'Global disable control', liveExecution: false };
    const state = readState(dir);
    // A timer checks every 5 minutes. Daily time is evaluated in London, independent of host timezone.
    if (trigger === 'timer') {
      const inbox = path.join(dir, 'inbox');
      const messages = fs.existsSync(inbox) ? fs.readdirSync(inbox).filter(n => /^[a-f0-9]{40,64}\.json$/.test(n)) : [];
      if (dailyDue(state.lastDaily, config.dailyTime, now)) trigger = 'daily';
      else if (messages.some(n => !state.events.includes(n))) trigger = 'notify';
      else return { idle: true, liveExecution: false };
    }
    const { revision, files } = loadSnapshot(config.repository, { local });
    const { prompts, errors, warnings } = scan(files);
    reconcile(state, prompts, revision, now.toISOString());
    // Any ambiguous ID or malformed metadata blocks the whole batch; partial scans are unsafe.
    const plan = select(prompts, state.tasks, trigger, config.batchLimit);
    const usage = usageGate(null, now.getTime());
    const report = { time: now.toISOString(), revision, canonical: !local, trigger, dryRun: true, liveExecution: false,
      usage, errors, warnings, candidates: errors.length ? [] : plan.selected.map(p => ({ id: p.id, file: p.file, hash: p.hash, agent: claudePlan(p) })),
      decisions: plan.decisions, blockers: ['Live execution is not implemented in this deployment', usage.reason],
      taskStates: Object.values(state.tasks).map(t => ({ id: t.id, state: t.state, attempts: t.attempts })) };
    record(state, { revision, trigger, candidates: report.candidates.map(p => p.id), errors: errors.length });
    if (trigger === 'daily' && !local) state.lastDaily = londonDay(now);
    const inbox = path.join(dir, 'inbox');
    if (!local && fs.existsSync(inbox)) {
      state.events = [...new Set([...state.events, ...fs.readdirSync(inbox).filter(n => /^[a-f0-9]{40,64}\.json$/.test(n))])];
    }
    // Separate report and state: state is the commit point; a crash before it repeats only a dry run.
    atomic(path.join(dir, 'latest-report.json'), report);
    fs.mkdirSync(path.join(dir, 'logs'), { recursive: true });
    fs.appendFileSync(path.join(dir, 'logs', `${now.toISOString().slice(0, 10)}.jsonl`), JSON.stringify(report) + '\n', { mode: 0o600 });
    if (!local) atomic(path.join(dir, 'state.json'), state);
    return report;
  });
}

export function notify(directory, revision) {
  if (!/^[a-f0-9]{40,64}$/.test(revision)) throw new Error('Invalid notification revision');
  const inbox = path.join(directory, 'inbox');
  fs.mkdirSync(inbox, { recursive: true, mode: 0o700 });
  try { fs.writeFileSync(path.join(inbox, `${revision}.json`), JSON.stringify({ revision }), { flag: 'wx', mode: 0o600 }); }
  catch (error) { if (error.code !== 'EEXIST') throw error; }
}

function main(args) {
  const [action, configFile, value] = args;
  if (!['daily', 'notify', 'timer', 'local', 'status', 'pause', 'resume'].includes(action) || !configFile) throw new Error('Usage: node matrix.mjs daily|notify|timer|local|status|pause|resume <config.json> [revision]');
  const config = readConfig(configFile), dir = config.stateDirectory;
  fs.mkdirSync(dir, { recursive: true, mode: 0o700 });
  if (action === 'notify') { notify(dir, value); return tick(config, { trigger: 'notify' }); }
  if (action === 'pause') { fs.writeFileSync(path.join(dir, 'PAUSED'), 'Owner paused\n'); return { paused: true }; }
  if (action === 'resume') { fs.rmSync(path.join(dir, 'PAUSED'), { force: true }); return { paused: false, liveExecution: false }; }
  if (action === 'status') return fs.existsSync(path.join(dir, 'latest-report.json')) ? JSON.parse(fs.readFileSync(path.join(dir, 'latest-report.json'), 'utf8')) : { reason: 'No successful reconciliation yet' };
  return tick(config, { trigger: action === 'local' ? 'daily' : action, local: action === 'local' });
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try { console.log(JSON.stringify(main(process.argv.slice(2)), null, 2)); }
  catch (error) { console.error(JSON.stringify({ error: error.message, liveExecution: false })); process.exitCode = 1; }
}
