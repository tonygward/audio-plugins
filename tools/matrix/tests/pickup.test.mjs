import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { pickUp, recoverPickupComments, pickupComment, marker } from '../pickup.mjs';
import { readState, reconcile, atomic } from '../state.mjs';

const revision = 'a'.repeat(40);
const prompt = id => ({ id, file: `shared/prompts/${id}.md`, hash: `hash-${id}-0123456789`, complete: false, deferred: false, excluded: false, blocked: false, execution: 'scheduled' });
const selected = (taskId, extra = {}) => ({ taskId, issue: 161, itemId: 'i1', repository: 'tonygward/audio-plugins', issueRepository: 'tonygward/songsmithstudio', promptHash: prompt(taskId).hash, ...extra });

function setup(ids = ['MAX-07', 'B-01']) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'pickup-'));
  const state = readState(dir);
  reconcile(state, ids.map(prompt), revision);
  atomic(path.join(dir, 'state.json'), state);
  return dir;
}
function io(overrides = {}) {
  const calls = { comments: [], checks: [] };
  return { calls, hasComment: async (...a) => { calls.checks.push(a); return overrides.existing ?? false; },
    comment: async (...a) => { if (overrides.fail) throw new Error('GitHub unavailable'); calls.comments.push(a); } };
}

test('claim is saved, then the issue gets one pickup comment with job, revision and repository', async () => {
  const dir = setup(), fake = io();
  const result = await pickUp(dir, selected('MAX-07'), { job: 'job-1', revision, io: fake });
  assert.deepEqual([result.claimed, result.comment.posted], [true, true]);
  const task = readState(dir).tasks['MAX-07'];
  assert.equal(task.state, 'claimed'); assert.equal(task.job, 'job-1'); assert.ok(task.pickupCommentAt);
  const [slug, issue, body] = fake.calls.comments[0];
  assert.equal(slug, 'tonygward/songsmithstudio'); assert.equal(issue, 161);
  for (const text of ['MAX-07', 'job-1', revision.slice(0, 12), 'tonygward/audio-plugins', marker('job-1')]) assert.ok(body.includes(text), text);
});

test('a failed comment keeps the claim, and recovery posts it exactly once', async () => {
  const dir = setup();
  const failed = await pickUp(dir, selected('MAX-07'), { job: 'job-1', revision, io: io({ fail: true }) });
  assert.equal(failed.claimed, true); assert.equal(failed.comment.posted, false);
  assert.equal(readState(dir).tasks['MAX-07'].state, 'claimed');
  assert.equal(readState(dir).tasks['MAX-07'].pickupCommentAt, null);
  const fake = io();
  assert.equal((await recoverPickupComments(dir, { revision, io: fake }))['MAX-07'].posted, true);
  assert.deepEqual(await recoverPickupComments(dir, { revision, io: fake }), {});
  assert.equal(fake.calls.comments.length, 1);
});

test('an existing marker comment is not duplicated after a restart', async () => {
  const dir = setup(), fake = io({ existing: true });
  await pickUp(dir, selected('MAX-07'), { job: 'job-1', revision, io: fake });
  assert.equal(fake.calls.comments.length, 0);
  assert.ok(readState(dir).tasks['MAX-07'].pickupCommentAt);
});

test('only one job can be active: a second claim is refused and posts nothing', async () => {
  const dir = setup(), fake = io();
  await pickUp(dir, selected('MAX-07'), { job: 'job-1', revision, io: fake });
  const second = await pickUp(dir, selected('B-01', { issue: 2 }), { job: 'job-2', revision, io: fake });
  assert.equal(second.claimed, false); assert.match(second.reason, /already active/);
  assert.equal(readState(dir).tasks['B-01'].state, 'pending');
  assert.equal(fake.calls.comments.length, 1);
});

test('concurrent invocations cannot create two jobs', async () => {
  const dir = setup(), fake = io();
  const results = await Promise.allSettled([pickUp(dir, selected('MAX-07'), { job: 'job-1', revision, io: fake }), pickUp(dir, selected('B-01'), { job: 'job-2', revision, io: fake })]);
  const claimed = Object.values(readState(dir).tasks).filter(task => task.state === 'claimed');
  assert.equal(claimed.length, 1);
  assert.equal(results.filter(r => r.status === 'fulfilled' && r.value.claimed).length, 1);
  assert.equal(fs.existsSync(path.join(dir, 'worker.lock')), false);
});

test('a changed prompt, a new revision or an unknown task is refused', async () => {
  const dir = setup(), fake = io();
  assert.equal((await pickUp(dir, selected('MAX-07', { promptHash: 'edited' }), { job: 'j', revision, io: fake })).claimed, false);
  assert.equal((await pickUp(dir, selected('MAX-07'), { job: 'j', revision: 'b'.repeat(40), io: fake })).claimed, false);
  assert.equal((await pickUp(dir, selected('X-99'), { job: 'j', revision, io: fake })).claimed, false);
  assert.equal(fake.calls.comments.length, 0);
});

