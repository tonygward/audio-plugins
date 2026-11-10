// Doing-card reconciliation (AUTO-11 requirement 10). Run before enabling board pickup and on
// every restart. Compares each Doing card with merged and open pull requests, the archived
// prompt and the durable worker state, so stale prose in an older issue can never decide.
// Read-only: it returns findings and never changes a card, an issue or the state file.
import { matrixBranchTask } from './board.mjs';

/**
 * kinds: requested (valid immediate request) | active (our running job) | in-review (open pull request)
 *        | release-pending (merged, delivery not yet verified) | inconsistent (blocks enabling)
 */
export function reconcileDoing(snapshot, prompts, pulls, tasks = {}) {
  if (!snapshot?.ok) return { ok: false, reason: snapshot?.reason ?? 'No board snapshot', findings: [] };
  const byId = new Map(prompts.map(p => [p.id, p]));
  const merged = new Map(), open = new Map();
  for (const pull of pulls) {
    const id = matrixBranchTask(pull) ?? (pull.state === 'OPEN' ? /^([A-Z][A-Z0-9]*-\d{2,})\b/.exec(pull.title)?.[1] : null);
    if (!id) continue;
    const into = pull.state === 'MERGED' ? merged : pull.state === 'OPEN' ? open : null;
    if (into && (pull.state === 'OPEN' || matrixBranchTask(pull))) into.set(id, [...(into.get(id) ?? []), pull.number]);
  }
  const doing = snapshot.cards.filter(card => card.status === 'Doing');
  const claimedIds = Object.values(tasks).filter(task => task.state === 'claimed').map(task => task.id);
  const findings = [];
  const add = (card, kind, detail) => findings.push({ position: card.position, issue: card.issue, taskId: card.taskId, kind, detail });

  const seen = new Set();
  for (const card of doing) {
    if (!card.issue) { add(card, 'inconsistent', 'Doing card is not an issue (pull request or draft)'); continue; }
    if (!card.taskId) { add(card, 'inconsistent', 'Doing card has no task ID in its title'); continue; }
    if (seen.has(card.taskId)) { add(card, 'inconsistent', 'Duplicate Doing card for this task'); continue; }
    seen.add(card.taskId);
    const prompt = byId.get(card.taskId), task = tasks[card.taskId];
    const mergedPulls = merged.get(card.taskId) ?? [], openPulls = open.get(card.taskId) ?? [];
    if (card.state !== 'OPEN') add(card, 'inconsistent', 'Issue is closed but the card is still Doing');
    else if (!prompt) add(card, 'inconsistent', 'No canonical prompt found');
    else if (prompt.complete && !mergedPulls.length) add(card, 'inconsistent', 'Prompt is archived as merged but no merged Matrix pull request was found');
    else if (mergedPulls.length) add(card, 'release-pending', `Merged in #${mergedPulls.join(', #')}; delivery must be verified before Done. Never recode`);
    else if (task?.state === 'claimed') add(card, 'active',`Claimed by job ${task.job ?? 'unknown'}${openPulls.length ? `; open pull request #${openPulls.join(', #')}` : ''}`);
    else if (task && ['interrupted', 'dead-letter'].includes(task.state)) add(card, 'inconsistent', `Durable state ${task.state}: needs an owner decision before pickup`);
    else if (openPulls.length) add(card, 'in-review', `Open pull request #${openPulls.join(', #')} already covers this task`);
    else add(card, 'requested', 'Immediate work requested; not yet claimed');
  }
  // A claimed task whose card is missing from Doing or Todo means board and state disagree.
  for (const id of claimedIds) {
    const card = snapshot.cards.find(c => c.taskId === id && ['Doing', 'Todo'].includes(c.status));
    if (!card) findings.push({ position: null, issue: tasks[id].issue ?? null, taskId: id, kind: 'inconsistent', detail: 'Claimed in worker state but no Doing or Todo card exists' });
  }
  if (claimedIds.length > 1) findings.push({ position: null, issue: null, taskId: null, kind: 'inconsistent', detail: `${claimedIds.length} tasks are claimed; only one job may be active` });
  const blocking = findings.filter(f => f.kind === 'inconsistent');
  return { ok: true, enableAllowed: blocking.length === 0, findings };
}
