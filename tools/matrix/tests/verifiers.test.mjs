import test from 'node:test';
import assert from 'node:assert/strict';
import { betaVerifiers } from '../verifiers.mjs';
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
    if (url.includes('/releases/tags/')) return JSON.stringify([{ name: win('0.5.4'), size: 100, digest: digests.windows }, { name: mac('0.5.4'), size: 200, digest: digests.macos }]);
    throw new Error(`unexpected ${args.join(' ')}`);
  };
}

test('site reports the website version, changelog versions and Pages deployment', async () => {
  const result = await betaVerifiers(gh()).site();
  assert.deepEqual(result, { version: '0.5.4', changelogVersions: ['0.5.4', '0.5.3'], deployed: true, commit: 'abc' });
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

test('downloads stay unconfigured without a protected-route check', async () => {
  await assert.rejects(betaVerifiers(gh()).downloads('0.5.4'), /not configured/);
});

test('downloads compare status and byte length from the protected route', async () => {
  const calls = [];
  const head = async (url, headers) => { calls.push([url, headers]); return url.endsWith('/windows') ? { status: 200, contentLength: '100' } : { status: 403, contentLength: '0' }; };
  const result = await betaVerifiers(gh(), { head, accessHeaders: { token: 't' } }).downloads('0.5.4');
  assert.deepEqual(result, { windows: { ok: true, bytes: 100 }, macos: { ok: false, bytes: 0 } });
  assert.equal(calls[0][0], 'https://beta.songsmith.uk/downloads/0.5.4/windows');
  assert.deepEqual(calls[0][1], { token: 't' });
  const stale = await betaVerifiers(gh(), { head }).downloads('0.5.5');
  assert.equal(stale.windows.ok, false);
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
  const io = { ...betaVerifiers(gh({ pages: false }), { head: async () => ({ status: 200, contentLength: '100' }) }),
    pull: async () => ({ state: 'MERGED', mergeSha: 'x', url: 'u' }), release: async () => ({ tag: 'v0.5.4', version: v, includesMerge: true, assets }),
    downloads: async () => ({ windows: { ok: true, bytes: 100 }, macos: { ok: true, bytes: 200 } }) };
  const result = await verifyDelivery({ repository: 'r', pull: 1 }, io);
  assert.deepEqual(result.missing, ['Beta website deployment is not confirmed']);
});
