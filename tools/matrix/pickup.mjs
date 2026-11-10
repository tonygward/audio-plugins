// Genuine pickup (AUTO-11): persist the claim first, then tell the tracking issue.
// A Doing card alone is not pickup evidence. The claim is durable before any comment is posted,
// so a crash or a failed comment can never lead to a second job for the same task.
import path from 'node:path';
import { atomic, lockedAsync, readState, claim, record } from './state.mjs';

export const marker = job => `<!-- matrix-pickup:${job} -->`;

export function pickupComment({ taskId, job, promptHash, revision, repository, limits = null }) {
  return [marker(job),
    `Matrix has picked up **${taskId}**.`, '',
    `- Matrix job: \`${job}\``,
    `- Prompt revision: \`${revision.slice(0, 12)}\` (prompt hash \`${promptHash.slice(0, 12)}\`)`,
    `- Implementation repository: \`${repository}\``,
    ...(limits ? [`- Limits: ${limits}`] : []), '',
    'Progress, the pull request, CI results and any blockers will be posted on this issue. Nothing is merged until the required checks pass.'].join('\n');
}

/**
 * Claim the selected card. Returns { claimed, ... }. Never throws for a refused claim.
 * io: { comment(repository, issue, body), hasComment(repository, issue, marker) }
 */
export async function pickUp(stateDirectory, selected, { job, revision, io, limits = null, now = () => new Date().toISOString() }) {
  return lockedAsync(stateDirectory, async () => {
    const state = readState(stateDirectory);
    const active = Object.values(state.tasks).find(task => task.state === 'claimed');
    if (active) return { claimed: false, reason: `Job ${active.job ?? active.id} is already active` };
    let ok;
    try { ok = claim(state, { id: selected.taskId, hash: selected.promptHash }, revision); }
    catch (error) { return { claimed: false, reason: error.message }; }
    if (!ok) { atomic(path.join(stateDirectory, 'state.json'), state); return { claimed: false, reason: 'Attempts exhausted; moved to dead letter' }; }
    Object.assign(state.tasks[selected.taskId], { job, issue: selected.issue, repository: selected.repository,
      issueRepository: selected.issueRepository, itemId: selected.itemId, claimedAt: now(), pickupCommentAt: null });
    record(state, { event: 'claimed', task: selected.taskId, job, issue: selected.issue });
    atomic(path.join(stateDirectory, 'state.json'), state); // durable before the comment
    return { claimed: true, comment: await postPickupComment(stateDirectory, state, selected.taskId, { revision, io, limits, now }) };
  });
}

// Idempotent: a marker check prevents duplicate comments after a retry or restart.
export async function postPickupComment(stateDirectory, state, taskId, { revision, io, limits = null, now = () => new Date().toISOString() }) {
  const task = state.tasks[taskId];
  if (!task || task.state !== 'claimed' || !task.job) return { posted: false, reason: 'No active claim' };
  if (task.pickupCommentAt) return { posted: false, reason: 'Already commented' };
  try {
    // The claim is durable. Move the card to Doing, then post the starting comment; both are safe to retry.
    if (!task.movedToDoingAt && io.moveToDoing) { await io.moveToDoing(task.itemId); task.movedToDoingAt = now(); }
    const slug = task.issueRepository ?? task.repository;
    if (!(await io.hasComment(slug, task.issue, marker(task.job)))) {
      await io.comment(slug, task.issue, pickupComment({ taskId, job: task.job, promptHash: task.hash, revision, repository: task.repository, limits }));
    }
    task.pickupCommentAt = now();
    record(state, { event: 'pickup-comment', task: taskId, job: task.job });
    atomic(path.join(stateDirectory, 'state.json'), state);
    return { posted: true };
  } catch (error) {
    // Keep the claim. The comment is retried by recoverPickupComments; never release or re-pick.
    record(state, { event: 'pickup-comment-failed', task: taskId, job: task.job, reason: String(error.message).slice(0, 200) });
    atomic(path.join(stateDirectory, 'state.json'), state);
    return { posted: false, reason: 'Comment failed; claim kept for retry' };
  }
}

export async function recoverPickupComments(stateDirectory, { revision, io, limits = null, now }) {
  return lockedAsync(stateDirectory, async () => {
    const state = readState(stateDirectory), results = {};
    for (const task of Object.values(state.tasks)) {
      if (task.state === 'claimed' && task.job && !task.pickupCommentAt) results[task.id] = await postPickupComment(stateDirectory, state, task.id, { revision, io, limits, now });
    }
    return results;
  });
}
