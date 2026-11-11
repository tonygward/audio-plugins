import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { runLinuxWorker, limitReason, resumePathsValid, validateLinuxConfig, preflight, baseEnvironment, resolveChangelogConflict, ciOutcome, CI_VALIDATION } from '../linux-worker.mjs';
import { runProcess } from '../process.mjs';
import { readState, atomic } from '../state.mjs';
import { blockStopped, resumeBlocked } from '../blocked.mjs';
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
function adapter({ builds = ['success'], onClaude, claudeFails, prState } = {}) {
  const seen = { claude: [], gh: [] }, results = new Map(), pr = { existing: [] };
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
      if (args[0] === 'pr' && args[1] === 'list') return { stdout: JSON.stringify(pr.existing), stderr: '' };
      if (args[0] === 'pr' && args[1] === 'create') return { stdout: 'https://github.com/example/plugins/pull/7\n', stderr: '' };
      if (args[0] === 'pr' && args[1] === 'edit') return { stdout: '', stderr: '' };
      if (args[0] === 'pr' && args[1] === 'view') return { stdout: JSON.stringify({ state: prState ?? 'OPEN' }), stderr: '' };
      if (args[0] === 'run' && args[1] === 'list') {
        const sha = args[args.indexOf('--commit') + 1];
        if (!results.has(sha)) results.set(sha, builds[Math.min(results.size, builds.length - 1)]);
        const pending = results.get(sha) === 'pending';
        return { stdout: JSON.stringify([{ databaseId: ++counter, status: pending ? 'in_progress' : 'completed', conclusion: pending ? null : results.get(sha), headSha: sha, event: 'pull_request' }]), stderr: '' };
      }
      if (args[0] === 'run' && args[1] === 'view') return { stdout: 'error C2065: undeclared identifier\n', stderr: '' };
    }
    throw Error('Unexpected command ' + exe + ' ' + args.join(' '));
  };
  return { run, seen, results, pr };
}
const options = (f, a, extra = {}) => ({ trigger: 'daily', run: a.run, loadUsage: goodUsage, clock: () => now, sleep: async () => {}, check: () => {}, environment: { PATH: process.env.PATH, HOME: f.root }, ...extra });
const body = (f, report) => fs.readFileSync(path.join(f.config.stateDirectory, `${report.outcomes[0].id}-pr.md`), 'utf8');

