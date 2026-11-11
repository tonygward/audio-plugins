import test from 'node:test';
import assert from 'node:assert/strict';
import { ciMarker, ciPassedComment, postCiPassedComment } from '../ci-comment.mjs';

const fakeIo = (opts = {}) => {
  const posted = [];
  return {
    posted,
    hasComment: async (slug, issue, marker) => posted.some(p => p.slug === slug && p.issue === issue && p.body.includes(marker)),
    comment: async (slug, issue, body) => { if (opts.fail) throw new Error('GitHub 502'); posted.push({ slug, issue, body }); }
  };
};
const sha = 'a'.repeat(40), pr = 'https://github.com/o/plugins/pull/7', run = 'https://github.com/o/plugins/actions/runs/9';
const task = (extra = {}) => ({ id: 'ACOMP-07', job: 'matrix-1', issue: 160, repository: 'o/plugins', ...extra });

test('one comment per task, pull request and head commit, with the pull request, run link, commit and next step', async () => {
  const io = fakeIo(), t = task();
  assert.deepEqual(await postCiPassedComment(t, { sha, pr, runUrl: run }, io), { posted: true });
  const [{ body }] = io.posted;
  assert.match(body, /CI passed for \*\*ACOMP-07\*\*/); assert.match(body, /pull\/7/); assert.match(body, /actions\/runs\/9/); assert.match(body, /`aaaaaaaaaaaa`/);
  assert.match(body, /Windows and macOS/); assert.match(body, /not delivered yet/);
  assert.ok(body.startsWith(ciMarker({ taskId: 'ACOMP-07', pr, sha })));
});

test('reruns, repeated polls and restarts do not repeat the comment', async () => {
  const io = fakeIo(), t = task();
  await postCiPassedComment(t, { sha, pr, runUrl: run }, io);
  assert.equal((await postCiPassedComment(t, { sha, pr, runUrl: run }, io)).posted, false);
  // A restart that lost the saved record still finds the marker on the issue.
  assert.deepEqual(await postCiPassedComment(task(), { sha, pr, runUrl: run }, io), { posted: true });
  assert.equal(io.posted.length, 1);
});

test('an amended head gets its own note; the same head on a different pull request does too', async () => {
  const io = fakeIo(), t = task();
  await postCiPassedComment(t, { sha, pr, runUrl: run }, io);
  await postCiPassedComment(t, { sha: 'b'.repeat(40), pr, runUrl: run }, io);
  await postCiPassedComment(task(), { sha, pr: 'https://github.com/o/plugins/pull/8', runUrl: run }, io);
  assert.equal(io.posted.length, 3);
});

test('the note goes to the tracking issue in its own repository, never the pull request', async () => {
  const io = fakeIo(), t = task({ issue: 21, issueRepository: 'o/website', repository: 'o/plugins' });
  await postCiPassedComment(t, { sha, pr, runUrl: run }, io);
  assert.deepEqual([io.posted[0].slug, io.posted[0].issue], ['o/website', 21]);
});

test('no linked issue means no guess: the note stays pending with a reason', async () => {
  const io = fakeIo(), t = task({ issue: undefined });
  const r = await postCiPassedComment(t, { sha, pr, runUrl: run }, io);
  assert.equal(r.posted, false); assert.match(r.reason, /No linked tracking issue/); assert.equal(io.posted.length, 0);
  assert.equal(Object.values(t.ciComments)[0].pending, true);
});

test('an API failure is kept as retry state and is never reported as posted', async () => {
  const t = task(), r = await postCiPassedComment(t, { sha, pr, runUrl: run }, fakeIo({ fail: true }));
  assert.equal(r.posted, false);
  const entry = Object.values(t.ciComments)[0];
  assert.equal(entry.pending, true); assert.match(entry.error, /502/); assert.equal(entry.postedAt, undefined);
  const io = fakeIo();
  assert.equal((await postCiPassedComment(t, { sha: entry.sha, pr: entry.pr, runUrl: entry.runUrl }, io)).posted, true);
  assert.equal(Object.values(t.ciComments)[0].pending, undefined);
});

test('the comment never claims delivery, a merge, or Done', () => {
  const body = ciPassedComment({ taskId: 'X-01', pr, sha, runUrl: run, job: 'j' });
  assert.doesNotMatch(body, /has been (merged|released|deployed)|is now done/i);
  assert.match(body, /card stays in Doing/);
});
