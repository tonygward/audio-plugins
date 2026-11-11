// One board-driven tick (AUTO-11), composed from board.mjs, doing.mjs, pickup.mjs and delivery.mjs.
// Deliberately separate from linux-worker.mjs: nothing here starts Claude, and the existing worker
// keeps using its Markdown selector until the owner approves the switch.
// Off unless config.boardPickup.enabled is true. dryRun defaults to true: decide and report, never claim.
import path from 'node:path';
import { atomic, lockedAsync, readState } from './state.mjs';
import { fetchBoard, fetchPulls, selectFromBoard, pullTaskState, matrixBranchTask, isDeferred } from './board.mjs';
import { reconcileDoing } from './doing.mjs';
import { pickUp, recoverPickupComments } from './pickup.mjs';
import { recoverCiComments } from './ci-comment.mjs';
import { advanceDelivery, recordMerge } from './delivery.mjs';
import { writeHandoff, clearHandoff } from './handoff.mjs';
import { blockStopped, resumeBlocked } from './blocked.mjs';
import { recoverInFlight } from './recover.mjs';
import { resolveFromIssues } from './issue-prompt.mjs';
import { scan } from './backlog.mjs';
import { dailyDue, londonDay } from './policy.mjs';

// Unlike state.reconcile (a worker restart marks claimed tasks interrupted), a board tick must keep an
// active claim intact: it only adds or refreshes tasks that are still unclaimed.
// The board's Execution field decides deferral where it is set: Run Now or Scheduled releases a prompt whose own wording reads as
// deferred (every backlog prompt says "not queued"), and Deferred holds one back. Prompts in excluded folders or marked blocked
// stay withdrawn whatever the board says.
export function refreshTasks(state, prompts, revision, now = new Date().toISOString(), cards = []) {
  for (const p of prompts) {
    const task = state.tasks[p.id] ??= { id: p.id, state: 'pending', attempts: 0, created: now };
    const card = cards.find(c => c.taskId === p.id && c.issue);
    if (p.complete && ['pending', 'withdrawn'].includes(task.state)) task.state = 'complete';
    else if (['pending', 'withdrawn'].includes(task.state)) task.state = isDeferred(card, p) || p.excluded || p.blocked ? 'withdrawn' : 'pending';
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
  const { graphql, io, loadUsage, loadPrompts, jobId, clock = Date.now, handoff = false, limits = null } = deps;
  const dir = config.stateDirectory, dryRun = settings.dryRun !== false;
  const stop = (reason, extra = {}) => ({ ran: true, dryRun, selected: null, blocker: reason, ...extra });

  if (handoff) clearHandoff(dir); // a hand-off is only ever from this tick
  const board = await fetchBoard(graphql, settings.projectId, { clock });
  if (!board.ok) return stop(board.reason);
  const pulled = await fetchPulls(graphql, settings.repositories);
  if (!pulled.ok) return stop(pulled.reason);
  // A broken prompt file must not stop delivery of work that has already merged: record merges and finish deliveries first,
  // then refuse to pick anything new until the prompt is fixed.
  let prompts = [], revision = null, promptError = null;
  try { ({ prompts, revision } = await loadPrompts()); } catch (error) { promptError = error; }

  // A card with no prompt file can still be built when its issue has enough detail: the prompt is generated from the issue and saved in the
  // pull request. Without enough detail the issue gets one comment and the card moves to Blocked.
  let issuePrompts = { prompts: [], blocked: [], notes: [] };
  if (!promptError && io.issueBody) {
    const early = readState(dir), todoSoon = dailyDue(early.lastWorkerDaily, config.dailyTime ?? '02:00', new Date(clock()));
    const { merged: mergedIds, open: openIds } = pullTaskState(pulled.pulls);
    issuePrompts = await resolveFromIssues({ board, prompts, tasks: early.tasks, io, statuses: todoSoon ? ['Doing', 'Todo'] : ['Doing'], executionRepositories: settings.executionRepositories, dryRun, merged: mergedIds, open: openIds });
    prompts = [...prompts, ...issuePrompts.prompts];
  }

  // Keep durable state in step with the repository and with merged pull requests (needs the lock).
  const recordedMerges = dryRun ? [] : await lockedAsync(dir, async () => {
    const state = readState(dir);
    if (!promptError) refreshTasks(state, prompts, revision, undefined, board.cards);
    const recorded = syncMerges(state, pulled.pulls);
    atomic(path.join(dir, 'state.json'), state);
    return recorded;
  });
  let state = readState(dir);

  // Resume anything half-way before considering new work: lost pickup comments, then deliveries.
  const delivery = {};
  if (!dryRun) {
    if (!promptError) await recoverPickupComments(dir, { revision, io, limits });
    await recoverCiComments(dir, { io });
    // A restart must not lose work whose branch is pushed and whose pull request is open: hand it back to the hosted Build and merge gate.
    if (io.prChecks) await recoverInFlight(dir, pulled.pulls, io, { now: clock });
    for (const [id, d] of Object.entries(readState(dir).deliveries ?? {})) if (d.stage !== 'done') delivery[id] = await advanceDelivery(dir, id, io);
    // Stopped work is reported once and parked in Blocked; an owner moving a card back to Todo or Doing is the explicit resume.
    await resumeBlocked(dir, board, io, { pulls: pulled.pulls });
    for (const itemId of (await blockStopped(dir, io)).moved) { const card = board.cards.find(c => c.itemId === itemId); if (card) card.status = 'Blocked'; }
    state = readState(dir);
  }

  if (promptError) return stop(`Prompt backlog is invalid, so nothing new is picked up: ${promptError.message}`, { delivery, recordedMerges });
  const doing = reconcileDoing(board, prompts, pulled.pulls, state.tasks);
  if (!doing.enableAllowed) return stop('Doing cards disagree with pull requests or worker state', { doing, delivery, recordedMerges });

  const usage = loadUsage(config.claudeUsage);
  if (!usage.allowed || usage.extraUsageEnabled !== false) return stop(usage.reason ?? 'Usage not verified', { doing, delivery, recordedMerges });
  const { merged, open } = pullTaskState(pulled.pulls);
  const active = Object.values(state.tasks).find(task => task.state === 'claimed');
  // Doing is a run-now request: offered at every run. Todo is the overnight queue: one task per London day, after dailyTime.
  const todoDue = dailyDue(state.lastWorkerDaily, config.dailyTime ?? '02:00', new Date(clock()));
  const result = selectFromBoard(board, prompts, { now: clock(), statuses: todoDue ? ['Doing', 'Todo'] : ['Doing'], usage: usage.snapshot, scopedUsagePercent: usage.scopedPercent ?? [],
    tasks: state.tasks, activeJob: active ? active.job ?? active.id : null, mergedTaskIds: merged, openPullTaskIds: open,
    executionPaths: new Set(settings.executionRepositories), promptRepository: settings.promptRepository });
  if (!result.selected) return stop(result.blocker, { decisions: result.decisions, doing, delivery, recordedMerges });
  if (dryRun) return { ran: true, dryRun, selected: result.selected, decisions: result.decisions, doing, delivery, recordedMerges };
  if (handoff) { writeHandoff(dir, { selected: result.selected, job: jobId, revision, now: clock(), resumedFrom: state.tasks[result.selected.taskId]?.resumedFrom ?? null }); return { ran: true, dryRun, handoff: true, selected: result.selected, decisions: result.decisions, doing, delivery, recordedMerges }; }
  const claimed = await pickUp(dir, result.selected, { job: jobId, revision, io, limits });
  if (claimed.claimed && result.selected.status === 'Todo') await lockedAsync(dir, async () => { const s = readState(dir); s.lastWorkerDaily = londonDay(new Date(clock())); atomic(path.join(dir, 'state.json'), s); });
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
    // One malformed prompt is skipped (it cannot be picked up) and reported; it must not stop every other task.
    for (const e of backlog.errors) console.error(JSON.stringify({ skippedPrompt: e.file, reason: e.reason }));
    return { prompts: backlog.prompts, revision };
  };
}
