import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { mergeGate, releaseManifest, websiteDispatch, buildPlan, worktreePlan } from '../validation.mjs';
const commit = 'a'.repeat(40);
function evidence() { return { expectedHead: commit, currentHead: commit, checks: ['windows', 'macos'].map(platform => ({ name: platform, platform, head: commit, conclusion: 'success' })), requiredChecks: ['windows', 'macos'], protectedBranch: true, ownerApproved: true, draft: false, warnings: 0, testsPassed: true }; }
test('Merge validation requires exact head, protections, independent green CI, approval and tests', () => {
  assert.equal(mergeGate(evidence()), true);
  for (const override of [{ currentHead: 'b'.repeat(40) }, { protectedBranch: false }, { ownerApproved: false }, { draft: true }, { warnings: 1 }, { testsPassed: false }, { requiredChecks: [] }, { checks: evidence().checks.slice(0, 1) }, { checks: evidence().checks.map(c => ({ ...c, conclusion: 'pending' })) }, { checks: [...evidence().checks, { name: 'security', head: commit, conclusion: 'failure' }] }]) assert.throws(() => mergeGate({ ...evidence(), ...override }));
});
function release(t) {
  const directory = fs.mkdtempSync(path.join(os.tmpdir(), 'matrix-release-'));
  t.after(() => fs.rmSync(directory, { recursive: true, force: true }));
  const version = '1.4.0';
  for (const name of [`Songsmith-Plugins-${version}-win64-setup.exe`, `Songsmith-Plugins-${version}-macOS.pkg`]) fs.writeFileSync(path.join(directory, name), 'test fixture, not installer');
  return { version, directory, commit, checkpoint: { approved: true, version, commit, scope: ['EQ-41'] }, build: { commit, branch: 'main', event: 'push', conclusion: 'success', windows: true, macos: true, installerTests: true, universalMac: true } };
}
test('Release manifest requires approved checkpoint and tested main universal installers', t => {
  const options = release(t), manifest = releaseManifest(options);
  assert.equal(manifest.assets.length, 2); assert.equal(manifest.assets[0].sha256.length, 64);
  assert.equal(manifest.assets[0].key.startsWith('releases/1.4.0/'), true);
  for (const override of [{ checkpoint: null }, { checkpoint: { ...options.checkpoint, approved: false } }, { checkpoint: { ...options.checkpoint, commit: 'b'.repeat(40) } }, { build: { ...options.build, macos: false } }, { build: { ...options.build, universalMac: false } }, { build: { ...options.build, branch: 'feature' } }, { build: { ...options.build, conclusion: 'failure' } }]) assert.throws(() => releaseManifest({ ...options, ...override }));
  fs.writeFileSync(path.join(options.directory, 'unexpected.zip'), 'extra'); assert.throws(() => releaseManifest(options));
});
test('No website dispatch until publication and both private immutable R2 checksums verified', t => {
  const manifest = releaseManifest(release(t));
  const published = { tag: 'v1.4.0', commit, draft: false, assets: manifest.assets };
  const copies = manifest.assets.map(a => ({ ...a, private: true, immutable: true }));
  const event = websiteDispatch(manifest, published, copies);
  assert.deepEqual(event, websiteDispatch(manifest, published, copies));
  for (const bad of [null, { ...published, draft: true }, { ...published, commit: 'b'.repeat(40) }, { ...published, assets: [] }]) assert.throws(() => websiteDispatch(manifest, bad, copies));
  for (const bad of [[], copies.slice(0, 1), copies.map(c => ({ ...c, sha256: 'wrong' })), copies.map(c => ({ ...c, private: false })), copies.map(c => ({ ...c, immutable: false }))]) assert.throws(() => websiteDispatch(manifest, published, bad));
});
test('Fixed worktree/build plans isolate identity and reject arbitrary commands', () => {
  const root = path.resolve('isolated');
  const work = worktreePlan(root, 'EQ-41', commit);
  assert.equal(work.branch, `matrix/eq-41-${commit.slice(0, 12)}`);
  assert.equal(work.directory.startsWith(root + path.sep), true);
  for (const id of ['../escape', 'EQ-41;whoami']) assert.throws(() => worktreePlan(root, id, commit));
  assert.throws(() => buildPlan('eq;whoami', root, root));
  const commands = buildPlan('eq', root, path.resolve('JUCE'));
  assert.equal(commands.length, 3); assert.equal(commands.some(c => c.executable.includes('install')), false);
  assert.equal(commands.every(c => c.timeoutSeconds > 0), true);
});
