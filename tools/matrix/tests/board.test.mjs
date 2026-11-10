import test from 'node:test';
import assert from 'node:assert/strict';
import { fetchBoard, fetchPulls, pullTaskState, normaliseCard, selectFromBoard, BOARD_STALE_MS } from '../board.mjs';
import { TELEMETRY_SOURCE } from '../policy.mjs';

const now = Date.parse('2026-11-10T02:00:00Z');
const repoA = 'tonygward/audio-plugins';
const usage = percentage => ({ source: TELEMETRY_SOURCE, percentage, observedAt: new Date(now - 1000).toISOString(), resetsAt: new Date(now + 3600000).toISOString() });
const prompt = (id, extra = {}) => ({ id, file: `shared/prompts/${id}.md`, hash: `h-${id}`, complete: false, deferred: false, blocked: false, execution: 'scheduled', dependencies: [], ...extra });
const card = (status, number, taskId, extra = {}) => ({ position: 0, itemId: `i${number}`, status, issue: number, state: 'OPEN', repository: repoA, taskId, title: `${taskId} — x`, ...extra });
const snapshot = (...cards) => ({ ok: true, fetchedAt: new Date(now - 1000).toISOString(), cards: cards.map((c, position) => ({ ...c, position })) });
const base = (extra = {}) => ({ now, usage: usage(10), executionPaths: new Set([repoA, 'tonygward/songsmith-website']), ...extra });
const prompts = ['A-01', 'B-01', 'C-01'].map(id => prompt(id));

test('moving A above B selects A; reversing selects B', () => {
  const a = card('Todo', 1, 'A-01'), b = card('Todo', 2, 'B-01');
  assert.equal(selectFromBoard(snapshot(a, b), prompts, base()).selected.taskId, 'A-01');
  assert.equal(selectFromBoard(snapshot(b, a), prompts, base()).selected.taskId, 'B-01');
});

test('ordering ignores task ID, issue number and prompt priority', () => {
  const ps = [prompt('A-01', { priority: 99 }), prompt('Z-99', { priority: 1 })];
  const result = selectFromBoard(snapshot(card('Todo', 900, 'Z-99'), card('Todo', 1, 'A-01')), ps, base());
  assert.equal(result.selected.taskId, 'Z-99');
});

test('Doing is picked before Todo, each in board order', () => {
  const result = selectFromBoard(snapshot(card('Todo', 1, 'A-01'), card('Doing', 2, 'B-01'), card('Backlog', 3, 'C-01')), prompts, base());
  assert.equal(result.selected.taskId, 'B-01');
  assert.equal(result.decisions.some(d => d.taskId === 'C-01'), false, 'Backlog excluded');
});

test('cards from other repositories and prefixes are considered, never refused by location', () => {
  const web = card('Todo', 19, 'WEB-03', { repository: 'tonygward/songsmith-website' });
  const result = selectFromBoard(snapshot(web, card('Todo', 2, 'A-01')), [prompt('WEB-03'), prompt('A-01')], base());
  assert.equal(result.selected.taskId, 'WEB-03');
  const noPath = selectFromBoard(snapshot(web, card('Todo', 2, 'A-01')), [prompt('WEB-03'), prompt('A-01')], base({ executionPaths: new Set([repoA]) }));
  assert.match(noPath.decisions[0].reason, /no scoped execution path for tonygward\/songsmith-website/);
  assert.equal(noPath.selected.taskId, 'A-01');
});

test('blocked top card records its real reason before the next card is considered', () => {
  const ps = [prompt('A-01', { dependencies: ['X-01'] }), prompt('B-01')];
  const result = selectFromBoard(snapshot(card('Todo', 1, 'A-01'), card('Todo', 2, 'B-01')), ps, base());
  assert.match(result.decisions[0].reason, /unmet dependencies: X-01/);
  assert.equal(result.selected.taskId, 'B-01');
});

test('merged work awaiting release is never recoded', () => {
  const fromSet = selectFromBoard(snapshot(card('Todo', 4, 'A-01'), card('Todo', 2, 'B-01')), prompts, base({ mergedTaskIds: new Set(['A-01']) }));
  assert.match(fromSet.decisions[0].reason, /merged; release pending/);
  assert.equal(fromSet.selected.taskId, 'B-01');
  const archived = selectFromBoard(snapshot(card('Todo', 4, 'A-01')), [prompt('A-01', { complete: true })], base());
  assert.equal(archived.selected, null);
});

