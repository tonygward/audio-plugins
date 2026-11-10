// Read-only beta delivery verifiers for delivery.mjs: private storage, beta website, downloads.
// They use only an injected `gh(args)` (read access to both repositories) and an optional
// injected HEAD request for the protected download route. No storage, Cloudflare or release
// credentials are needed or held here.
//
// Honest limits:
// - Private storage is attested, not read directly. The trusted distribution workflow HEADs and reads back
//   both objects before it records them in the website's release.json; this module checks that run
//   succeeded, that release.json matches the GitHub release asset names, sizes and SHA-256 digests, and
//   uses those sizes as the storage evidence.
// - The deployed site is confirmed from the Cloudflare Pages check on the commit that last changed
//   release.json, since the live site sits behind Cloudflare Access.
// - Authenticated downloads need a credential the website's Worker accepts. Its Worker currently
//   requires an invited tester email, which a Cloudflare service token does not carry, so this
//   check stays unconfigured (and delivery stays blocked) until the owner decides how to allow it.
const PAGES_APP = 'cloudflare-workers-and-pages';

export function betaVerifiers(gh, { sourceRepo = 'tonygward/audio-plugins', websiteRepo = 'tonygward/songsmith-website',
  siteUrl = 'https://beta.songsmith.uk', head = null, accessHeaders = {} } = {}) {
  const text = async args => gh(args);
  const json = async args => JSON.parse(await gh(args));
  const websiteFile = async file => Buffer.from((await text(['api', `repos/${websiteRepo}/contents/${file}`, '--jq', '.content'])).trim(), 'base64').toString('utf8');
  const release = async () => JSON.parse(await websiteFile('public/release.json'));

  return {
    site: async () => {
      const current = await release();
      const changelog = await websiteFile('public/changelog.md');
      const changelogVersions = [...changelog.matchAll(/^#\s+(\d+\.\d+\.\d+)\s*$/gm)].map(match => match[1]);
      const commits = await json(['api', `repos/${websiteRepo}/commits?path=public/release.json&per_page=1`]);
      const sha = commits[0]?.sha;
      let deployed = false;
      if (sha) {
        const runs = await json(['api', `repos/${websiteRepo}/commits/${sha}/check-runs`]);
        deployed = (runs.check_runs ?? []).some(run => run.app?.slug === PAGES_APP && run.conclusion === 'success');
      }
      return { version: current.version, changelogVersions, deployed, commit: sha ?? null };
    },

    r2: async version => {
      const current = await release();
      if (current.version !== version) throw new Error(`website manifest is at ${current.version}, so storage for ${version} cannot be confirmed from it`);
      const run = await json(['api', `repos/${sourceRepo}/actions/runs/${current.provenance?.distributionRunId}`]);
      if (run.conclusion !== 'success' || run.event !== 'release' || run.head_branch !== `v${version}`) return [];
      const assets = await json(['api', `repos/${sourceRepo}/releases/tags/v${version}`, '--jq', '[.assets[]|{name,size,digest}]']);
      return Object.values(current.installers ?? {}).filter(installer => assets.some(asset =>
        asset.name === installer.name && asset.size === installer.size && asset.digest === `sha256:${installer.sha256}`))
        .map(installer => ({ name: installer.name, size: installer.size }));
    },

    downloads: async version => {
      if (!head) throw new Error('Authenticated download verifier is not configured');
      const current = await release();
      const result = {};
      for (const platform of ['windows', 'macos']) {
        const installer = current.installers?.[platform];
        if (current.version !== version || !installer) { result[platform] = { ok: false, bytes: 0 }; continue; }
        const response = await head(`${siteUrl}/downloads/${version}/${platform}`, accessHeaders);
        result[platform] = { ok: response.status === 200, bytes: Number(response.contentLength) || 0 };
      }
      return result;
    }
  };
}
