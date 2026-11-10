import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { boardTick, syncMerges, refreshTasks } from '../board-run.mjs';
import { readState, atomic } from '../state.mjs';

const repo = 'tonygward/audio-plugins', revision = 'a'.repeat(40);
const prompt = id => ({ id, file: `shared/prompts/${id}.md`, hash: `hash-${id}-0123456789`, complete: false, deferred: false, excluded: false, blocked: false, execution: 'scheduled', dependencies: [], repository: repo });
const node = (n, status, title) => ({ id: `item-${n}`, status: { name: status }, content: { __typename: 'Issue', number: n, title, state: 'OPEN', repository: { nameWithOwner: repo } } });

const NOON = Date.parse('2026-11-10T12:00:00Z');
function harness({ cards, pulls = [], usage = { allowed: true, extraUsageEnabled: false, snapshot: null }, enabled = true, dryRun, now = NOON, lastWorkerDaily } = {}) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'tick-'));
  const config = { stateDirectory: dir, dailyTime: '02:00', claudeUsage: {}, boardPickup: { enabled, ...(dryRun === undefined ? {} : { dryRun }), projectId: 'P', repositories: [repo], executionRepositories: [repo], promptRepository: repo } };
  if (lastWorkerDaily) { const seeded = readState(dir); seeded.lastWorkerDaily = lastWorkerDaily; atomic(path.join(dir, 'state.json'), seeded); }
  const comments = [], calls = [];
  const io = {
    moveToBlocked: async id => calls.push('blocked:' + id),
    hasComment: async (r, i, marker) => comments.some(c => c.body.includes(marker)),
    comment: async (r, i, body) => { comments.push({ r, i, body }); }, closeIssue: async () => calls.push('close'), setDone: async () => calls.push('done'),
    pull: async () => ({ state: 'MERGED', mergeSha: 'c'.repeat(40), url: 'u' }),
    release: async () => ({ tag: 't', url: 'u', sourceSha: 'b'.repeat(40), includesMerge: true, version: '0.5.4', assets: [{ name: 'Songsmith-Plugins-0.5.4-win64-setup.exe', size: 1 }, { name: 'Songsmith-Plugins-0.5.4-macOS.pkg', size: 2 }] }),
    r2: async () => [{ name: 'Songsmith-Plugins-0.5.4-win64-setup.exe', size: 1 }, { name: 'Songsmith-Plugins-0.5.4-macOS.pkg', size: 2 }],
    site: async () => ({ version: '0.5.4', changelogVersions: ['0.5.4'] }),
    downloads: async () => ({ windows: { ok: true, bytes: 1 }, macos: { ok: true, bytes: 2 } })
  };
  const graphql = async (query, vars) => vars.owner
    ? { data: { repository: { pullRequests: { pageInfo: { hasNextPage: false }, nodes: pulls.filter(p => p.state === vars.states[0]) } } } }
    : { data: { node: { views: { totalCount: 1 }, items: { totalCount: cards.length, pageInfo: { hasNextPage: false }, nodes: cards } } } };
  const snapshot = { source: 'claude-code-usage-report-weekly-all', percentage: 10, observedAt: new Date(now - 1000).toISOString(), resetsAt: new Date(now + 3600000).toISOString() };
  const deps = { graphql, io, jobId: 'job-9', clock: () => now, loadPrompts: async () => ({ prompts: ['A-01', 'B-01'].map(prompt), revision }),
    loadUsage: () => ({ ...usage, snapshot: usage.snapshot ?? snapshot, reason: usage.reason }) };
  return { config, deps, dir, comments, calls };
}
const todo = [node(1, 'Todo', 'A-01 — first'), node(2, 'Todo', 'B-01 — second')];

test('disabled by default: nothing is read or written', async () => {
  const h = harness({ cards: todo, enabled: false });
  assert.deepEqual(await boardTick(h.config, h.deps), { ran: false, reason: 'Board pickup is not enabled' });
});

