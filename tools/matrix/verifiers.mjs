// Read-only beta delivery verifiers for delivery.mjs: private storage, beta website and the archived prompt.
// They use only an injected `gh(args)` (read access to both repositories). No storage, Cloudflare or release
// credentials are needed or held here. Downloads are not checked: the final comment publishes the website link.
//
// Honest limits:
// - Private storage is attested, not read directly. The trusted distribution workflow HEADs and reads back
//   both objects before it records them in the website's release.json; this module checks that run
//   succeeded, that release.json matches the GitHub release asset names, sizes and SHA-256 digests, and
//   uses those sizes as the storage evidence.
// - The deployed site is confirmed from the Cloudflare Pages check on the commit that last changed
//   release.json, since the live site sits behind Cloudflare Access.
const PAGES_APP = 'cloudflare-workers-and-pages';

// The archive workflow moves a merged prompt from <folder>/prompts/X.md to <folder>/prompts/done/X.md.
export const archivedPath = file => file.split('/').includes('done') ? file : file.replace(/\/prompts\/(?!.*\/)/, '/prompts/done/');

export function betaVerifiers(gh, { sourceRepo = 'tonygward/audio-plugins', websiteRepo = 'tonygward/songsmith-website',
  siteUrl = 'https://beta.songsmith.uk' } = {}) {
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
      return { version: current.version, changelogVersions, deployed, commit: sha ?? null, url: siteUrl };
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

    // Lists the done/ folder on main (it always exists) rather than fetching the file, so "not there yet" is a plain false.
    promptArchived: async file => {
      const target = archivedPath(file), folder = target.slice(0, target.lastIndexOf('/'));
      const names = await json(['api', `repos/${sourceRepo}/contents/${folder}?ref=main`, '--jq', '[.[].name]']);
      return Array.isArray(names) && names.includes(target.slice(target.lastIndexOf('/') + 1));
    }
  };
}
