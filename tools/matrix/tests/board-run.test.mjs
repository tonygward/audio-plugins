import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { boardTick, syncMerges, refreshTasks } from '../board-run.mjs';
import { readState } from '../state.mjs';

const repo = 'tonygward/audio-plugins', revision = 'a'.repeat(40);
const prompt = id => ({ id, file: `shared/prompts/${id}.md`, hash: `hash-${id}-0123456789`, complete: false, deferred: false, excluded: false, blocked: false, execution: 'scheduled', dependencies: [], repository: repo });
const node = (n, status, title) => ({ id: `item-${n}`, status: { name: status }, content: { __typename: 'Issue', number: n, title, state: 'OPEN', repository: { nameWithOwner: repo } } });

function harness({ cards, pulls = [], usage = { allowed: true, extraUsageEnabled: false, snapshot: null }, enabled = true, dryRun } = {}) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'tick-'));
  const config = { stateDirectory: dir, claudeUsage: {}, boardPickup: { enabled, ...(dryRun === undefined ? {} : { dryRun }), projectId: 'P', repositories: [repo], executionRepositories: [repo], promptRepository: repo } };
  const now = Date.now();
  const comments = [], calls = [];
  const io = {
    hasComment: async (r, i, marker) => comments.some(c => c.body.includes(marker)),
    comment: async (r, i, body) => { comments.push({ r, i, body }); }, closeIssue: async () => calls.push('close'), setDone: async () => calls.push('done'),
    pull: async () => ({ state: 'MERGED', mergeSha: 'c'.repeat(40), url: 'u' }),
    release: async () => ({ tag: 't', url: 'u', sourceSha: 'c'.repeat(40), version: '0.5.4', assets: [{ name: 'Songsmith-Plugins-0.5.4-win64-setup.exe', size: 1 }, { name: 'Songsmith-Plugins-0.5.4-macOS.pkg', size: 2 }] }),
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
