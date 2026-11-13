import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { advanceDelivery, recordMerge, verifyDelivery, finalMarker } from '../delivery.mjs';
import { readState, atomic } from '../state.mjs';

const sha = 'c'.repeat(40), version = '0.5.4';
const win = `Songsmith-Plugins-${version}-win64-setup.exe`, mac = `Songsmith-Plugins-${version}-macOS.pkg`;
const release = (assets, includesMerge = true) => async () => ({ tag: `v${version}`, url: 'https://example.test/rel', sourceSha: 'b'.repeat(40), includesMerge, version, assets });
const good = [{ name: win, size: 100 }, { name: mac, size: 200 }];

function setup() {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'delivery-'));
  const state = readState(dir);
  state.tasks['MAX-07'] = { id: 'MAX-07', state: 'claimed', job: 'job-1', issue: 161, itemId: 'item-1', repository: 'tonygward/audio-plugins', issueRepository: 'tonygward/audio-plugins' };
  recordMerge(state, 'MAX-07', { repository: 'tonygward/audio-plugins', pull: 180 });
  atomic(path.join(dir, 'state.json'), state);
  return dir;
}

function fakeIo(over = {}) {
  const io = {
    log: [], comments: [],
    pull: async () => ({ state: 'MERGED', mergeSha: sha, url: 'https://example.test/pull/180' }),
    release: release(good),
    r2: async () => good,
    site: async () => ({ version, changelogVersions: [version, '0.5.3'] }),
    hasComment: async (repo, issue, marker) => io.comments.some(c => c.includes(marker)),
    comment: async (repo, issue, body) => { io.log.push('comment'); if (over.failComment?.(body)) throw new Error('GitHub down'); io.comments.push(body); },
    closeIssue: async () => { io.log.push('close'); if (over.failClose) throw new Error('close refused'); },
    setDone: async () => { io.log.push('done'); if (over.failDone) throw new Error('board error'); },
    ...over.io
  };
  return io;
}

test('verified delivery posts the final comment, then closes, then sets Done', async () => {
  const dir = setup(), io = fakeIo();
  assert.deepEqual(await advanceDelivery(dir, 'MAX-07', io), { stage: 'done', blocked: null });
  assert.deepEqual(io.log, ['comment', 'close', 'done']);
  for (const text of ['MAX-07', '180', sha.slice(0, 12), version, win, mac, 'Download: https://beta.songsmith.uk', 'job-1']) assert.ok(io.comments[0].includes(text), text);
});

test('each blocker keeps the issue open and the card in Doing, with one comment', async () => {
  const cases = {
    'no release yet': { release: async () => null },
    'unmerged pull request': { pull: async () => ({ state: 'OPEN' }) },
    'extra release asset': { release: release([...good, { name: 'x.zip', size: 1 }]) },
    'wrong installer name': { release: release([{ name: 'setup.exe', size: 1 }, good[1]]) },
    'release does not include the merge': { release: release(good, false) },
    'private storage mismatch': { r2: async () => [{ name: win, size: 99 }, good[1]] },
    'website shows old version': { site: async () => ({ version: '0.5.3', changelogVersions: ['0.5.3'] }) },
    'changelog missing version': { site: async () => ({ version, changelogVersions: ['0.5.3'] }) },
    'verification throws': { r2: async () => { throw new Error('timeout'); } }
  };
  for (const [name, io] of Object.entries(cases)) {
    const dir = setup(), fake = fakeIo({ io });
    const first = await advanceDelivery(dir, 'MAX-07', fake);
    assert.equal(first.stage, 'pending', name); assert.ok(first.blocked, name);
    await advanceDelivery(dir, 'MAX-07', fake);
    assert.deepEqual(fake.log.filter(x => x !== 'comment'), [], `${name}: must not close or set Done`);
    assert.equal(fake.comments.length, 1, `${name}: exactly one blocker comment`);
    assert.equal(readState(dir).deliveries['MAX-07'].stage, 'pending', name);
  }
});

test('a failed final comment blocks closing and Done, and resumes without recoding', async () => {
  const dir = setup();
  const down = fakeIo({ failComment: body => body.includes(finalMarker('MAX-07')) });
  const first = await advanceDelivery(dir, 'MAX-07', down);
  assert.equal(first.stage, 'verified'); assert.match(first.blocked, /Final comment failed/);
  assert.deepEqual(down.log, ['comment']);
  const ok = fakeIo();
  assert.equal((await advanceDelivery(dir, 'MAX-07', ok)).stage, 'done');
  assert.deepEqual(ok.log, ['comment', 'close', 'done']);
});