test('dry run is the default: reports the pick, but claims, comments and writes nothing', async () => {
  const h = harness({ cards: todo });
  const result = await boardTick(h.config, h.deps);
  assert.deepEqual([result.dryRun, result.selected.taskId], [true, 'A-01']);
  assert.equal(h.comments.length, 0);
  assert.deepEqual(readState(h.dir).tasks, {});
});

test('live tick claims the top card and comments; the next tick sees the active job', async () => {
  const h = harness({ cards: todo, dryRun: false });
  const first = await boardTick(h.config, h.deps);
  assert.equal(first.selected.taskId, 'A-01'); assert.equal(first.claim.claimed, true);
  assert.equal(h.comments.length, 1); assert.equal(readState(h.dir).tasks['A-01'].state, 'claimed');
  const second = await boardTick(h.config, h.deps);
  assert.equal(second.selected, null); assert.match(second.blocker, /Active job job-9/);
  assert.equal(readState(h.dir).tasks['B-01'].state, 'pending');
  assert.equal(h.comments.length, 1);
});

test('usage at 60% or an unverified allowance stops pickup', async () => {
  const blocked = harness({ cards: todo, dryRun: false, usage: { allowed: false, reason: 'Weekly usage at or above 60%' } });
  assert.match((await boardTick(blocked.config, blocked.deps)).blocker, /60%/);
  assert.equal(Object.values(readState(blocked.dir).tasks).some(task => task.state === 'claimed'), false);
  const paid = harness({ cards: todo, dryRun: false, usage: { allowed: true, extraUsageEnabled: true } });
  assert.equal((await boardTick(paid.config, paid.deps)).selected, null);
});

test('inconsistent Doing cards stop pickup entirely', async () => {
  const closed = { ...node(3, 'Doing', 'A-01 — x') }; closed.content.state = 'CLOSED';
  const h = harness({ cards: [closed, ...todo], dryRun: false });
  const result = await boardTick(h.config, h.deps);
  assert.equal(result.selected, null); assert.match(result.blocker, /Doing cards disagree/);
  assert.equal(h.comments.length, 0);
});

test('a board or pull request read failure does no work', async () => {
  const h = harness({ cards: todo, dryRun: false });
  const result = await boardTick(h.config, { ...h.deps, graphql: async () => { throw new Error('offline'); } });
  assert.equal(result.selected, null); assert.match(result.blocker, /read failed: offline/);
});

test('merge hook: a merged Matrix pull request moves a claimed task through delivery to Done', async () => {
  const h = harness({ cards: todo, dryRun: false });
  await boardTick(h.config, h.deps);
  const merged = harness({ cards: [node(1, 'Doing', 'A-01 — first')], dryRun: false, pulls: [{ number: 180, state: 'MERGED', title: 'A-01: Matrix implementation', headRefName: 'matrix/a-01-abc123' }] });
  merged.config.stateDirectory = h.dir; merged.comments.push(...h.comments);
  const result = await boardTick(merged.config, merged.deps);
  assert.deepEqual(result.recordedMerges, ['A-01']);
  assert.equal(result.delivery['A-01'].stage, 'done');
  assert.deepEqual(merged.calls, ['close', 'done']);
  assert.equal(readState(h.dir).deliveries['A-01'].stage, 'done');
});

test('syncMerges ignores prompt-only pull requests and tasks with no pickup record', () => {
  const state = { tasks: { 'A-01': { id: 'A-01', job: 'j', itemId: 'i', issue: 1, repository: repo }, 'B-01': { id: 'B-01' } } };
  assert.deepEqual(syncMerges(state, [{ number: 1, state: 'MERGED', headRefName: 'a-01-prompt', repository: repo }, { number: 2, state: 'MERGED', headRefName: 'matrix/b-01-abc123', repository: repo }]), []);
  assert.deepEqual(syncMerges(state, [{ number: 3, state: 'MERGED', headRefName: 'matrix/a-01-abc123', repository: repo }]), ['A-01']);
  assert.deepEqual(syncMerges(state, [{ number: 4, state: 'MERGED', headRefName: 'matrix/a-01-abc123', repository: repo }]), []);
});