test('deferred, closed, claimed, duplicate, PR and untitled cards are skipped with reasons', () => {
  const ps = [prompt('A-01', { deferred: true }), prompt('B-01'), prompt('C-01')];
  const cards = [card('Todo', 1, 'A-01'), card('Todo', 2, 'B-01', { state: 'CLOSED' }), card('Todo', 3, 'C-01'), card('Todo', 3, 'C-01'),
    card('Todo', 5, null, { issue: null }), card('Todo', 6, null)];
  const reasons = selectFromBoard(snapshot(...cards), ps, base({ tasks: { 'C-01': { state: 'claimed' } } })).decisions.map(d => d.reason);
  assert.deepEqual(reasons.map(r => r.split(':')[0]), ['prompt is marked deferred', 'issue is closed', 'durable state', 'duplicate card for task', 'not an issue card (implementation PR or draft)', 'no task ID in issue title']);
});

test('at most one task is selected and an active job blocks pickup', () => {
  const result = selectFromBoard(snapshot(card('Todo', 1, 'A-01'), card('Todo', 2, 'B-01')), prompts, base());
  assert.equal(result.decisions.filter(d => d.reason === 'selected').length, 1);
  assert.equal(result.decisions[1].reason, 'eligible; one task per run');
  const busy = selectFromBoard(snapshot(card('Todo', 1, 'A-01')), prompts, base({ activeJob: 'job-7' }));
  assert.equal(busy.selected, null);
  assert.match(busy.blocker, /Active job job-7/);
});

test('usage gate: 59.9% permits, 60%+ blocks, unknown or stale blocks, scoped meters block', () => {
  const s = snapshot(card('Todo', 1, 'A-01'));
  assert.ok(selectFromBoard(s, prompts, base({ usage: usage(59.9) })).selected);
  assert.equal(selectFromBoard(s, prompts, base({ usage: usage(60) })).selected, null);
  assert.equal(selectFromBoard(s, prompts, base({ usage: usage(95) })).selected, null);
  assert.equal(selectFromBoard(s, prompts, base({ usage: null })).selected, null);
  assert.equal(selectFromBoard(s, prompts, base({ usage: { ...usage(10), observedAt: new Date(now - 120000).toISOString() } })).selected, null);
  assert.equal(selectFromBoard(s, prompts, base({ scopedUsagePercent: [10, 60] })).selected, null);
  assert.equal(selectFromBoard(s, prompts, base({ scopedUsagePercent: [10, NaN] })).selected, null);
  assert.ok(selectFromBoard(s, prompts, base({ scopedUsagePercent: [10, 59.9] })).selected);
});

test('a global usage block stops the invocation; no decisions walk the queue', () => {
  const result = selectFromBoard(snapshot(card('Todo', 1, 'A-01'), card('Todo', 2, 'B-01')), prompts, base({ usage: usage(70) }));
  assert.deepEqual(result.decisions, []);
});

test('failed, stale or missing snapshots trigger no work', () => {
  assert.equal(selectFromBoard({ ok: false, reason: 'Partial board snapshot' }, prompts, base()).blocker, 'Partial board snapshot');
  assert.equal(selectFromBoard(null, prompts, base()).selected, null);
  const old = { ...snapshot(card('Todo', 1, 'A-01')), fetchedAt: new Date(now - BOARD_STALE_MS - 1).toISOString() };
  assert.match(selectFromBoard(old, prompts, base()).blocker, /Stale/);
});

const node = (n, status, title = `T-0${n} — x`) => ({ id: `i${n}`, status: { name: status }, content: { __typename: 'Issue', number: n, title, state: 'OPEN', repository: { nameWithOwner: repoA } } });
const page = (nodes, { next = false, total = nodes.length, views = 1 } = {}) => ({ data: { node: { views: { totalCount: views }, items: { totalCount: total, pageInfo: { hasNextPage: next, endCursor: next ? 'c1' : null }, nodes } } } });

