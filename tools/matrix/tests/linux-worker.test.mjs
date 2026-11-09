import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { runLinuxWorker, validateLinuxConfig, preflight, baseEnvironment, resolveChangelogConflict, ciOutcome, CI_VALIDATION } from '../linux-worker.mjs';
import { runProcess } from '../process.mjs';
import { readState } from '../state.mjs';
import { TELEMETRY_SOURCE } from '../policy.mjs';
const gitExecutable = execFileSync(process.platform === 'win32' ? 'where.exe' : 'which', ['git'], { encoding: 'utf8' }).trim().split(/\r?\n/)[0];
const now = Date.now();
const goodUsage = () => ({ allowed: true, reason: 'ok', extraUsageEnabled: false, snapshot: { source: TELEMETRY_SOURCE, percentage: 20, observedAt: new Date(now).toISOString(), resetsAt: new Date(now + 3600000).toISOString() } });
const agentJson = JSON.stringify({ type: 'result', subtype: 'success', is_error: false, num_turns: 3, total_cost_usd: 0.1 });
const CLAUDE = '/usr/local/bin/claude', GH = '/usr/bin/gh';
const branches = f => execFileSync(gitExecutable, ['--git-dir', f.remote, 'branch', '--list', 'matrix/*'], { encoding: 'utf8' }).trim();