test('refreshTasks never disturbs a claimed task or reopens finished ones', () => {
  const state = { tasks: { 'A-01': { id: 'A-01', state: 'claimed', job: 'j', hash: 'old' }, 'B-01': { id: 'B-01', state: 'interrupted', hash: 'old' } } };
  refreshTasks(state, [prompt('A-01'), prompt('B-01'), prompt('C-01')], revision);
  assert.deepEqual([state.tasks['A-01'].state, state.tasks['A-01'].hash, state.tasks['A-01'].job], ['claimed', 'old', 'j']);
  assert.equal(state.tasks['B-01'].state, 'interrupted');
  assert.equal(state.tasks['C-01'].state, 'pending');
});

// ---- hand-off mode and overnight window ----
import { readHandoff, adoptHandoff, writeHandoff, clearHandoff } from '../handoff.mjs';
import { londonDay } from '../policy.mjs';

test('hand-off mode writes the choice for the worker and claims nothing', async () => {
  const h = harness({ cards: todo, dryRun: false });
  const result = await boardTick(h.config, { ...h.deps, handoff: true });
  assert.equal(result.handoff, true); assert.equal(result.selected.taskId, 'A-01');
  const file = readHandoff(h.dir, { now: h.deps.clock() });
  assert.deepEqual([file.taskId, file.job, file.status, file.issue], ['A-01', 'job-9', 'Todo', 1]);
  assert.equal(Object.values(readState(h.dir).tasks).some(task => task.state === 'claimed'), false);
  assert.equal(h.comments.length, 0);
});

test('a tick clears any earlier hand-off, even when it then selects nothing', async () => {
  const h = harness({ cards: todo, dryRun: false });
  await boardTick(h.config, { ...h.deps, handoff: true });
  const blocked = { ...h.deps, handoff: true, loadUsage: () => ({ allowed: false, reason: 'Weekly usage at or above 60%' }) };
  assert.equal((await boardTick(h.config, blocked)).selected, null);
  assert.equal(readHandoff(h.dir, { now: h.deps.clock() }), null);
});

test('a Doing card with no claim is a run-now request: picked ahead of Todo', async () => {
  const stray = harness({ cards: [node(2, 'Todo', 'B-01 — second'), node(1, 'Doing', 'A-01 — first')], dryRun: false });
  const result = await boardTick(stray.config, { ...stray.deps, handoff: true });
  assert.equal(result.selected.taskId, 'A-01');
});

test('hand-off validation: fresh, complete and unchanged only', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'handoff-'));
  const sel = { taskId: 'A-01', promptHash: 'h1', status: 'Todo', issue: 1, itemId: 'i', repository: repo, issueRepository: repo };
  const t0 = Date.now();
  writeHandoff(dir, { selected: sel, job: 'j', revision, now: t0 });
  assert.equal(readHandoff(dir, { now: t0 + 1000 }).taskId, 'A-01');
  assert.equal(readHandoff(dir, { now: t0 + 16 * 60000 }), null, 'stale');
  assert.equal(readHandoff(dir, { now: t0 - 60000 }), null, 'from the future');
  fs.writeFileSync(path.join(dir, 'board-handoff.json'), '{"schema":1}');
  assert.equal(readHandoff(dir, { now: t0 }), null, 'incomplete');
  fs.writeFileSync(path.join(dir, 'board-handoff.json'), 'not json');
  assert.equal(readHandoff(dir, { now: t0 }), null, 'corrupt');
  clearHandoff(dir); clearHandoff(dir);
  const state = { tasks: { 'A-01': { id: 'A-01', state: 'pending' } } };
  const handoff = { taskId: 'A-01', promptHash: 'h1' };
  assert.ok(adoptHandoff(state, [{ id: 'A-01', hash: 'h1' }], handoff).prompt);
  assert.match(adoptHandoff(state, [{ id: 'A-01', hash: 'h2' }], handoff).reason, /changed/);
  assert.match(adoptHandoff(state, [], handoff).reason, /no longer exists/);
  assert.match(adoptHandoff({ tasks: { 'A-01': { state: 'claimed' } } }, [{ id: 'A-01', hash: 'h1' }], handoff).reason, /not pending \(claimed\)/);
});

