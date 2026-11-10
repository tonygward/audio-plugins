// Board-ordered pickup (AUTO-11). Pure decision logic plus a read-only board fetcher.
// Not wired into the worker and not activated: nothing here claims, comments or writes.
// Ordering comes only from the project's POSITION order (the visible manual drag order of
// the single table view). Never from prompt priority, task ID, issue number or repository.
import { usageGate, WEEKLY_LIMIT_PERCENT } from './policy.mjs';

export const BOARD_STALE_MS = 60000;
export const MAX_PAGES = 20;
export const EXPECTED_VIEW_COUNT = 1; // POSITION was verified with A/B drags in one view only.
const taskPattern = /^([A-Z][A-Z0-9]*-\d{2,})\b/;

export const BOARD_QUERY = `query($id: ID!, $after: String) {
  node(id: $id) { ... on ProjectV2 {
    views(first: 10) { totalCount }
    items(first: 100, after: $after, orderBy: { field: POSITION, direction: ASC }) {
      totalCount pageInfo { hasNextPage endCursor }
      nodes {
        id
        status: fieldValueByName(name: "Status") { ... on ProjectV2ItemFieldSingleSelectValue { name } }
        content { __typename ... on Issue { number title state repository { nameWithOwner } } }
      }
    }
  } }
}`;

// Read the whole board in POSITION order. Any API error, partial page set, or unexpected view
// layout returns { ok: false } so the caller does no work. There is no fallback ordering.
export async function fetchBoard(graphql, projectId, { clock = Date.now } = {}) {
  const cards = [];
  let after = null, total = null, views = null;
  try {
    for (let page = 0; page < MAX_PAGES; page++) {
      const project = (await graphql(BOARD_QUERY, { id: projectId, after }))?.data?.node;
      if (!project?.items) return { ok: false, reason: 'Board response missing items' };
      views = project.views?.totalCount; total = project.items.totalCount;
      cards.push(...project.items.nodes);
      if (!project.items.pageInfo.hasNextPage) {
        if (views !== EXPECTED_VIEW_COUNT) return { ok: false, reason: `Board order verified for ${EXPECTED_VIEW_COUNT} view only; found ${views}` };
        if (cards.length !== total) return { ok: false, reason: 'Partial board snapshot' };
        return { ok: true, fetchedAt: new Date(clock()).toISOString(), cards: cards.map(normaliseCard) };
      }
      after = project.items.pageInfo.endCursor;
      if (!after) return { ok: false, reason: 'Pagination cursor missing' };
    }
    return { ok: false, reason: 'Board exceeds page limit' };
  } catch (error) {
    return { ok: false, reason: `Board read failed: ${error.message}` };
  }
}

export function normaliseCard(node, index) {
  const issue = node.content?.__typename === 'Issue' ? node.content : null;
  return { position: index, itemId: node.id, status: node.status?.name ?? null, issue: issue?.number ?? null,
    state: issue?.state ?? null, repository: issue?.repository?.nameWithOwner ?? null,
    taskId: issue ? taskPattern.exec(issue.title)?.[1] ?? null : null, title: issue?.title ?? node.content?.title ?? '' };
}

/**
 * Choose at most one card. Returns { selected, decisions, blocker }.
 * snapshot: result of fetchBoard (ok: true) | prompts: parsePrompt results from backlog.scan
 * context: { now, usage, scopedUsagePercent[], tasks, activeJob, mergedTaskIds, executionPaths }
 */
export function selectFromBoard(snapshot, prompts, context) {
  const { now = Date.now(), usage, scopedUsagePercent = [], tasks = {}, activeJob = null, mergedTaskIds = new Set(), executionPaths = new Set() } = context;
  const stop = reason => ({ selected: null, decisions: [], blocker: reason });
  if (!snapshot?.ok) return stop(snapshot?.reason ?? 'No board snapshot');
  const age = now - Date.parse(snapshot.fetchedAt);
  if (!Number.isFinite(age) || age < 0 || age > BOARD_STALE_MS) return stop('Stale or invalid board snapshot');
  // Global gates stop the whole invocation. The queue is never walked down to bypass them.
  const gate = usageGate(usage, now);
  if (!gate.allowed) return stop(gate.reason);
  if (scopedUsagePercent.some(p => typeof p !== 'number' || !Number.isFinite(p) || p >= WEEKLY_LIMIT_PERCENT)) return stop(`Scoped weekly usage at or above ${WEEKLY_LIMIT_PERCENT}% or unknown`);
  if (activeJob) return stop(`Active job ${activeJob} must finish first (one active job)`);

  const byId = new Map(prompts.map(p => [p.id, p]));
  const complete = new Set(prompts.filter(p => p.complete).map(p => p.id));
  // Doing requests immediate work; Todo is the overnight queue. Each keeps board order.
  const queue = ['Doing', 'Todo'].flatMap(status => snapshot.cards.filter(card => card.status === status));
  const decisions = [], seen = new Set();
  let selected = null;
  for (const card of queue) {
    const prompt = card.taskId ? byId.get(card.taskId) : null;
    const task = card.taskId ? tasks[card.taskId] : null;
    let reason = !card.issue ? 'not an issue card (implementation PR or draft)' : !card.taskId ? 'no task ID in issue title'
      : seen.has(card.taskId) ? 'duplicate card for task' : card.state !== 'OPEN' ? 'issue is closed'
      : mergedTaskIds.has(card.taskId) || complete.has(card.taskId) ? 'implementation merged; release pending, do not recode'
      : !prompt ? 'no canonical prompt found'
      : prompt.deferred || prompt.execution === 'deferred' ? 'prompt is marked deferred'
      : prompt.blocked ? 'prompt is blocked'
      : task && ['claimed', 'interrupted', 'awaiting-review', 'complete', 'dead-letter'].includes(task.state) ? `durable state: ${task.state}`
      : !executionPaths.has(card.repository) ? `no scoped execution path for ${card.repository}` : null;
    if (!reason && prompt) {
      const missing = prompt.dependencies.filter(id => !complete.has(id));
      if (missing.length) reason = `unmet dependencies: ${missing.join(', ')}`;
    }
    if (card.taskId) seen.add(card.taskId);
    decisions.push({ position: card.position, status: card.status, issue: card.issue, taskId: card.taskId, repository: card.repository, reason: reason ?? (selected ? 'eligible; one task per run' : 'selected') });
    if (!reason && !selected) selected = { ...card, prompt: prompt.file, promptHash: prompt.hash };
  }
  return { selected, decisions, blocker: selected ? null : 'No eligible card' };
}
