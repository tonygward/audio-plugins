import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { githubIo } from '../github-io.mjs';
import { advanceDelivery, recordMerge } from '../delivery.mjs';
import { readState, atomic } from '../state.mjs';

const repo = 'tonygward/audio-plugins', merge = 'a'.repeat(40);
const fakeGh = (routes, calls = []) => async args => {
  calls.push(args);
  const key = args.slice(0, 2).join(' ');
  const route = routes[key === 'api repos/x' ? 'api' : key] ?? routes[args[0]];
  if (route === undefined) throw new Error(`unexpected gh ${args.join(' ')}`);
  return typeof route === 'function' ? route(args) : route;
};

test('pull reads state, merge commit and url', async () => {
  const io = githubIo(fakeGh({ 'pr view': JSON.stringify({ state: 'MERGED', mergeCommit: { oid: merge }, url: 'u' }) }));
  assert.deepEqual(await io.pull(repo, 177), { state: 'MERGED', mergeSha: merge, url: 'u' });
  const open = githubIo(fakeGh({ 'pr view': JSON.stringify({ state: 'OPEN', mergeCommit: null, url: 'u' }) }));
  assert.equal((await open.pull(repo, 1)).mergeSha, null);
});

test('release picks the earliest release that contains the merge commit', async () => {
  const calls = [], compared = [];
  const io = githubIo(fakeGh({
    'release list': JSON.stringify([{ tagName: 'v0.5.4', publishedAt: '2026-11-10T00:00:00Z' }, { tagName: 'v0.5.2', publishedAt: '2026-11-08T00:00:00Z' }, { tagName: 'v0.5.3', publishedAt: '2026-11-09T00:00:00Z' }]),
    'release view': args => JSON.stringify({ tagName: args[2], url: `u/${args[2]}`, targetCommitish: `sha-${args[2]}`, assets: [{ name: `a-${args[2]}`, size: 5, extra: 1 }] }),
    api: args => { compared.push(args[1]); return args[1].endsWith('sha-v0.5.2') ? 'behind\n' : 'ahead\n'; }
  }, calls));
  const release = await io.release(merge, repo);
  assert.equal(release.version, '0.5.3'); assert.equal(release.includesMerge, true);
  assert.deepEqual(release.assets, [{ name: 'a-v0.5.3', size: 5 }]);
  assert.deepEqual(compared.map(c => c.split('...')[1]), ['sha-v0.5.2', 'sha-v0.5.3']);
});

test('release returns null when no release contains the merge', async () => {
  const io = githubIo(fakeGh({ 'release list': JSON.stringify([{ tagName: 'v0.5.2', publishedAt: '2026-11-08T00:00:00Z' }]),
    'release view': JSON.stringify({ tagName: 'v0.5.2', url: 'u', targetCommitish: 's', assets: [] }), api: 'diverged\n' }));
  assert.equal(await io.release(merge, repo), null);
});

test('comments, closing and Done use the expected gh arguments', async () => {
  const calls = [];
  const io = githubIo(fakeGh({ 'issue view': JSON.stringify({ comments: [{ body: 'hello <!-- m:1 -->' }] }), issue: '', 'api graphql': '{}' }, calls), { projectId: 'P', statusFieldId: 'F', doneOptionId: 'O' });
  assert.equal(await io.hasComment(repo, 4, '<!-- m:1 -->'), true);
  assert.equal(await io.hasComment(repo, 4, '<!-- m:2 -->'), false);
  await io.comment(repo, 4, 'body text'); await io.closeIssue(repo, 4); await io.setDone('item-1');
  assert.deepEqual(calls.slice(2).map(c => c.join(' ')), [
    `issue comment 4 --repo ${repo} --body body text`, `issue close 4 --repo ${repo} --reason completed`,
    'api graphql -f query=' + calls.at(-1)[3].slice(6) + ' -f p=P -f i=item-1 -f f=F -f o=O']);
  assert.match(calls.at(-1)[3], /updateProjectV2ItemFieldValue/);
});

test('setDone refuses to run without the board field configuration', async () => {
  await assert.rejects(githubIo(fakeGh({})).setDone('item-1'), /not configured/);
});

test('without storage, website and download verifiers a merged task never reaches Done', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'gio-')), state = readState(dir);
  state.tasks['A-01'] = { id: 'A-01', state: 'claimed', job: 'j', issue: 4, itemId: 'item-1', repository: repo, issueRepository: repo };
  recordMerge(state, 'A-01', { repository: repo, pull: 177 });
  atomic(path.join(dir, 'state.json'), state);
  const calls = [];
  const gh = fakeGh({
    'pr view': JSON.stringify({ state: 'MERGED', mergeCommit: { oid: merge }, url: 'u' }),
    'release list': JSON.stringify([{ tagName: 'v0.5.3', publishedAt: '2026-11-09T00:00:00Z' }]),
    'release view': JSON.stringify({ tagName: 'v0.5.3', url: 'u', targetCommitish: 's', assets: [{ name: 'Songsmith-Plugins-0.5.3-win64-setup.exe', size: 1 }, { name: 'Songsmith-Plugins-0.5.3-macOS.pkg', size: 2 }] }),
    api: 'ahead\n', 'issue view': JSON.stringify({ comments: [] }), issue: '', project: ''
  }, calls);
  const result = await advanceDelivery(dir, 'A-01', githubIo(gh, { projectId: 'P', statusFieldId: 'F', doneOptionId: 'O' }));
  assert.equal(result.stage, 'pending'); assert.match(result.blocked, /Private storage verifier is not configured/);
  assert.equal(calls.some(c => c[0] === 'project' || (c[0] === 'issue' && c[1] === 'close')), false);
});

test('moveToBlocked uses the board mutation and refuses without its option id', async () => {
  const calls = [];
  const io = githubIo(fakeGh({ 'api graphql': '{}' }, calls), { projectId: 'P', statusFieldId: 'F', blockedOptionId: 'B' });
  await io.moveToBlocked('item-1');
  assert.match(calls[0].join(' '), /-f o=B$/); assert.match(calls[0][3], /updateProjectV2ItemFieldValue/);
  await assert.rejects(githubIo(fakeGh({}), { projectId: 'P', statusFieldId: 'F' }).moveToBlocked('item-1'), /Blocked field is not configured/);
});

test('prChecks returns the head commit and the check roll-up', async () => {
  const io = githubIo(fakeGh({ 'pr view': JSON.stringify({ headRefOid: merge, statusCheckRollup: [{ status: 'COMPLETED', conclusion: 'SUCCESS' }] }) }));
  assert.deepEqual(await io.prChecks(repo, 9), { headSha: merge, checks: [{ status: 'COMPLETED', conclusion: 'SUCCESS' }] });
});
