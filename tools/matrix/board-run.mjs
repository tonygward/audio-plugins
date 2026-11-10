// One board-driven tick (AUTO-11), composed from board.mjs, doing.mjs, pickup.mjs and delivery.mjs.
// Deliberately separate from linux-worker.mjs: nothing here starts Claude, and the existing worker
// keeps using its Markdown selector until the owner approves the switch.
// Off unless config.boardPickup.enabled is true. dryRun defaults to true: decide and report, never claim.
import path from 'node:path';
import { atomic, lockedAsync, readState } from './state.mjs';
import { fetchBoard, fetchPulls, selectFromBoard, pullTaskState, matrixBranchTask } from './board.mjs';
import { reconcileDoing } from './doing.mjs';
import { pickUp, recoverPickupComments } from './pickup.mjs';
import { advanceDelivery, recordMerge } from './delivery.mjs';
import { writeHandoff, clearHandoff } from './handoff.mjs';
import { dailyDue } from './policy.mjs';
import { scan } from './backlog.mjs';

// Unlike state.reconcile (a worker restart marks claimed tasks interrupted), a board tick must keep an
// active claim intact: it only adds or refreshes tasks that are still unclaimed.
export function refreshTasks(state, prompts, revision, now = new Date().toISOString()) {
  for (const p of prompts) {
    const task = state.tasks[p.id] ??= { id: p.id, state: 'pending', attempts: 0, created: now };
    if (p.complete && ['pending', 'withdrawn'].includes(task.state)) task.state = 'complete';
    else if (['pending', 'withdrawn'].includes(task.state)) task.state = p.deferred || p.excluded || p.blocked ? 'withdrawn' : 'pending';
    if (['pending', 'withdrawn', 'complete'].includes(task.state)) Object.assign(task, { file: p.file, hash: p.hash, execution: p.execution, revision, updated: now });
  }
}

// Merge hook: a claimed task whose Matrix pull request has merged enters the delivery lifecycle.
export function syncMerges(state, pulls) {
  const recorded = [];
  for (const task of Object.values(state.tasks)) {
    if (!task.job || !task.itemId || state.deliveries?.[task.id]) continue;
    const pull = pulls.find(p => p.state === 'MERGED' && matrixBranchTask(p) === task.id);
    if (pull) { recordMerge(state, task.id, { repository: pull.repository, pull: pull.number }); recorded.push(task.id); }
  }
  return recorded;
}

export async function boardTick(config, deps) {
  const settings = config.boardPickup;
  if (settings?.enabled !== true) return { ran: false, reason: 'Board pickup is not enabled' };
  const { graphql, io, loadUsage, loadPrompts, jobId, clock = Date.now, handoff = false } = deps;
  const dir = config.stateDirectory, dryRun = settings.dryRun !== false;
  const stop = (reason, extra = {}) => ({ ran: true, dryRun, selected: null, blocker: reason, ...extra });

  if (handoff) clearHandoff(dir); // a hand-off is only ever from this tick
  const board = await fetchBoard(graphql, settings.projectId, { clock });
  if (!board.ok) return stop(board.reason);
  const pulled = await fetchPulls(graphql, settings.repositories);
  if (!pulled.ok) return stop(pulled.reason);
  const { prompts, revision } = await loadPrompts();

  // Keep durable state in step with the repository and with merged pull requests (needs the lock).
  const recordedMerges = dryRun ? [] : await lockedAsync(dir, async () => {
    const state = readState(dir);
    refreshTasks(state, prompts, revision);
    const recorded = syncMerges(state, pulled.pulls);
    atomic(path.join(dir, 'state.json'), state);
    return recorded;
  });
  const state = readState(dir);

  // Resume anything half-way before considering new work: lost pickup comments, then deliveries.
  const delivery = {};
  if (!dryRun) {
    await recoverPickupComments(dir, { revision, io });
    for (const [id, d] of Object.entries(readState(dir).deliveries ?? {})) if (d.stage !== 'done') delivery[id] = await advanceDelivery(dir, id, io);
  }

  const doing = reconcileDoing(board, prompts, pulled.pulls, state.tasks);
  if (!doing.enableAllowed) return stop('Doing cards disagree with pull requests or worker state', { doing, delivery, recordedMerges });

  const usage = loadUsage(config.claudeUsage);
  if (!usage.allowed || usage.extraUsageEnabled !== false) return stop(usage.reason ?? 'Usage not verified', { doing, delivery, recordedMerges });
  const { merged, open } = pullTaskState(pulled.pulls);
  const active = Object.values(state.tasks).find(task => task.state === 'claimed');
  const result = selectFromBoard(board, prompts, { now: clock(), usage: usage.snapshot, scopedUsagePercent: usage.scopedPercent ?? [],
    tasks: state.tasks, activeJob: active ? active.job ?? active.id : null, mergedTaskIds: merged, openPullTaskIds: open,
    executionPaths: new Set(settings.executionRepositories), promptRepository: settings.promptRepository,
    // Doing is immediate work; Todo is the overnight queue, once per London day after dailyTime.
    statuses: dailyDue(state.lastWorkerDaily, config.dailyTime ?? '02:00', new Date(clock())) ? ['Doing', 'Todo'] : ['Doing'] });
  if (!result.selected) return stop(result.blocker, { decisions: result.decisions, doing, delivery, recordedMerges });
  if (dryRun) return { ran: true, dryRun, selected: result.selected, decisions: result.decisions, doing, delivery, recordedMerges };
  if (handoff) { writeHandoff(dir, { selected: result.selected, job: jobId, revision, now: clock() }); return { ran: true, dryRun, handoff: true, selected: result.selected, decisions: result.decisions, doing, delivery, recordedMerges }; }
  const claimed = await pickUp(dir, result.selected, { job: jobId, revision, io });
  return { ran: true, dryRun, selected: claimed.claimed ? result.selected : null, claim: claimed, decisions: result.decisions, doing, delivery, recordedMerges };
}

// Adapters for a real run. `gh` returns stdout; `git` returns stdout from the source checkout.
export function ghGraphql(gh) {
  return async (query, variables) => {
    const args = ['api', 'graphql', '-f', `query=${query}`];
    for (const [key, value] of Object.entries(variables)) {
      if (value === null || value === undefined) continue;
      if (Array.isArray(value)) for (const item of value) args.push('-f', `${key}[]=${item}`);
      else args.push(typeof value === 'string' ? '-f' : '-F', `${key}=${value}`);
    }
    return JSON.parse(await gh(args));
  };
}

export function promptLoader(git) {
  return async () => {
    await git(['fetch', '--no-tags', 'origin', 'main']);
    const revision = await git(['rev-parse', '--verify', 'refs/remotes/origin/main']);
    if (!/^[a-f0-9]{40,64}$/.test(revision)) throw new Error('Invalid main revision');
    const files = new Map();
    for (const file of (await git(['ls-tree', '-r', '--name-only', revision])).split(String.fromCharCode(10))) {
      if (/^(eq|analog-compressor|maximiser|shared)\/prompts\/.+\.md$/.test(file)) files.set(file, await git(['show', `${revision}:${file}`]));
    }
    const backlog = scan(files);
    if (backlog.errors.length) throw new Error('Backlog validation failed');
    return { prompts: backlog.prompts, revision };
  };
}
