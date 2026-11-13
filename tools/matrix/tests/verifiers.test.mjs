import test from 'node:test';
import assert from 'node:assert/strict';
import { betaVerifiers, archivedPath } from '../verifiers.mjs';
import { verifyDelivery } from '../delivery.mjs';

const win = v => `Songsmith-Plugins-${v}-win64-setup.exe`, mac = v => `Songsmith-Plugins-${v}-macOS.pkg`;
const sha = { windows: 'a'.repeat(64), macos: 'b'.repeat(64) };
const releaseJson = (v, over = {}) => JSON.stringify({ version: v, installers: {
  windows: { name: win(v), size: 100, sha256: sha.windows }, macos: { name: mac(v), size: 200, sha256: sha.macos } }, provenance: { distributionRunId: 55 }, ...over });
const b64 = s => Buffer.from(s).toString('base64');

function gh({ site = '0.5.4', changelog = '# 0.5.4\n\n- x\n\n# 0.5.3\n', pages = true, run = { conclusion: 'success', event: 'release', head_branch: 'v0.5.4' },
  digests = { windows: `sha256:${sha.windows}`, macos: `sha256:${sha.macos}` }, over = {} } = {}) {
  return async args => {
    const url = args[1] ?? '';
    if (url.endsWith('contents/public/release.json')) return b64(releaseJson(site, over)) + '\n';
    if (url.endsWith('contents/public/changelog.md')) return b64(changelog) + '\n';
    if (url.includes('/commits?path=public/release.json')) return JSON.stringify([{ sha: 'abc' }]);
    if (url.endsWith('/check-runs')) return JSON.stringify({ check_runs: [{ app: { slug: pages ? 'cloudflare-workers-and-pages' : 'other' }, conclusion: 'success' }] });
    if (url.includes('/actions/runs/')) return JSON.stringify(run);
    if (url.includes('/contents/analog-compressor/prompts/done')) return JSON.stringify(['15-old.md', '20-x.md']);
    if (url.includes('/releases/tags/')) return JSON.stringify([{ name: win('0.5.4'), size: 100, digest: digests.windows }, { name: mac('0.5.4'), size: 200, digest: digests.macos }]);
    throw new Error(`unexpected ${args.join(' ')}`);
  };
}

test('site reports the website version, changelog versions and Pages deployment', async () => {
  const result = await betaVerifiers(gh()).site();
  assert.deepEqual(result, { version: '0.5.4', changelogVersions: ['0.5.4', '0.5.3'], deployed: true, commit: 'abc', url: 'https://beta.songsmith.uk' });
  assert.equal((await betaVerifiers(gh({ pages: false })).site()).deployed, false);
});

test('r2 evidence needs a successful distribution run and matching names, sizes and digests', async () => {
  assert.deepEqual(await betaVerifiers(gh()).r2('0.5.4'), [{ name: win('0.5.4'), size: 100 }, { name: mac('0.5.4'), size: 200 }]);
  await assert.rejects(betaVerifiers(gh()).r2('0.5.5'), /website manifest is at 0.5.4/);
  assert.deepEqual(await betaVerifiers(gh({ run: { conclusion: 'failure', event: 'release', head_branch: 'v0.5.4' } })).r2('0.5.4'), []);
  assert.deepEqual(await betaVerifiers(gh({ run: { conclusion: 'success', event: 'push', head_branch: 'v0.5.4' } })).r2('0.5.4'), []);
  const wrong = await betaVerifiers(gh({ digests: { windows: 'sha256:deadbeef', macos: `sha256:${sha.macos}` } })).r2('0.5.4');
  assert.deepEqual(wrong.map(o => o.name), [mac('0.5.4')]);
});

test('downloads are not verified: there is no download verifier', () => {
  assert.equal(betaVerifiers(gh()).downloads, undefined);
});

test('promptArchived looks for the prompt in its done/ folder on main', async () => {
  assert.equal(archivedPath('analog-compressor/prompts/20-x.md'), 'analog-compressor/prompts/done/20-x.md');
  assert.equal(archivedPath('shared/prompts/done/SUI-01-x.md'), 'shared/prompts/done/SUI-01-x.md');
  const v = betaVerifiers(gh());
  assert.equal(await v.promptArchived('analog-compressor/prompts/20-x.md'), true);
  assert.equal(await v.promptArchived('analog-compressor/prompts/21-y.md'), false);
});

test('a website that missed the release (as 0.5.3 did) blocks delivery', async () => {
  const io = { ...betaVerifiers(gh({ site: '0.5.2', changelog: '# 0.5.2\n' })),
    pull: async () => ({ state: 'MERGED', mergeSha: 'x', url: 'u' }),
    release: async () => ({ tag: 'v0.5.3', url: 'u', sourceSha: 's', includesMerge: true, version: '0.5.3', assets: [{ name: win('0.5.3'), size: 100 }, { name: mac('0.5.3'), size: 200 }] }) };
  const result = await verifyDelivery({ repository: 'r', pull: 1 }, io);
  assert.equal(result.verified, false);
  assert.ok(result.missing.some(m => /Beta website shows 0\.5\.2, not 0\.5\.3/.test(m)), result.missing.join(' | '));
});

test('an unconfirmed Pages deployment blocks delivery even when versions match', async () => {
  const v = '0.5.4', assets = [{ name: win(v), size: 100 }, { name: mac(v), size: 200 }];
  const io = { ...betaVerifiers(gh({ pages: false })),
    pull: async () => ({ state: 'MERGED', mergeSha: 'x', url: 'u' }), release: async () => ({ tag: 'v0.5.4', version: v, includesMerge: true, assets }) };
  const result = await verifyDelivery({ repository: 'r', pull: 1 }, io);
  assert.deepEqual(result.missing, ['Beta website deployment is not confirmed']);
});