function fixture(t) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'matrix-linux-'));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  const repo = path.join(root, 'repo'), remote = path.join(root, 'remote.git'); fs.mkdirSync(repo);
  const git = (args, cwd = repo) => execFileSync(gitExecutable, ['-c', 'core.hooksPath=' + (process.platform === 'win32' ? 'NUL' : '/dev/null'), ...args], { cwd, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim();
  git(['init', '-b', 'main']); git(['config', 'user.name', 'Test']); git(['config', 'user.email', 't@example.invalid']);
  fs.mkdirSync(path.join(repo, 'eq/Source'), { recursive: true }); fs.mkdirSync(path.join(repo, 'eq/prompts'), { recursive: true });
  fs.writeFileSync(path.join(repo, '.gitignore'), 'build/\n');
  fs.writeFileSync(path.join(repo, 'AGENTS.md'), 'rules'); fs.writeFileSync(path.join(repo, 'CLAUDE.md'), 'rules');
  fs.writeFileSync(path.join(repo, 'CHANGELOG.md'), '# Changelog\n\n## 1.0.0\n\nFirst.\n');
  fs.writeFileSync(path.join(repo, 'eq/Source/example.cpp'), 'before\n');
  fs.writeFileSync(path.join(repo, 'eq/prompts/41-fixture.md'), '---\nexecution: build-now\nmodel: sonnet\neffort: low\n---\n# Fixture task\nFix the example.');
  git(['add', '.']); git(['commit', '-m', 'baseline']);
  execFileSync(gitExecutable, ['init', '--bare', remote], { stdio: 'pipe' }); git(['remote', 'add', 'origin', remote]); git(['push', '-u', 'origin', 'main']);
  fs.writeFileSync(path.join(root, 'token'), 'fake-token\n');
  const config = { repository: repo, stateDirectory: path.join(root, 'state'), enabled: true, liveExecution: true, autoMerge: false, releasePublication: false, batchLimit: 3, dailyTime: '02:00',
    claudeUsage: { enabled: true, executable: CLAUDE },
    worker: { enabled: true, mode: 'ci', repositorySlug: 'example/plugins', worktreeRoot: path.join(root, 'worktrees'), gitExecutable, ghExecutable: GH, tokenFile: path.join(root, 'token'),
      maximumPrompts: 1, maximumTurns: 10, maximumBudgetUsd: 5, timeoutSeconds: 60, ciTimeoutSeconds: 600, maximumRepairs: 2, pollSeconds: 5 } };
  return { root, repo, git, remote, config };
}

// Real git; fake Claude and GitHub. `builds` gives the hosted-build conclusion for each new head commit, in order.
function adapter({ builds = ['success'], onClaude, claudeFails } = {}) {
  const seen = { claude: [], gh: [] }, results = new Map();
  let counter = 0;
  const run = async (exe, args, o) => {
    if (exe === gitExecutable) {
      if (args.includes('remote') && args.includes('get-url')) return { stdout: 'https://github.com/example/plugins\n', stderr: '' };
      // The test remote is a local bare repository: drop the GitHub credential helper settings.
      const cleaned = []; for (let i = 0; i < args.length; i++) { if (args[i] === '-c' && /^credential\.helper/.test(args[i + 1])) { i++; continue; } cleaned.push(args[i]); }
      return runProcess(exe, cleaned, o);
    }
    if (exe === CLAUDE) {
      seen.claude.push({ args, env: o.env, input: o.input, cwd: o.cwd });
      if (claudeFails) throw Object.assign(Error('Process failed'), { stdout: JSON.stringify(claudeFails), stderr: '', exitCode: 1 });
      if (onClaude) onClaude(o.cwd, seen.claude.length); else fs.appendFileSync(path.join(o.cwd, 'eq/Source/example.cpp'), `change ${seen.claude.length}\n`);
      return { stdout: agentJson, stderr: '' };
    }
    if (exe === GH) {
      seen.gh.push({ args, env: o.env });
      if (args[0] === 'pr' && args[1] === 'list') return { stdout: '[]', stderr: '' };
      if (args[0] === 'pr' && args[1] === 'create') return { stdout: 'https://github.com/example/plugins/pull/7\n', stderr: '' };
      if (args[0] === 'pr' && args[1] === 'edit') return { stdout: '', stderr: '' };
      if (args[0] === 'run' && args[1] === 'list') {
        const sha = args[args.indexOf('--commit') + 1];
        if (!results.has(sha)) results.set(sha, builds[Math.min(results.size, builds.length - 1)]);
        return { stdout: JSON.stringify([{ databaseId: ++counter, status: 'completed', conclusion: results.get(sha), headSha: sha, event: 'pull_request' }]), stderr: '' };
      }
      if (args[0] === 'run' && args[1] === 'view') return { stdout: 'error C2065: undeclared identifier\n', stderr: '' };
    }
    throw Error('Unexpected command ' + exe + ' ' + args.join(' '));
  };
  return { run, seen };
}
const options = (f, a, extra = {}) => ({ trigger: 'daily', run: a.run, loadUsage: goodUsage, clock: () => now, sleep: async () => {}, check: () => {}, environment: { PATH: process.env.PATH, HOME: f.root }, ...extra });
const body = (f, report) => fs.readFileSync(path.join(f.config.stateDirectory, `${report.outcomes[0].id}-pr.md`), 'utf8');

test('config must keep merge and release off, use ci mode and sensible limits', t => {
  const f = fixture(t); validateLinuxConfig(f.config);
  for (const mutate of [c => { c.autoMerge = true; }, c => { c.releasePublication = true; }, c => { c.worker.mode = 'scheduled'; }, c => { c.worker.maximumRepairs = 3; }, c => { c.worker.tokenFile = 'relative'; }, c => { c.claudeUsage.enabled = false; }, c => { c.worker.repositorySlug = 'bad slug'; }]) {
    const c = structuredClone(f.config); mutate(c); assert.throws(() => validateLinuxConfig(c));
  }
});

test('preflight fails closed on root, Docker socket, credentials, missing proxy and capabilities', () => {
  const good = { env: { HTTPS_PROXY: 'http://p:3128' }, exists: () => false, status: () => 'CapEff:\t0000000000000000\n', uid: 10001 };
  preflight(good);
  assert.throws(() => preflight({ ...good, uid: 0 }), /root/);
  assert.throws(() => preflight({ ...good, exists: p => p === '/var/run/docker.sock' }), /Docker socket/);
  assert.throws(() => preflight({ ...good, env: { HTTPS_PROXY: 'x', RELEASE_PUBLISH_TOKEN: 'secret' } }), /credentials/);
  assert.throws(() => preflight({ ...good, env: { GH_TOKEN: 'secret', HTTPS_PROXY: 'x' } }), /credentials/);
  assert.throws(() => preflight({ ...good, env: {} }), /proxy/);
  assert.throws(() => preflight({ ...good, status: () => 'CapEff:\t00000000a80425fb\n' }), /capabilities/);
  assert.throws(() => preflight({ ...good, status: () => { throw Error('no'); } }), /cannot read/);
});

test('the base environment passes proxies and drops every credential', () => {
  const env = baseEnvironment({ PATH: '/bin', HOME: '/h', HTTPS_PROXY: 'http://p', GH_TOKEN: 's', ANTHROPIC_API_KEY: 'k', RELEASE_PUBLISH_TOKEN: 'r', CLOUDFLARE_API_TOKEN: 'c' });
  assert.equal(env.HTTPS_PROXY, 'http://p'); assert.equal(env.PATH, '/bin');
  for (const key of ['GH_TOKEN', 'ANTHROPIC_API_KEY', 'RELEASE_PUBLISH_TOKEN', 'CLOUDFLARE_API_TOKEN']) assert.equal(env[key], undefined);
});

test('a changelog conflict keeps both sides; any other shape is left alone', () => {
  const conflict = '# C\n\n<<<<<<< HEAD\n## Unreleased\n\n- mine\n=======\n## 1.3.3\n\nTheirs.\n>>>>>>> origin/main\n\n## 1.3.2\n';
  const fixed = resolveChangelogConflict(conflict);
  assert.match(fixed, /## Unreleased\n\n- mine\n\n## 1\.3\.3\n\nTheirs\.\n\n## 1\.3\.2/);
  assert.equal(/<<<<<<<|>>>>>>>|^=======/m.test(fixed), false);
  assert.equal(resolveChangelogConflict(conflict + conflict), null);
  assert.equal(resolveChangelogConflict('<<<<<<< a\nx\n=======\ny\n'), null);
});

test('only a finished Build for this exact head commit counts', () => {
  const sha = 'a'.repeat(40), other = 'b'.repeat(40), row = (extra) => ({ headSha: sha, event: 'pull_request', status: 'completed', conclusion: 'success', databaseId: 1, ...extra });
  assert.deepEqual(ciOutcome([], sha), { state: 'none' });
  assert.deepEqual(ciOutcome([row({ headSha: other })], sha), { state: 'none' });
  assert.deepEqual(ciOutcome([row({ event: 'push' })], sha), { state: 'none' });
  assert.equal(ciOutcome([row({ status: 'in_progress', conclusion: null })], sha).state, 'pending');
  assert.equal(ciOutcome([row()], sha).state, 'success');
  assert.equal(ciOutcome([row({ conclusion: 'cancelled' })], sha).state, 'failure');
  assert.equal(ciOutcome([row({ conclusion: 'failure' }), row({ databaseId: 2 })], sha).state, 'success');
});

test('happy path: change, push, draft PR with the hosted-build validation line, green build, no merge', async t => {
  const f = fixture(t), a = adapter(), report = await runLinuxWorker(f.config, options(f, a));
  assert.equal(report.merged, false); assert.equal(report.released, false);
  assert.equal(report.outcomes.length, 1); assert.equal(report.outcomes[0].blocked, false);
  const create = a.seen.gh.find(c => c.args[1] === 'create');
  assert.ok(create); assert.ok(create.args.includes('--draft'));
  assert.ok(body(f, report).includes(`Validation: ${CI_VALIDATION}`)); assert.doesNotMatch(body(f, report), /## Blocked/);
  assert.match(branches(f), /matrix\//);
  assert.equal(Object.values(readState(f.config.stateDirectory).tasks).find(x => x.pr).state, 'awaiting-review');
  for (const call of a.seen.gh) assert.ok(!call.args.includes('merge') && !call.args.includes('ready'));
});

test('the agent never receives the GitHub token; gh calls do', async t => {
  const f = fixture(t), a = adapter(); await runLinuxWorker(f.config, options(f, a));
  for (const c of a.seen.claude) { assert.equal(c.env.GH_TOKEN, undefined); assert.ok(!JSON.stringify(c.args).includes('fake-token')); }
  assert.ok(a.seen.gh.length > 0 && a.seen.gh.every(c => c.env.GH_TOKEN === 'fake-token'));
});

test('a failed hosted build gets one repair from a log file, not from pasted output; the new head is rechecked', async t => {
  const f = fixture(t), a = adapter({ builds: ['failure', 'success'] }), report = await runLinuxWorker(f.config, options(f, a));
  assert.equal(a.seen.claude.length, 2);
  assert.match(a.seen.claude[1].input, /build\/matrix\/ci-failure\.log/); assert.doesNotMatch(a.seen.claude[1].input, /C2065/);
  assert.match(fs.readFileSync(path.join(a.seen.claude[1].cwd, 'build/matrix/ci-failure.log'), 'utf8'), /C2065/);
  assert.equal(report.outcomes[0].blocked, false);
});

test('repairs are bounded; a build that keeps failing leaves a Blocked draft', async t => {
  const f = fixture(t), a = adapter({ builds: ['failure'] }), report = await runLinuxWorker(f.config, options(f, a));
  assert.equal(a.seen.claude.length, 3); assert.equal(report.outcomes[0].blocked, true);
  assert.ok(a.seen.gh.some(c => c.args[0] === 'pr' && c.args[1] === 'edit'));
  assert.match(body(f, report), /## Blocked[\s\S]*Hosted Build failed/);
});

test('changes outside the permitted scope stop the run before anything is pushed', async t => {
  const f = fixture(t), a = adapter({ onClaude: cwd => fs.writeFileSync(path.join(cwd, 'evil.ps1'), 'x') }), report = await runLinuxWorker(f.config, options(f, a));
  assert.equal(report.outcomes[0].interrupted, true);
  assert.equal(branches(f), '');
  assert.ok(!a.seen.gh.some(c => c.args[1] === 'create'));
});

test('a changelog-only conflict with main is resolved automatically', async t => {
  let moved = false;
  const f = fixture(t), a = adapter({ onClaude: cwd => {
    fs.writeFileSync(path.join(cwd, 'CHANGELOG.md'), '# Changelog\n\n## Unreleased\n\n- mine\n\n## 1.0.0\n\nFirst.\n');
    fs.appendFileSync(path.join(cwd, 'eq/Source/example.cpp'), 'mine\n');
    if (moved) return; moved = true;
    // Main moves on and bumps the changelog while the agent works (after the worker's own freshness check).
    f.git(['checkout', '-q', '-b', 'tmp']); fs.writeFileSync(path.join(f.repo, 'CHANGELOG.md'), '# Changelog\n\n## 1.0.1\n\nBump.\n\n## 1.0.0\n\nFirst.\n');
    f.git(['commit', '-qam', 'bump']); f.git(['push', 'origin', 'HEAD:main']); f.git(['checkout', '-q', 'main']); f.git(['branch', '-qD', 'tmp']);
  } });
  const report = await runLinuxWorker(f.config, options(f, a));
  assert.equal(report.outcomes[0].interrupted, undefined);
  const worktree = Object.values(readState(f.config.stateDirectory).tasks).find(x => x.worktree).worktree;
  const text = fs.readFileSync(path.join(worktree, 'CHANGELOG.md'), 'utf8');
  assert.match(text, /## Unreleased[\s\S]*## 1\.0\.1/); assert.doesNotMatch(text, /<<<<<<<|>>>>>>>/);
});

test('paused, disabled and non-isolated runs do nothing', async t => {
  const f = fixture(t), a = adapter();
  fs.mkdirSync(f.config.stateDirectory, { recursive: true }); fs.writeFileSync(path.join(f.config.stateDirectory, 'PAUSED'), 'x');
  assert.equal((await runLinuxWorker(f.config, options(f, a))).reason, 'Worker paused');
  fs.rmSync(path.join(f.config.stateDirectory, 'PAUSED'));
  const off = structuredClone(f.config); off.worker.enabled = false;
  assert.equal((await runLinuxWorker(off, options(f, a))).liveExecution, false);
  await assert.rejects(runLinuxWorker(f.config, options(f, a, { check: () => { throw Error('Isolation check failed: must not run as root'); } })), /Isolation/);
  assert.equal(a.seen.claude.length, 0);
});

test('usage at or above the limit starts no agent', async t => {
  const f = fixture(t), a = adapter();
  await runLinuxWorker(f.config, options(f, a, { loadUsage: () => ({ allowed: false, reason: 'Weekly usage at or above 70%', snapshot: null }) }));
  assert.equal(a.seen.claude.length, 0);
});

test('when Claude stops with an error the log says why, without the model text', async t => {
  const f = fixture(t), a = adapter({ claudeFails: { type: 'result', subtype: 'error_max_turns', is_error: true, num_turns: 31, total_cost_usd: 1.5, result: 'SECRET MODEL TEXT' } });
  const report = await runLinuxWorker(f.config, options(f, a));
  assert.equal(report.outcomes[0].interrupted, true);
  const log = fs.readFileSync(path.join(f.config.stateDirectory, 'logs', 'worker.jsonl'), 'utf8');
  assert.match(log, /"stage":"agent-failed"[^\n]*"subtype":"error_max_turns"[^\n]*"turns":31/); assert.doesNotMatch(log, /SECRET MODEL TEXT/);
});