test('Todo is the overnight queue: one a day after dailyTime; Doing is offered at every run', async () => {
  const mixed = [node(1, 'Todo', 'A-01 — first'), node(2, 'Doing', 'B-01 — second')];
  const used = harness({ cards: [node(1, 'Todo', 'A-01 — first')], lastWorkerDaily: '2026-11-10' });
  assert.equal((await boardTick(used.config, used.deps)).selected, null, 'the slot for this London day is used');
  const early = harness({ cards: [node(1, 'Todo', 'A-01 — first')], now: Date.parse('2026-11-10T01:30:00Z'), lastWorkerDaily: '2026-11-09' });
  assert.equal((await boardTick(early.config, early.deps)).selected, null, 'before 02:00 London');
  const due = harness({ cards: [node(1, 'Todo', 'A-01 — first')], now: Date.parse('2026-11-10T02:30:00Z'), lastWorkerDaily: '2026-11-09' });
  assert.equal((await boardTick(due.config, due.deps)).selected.taskId, 'A-01', 'after 02:00 London with yesterday as the last slot');
  const run = harness({ cards: mixed, lastWorkerDaily: '2026-11-10' });
  assert.equal((await boardTick(run.config, run.deps)).selected.taskId, 'B-01', 'Doing ignores the daily slot');
});

test('a live Todo pickup uses up the day; a Doing pickup does not', async () => {
  const t1 = harness({ cards: [node(1, 'Todo', 'A-01 — first')], dryRun: false, lastWorkerDaily: '2026-11-09' });
  assert.equal((await boardTick(t1.config, t1.deps)).selected.taskId, 'A-01');
  assert.equal(readState(t1.dir).lastWorkerDaily, '2026-11-10');
  const d1 = harness({ cards: [node(1, 'Doing', 'A-01 — first')], dryRun: false, lastWorkerDaily: '2026-11-09' });
  assert.equal((await boardTick(d1.config, d1.deps)).selected.taskId, 'A-01');
  assert.equal(readState(d1.dir).lastWorkerDaily, '2026-11-09');
});

test('the tick moves stopped work to Blocked, and does not let it hold up other work', async () => {
  const h = harness({ cards: [node(1, 'Doing', 'A-01 — first'), node(2, 'Doing', 'B-01 — second')], dryRun: false });
  const state = readState(h.dir);
  state.tasks['A-01'] = { id: 'A-01', state: 'interrupted', attempts: 1, job: 'old', issue: 1, itemId: 'item-1', repository: repo, issueRepository: repo, reason: 'Agent produced no changes' };
  atomic(path.join(h.dir, 'state.json'), state);
  const result = await boardTick(h.config, { ...h.deps, handoff: true });
  assert.ok(h.calls.includes('blocked:item-1'));
  assert.equal(result.selected.taskId, 'B-01', 'the blocked card no longer stops the queue');
  assert.ok(h.comments.some(c => c.body.includes('A-01 is Blocked')));
});

test('the tick resumes blocked work whose card an owner moved back to Todo', async () => {
  const h = harness({ cards: [node(1, 'Todo', 'A-01 — first')], dryRun: false });
  const state = readState(h.dir);
  state.tasks['A-01'] = { id: 'A-01', state: 'interrupted', attempts: 2, job: 'old', issue: 1, itemId: 'item-1', repository: repo, issueRepository: repo, reason: 'x', blockedCommentAt: 't' };
  atomic(path.join(h.dir, 'state.json'), state);
  const result = await boardTick(h.config, { ...h.deps, handoff: true });
  assert.equal(result.selected.taskId, 'A-01');
  assert.ok(readState(h.dir).tasks['A-01'].resumedAt);
});