test('fetchBoard keeps API order across pages and normalises cards', async () => {
  const calls = [];
  const pages = [page([node(1, 'Todo'), node(2, 'Doing')], { next: true, total: 3 }), page([node(3, 'Backlog')], { total: 3 })];
  const result = await fetchBoard(async (query, vars) => { calls.push(vars.after); assert.match(query, /POSITION/); return pages.shift(); }, 'P', { clock: () => now });
  assert.equal(result.ok, true);
  assert.deepEqual(result.cards.map(c => [c.issue, c.status, c.taskId]), [[1, 'Todo', 'T-01'], [2, 'Doing', 'T-02'], [3, 'Backlog', 'T-03']]);
  assert.deepEqual(calls, [null, 'c1']);
});

test('fetchBoard fails closed on API errors, partial snapshots and unverified views', async () => {
  assert.match((await fetchBoard(async () => { throw new Error('network'); }, 'P')).reason, /Board read failed: network/);
  assert.equal((await fetchBoard(async () => ({ data: { node: null } }), 'P')).ok, false);
  assert.equal((await fetchBoard(async () => page([node(1, 'Todo')], { total: 2 }), 'P')).reason, 'Partial board snapshot');
  assert.match((await fetchBoard(async () => page([node(1, 'Todo')], { views: 2 }), 'P')).reason, /verified for 1 view only/);
  const noCursor = page([node(1, 'Todo')], { next: true, total: 5 }); noCursor.data.node.items.pageInfo.endCursor = null;
  assert.equal((await fetchBoard(async () => noCursor, 'P')).reason, 'Pagination cursor missing');
});

test('normaliseCard handles pull requests and draft cards', () => {
  assert.equal(normaliseCard({ id: 'x', status: { name: 'Todo' }, content: { __typename: 'PullRequest', title: 'SUI-16: fix' } }, 0).issue, null);
  assert.equal(normaliseCard({ id: 'y', status: null, content: null }, 1).status, null);
});

const pr = (state, headRefName, title) => ({ state, headRefName, title, number: 1 });

test('only Matrix implementation branches count as merged; prompt-only and fix PRs do not', () => {
  const { merged, open } = pullTaskState([
    pr('MERGED', 'matrix/sui-16-0d2f16c74f0a', 'SUI-16: Matrix implementation'),
    pr('MERGED', 'sui-16-prompt', 'SUI-16: prompt for a fully visible version label'),
    pr('MERGED', 'fix/worker-turn-check', 'AUTO-01: turn check becomes a runaway guard'),
    pr('CLOSED', 'matrix/sui-09-9ed1107ea40d', 'SUI-09: Matrix implementation'),
    pr('OPEN', 'auto-11-board-driven-pickup', 'AUTO-11: board-ordered pickup (partial)'),
    pr('OPEN', 'matrix/max-07-abc123', 'Loudness work')]);
  assert.deepEqual([...merged], ['SUI-16']);
  assert.deepEqual([...open].sort(), ['AUTO-11', 'MAX-07']);
});

test('an open pull request already covering a task blocks recoding it', () => {
  const result = selectFromBoard(snapshot(card('Todo', 1, 'A-01'), card('Todo', 2, 'B-01')), prompts, base({ openPullTaskIds: new Set(['A-01']) }));
  assert.match(result.decisions[0].reason, /open pull request/);
  assert.equal(result.selected.taskId, 'B-01');
});

test('fetchPulls reads open and merged pulls per repository and fails closed', async () => {
  const seen = [];
  const ok = await fetchPulls(async (query, vars) => { seen.push(`${vars.name}:${vars.states[0]}`);
    return { data: { repository: { pullRequests: { pageInfo: { hasNextPage: false }, nodes: [{ number: 1, state: vars.states[0], title: 'T-01: x', headRefName: 'b' }] } } } }; }, ['o/a', 'o/b']);
  assert.equal(ok.ok, true);
  assert.deepEqual(seen, ['a:OPEN', 'a:MERGED', 'b:OPEN', 'b:MERGED']);
  assert.equal(ok.pulls.length, 4);
  assert.equal((await fetchPulls(async () => ({ data: { repository: null } }), ['o/a'])).ok, false);
  assert.match((await fetchPulls(async () => { throw new Error('boom'); }, ['o/a'])).reason, /read failed: boom/);
});
