import fs from 'node:fs';
import path from 'node:path';
import { createHash } from 'node:crypto';
import { releaseAssets } from './policy.mjs';

// Evidence is supplied by a future trusted GitHub adapter, never by agent output.
export function mergeGate(evidence) {
  const { expectedHead, currentHead, checks, requiredChecks, protectedBranch, ownerApproved, draft, warnings, testsPassed } = evidence;
  if (!/^[a-f0-9]{40,64}$/.test(expectedHead ?? '') || currentHead !== expectedHead) throw new Error('PR head changed');
  if (!protectedBranch || !ownerApproved || draft || warnings !== 0 || testsPassed !== true) throw new Error('PR approval/build policy not satisfied');
  if (!Array.isArray(requiredChecks) || !requiredChecks.length || !Array.isArray(checks)) throw new Error('Required checks unknown');
  for (const name of requiredChecks) {
    const matches = checks.filter(c => c.name === name && c.head === expectedHead);
    if (matches.length !== 1 || matches[0].conclusion !== 'success') throw new Error(`Required check not green: ${name}`);
  }
  if (!['windows', 'macos'].every(platform => checks.some(c => c.platform === platform && c.head === expectedHead && c.conclusion === 'success' && requiredChecks.includes(c.name)))) throw new Error('Independent Windows/macOS validation missing');
  if (checks.some(c => c.head === expectedHead && ['failure', 'cancelled', 'timed_out', 'action_required'].includes(c.conclusion))) throw new Error('CI failure');
  return true;
}

export function releaseManifest({ version, directory, commit, checkpoint, build }) {
  if (!checkpoint?.approved || !checkpoint.scope?.length || checkpoint.version !== version || checkpoint.commit !== commit) throw new Error('Approved version/commit-specific checkpoint required');
  if (!build || build.commit !== commit || build.branch !== 'main' || build.event !== 'push' || build.conclusion !== 'success' || !build.windows || !build.macos || !build.installerTests || !build.universalMac) throw new Error('Tested main CI installers required');
  if (!/^[a-f0-9]{40,64}$/.test(commit)) throw new Error('Invalid release commit');
  const names = releaseAssets(version, fs.readdirSync(directory));
  const assets = names.map(name => {
    const file = path.join(directory, name), stat = fs.lstatSync(file);
    if (!stat.isFile() || stat.isSymbolicLink() || stat.size === 0) throw new Error('Invalid installer file');
    return { name, size: stat.size, sha256: createHash('sha256').update(fs.readFileSync(file)).digest('hex'), key: `releases/${version}/${name}` };
  });
  return { version, commit, checkpoint: checkpoint.scope, assets, unsigned: true };
}

export function websiteDispatch(manifest, published, verifiedCopies) {
  releaseAssets(manifest.version, manifest.assets.map(a => a.name));
  if (!published || published.draft || published.tag !== `v${manifest.version}` || published.commit !== manifest.commit || published.assets?.length !== 2 || manifest.assets.some(a => !published.assets.some(b => a.name === b.name && a.size === b.size && a.sha256 === b.sha256))) throw new Error('Matching published release required');
  if (!Array.isArray(verifiedCopies) || verifiedCopies.length !== 2 || manifest.assets.some(a => !verifiedCopies.some(b => b.key === a.key && b.size === a.size && b.sha256 === a.sha256 && b.private === true && b.immutable === true))) throw new Error('Both immutable private R2 copies must be verified');
  // Website receiver must deduplicate this identity and update manifest + changelog together.
  return { event_type: 'songsmith-release-ready', client_payload: { id: `${manifest.version}:${manifest.commit}`, version: manifest.version, commit: manifest.commit } };
}

export function buildPlan(plugin, worktree, juceDirectory) {
  const names = { eq: 'EQTests', 'analog-compressor': 'AnalogCompressorTests', maximiser: 'MaximiserTests' };
  if (!names[plugin]) throw new Error('Unknown plugin');
  const source = path.join(worktree, plugin), build = path.join(source, 'build');
  return [
    { executable: 'cmake', args: ['-S', source, '-B', build, '-G', 'Visual Studio 18 2026', '-A', 'x64', `-DJUCE_DIR=${juceDirectory}`], timeoutSeconds: 600 },
    { executable: 'cmake', args: ['--build', build, '--config', 'Release'], timeoutSeconds: 1800 },
    { executable: path.join(build, `${names[plugin]}_artefacts`, 'Release', `${names[plugin]}.exe`), args: [], timeoutSeconds: 600 }
  ];
}

export function worktreePlan(root, id, revision) {
  if (!/^[A-Z][A-Z0-9]*-\d{2,}$/.test(id) || !/^[a-f0-9]{40,64}$/.test(revision)) throw new Error('Invalid work identity');
  if (!path.isAbsolute(root)) throw new Error('Absolute isolated worktree root required');
  const branch = `matrix/${id.toLowerCase()}-${revision.slice(0, 12)}`, directory = path.join(root, `${id}-${revision.slice(0, 12)}`);
  return { branch, directory, executable: 'git', args: ['worktree', 'add', '-b', branch, directory, revision] };
}