test('an invalid prompt file does not stop a merged task being delivered, but nothing new is picked up', async () => {
  const h = harness({ cards: todo, dryRun: false });
  await boardTick(h.config, h.deps);
  const merged = harness({ cards: [node(1, 'Doing', 'A-01 — first'), node(2, 'Todo', 'B-01 — second')], dryRun: false, pulls: [{ number: 180, state: 'MERGED', title: 'A-01: Matrix implementation', headRefName: 'matrix/a-01-abc123' }] });
  merged.config.stateDirectory = h.dir; merged.comments.push(...h.comments);
  merged.deps.loadPrompts = async () => { throw new Error('Backlog validation failed'); };
  const result = await boardTick(merged.config, merged.deps);
  assert.deepEqual(result.recordedMerges, ['A-01']);
  assert.equal(result.delivery['A-01'].stage, 'done');
  assert.deepEqual(merged.calls, ['close', 'done']);
  assert.equal(result.selected, null); assert.match(result.blocker, /Prompt backlog is invalid/);
});

test('the tick recovers a restart-interrupted task with an open pull request instead of parking it in Blocked', async () => {
  const sha = 'a'.repeat(40);
  const h = harness({ cards: [node(1, 'Doing', 'A-01 — first')], dryRun: false, pulls: [{ number: 9, state: 'OPEN', title: 'A-01: Matrix implementation', headRefName: 'matrix/a-01-abc123' }] });
  h.deps.io.prChecks = async () => ({ headSha: sha, checks: [{ status: 'IN_PROGRESS' }] });
  const state = readState(h.dir);
  state.tasks['A-01'] = { id: 'A-01', state: 'interrupted', reason: 'Worker restarted; inspect retained work before retry', attempts: 1, job: 'old', issue: 1, itemId: 'item-1', repository: repo, issueRepository: repo, branch: 'matrix/a-01-abc123', commit: sha, phase: 'pushed' };
  atomic(path.join(h.dir, 'state.json'), state);
  await boardTick(h.config, h.deps);
  assert.equal(readState(h.dir).tasks['A-01'].state, 'awaiting-review');
  assert.ok(!h.calls.includes('blocked:item-1'));
});

const SPEC = '## Objective\nAdd a thing musicians can hear in the plugin.\n\n## Implementation\nChange the engine and the editor labels for it.\n\n## Acceptance criteria and tests\nThe thing is audible and a unit test covers it.\n';

test('a Doing card with no prompt file is built from its issue: the prompt is generated and handed to the worker', async () => {
  const h = harness({ cards: [node(9, 'Doing', 'Z-09 — Add a thing')], dryRun: false });
  h.deps.io.issueBody = async () => SPEC;
  const result = await boardTick(h.config, { ...h.deps, handoff: true });
  assert.equal(result.selected.taskId, 'Z-09');
  const handoff = JSON.parse(fs.readFileSync(path.join(h.dir, 'board-handoff.json'), 'utf8'));
  assert.equal(handoff.taskId, 'Z-09'); assert.match(handoff.generated.file, /^shared\/prompts\/Z-09-add-a-thing\.md$/); assert.match(handoff.generated.text, /Generated by Matrix/);
  assert.equal(h.comments.length, 0);
});

test('a Doing card with no prompt file and a thin issue gets one comment and moves to Blocked; other cards carry on', async () => {
  const h = harness({ cards: [node(9, 'Doing', 'Z-09 — Add a thing'), node(1, 'Doing', 'A-01 — first')], dryRun: false });
  h.deps.io.issueBody = async () => '## Objective\nDo the thing, and do it well for musicians.';
  const result = await boardTick(h.config, { ...h.deps, handoff: true });
  assert.ok(h.calls.includes('blocked:item-9'));
  assert.ok(h.comments.some(c => c.body.includes('Z-09 is Blocked') && c.body.includes('acceptance criteria')));
  assert.equal(result.selected.taskId, 'A-01');
});

test('in a dry run the issue is assessed but nothing is commented or moved', async () => {
  const h = harness({ cards: [node(9, 'Doing', 'Z-09 — Add a thing')] });
  h.deps.io.issueBody = async () => 'Just do it.';
  await boardTick(h.config, h.deps);
  assert.equal(h.comments.length, 0); assert.ok(!h.calls.includes('blocked:item-9'));
});
