import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { blockStopped, resumeBlocked, blockedComment, blockedMarker, heldMarker } from '../blocked.mjs';
import { readState, atomic } from '../state.mjs';

function seed(tasks) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'blocked-'));
  const state = readState(dir);
  for (const [id, extra] of Object.entries(tasks)) state.tasks[id] = { id, state: 'interrupted', attempts: 1, job: 'job-1', issue: 7, itemId: 'item-' + id, repository: 'o/r', issueRepository: 'o/r', reason: 'Agent produced no changes', ...extra };
  atomic(path.join(dir, 'state.json'), state);
  return dir;
}
function fakeIo({ failMove = false, failComment = false, existing = [] } = {}) {
  const log = { moves: [], comments: [] };
  return { log, io: {
    moveToBlocked: async id => { if (failMove) throw Error('board down'); log.moves.push(id); },
    hasComment: async (r, i, marker) => existing.includes(marker) || log.comments.some(c => c.body.includes(marker)),
    comment: async (r, i, body) => { if (failComment) throw Error('GitHub down'); log.comments.push({ r, i, body }); } } };
}
const board = (...cards) => ({ ok: true, cards: cards.map(([taskId, status, issue = 7]) => ({ taskId, status, issue })) });

test('an interrupted task is reported once and its card moves to Blocked', async () => {
  const dir = seed({ 'A-01': {} }), { io, log } = fakeIo();
  await blockStopped(dir, io); await blockStopped(dir, io);
  assert.deepEqual(log.moves, ['item-A-01']);
  assert.equal(log.comments.length, 1);
  assert.match(log.comments[0].body, /A-01 is Blocked/); assert.match(log.comments[0].body, /Agent produced no changes/);
  assert.match(log.comments[0].body, /move this card back to Todo or Doing/);
  assert.ok(readState(dir).tasks['A-01'].blockedCommentAt);
});

test('dead-lettered tasks are reported too; claimed, pending and complete tasks are not', async () => {
  const dir = seed({ 'A-01': { state: 'dead-letter' }, 'B-01': { state: 'claimed' }, 'C-01': { state: 'pending' }, 'D-01': { state: 'complete' }, 'E-01': { issue: undefined } });
  const { io, log } = fakeIo(); await blockStopped(dir, io);
  assert.deepEqual(log.moves, ['item-A-01']);
});

test('a failed comment or board move keeps the task for retry and never loses the report', async () => {
  const dir = seed({ 'A-01': {} });
  const bad = fakeIo({ failComment: true }); await blockStopped(dir, bad.io);
  assert.equal(readState(dir).tasks['A-01'].blockedCommentAt, undefined);
  assert.ok(readState(dir).tasks['A-01'].movedBlockedAt, 'the move already happened and is not repeated');
  const good = fakeIo(); await blockStopped(dir, good.io);
  assert.deepEqual(good.log.moves, [], 'no second move'); assert.equal(good.log.comments.length, 1);
  assert.ok(readState(dir).tasks['A-01'].blockedCommentAt);
  assert.ok(readState(dir).history.some(h => h.event === 'blocked-failed'));
});

test('a comment that already exists is not posted again after a crash', async () => {
  const dir = seed({ 'A-01': {} }), { io, log } = fakeIo({ existing: [blockedMarker('A-01', 'job-1')] });
  await blockStopped(dir, io); assert.equal(log.comments.length, 0);
  assert.ok(readState(dir).tasks['A-01'].blockedCommentAt);
});

test('the comment says what to do next, with and without committed work', () => {
  assert.match(blockedComment({ id: 'A-01', job: 'j', attempts: 1, reason: 'r' }), /starts the task again from a clean worktree/);
  const withWork = blockedComment({ id: 'A-01', job: 'j', attempts: 2, reason: 'Run time limit', commit: 'abc', branch: 'matrix/a-01-123', pr: 'https://x/pull/9' });
  assert.match(withWork, /committed work/); assert.match(withWork, /matrix\/a-01-123/); assert.match(withWork, /pull\/9/);
});

test('moving the card back to Todo or Doing resumes a task with no committed work, with a fresh budget', async () => {
  for (const status of ['Todo', 'Doing']) {
    const dir = seed({ 'A-01': { blockedCommentAt: 't', movedBlockedAt: 't', claimedAt: 't', runStartedAt: 't', attempts: 2, phase: 'interrupted' } });
    const { io } = fakeIo(); const r = await resumeBlocked(dir, board(['A-01', status]), io);
    assert.deepEqual(r.resumed, ['A-01'], status);
    const task = readState(dir).tasks['A-01'];
    assert.equal(task.state, 'pending'); assert.equal(task.attempts, 0); assert.ok(task.resumedAt);
    for (const key of ['blockedCommentAt', 'runStartedAt', 'job', 'reason', 'claimedAt']) assert.equal(task[key], undefined, key);
  }
});

test('a card still in Blocked, Backlog or Done is not resumed; nothing is retried by a poll', async () => {
  for (const status of ['Blocked', 'Backlog', 'Done']) {
    const dir = seed({ 'A-01': { blockedCommentAt: 't' } }), { io } = fakeIo();
    assert.deepEqual((await resumeBlocked(dir, board(['A-01', status]), io)).resumed, [], status);
    assert.equal(readState(dir).tasks['A-01'].state, 'interrupted');
  }
});

test('committed work is never started again: one comment and the card goes back to Blocked', async () => {
  const dir = seed({ 'A-01': { blockedCommentAt: 't', commit: 'abc123', branch: 'matrix/a-01-123', pr: 'https://x/pull/9' } }), { io, log } = fakeIo();
  const first = await resumeBlocked(dir, board(['A-01', 'Todo']), io); await resumeBlocked(dir, board(['A-01', 'Todo']), io);
  assert.deepEqual(first.held, ['A-01']); assert.deepEqual(first.resumed, []);
  assert.equal(readState(dir).tasks['A-01'].state, 'interrupted');
  assert.deepEqual(log.moves, ['item-A-01', 'item-A-01']);
  assert.equal(log.comments.length, 1); assert.match(log.comments[0].body, new RegExp(heldMarker('A-01', 'job-1').replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&')));
  assert.match(log.comments[0].body, /would duplicate it/);
});

test('a resume remembers the stopped job so the next starting comment can say it is a resume', async () => {
  const dir = seed({ 'A-01': { blockedCommentAt: 't', branch: 'matrix/a-01-abc', phase: 'pushed', pickup: { reason: 'Run now (Doing)' } } }), { io } = fakeIo();
  await resumeBlocked(dir, board(['A-01', 'Todo']), io);
  const task = readState(dir).tasks['A-01'];
  assert.equal(task.resumedFrom.job, 'job-1'); assert.match(task.resumedFrom.reason, /no changes/); assert.equal(task.resumedFrom.checkpoint, 'pushed');
  assert.equal(task.pickup, undefined, 'the old pickup reason is cleared');
});
