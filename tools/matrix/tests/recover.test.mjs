import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { recoverInFlight, checkState, recoverable, CI_TIMEOUT_MS } from '../recover.mjs';
import { blockStopped } from '../blocked.mjs';
import { readState, atomic } from '../state.mjs';

const SHA = 'a'.repeat(40), repo = 'o/r';
function seed(extra = {}) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'recover-')), state = readState(dir);
  state.tasks['A-01'] = { id: 'A-01', state: 'interrupted', reason: 'Worker restarted; inspect retained work before retry', attempts: 1, job: 'j', issue: 7, itemId: 'i1', repository: repo, branch: 'matrix/a-01-abc', commit: SHA, phase: 'pushed', ...extra };
  atomic(path.join(dir, 'state.json'), state); return dir;
}
const pull = (state = 'OPEN') => [{ number: 9, state, headRefName: 'matrix/a-01-abc', repository: repo, url: 'https://github.com/o/r/pull/9' }];
const io = (headSha = SHA, checks = [{ status: 'COMPLETED', conclusion: 'SUCCESS' }]) => ({ prChecks: async () => ({ headSha, checks }) });

test('check roll-up: failure beats pending beats success; no checks yet is pending', () => {
  assert.equal(checkState([]), 'pending');
  assert.equal(checkState([{ status: 'COMPLETED', conclusion: 'SUCCESS' }, { status: 'COMPLETED', conclusion: 'SKIPPED' }]), 'success');
  assert.equal(checkState([{ status: 'COMPLETED', conclusion: 'SUCCESS' }, { status: 'IN_PROGRESS' }]), 'pending');
  assert.equal(checkState([{ status: 'IN_PROGRESS' }, { status: 'COMPLETED', conclusion: 'FAILURE' }]), 'failure');
});

test('a restart during CI with an open pull request is handed back to the merge gate, not blocked', async () => {
  for (const checks of [[{ status: 'COMPLETED', conclusion: 'SUCCESS' }], [{ status: 'IN_PROGRESS' }], []]) {
    const dir = seed(); const r = await recoverInFlight(dir, pull(), io(SHA, checks));
    assert.deepEqual(r.recovered, ['A-01']);
    const task = readState(dir).tasks['A-01'];
    assert.equal(task.state, 'awaiting-review'); assert.match(task.pr, /pull\/9$/); assert.equal(task.reason, undefined);
    const moved = []; await blockStopped(dir, { moveToBlocked: async id => moved.push(id), hasComment: async () => false, comment: async () => {} });
    assert.deepEqual(moved, [], 'recovered work is not parked in Blocked');
  }
});

test('a failed build, a different head or a closed pull request stays interrupted and is reported', async () => {
  const failing = seed(); assert.deepEqual((await recoverInFlight(failing, pull(), io(SHA, [{ status: 'COMPLETED', conclusion: 'FAILURE' }]))).recovered, []);
  assert.equal(readState(failing).tasks['A-01'].state, 'interrupted'); assert.match(readState(failing).tasks['A-01'].reason, /Build failed/);
  const moved = seed(); await recoverInFlight(moved, pull(), io('b'.repeat(40)));
  assert.match(readState(moved).tasks['A-01'].reason, /not the commit Matrix pushed/);
  const closed = seed(); assert.deepEqual((await recoverInFlight(closed, pull('CLOSED'), io())).recovered, []);
  assert.match(readState(closed).tasks['A-01'].reason, /Worker restarted/);
});

test('recovered work that stays pending past the CI timeout is held for review; a read failure changes nothing', async () => {
  const dir = seed(); await recoverInFlight(dir, pull(), io(SHA, [{ status: 'IN_PROGRESS' }]), { now: () => Date.now() });
  const later = () => Date.now() + CI_TIMEOUT_MS + 60000;
  await recoverInFlight(dir, pull(), io(SHA, [{ status: 'IN_PROGRESS' }]), { now: later });
  assert.equal(readState(dir).tasks['A-01'].state, 'interrupted'); assert.match(readState(dir).tasks['A-01'].reason, /90 minutes/);
  const broken = seed(); await recoverInFlight(broken, pull(), { prChecks: async () => { throw new Error('GitHub down'); } });
  assert.equal(readState(broken).tasks['A-01'].state, 'interrupted'); assert.ok(readState(broken).history.some(h => h.event === 'recovery-failed'));
});

test('only restart-interrupted tasks with a pushed commit are candidates; merged work is left to delivery', async () => {
  assert.equal(recoverable({ state: 'interrupted', reason: 'Agent produced no changes', branch: 'b', commit: 'c', issue: 1 }), false);
  assert.equal(recoverable({ state: 'interrupted', reason: 'Worker restarted; x', commit: 'c', issue: 1 }), false);
  const dir = seed(); assert.deepEqual((await recoverInFlight(dir, pull('MERGED'), io())).recovered, []);
  const delivered = seed(); const s = readState(delivered); s.deliveries = { 'A-01': { stage: 'pending' } }; atomic(path.join(delivered, 'state.json'), s);
  assert.deepEqual((await recoverInFlight(delivered, pull(), io())).recovered, []);
  const moved = []; await blockStopped(delivered, { moveToBlocked: async id => moved.push(id), hasComment: async () => false, comment: async () => {} });
  assert.deepEqual(moved, [], 'merged and delivering work is never parked in Blocked');
});