test('a failed close or Done resumes at that stage without a second final comment', async () => {
  const dir = setup(), first = fakeIo({ failClose: true });
  assert.match((await advanceDelivery(dir, 'MAX-07', first)).blocked, /Closing the issue failed/);
  const second = fakeIo({ failDone: true }); second.comments = first.comments;
  assert.match((await advanceDelivery(dir, 'MAX-07', second)).blocked, /Setting the card to Done failed/);
  assert.equal(readState(dir).deliveries['MAX-07'].stage, 'closed');
  const last = fakeIo(); last.comments = first.comments;
  assert.equal((await advanceDelivery(dir, 'MAX-07', last)).stage, 'done');
  assert.deepEqual(last.log, ['done']);
  assert.equal(first.comments.filter(c => c.includes(finalMarker('MAX-07'))).length, 1);
});

test('a blocked delivery that later succeeds completes; Done is final and repeat calls do nothing', async () => {
  const dir = setup();
  await advanceDelivery(dir, 'MAX-07', fakeIo({ io: { release: async () => null } }));
  assert.equal((await advanceDelivery(dir, 'MAX-07', fakeIo())).stage, 'done');
  const again = fakeIo();
  assert.deepEqual(await advanceDelivery(dir, 'MAX-07', again), { stage: 'done', blocked: null });
  assert.deepEqual(again.log, []);
});

test('recordMerge needs a pickup record and is idempotent; unknown tasks are refused', async () => {
  assert.throws(() => recordMerge({ tasks: { 'A-01': { id: 'A-01', state: 'claimed' } } }, 'A-01', { repository: 'r', pull: 1 }), /no pickup record/);
  const dir = setup();
  assert.equal(recordMerge(readState(dir), 'MAX-07', { repository: 'tonygward/audio-plugins', pull: 999 }).pull, 180);
  assert.deepEqual(await advanceDelivery(dir, 'X-99', fakeIo()), { stage: null, blocked: 'No delivery recorded for this task' });
});

test('verifyDelivery reports every missing item together', async () => {
  const io = fakeIo({ io: { r2: async () => [], site: async () => ({ version: '0.0.1', changelogVersions: [] }) } });
  const result = await verifyDelivery({ repository: 'r', pull: 1 }, io);
  assert.equal(result.verified, false);
  assert.ok(result.missing.length >= 4, result.missing.join(' | '));
});

test('downloads are not checked: the final comment publishes the website link', async () => {
  const dir = setup(), io = fakeIo({ io: { site: async () => ({ version, changelogVersions: [version], url: 'https://beta.example.test' }) } });
  assert.equal((await advanceDelivery(dir, 'MAX-07', io)).stage, 'done');
  assert.match(io.comments[0], /Download: https:\/\/beta\.example\.test/);
  assert.doesNotMatch(io.comments[0], /download route|downloads checked/i);
});

test('a task keeps a canonical prompt: Done waits until it is archived in done/', async () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'delivery-'));
  const state = readState(dir);
  state.tasks['ACOMP-20'] = { id: 'ACOMP-20', state: 'claimed', job: 'job-2', issue: 300, itemId: 'item-2', file: 'analog-compressor/prompts/20-x.md', issueRepository: 'tonygward/audio-plugins' };
  recordMerge(state, 'ACOMP-20', { repository: 'tonygward/audio-plugins', pull: 301 });
  atomic(path.join(dir, 'state.json'), state);
  const asked = [];
  const missing = fakeIo({ io: { promptArchived: async file => { asked.push(file); return false; } } });
  const first = await advanceDelivery(dir, 'ACOMP-20', missing);
  assert.equal(first.stage, 'pending'); assert.match(first.blocked, /Canonical prompt is not archived in done\/ yet: analog-compressor\/prompts\/20-x\.md/);
  assert.deepEqual(asked, ['analog-compressor/prompts/20-x.md']);
  assert.equal((await advanceDelivery(dir, 'ACOMP-20', fakeIo({ io: { promptArchived: async () => true } }))).stage, 'done');
});

test('finishing delivery moves a claimed or reviewed task to complete so it cannot hold the job slot', async () => {
  for (const before of ['claimed', 'awaiting-review']) {
    const dir = setup(), state = readState(dir); state.tasks['MAX-07'].state = before; atomic(path.join(dir, 'state.json'), state);
    assert.equal((await advanceDelivery(dir, 'MAX-07', fakeIo())).stage, 'done');
    assert.equal(readState(dir).tasks['MAX-07'].state, 'complete', before);
  }
  const dir = setup();
  await advanceDelivery(dir, 'MAX-07', fakeIo({ io: { release: async () => null } }));
  assert.equal(readState(dir).tasks['MAX-07'].state, 'claimed', 'not complete while delivery is blocked');
});