test('pickup comment says what happens next in plain words', () => {
  const body = pickupComment({ taskId: 'MAX-07', job: 'job-1', promptHash: 'h'.repeat(20), revision, repository: 'tonygward/audio-plugins' });
  assert.match(body, /Matrix has picked up \*\*MAX-07\*\*/);
  assert.match(body, /Nothing is merged until the required checks pass/);
});

// ---- AUTO-14: why was this card picked up ----
import { pickupProvenance, provenanceLines, UNKNOWN_REASON } from '../pickup.mjs';
const AT = '2026-11-10T21:36:41.000Z';

test('provenance: Todo is Scheduled with the London date, Doing is Run now, neither claims a human actor', () => {
  const todo = pickupProvenance({ status: 'Todo', selectedAt: AT });
  assert.deepEqual([todo.reason, todo.preClaimStatus, todo.scheduleDate, todo.initiator], ['Scheduled (Todo)', 'Todo', '2026-11-10', 'Scheduled overnight queue']);
  const doing = pickupProvenance({ status: 'Doing', selectedAt: AT });
  assert.deepEqual([doing.reason, doing.scheduleDate], ['Run now (Doing)', undefined]);
  assert.match(doing.initiator, /^Unknown/); assert.doesNotMatch(JSON.stringify(doing), /Tony|manually/i);
});

test('provenance: a resume names the earlier job and checkpoint; anything unreliable is Unknown, never guessed', () => {
  const resumed = pickupProvenance({ status: 'Todo', selectedAt: AT, resumedFrom: { job: 'old-job', reason: 'Run time limit', branch: 'matrix/a-01-abc' } });
  assert.equal(resumed.reason, 'Resumed (Todo)'); assert.equal(resumed.resumedFrom.job, 'old-job');
  for (const bad of [{}, { status: 'Blocked', selectedAt: AT }, { status: 'Doing', selectedAt: 'not a date' }, { status: 'Done', selectedAt: AT }]) assert.equal(pickupProvenance(bad).reason, UNKNOWN_REASON);
});

test('the starting comment shows the reason, the status before the claim and the London time; unknown says so', () => {
  const body = pickupComment({ taskId: 'MAX-07', job: 'j', promptHash: 'h'.repeat(20), revision, repository: 'r', provenance: pickupProvenance({ status: 'Doing', selectedAt: AT }) });
  assert.match(body, /- Pickup reason: Run now \(Doing\)/); assert.match(body, /- Before claim: Doing/); assert.match(body, /- Selected at: 2026-11-10 21:36 Europe\/London/);
  assert.match(body, /- Initiator: Unknown \(board status requested immediate processing\)/); assert.match(body, /- Matrix job: `j`/);
  const scheduled = pickupComment({ taskId: 'A-01', job: 'j', promptHash: 'h'.repeat(20), revision, repository: 'r', provenance: pickupProvenance({ status: 'Todo', selectedAt: '2026-07-01T01:30:00Z' }) });
  assert.match(scheduled, /Selected at: 2026-07-01 02:30 Europe\/London/); assert.match(scheduled, /London schedule date: 2026-07-01/);
  assert.deepEqual(provenanceLines(undefined), [`- Pickup reason: ${UNKNOWN_REASON}`]);
});

test('the reason is saved with the claim and the comment keeps it, even after the card is Doing and the comment is retried later', async () => {
  const dir = setup(), failing = io({ fail: true });
  await pickUp(dir, selected('MAX-07', { status: 'Todo', provenance: pickupProvenance({ status: 'Todo', selectedAt: AT }) }), { job: 'job-1', revision, io: failing });
  assert.equal(readState(dir).tasks['MAX-07'].pickup.preClaimStatus, 'Todo');
  const good = io(); await recoverPickupComments(dir, { revision, io: good });
  assert.equal(good.calls.comments.length, 1);
  assert.match(good.calls.comments[0][2], /Pickup reason: Scheduled \(Todo\)/); assert.match(good.calls.comments[0][2], /Before claim: Todo/);
});

test('a claim with no provenance says Unknown and logs a diagnostic', async () => {
  const dir = setup(), fake = io();
  await pickUp(dir, selected('MAX-07'), { job: 'job-1', revision, io: fake });
  assert.match(fake.calls.comments[0][2], /Pickup reason: Unknown \(provenance unavailable\)/);
  assert.ok(readState(dir).history.some(h => h.event === 'pickup-provenance-unknown'));
});
