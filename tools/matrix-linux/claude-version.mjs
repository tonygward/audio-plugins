// Which Claude Code version should the Matrix agent image carry? (Automatic update, host side.)
// The image is immutable and the container has no way to rebuild itself, so updating is a host job: Start-MatrixAgent.ps1 -Action Update
// asks this module for a target, builds a candidate image, proves it, and only then swaps it in (keeping the previous image to roll back to).
//
// Rules, chosen to keep supply-chain risk low while staying current:
//   - stable x.y.z only (no pre-releases), never a deprecated version, never one already rejected by a failed trial;
//   - never a downgrade, and never a new major version without an explicit owner flag;
//   - a version must be at least `minAgeDays` old, so a bad release is usually withdrawn or deprecated before Matrix takes it;
//   - never newer than the registry's `latest` tag.
// Pure and dependency-free so it can be tested without a network.

export const PACKAGE = '@anthropic-ai/claude-code';

export const parseVersion = v => {
  const m = /^(\d+)\.(\d+)\.(\d+)$/.exec(String(v ?? ''));
  return m ? [Number(m[1]), Number(m[2]), Number(m[3])] : null;
};

export function compareVersions(a, b) {
  const x = parseVersion(a), y = parseVersion(b);
  if (!x || !y) throw new Error(`Not a stable version: ${!x ? a : b}`);
  for (let i = 0; i < 3; i++) if (x[i] !== y[i]) return x[i] < y[i] ? -1 : 1;
  return 0;
}

const DAY = 86400000;

/**
 * registry: the npm document ({ 'dist-tags': { latest }, time: { [version]: iso }, versions: { [version]: { deprecated? } } }).
 * Returns { action: 'update' | 'none', target?, current, reason }.
 */
export function chooseVersion({ current, registry, now = Date.now(), minAgeDays = 3, rejected = [], allowMajor = false }) {
  const cur = parseVersion(current);
  if (!cur) throw new Error(`The current version "${current}" is not a stable x.y.z version`);
  const latest = registry?.['dist-tags']?.latest;
  if (!parseVersion(latest)) return { action: 'none', current, reason: 'The registry reports no stable latest version' };
  const times = registry.time ?? {}, versions = registry.versions ?? {};
  const eligible = [], waiting = [];
  for (const version of Object.keys(versions)) {
    const v = parseVersion(version);
    if (!v || compareVersions(version, current) <= 0 || compareVersions(version, latest) > 0) continue;
    if (versions[version]?.deprecated || rejected.includes(version)) continue;
    if (v[0] !== cur[0] && !allowMajor) continue;
    const published = Date.parse(times[version]);
    if (!Number.isFinite(published)) continue; // no trustworthy publish time: never guess
    if (now - published < minAgeDays * DAY) waiting.push({ version, readyAt: new Date(published + minAgeDays * DAY).toISOString() });
    else eligible.push(version);
  }
  if (eligible.length) {
    const target = eligible.sort(compareVersions).at(-1);
    return { action: 'update', target, current, reason: `${target} is newer than ${current}, stable, not deprecated, and at least ${minAgeDays} days old` };
  }
  if (waiting.length) {
    const next = waiting.sort((a, b) => compareVersions(a.version, b.version)).at(-1);
    return { action: 'none', current, reason: `${next.version} is newer but younger than ${minAgeDays} days; eligible from ${next.readyAt}` };
  }
  return { action: 'none', current, reason: 'Already on the newest eligible version' };
}

export async function fetchRegistry(fetchImpl = fetch, timeoutMs = 60000) {
  const response = await fetchImpl(`https://registry.npmjs.org/${PACKAGE.replace('/', '%2f')}`, { headers: { accept: 'application/json' }, signal: AbortSignal.timeout(timeoutMs) });
  if (!response.ok) throw new Error(`npm registry answered ${response.status}`);
  const doc = await response.json();
  if (doc?.name !== PACKAGE || typeof doc.versions !== 'object') throw new Error('Unexpected registry response');
  return doc;
}

// node claude-version.mjs decide --current 2.1.322 [--min-age-days 3] [--rejected 2.1.400,2.1.401] [--allow-major]
if (process.argv[1] && import.meta.url.endsWith(process.argv[1].replace(/\\/g, '/').split('/').pop())) {
  try {
    const [command, ...rest] = process.argv.slice(2), arg = name => { const i = rest.indexOf(name); return i >= 0 ? rest[i + 1] : undefined; };
    if (command !== 'decide') throw new Error('Usage: decide --current <x.y.z> [--min-age-days n] [--rejected a,b] [--allow-major]');
    const result = chooseVersion({ current: arg('--current'), registry: await fetchRegistry(), minAgeDays: Number(arg('--min-age-days') ?? 3),
      rejected: (arg('--rejected') ?? '').split(',').filter(Boolean), allowMajor: rest.includes('--allow-major') });
    console.log(JSON.stringify(result));
  } catch (error) { console.log(JSON.stringify({ action: 'error', reason: String(error.message ?? error) })); process.exitCode = 1; }
}