test('config must keep merge and release off, use ci mode and sensible limits', t => {
  const f = fixture(t); validateLinuxConfig(f.config);
  for (const mutate of [c => { c.autoMerge = true; }, c => { c.releasePublication = true; }, c => { c.worker.mode = 'scheduled'; }, c => { c.worker.maximumRepairs = 4; }, c => { c.worker.maximumRunMinutes = 4; }, c => { c.worker.maximumRunMinutes = 601; }, c => { c.worker.tokenFile = 'relative'; }, c => { c.claudeUsage.enabled = false; }, c => { c.worker.repositorySlug = 'bad slug'; }]) {
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
  await runLinuxWorker(f.config, options(f, a, { loadUsage: () => ({ allowed: false, reason: 'Weekly usage at or above 60%', snapshot: null }) }));
  assert.equal(a.seen.claude.length, 0);
});

test('when Claude stops with an error the log says why, without the model text', async t => {
  const f = fixture(t), a = adapter({ claudeFails: { type: 'result', subtype: 'error_max_turns', is_error: true, num_turns: 31, total_cost_usd: 1.5, result: 'SECRET MODEL TEXT' } });
  const report = await runLinuxWorker(f.config, options(f, a));
  assert.equal(report.outcomes[0].interrupted, true);
  const log = fs.readFileSync(path.join(f.config.stateDirectory, 'logs', 'worker.jsonl'), 'utf8');
  assert.match(log, /"stage":"agent-failed"[^\n]*"subtype":"error_max_turns"[^\n]*"turns":31/); assert.doesNotMatch(log, /SECRET MODEL TEXT/);
});

// ---- Board pickup switch-over (AUTO-11) ----
import { writeHandoff, readHandoff } from '../handoff.mjs';
import { parsePrompt } from '../backlog.mjs';
import { validateBoardConfig } from '../linux-worker.mjs';

const boardConfig = f => ({ ...f.config, batchLimit: 1, boardPickup: { enabled: true, projectId: 'P', repositories: ['example/plugins'], executionRepositories: ['example/plugins'],
  promptRepository: 'example/plugins', statusFieldId: 'F', doingOptionId: 'D', doneOptionId: 'O' } });
const taskHash = f => parsePrompt('eq/prompts/41-fixture.md', fs.readFileSync(path.join(f.repo, 'eq/prompts/41-fixture.md'), 'utf8')).hash;
const handoffFor = (f, extra = {}) => writeHandoff(f.config.stateDirectory, { job: 'matrix-test-1', revision: 'a'.repeat(40), now,
  selected: { taskId: 'EQ-41', promptHash: taskHash(f), status: 'Doing', issue: 41, itemId: 'item-41', repository: 'example/plugins', issueRepository: 'example/plugins', ...extra } });
// The shared fake GitHub adapter does not know issue commands, so wrap it.
const withIssues = a => { const comments = [], moves = []; const run = async (exe, args, o) => {
  if (exe === GH && args[0] === 'issue' && args[1] === 'view') return { stdout: JSON.stringify({ comments: comments.map(body => ({ body })) }), stderr: '' };
  if (exe === GH && args[0] === 'issue' && args[1] === 'comment') { if (a.failComment) throw Error('GitHub down'); comments.push(args[args.indexOf('--body') + 1]); return { stdout: '', stderr: '' }; }
  if (exe === GH && args[0] === 'api' && args[1] === 'graphql') { moves.push(args.find(a => a.startsWith('o=')).slice(2)); return { stdout: '', stderr: '' }; }
  return a.run(exe, args, o); }; return { run, seen: a.seen, results: a.results, pr: a.pr, comments, moves, set failComment(v) { a.failComment = v; } }; };

test('board mode without a hand-off idles: no fallback to the Markdown selector', async t => {
  const f = fixture(t), a = adapter(), report = await runLinuxWorker(boardConfig(f), options(f, a, { trigger: 'timer' }));
  assert.equal(report.idle, true); assert.equal(a.seen.claude.length, 0);
  assert.equal(Object.values(readState(f.config.stateDirectory).tasks).some(x => x.state === 'claimed'), false);
});

test('board mode builds the chosen task, claims it, and comments on the issue with the job', async t => {
  const f = fixture(t), a = withIssues(adapter()); handoffFor(f);
  const report = await runLinuxWorker(boardConfig(f), options(f, a, { trigger: 'timer' }));
  assert.equal(report.outcomes.length, 1);
  const task = readState(f.config.stateDirectory).tasks['EQ-41'];
  assert.equal(task.state, 'awaiting-review'); assert.equal(task.job, 'matrix-test-1'); assert.equal(task.issue, 41); assert.equal(task.itemId, 'item-41');
  assert.ok(task.pickupCommentAt);
  assert.equal(a.comments.length, 1); assert.match(a.comments[0], /Matrix has picked up \*\*EQ-41\*\*/); assert.match(a.comments[0], /matrix-test-1/);
  assert.equal(readHandoff(f.config.stateDirectory, { now }), null, 'hand-off is consumed');
});

test('a stale, changed or already-claimed hand-off starts nothing', async t => {
  const stale = fixture(t); handoffFor(stale);
  const old = await runLinuxWorker(boardConfig(stale), options(stale, adapter(), { trigger: 'timer', clock: () => now + 3600000 }));
  assert.equal(old.idle, true);
  const changed = fixture(t); handoffFor(changed, { promptHash: 'edited' });
  const a = adapter(), report = await runLinuxWorker(boardConfig(changed), options(changed, a, { trigger: 'timer' }));
  assert.match(report.reason, /Prompt changed/); assert.equal(a.seen.claude.length, 0);
});

test('the card moves to Doing and the starting comment quotes the limits, before any code is written', async t => {
  const f = fixture(t), a = withIssues(adapter()); handoffFor(f);
  await runLinuxWorker(boardConfig(f), options(f, a, { trigger: 'timer' }));
  assert.deepEqual(a.moves, ['D']);
  assert.match(a.comments[0], /Limits: up to 10 Claude turns/); assert.match(a.comments[0], /60% weekly Claude usage/);
  assert.ok(readState(f.config.stateDirectory).tasks['EQ-41'].movedToDoingAt);
});

test('no code is written when the starting comment cannot be posted; the task is blocked', async t => {
  const f = fixture(t), base = adapter(), a = withIssues(base); a.failComment = true; handoffFor(f);
  const report = await runLinuxWorker(boardConfig(f), options(f, a, { trigger: 'timer' }));
  assert.equal(report.outcomes[0].interrupted, true); assert.equal(base.seen.claude.length, 0, 'Claude must not run');
  const task = readState(f.config.stateDirectory).tasks['EQ-41'];
  assert.equal(task.state, 'interrupted'); assert.match(task.reason, /Starting comment or Doing move failed/);
});

test('board configuration enforces one task and complete settings', t => {
  const f = fixture(t), good = boardConfig(f);
  assert.doesNotThrow(() => validateBoardConfig(good));
  assert.throws(() => validateBoardConfig({ ...good, batchLimit: 3 }), /batchLimit 1/);
  assert.throws(() => validateBoardConfig({ ...good, worker: { ...good.worker, maximumPrompts: 2 } }), /maximumPrompts 1/);
  assert.throws(() => validateBoardConfig({ ...good, boardPickup: { ...good.boardPickup, projectId: '' } }), /Invalid boardPickup/);
  assert.throws(() => validateBoardConfig({ ...good, boardPickup: { ...good.boardPickup, doneOptionId: '' } }), /doneOptionId/);
  assert.throws(() => validateBoardConfig({ ...good, boardPickup: { ...good.boardPickup, doingOptionId: undefined } }), /doingOptionId/);
  assert.throws(() => validateBoardConfig({ ...good, boardPickup: { ...good.boardPickup, tokenFile: 'relative' } }), /absolute/);
  assert.doesNotThrow(() => validateBoardConfig({ ...good, boardPickup: { enabled: false } }));
  assert.doesNotThrow(() => validateBoardConfig(f.config));
});

test('a Todo pickup uses up the overnight slot for the London day; a Doing pickup does not', async t => {
  for (const [status, expected] of [['Todo', true], ['Doing', false]]) {
    const f = fixture(t), a = withIssues(adapter()); handoffFor(f, { status });
    await runLinuxWorker(boardConfig(f), options(f, a, { trigger: 'timer' }));
    assert.equal(Boolean(readState(f.config.stateDirectory).lastWorkerDaily), expected, status);
  }
});

test('the run budget stops the agent before it starts and leaves the task interrupted with the reason', async t => {
  const f = fixture(t), base = adapter(), a = withIssues(base); handoffFor(f);
  const config = boardConfig(f); config.worker = { ...config.worker, maximumRunMinutes: 5 };
  // Time stands still until the claim is saved, then six minutes pass before the agent would start.
  const clock = () => now + (readState(f.config.stateDirectory).tasks['EQ-41']?.runStartedAt ? 360000 : 0);
  const report = await runLinuxWorker(config, options(f, a, { trigger: 'timer', clock }));
  assert.equal(report.outcomes[0].interrupted, true);
  assert.equal(base.seen.claude.length, 0, 'Claude must not run once the budget is spent');
  const task = readState(f.config.stateDirectory).tasks['EQ-41'];
  assert.equal(task.state, 'interrupted'); assert.ok(task.runStartedAt);
  assert.match(task.reason, /Run time limit of 5 minutes/);
});

test('a pull request that merged the moment Build passed is left alone: no extra push, not reported as blocked', async t => {
  const f = fixture(t), a = adapter({ prState: 'MERGED' }), report = await runLinuxWorker(f.config, options(f, a));
  assert.equal(report.outcomes[0].blocked, false);
  assert.equal(a.seen.gh.filter(c => c.args[0] === 'pr' && c.args[1] === 'view').length, 1);
  assert.equal(Object.values(readState(f.config.stateDirectory).tasks).find(x => x.pr).state, 'awaiting-review');
});

const ISSUE_SPEC = '## Objective\nAdd an example improvement musicians can hear.\n\n## Implementation\nChange the example source file in the EQ plugin.\n\n## Acceptance criteria and tests\nThe example changes and a test covers it.\n';

test('a prompt generated from the issue is saved in the same pull request as the change', async t => {
  const { promptFromIssue } = await import('../issue-prompt.mjs');
  const generated = promptFromIssue({ taskId: 'EQ-42', issue: 42, title: 'EQ-42 — Example improvement' }, ISSUE_SPEC);
  const f = fixture(t), a = withIssues(adapter());
  writeHandoff(f.config.stateDirectory, { job: 'matrix-test-1', revision: 'a'.repeat(40), now,
    selected: { taskId: 'EQ-42', promptHash: generated.hash, status: 'Doing', issue: 42, itemId: 'item-42', repository: 'example/plugins', issueRepository: 'example/plugins', generated: generated.generated } });
  const report = await runLinuxWorker(boardConfig(f), options(f, a, { trigger: 'timer' }));
  assert.equal(report.outcomes.length, 1); assert.equal(report.outcomes[0].blocked, false);
  const branch = branches(f).replace('*', '').trim();
  const saved = execFileSync(gitExecutable, ['--git-dir', f.remote, 'show', `${branch}:eq/prompts/42-example-improvement.md`], { encoding: 'utf8' });
  assert.match(saved, /id: EQ-42/); assert.match(saved, /Generated by Matrix/);
  assert.match(body(f, report), /no prompt file/);
  assert.equal(readState(f.config.stateDirectory).tasks['EQ-42'].state, 'awaiting-review');
});

test('a generated prompt whose hash does not match the hand-off is not adopted', async t => {
  const { promptFromIssue } = await import('../issue-prompt.mjs');
  const generated = promptFromIssue({ taskId: 'EQ-42', issue: 42, title: 'EQ-42 — Example improvement' }, ISSUE_SPEC);
  const f = fixture(t), a = adapter();
  writeHandoff(f.config.stateDirectory, { job: 'j', revision: 'a'.repeat(40), now,
    selected: { taskId: 'EQ-42', promptHash: 'tampered', status: 'Doing', issue: 42, itemId: 'i', repository: 'example/plugins', issueRepository: 'example/plugins', generated: generated.generated } });
  const report = await runLinuxWorker(boardConfig(f), options(f, a, { trigger: 'timer' }));
  assert.equal(report.idle, true); assert.equal(a.seen.claude.length, 0);
});

test('the starting comment records why the card was picked up, from the hand-off, not from the status after the claim', async t => {
  const f = fixture(t), a = withIssues(adapter()); handoffFor(f);
  await runLinuxWorker(boardConfig(f), options(f, a, { trigger: 'timer' }));
  const task = readState(f.config.stateDirectory).tasks['EQ-41'];
  assert.equal(task.pickup.reason, 'Run now (Doing)'); assert.equal(task.pickup.preClaimStatus, 'Doing');
  assert.match(a.comments[0], /Pickup reason: Run now \(Doing\)/); assert.match(a.comments[0], /Before claim: Doing/);
  const todo = fixture(t), b = withIssues(adapter()); handoffFor(todo, { status: 'Todo' });
  await runLinuxWorker(boardConfig(todo), options(todo, b, { trigger: 'timer' }));
  assert.match(b.comments[0], /Pickup reason: Scheduled \(Todo\)/); assert.match(b.comments[0], /London schedule date: /);
});

// ---- Limits, parking and resuming committed work (AUTO-12) ----
test('a turn, cost or time limit stops the task and the reason names the limit', async t => {
  const cases = [
    [{ claudeFails: { type: 'result', subtype: 'error_max_turns', is_error: true, num_turns: 11, total_cost_usd: 1 } }, /Claude turn limit \(10 turns\)/],
    [{ claudeFails: { type: 'result', subtype: 'error_max_budget_usd', is_error: true, num_turns: 4, total_cost_usd: 5 } }, /Claude cost limit \(5 dollars\)/],
    [{ onClaude: () => { throw Error('Process timed out'); } }, /time limit for one attempt \(1 minutes\)/],
  ];
  for (const [extra, expected] of cases) {
    const f = fixture(t), a = adapter(extra), report = await runLinuxWorker(f.config, options(f, a));
    assert.equal(report.outcomes[0].interrupted, true);
    const task = readState(f.config.stateDirectory).tasks['EQ-41'];
    assert.equal(task.state, 'interrupted'); assert.match(task.reason, expected); assert.doesNotMatch(task.reason, /inspect retained worktree/);
    assert.equal(task.commit, undefined, 'nothing was committed, so a resume starts clean');
  }
});

test('limitReason names only configured limits and ignores other failures', () => {
  const w = { maximumTurns: 60, maximumBudgetUsd: 5, timeoutSeconds: 1800 };
  assert.match(limitReason({ subtype: 'error_max_turns' }, null, w), /60 turns/);
  assert.match(limitReason({ subtype: 'error_max_budget_usd' }, null, w), /5 dollars/);
  assert.match(limitReason({}, Error('Process timed out'), w), /30 minutes/);
  assert.equal(limitReason({ subtype: 'error_during_execution' }, Error('Process failed'), w), null);
  assert.equal(limitReason({ exitCode: 1 }, Error('Process failed'), w), null);
});

test('resume only trusts a branch and worktree where Matrix keeps them', () => {
  const root = path.join(os.tmpdir(), 'wt');
  assert.equal(resumePathsValid({ worktree: path.join(root, 'EQ-41-abc123'), branch: 'matrix/eq-41-abc123' }, root), true);
  for (const bad of [{ worktree: path.join(os.tmpdir(), 'elsewhere'), branch: 'matrix/eq-41-abc123' }, { worktree: root, branch: 'matrix/eq-41-abc123' },
    { worktree: path.join(root, '..', 'x'), branch: 'matrix/eq-41-abc123' }, { worktree: path.join(root, 'EQ-41-abc123'), branch: 'main' },
    { worktree: path.join(root, 'EQ-41-abc123'), branch: 'matrix/eq-41-abc123;rm' }, { branch: 'matrix/eq-41-abc123' }]) assert.equal(resumePathsValid(bad, root), false, JSON.stringify(bad));
});

test('a Build that keeps failing parks the task as interrupted, keeping the pull request and commit for a resume', async t => {
  const f = fixture(t), a = adapter({ builds: ['failure'] }), report = await runLinuxWorker(f.config, options(f, a));
  assert.equal(report.outcomes[0].blocked, true); assert.equal(report.outcomes[0].interrupted, true);
  const task = readState(f.config.stateDirectory).tasks['EQ-41'];
  assert.equal(task.state, 'interrupted'); assert.match(task.reason, /Hosted Build failed/);
  assert.equal(task.pr, 'https://github.com/example/plugins/pull/7'); assert.ok(task.commit); assert.ok(task.worktree && fs.existsSync(task.worktree));
});

// A first run that stops with committed work and an open pull request, then an explicit resume by moving the card back to Doing.
const quietIo = () => ({ moveToBlocked: async () => {}, hasComment: async () => false, comment: async () => {} });
async function stopThenResume(t, { builds, firstRun = {} }) {
  const f = fixture(t), base = adapter({ builds }), a = withIssues(base), config = boardConfig(f), dir = f.config.stateDirectory; handoffFor(f);
  const first = await runLinuxWorker(config, options(f, a, { trigger: 'timer', ...firstRun }));
  assert.equal(first.outcomes[0].interrupted, true);
  await blockStopped(dir, quietIo());
  const stopped = readState(dir).tasks['EQ-41'];
  base.pr.existing = [{ url: stopped.pr, headRefOid: stopped.commit, state: 'OPEN' }];
  await resumeBlocked(dir, { ok: true, cards: [{ taskId: 'EQ-41', status: 'Doing', issue: 41 }] }, quietIo(), { pulls: [{ headRefName: stopped.branch, state: 'OPEN' }] });
  const resumed = readState(dir).tasks['EQ-41']; assert.equal(resumed.state, 'pending'); assert.equal(resumed.resumeWork, true);
  const again = () => writeHandoff(dir, { job: 'matrix-test-2', revision: 'a'.repeat(40), now, resumedFrom: resumed.resumedFrom,
    selected: { taskId: 'EQ-41', promptHash: taskHash(f), status: 'Doing', issue: 41, itemId: 'item-41', repository: 'example/plugins', issueRepository: 'example/plugins' } });
  return { f, base, a, config, dir, stopped, resumed, again };
}

test('an explicit resume carries on from the saved branch and pull request: one agent repair, no second pull request, no repeat coding', async t => {
  const { f, base, a, config, dir, stopped, again } = await stopThenResume(t, { builds: ['failure', 'failure', 'failure', 'success'] });
  assert.equal(base.seen.claude.length, 3); again();
  const report = await runLinuxWorker(config, options(f, a, { trigger: 'timer' }));
  assert.equal(report.outcomes[0].interrupted, undefined); assert.equal(report.outcomes[0].blocked, false);
  assert.equal(base.seen.claude.length, 4, 'one more agent run, on the failed build');
  assert.match(base.seen.claude[3].input, /Matrix is resuming this task/); assert.match(base.seen.claude[3].input, /Do not start over/); assert.match(base.seen.claude[3].input, /ci-failure\.log/);
  assert.equal(base.seen.claude[3].cwd, stopped.worktree, 'the same worktree, not a new one');
  assert.equal(base.seen.gh.filter(c => c.args[0] === 'pr' && c.args[1] === 'create').length, 1, 'the existing pull request is reused');
  assert.doesNotMatch(fs.readFileSync(path.join(dir, 'EQ-41-pr.md'), 'utf8'), /## Blocked/, 'the old Blocked section is gone from the description');
  const task = readState(dir).tasks['EQ-41'];
  assert.equal(task.state, 'awaiting-review'); assert.equal(task.pr, stopped.pr); assert.equal(task.resumeWork, undefined); assert.equal(task.branch, stopped.branch);
  const subjects = execFileSync(gitExecutable, ['--git-dir', f.remote, 'log', '--format=%s', stopped.branch], { encoding: 'utf8' });
  assert.match(subjects, /Matrix resumed implementation/);
  assert.match(a.comments.at(-1), /Pickup reason: Resumed \(Doing\)/); assert.match(a.comments.at(-1), /Resumed from: job `matrix-test-1`/);
});

test('a resume whose pushed commit now has a green Build starts no agent, even after the worktree was lost', async t => {
  let elapsed = 0;
  const { f, base, a, config, dir, stopped, again } = await stopThenResume(t, { builds: ['pending'], firstRun: { clock: () => now + elapsed, sleep: async ms => { elapsed += ms; } } });
  assert.match(readState(dir).tasks['EQ-41'].resumedFrom.reason, /did not finish in time/);
  assert.equal(base.seen.claude.length, 1);
  base.results.set(stopped.commit, 'success');
  f.git(['worktree', 'remove', '--force', stopped.worktree]); assert.equal(fs.existsSync(stopped.worktree), false);
  again(); elapsed = 0;
  const report = await runLinuxWorker(config, options(f, a, { trigger: 'timer' }));
  assert.equal(report.outcomes[0].blocked, false); assert.equal(base.seen.claude.length, 1, 'no agent: nothing needed fixing');
  assert.ok(fs.existsSync(stopped.worktree), 'recreated from the pushed branch');
  assert.equal(readState(dir).tasks['EQ-41'].state, 'awaiting-review');
  assert.match(fs.readFileSync(path.join(dir, 'logs', 'worker.jsonl'), 'utf8'), /"stage":"worktree-resumed"/);
});

test('a resume refuses a saved worktree that is not where Matrix keeps them, and runs nothing', async t => {
  const { f, base, a, config, dir, again } = await stopThenResume(t, { builds: ['failure'] });
  const state = readState(dir); state.tasks['EQ-41'].worktree = path.join(os.tmpdir(), 'somewhere-else'); atomic(path.join(dir, 'state.json'), state);
  again(); const before = base.seen.claude.length;
  const report = await runLinuxWorker(config, options(f, a, { trigger: 'timer' }));
  assert.equal(report.outcomes[0].interrupted, true); assert.equal(base.seen.claude.length, before);
  assert.match(readState(dir).tasks['EQ-41'].reason, /not where Matrix keeps them/);
});
