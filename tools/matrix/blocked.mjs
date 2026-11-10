// Blocked outcomes and explicit resume (AUTO-12 first slice).
// A task that stops (interrupted, dead-lettered, limit reached) gets one checkpoint comment on its issue and its card moves to
// Blocked. It is never re-queued by a poll. Moving the card back to Todo or Doing is the explicit resume. Both steps are
// idempotent and safe to retry: the comment is deduplicated by a marker and the saved state records what already happened.
import path from 'node:path';
import { atomic, lockedAsync, readState, record } from './state.mjs';

export const blockedMarker = (id, job) => `<!-- matrix-blocked:${id}:${job ?? 'unknown'} -->`;
export const heldMarker = (id, job) => `<!-- matrix-resume-held:${id}:${job ?? 'unknown'} -->`;
const STOPPED = ['interrupted', 'dead-letter'];
const stopped = task => STOPPED.includes(task.state) && task.issue;
// (Work that merged is in delivery, not blocked: see the guard in blockStopped.)
// Committed work has a branch and probably a pull request. Starting again would duplicate it, so it is held for the owner.
const hasWork = task => Boolean(task.commit || task.pr);

export function blockedComment(task) {
  const lines = [blockedMarker(task.id, task.job), `**${task.id} is Blocked.**`, '',
    `- Reason: ${task.reason ?? 'The run stopped'}`,
    `- Matrix job: \`${task.job ?? 'unknown'}\`; attempts used: ${task.attempts ?? 0}`,
    ...(task.branch ? [`- Branch: \`${task.branch}\`${task.worktree ? ' (worktree retained)' : ''}`] : []),
    ...(task.pr ? [`- Pull request: ${task.pr}`] : []),
    ...(task.runStartedAt ? [`- Run started: ${task.runStartedAt}`] : []), '',
    hasWork(task)
      ? '**To resume:** this task already has committed work, so an owner needs to review the branch first. Automatic resume of committed work is not built yet.'
      : '**To resume:** move this card back to Todo or Doing. Matrix starts the task again from a clean worktree; nothing was committed.',
    'The issue stays open. Matrix will not retry on its own.'];
  return lines.join('\n');
}

/**
 * Comment and move the card for every stopped task that has not been reported yet.
 * io: { comment(repo, issue, body), hasComment(repo, issue, marker), moveToBlocked(itemId) }
 * Returns the item ids moved, so the caller can refresh its view of the board.
 */
export async function blockStopped(stateDirectory, io, { now = () => new Date().toISOString() } = {}) {
  return lockedAsync(stateDirectory, async () => {
    const state = readState(stateDirectory), moved = [];
    for (const task of Object.values(state.tasks)) {
      if (!stopped(task) || task.blockedCommentAt || state.deliveries?.[task.id]) continue;
      try {
        if (!task.movedBlockedAt && task.itemId && io.moveToBlocked) { await io.moveToBlocked(task.itemId); task.movedBlockedAt = now(); moved.push(task.itemId); }
        const slug = task.issueRepository ?? task.repository;
        if (!(await io.hasComment(slug, task.issue, blockedMarker(task.id, task.job)))) await io.comment(slug, task.issue, blockedComment(task));
        task.blockedCommentAt = now();
        record(state, { event: 'blocked', task: task.id, job: task.job });
      } catch (error) {
        record(state, { event: 'blocked-failed', task: task.id, job: task.job, reason: String(error.message).slice(0, 200) });
      }
      atomic(path.join(stateDirectory, 'state.json'), state);
    }
    return { moved };
  });
}

/**
 * A blocked task whose card has been moved back to Todo or Doing is an explicit resume.
 * Without committed work it is queued again with a fresh budget. With committed work it is held: one comment, card back to Blocked.
 */
export async function resumeBlocked(stateDirectory, board, io, { now = () => new Date().toISOString() } = {}) {
  return lockedAsync(stateDirectory, async () => {
    const state = readState(stateDirectory), resumed = [], held = [];
    for (const task of Object.values(state.tasks)) {
      if (!stopped(task) || !task.blockedCommentAt) continue;
      const card = board.cards.find(c => c.taskId === task.id && c.issue === task.issue);
      if (!card || !['Todo', 'Doing'].includes(card.status)) continue;
      if (!hasWork(task)) {
        // Remember where the work stopped, so the next starting comment can say this is a resume and from what.
        task.resumedFrom = { job: task.job, reason: task.reason, branch: task.branch, pr: task.pr, checkpoint: task.phase };
        for (const key of ['blockedCommentAt', 'movedBlockedAt', 'pickupCommentAt', 'movedToDoingAt', 'claimedAt', 'runStartedAt', 'job', 'reason', 'phase', 'commit', 'pr', 'pickup']) delete task[key];
        Object.assign(task, { state: 'pending', attempts: 0, resumedAt: now() });
        record(state, { event: 'resumed', task: task.id });
        resumed.push(task.id);
      } else {
        try {
          if (io.moveToBlocked && task.itemId) await io.moveToBlocked(task.itemId);
          const slug = task.issueRepository ?? task.repository;
          if (!(await io.hasComment(slug, task.issue, heldMarker(task.id, task.job)))) {
            await io.comment(slug, task.issue, `${heldMarker(task.id, task.job)}\n**${task.id} was not restarted.** It has committed work${task.branch ? ` on \`${task.branch}\`` : ''}${task.pr ? ` (${task.pr})` : ''}, and starting again would duplicate it. An owner needs to review that work; the card is back in Blocked.`);
          }
          record(state, { event: 'resume-held', task: task.id, job: task.job });
          held.push(task.id);
        } catch (error) { record(state, { event: 'resume-held-failed', task: task.id, reason: String(error.message).slice(0, 200) }); }
      }
      atomic(path.join(stateDirectory, 'state.json'), state);
    }
    return { resumed, held };
  });
}
