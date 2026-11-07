import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { work, validateChanges, validateWorker, agentArguments, approve, approvalDigest, parseAgent, verifyGitPointer } from '../worker.mjs';
import { runProcess, limitedEnvironment } from '../process.mjs';
import { readState, atomic, lockedAsync } from '../state.mjs';
import { digest } from '../backlog.mjs';
import { TELEMETRY_SOURCE } from '../policy.mjs';
const gitExecutable = execFileSync(process.platform === 'win32' ? 'where.exe' : 'which', ['git'], { encoding: 'utf8' }).trim().split(/\r?\n/)[0];
const now = Date.now();
const goodUsage = () => ({ allowed: true, reason: 'Below threshold', extraUsageEnabled: false, snapshot: { source: TELEMETRY_SOURCE, percentage: 20, observedAt: new Date(now).toISOString(), resetsAt: new Date(now + 3600000).toISOString() } });
function fixture(t) {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'matrix-worker-'));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  const repo = path.join(root, 'repo'), remote = path.join(root, 'remote.git'); fs.mkdirSync(repo);
  const git = args => execFileSync(gitExecutable, ['-c', 'core.hooksPath=' + (process.platform === 'win32' ? 'NUL' : '/dev/null'), ...args], { cwd: repo, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim();
  git(['init', '-b', 'main']); git(['config', 'user.name', 'Matrix Test']); git(['config', 'user.email', 'matrix@example.invalid']);
  fs.mkdirSync(path.join(repo, 'eq/Source'), { recursive: true }); fs.mkdirSync(path.join(repo, 'eq/prompts'), { recursive: true });
  fs.writeFileSync(path.join(repo, '.gitignore'), 'build/\n');
  fs.writeFileSync(path.join(repo, 'AGENTS.md'), 'Preserve sessions; no releases.'); fs.writeFileSync(path.join(repo, 'CLAUDE.md'), 'Follow AGENTS.md.');
  fs.writeFileSync(path.join(repo, 'eq/Source/example.cpp'), 'before\n');
  fs.writeFileSync(path.join(repo, 'eq/prompts/41-fixture.md'), '---\nexecution: build-now\nmodel: sonnet\neffort: low\n---\n# Fixture task\nFix the example.');
  git(['add', '.']); git(['commit', '-m', 'Fixture baseline']);
  execFileSync(gitExecutable, ['init', '--bare', remote], { stdio: 'pipe' }); git(['remote', 'add', 'origin', remote]); git(['push', '-u', 'origin', 'main']);
  const config = { repository: repo, stateDirectory: path.join(root, 'state'), enabled: true, liveExecution: true, autoMerge: false, releasePublication: false, batchLimit: 3, dailyTime: '02:00',
    claudeUsage: { enabled: true, executable: process.execPath }, worker: { enabled: true, gitExecutable, ghExecutable: process.execPath,
      powershellExecutable: process.execPath, buildScript: path.join(root, 'trusted-build.ps1'), buildCredentialFile: path.join(root, 'credentials.xml'), approvalFile: path.join(root, 'approval.json'),
      worktreeRoot: path.join(root, 'worktrees'), juceDirectory: path.join(root, 'JUCE'), repositorySlug: 'example/plugins', maximumPrompts: 1, maximumTurns: 10, maximumBudgetUsd: 5, timeoutSeconds: 60 } };
  fs.writeFileSync(config.worker.buildScript, 'trusted script fixture');
  const revision = git(['rev-parse', 'HEAD']);
  return { root, repo, git, config, revision };
}
function adapter(f, options = {}) {
  const calls = [], counts = { agent: 0, build: 0, pr: 0 };
  const run = async (exe, args, invocation) => {
    calls.push({ exe, args, cwd: invocation.cwd });
    if (args.includes('remote') && args.includes('get-url')) return { stdout: 'https://github.com/example/plugins\n', stderr: '' };
    if (args[0] === '-p') {
      counts.agent++;
      if (options.agentFailure) throw Error('Agent interrupted');
      const target = options.unsafe ? '.github/workflows/unsafe.yml' : options.ui ? 'eq/Source/PluginEditor.cpp' : 'eq/Source/example.cpp';
      const code = 'const fs=require("node:fs"),p=require("node:path");const target=p.resolve(process.argv[1]);fs.mkdirSync(p.dirname(target),{recursive:true});fs.writeFileSync(target,"implemented\\n");console.log(JSON.stringify({type:"result",subtype:"success",is_error:false,num_turns:1,total_cost_usd:0.01}));';
      const response = await runProcess(process.execPath, ['-e', code, target], invocation);
      if (options.pauseAfterAgent) fs.writeFileSync(path.join(f.config.stateDirectory, 'PAUSED'), '');
      return response;
    }
    if (args[0] === '-NoProfile') {
      counts.build++;
      if (counts.build <= (options.failedBuilds ?? 0)) throw Error('Simulated build failure');
      if (options.changeDuringBuild) fs.appendFileSync(path.join(invocation.cwd, 'eq/Source/example.cpp'), 'tampered');
      if (options.changeGitPointer) {
        try {
          const pointer = path.join(invocation.cwd, '.git');
          fs.writeFileSync(pointer, 'gitdir: invalid-path');
        }
        catch (error) { counts.pointerError = error.code; throw error; }
      }
      return { stdout: options.warning ? 'warning C4996: review required' : 'Build and tests passed', stderr: '' };
    }
    if (args[0] === 'pr') {
      if (args[1] === 'list') return { stdout: JSON.stringify(options.existingPR ?? []), stderr: '' };
      counts.pr++;
      if (options.prFailure) throw Error('Uncertain remote PR creation');
      return { stdout: 'https://github.com/example/plugins/pull/1\n', stderr: '' };
    }
    return runProcess(exe, args, invocation);
  };
  return { run, calls, counts, loadApproval: () => ({ expiresAt: new Date(now + 3600000).toISOString() }), loadUsage: options.loadUsage ?? goodUsage, clock: () => now };
}
test('Real Git worktree → simulated Claude process → validation → commit/push → draft PR', async t => {
  const f = fixture(t), a = adapter(f); const result = await work(f.config, a);
  assert.deepEqual(result.outcomes, [{ id: 'EQ-41', pr: 'https://github.com/example/plugins/pull/1', blocked: false }]);
  const task = readState(f.config.stateDirectory).tasks['EQ-41'];
  assert.equal(task.state, 'awaiting-review'); assert.equal(task.phase, 'draft-pr');
  assert.equal(fs.readFileSync(path.join(f.repo, 'eq/Source/example.cpp'), 'utf8'), 'before\n');
  assert.equal(f.git(['rev-parse', 'main']), f.revision);
  assert.equal(f.git(['rev-parse', `refs/remotes/origin/${task.branch}`]), task.commit);
  assert.ok(a.calls.find(c => c.args[0] === 'pr' && c.args[1] === 'create').args.includes('--draft'));
  assert.ok(a.calls.filter(c => c.args.includes('push')).every(c => !c.args.includes('--force') && !c.args.includes('main')));
  await work(f.config, a); assert.equal(a.counts.agent, 1); assert.equal(a.counts.pr, 1);
});
test('Disabled worker performs no external operation', async () => {
  const result = await work({ enabled: true, liveExecution: false, worker: { enabled: true } }, { run: () => { throw Error('must not run'); } });
  assert.equal(result.liveExecution, false);
});

test('Build launcher receives the approved worktree root', async t => {
  const f = fixture(t), a = adapter(f); await work(f.config, a);
  const build = a.calls.find(c => c.args[0] === '-NoProfile');
  assert.equal(build.args[build.args.indexOf('-WorktreeRoot') + 1], f.config.worker.worktreeRoot);
  const request = JSON.parse(fs.readFileSync(build.args[build.args.indexOf('-RequestFile') + 1], 'utf8'));
  const targets = request.isolationTargets;
  assert.deepEqual(targets.map(t => t.path), [path.join(f.repo, '.git'), path.join(f.repo, '.git', 'HEAD'), path.join(f.repo, '.git', 'config'), f.config.stateDirectory, f.config.worker.worktreeRoot, f.config.worker.buildCredentialFile, f.config.worker.approvalFile]);
});

test('An outstanding ACL lease stops work before any external operation', async t => {
  const f = fixture(t), a = adapter(f);
  fs.writeFileSync(path.join(path.dirname(f.config.worker.buildCredentialFile), 'active-build-lease.json'), '{}');
  await assert.rejects(work(f.config, a), /Outstanding build ACL lease/);
  assert.equal(a.calls.length, 0);
});

test('A failed ACL recovery prevents repair and publication', async t => {
  const f = fixture(t), a = adapter(f), original = a.run;
  a.run = async (exe, args, invocation) => {
    if (args[0] === '-NoProfile') {
      fs.writeFileSync(path.join(path.dirname(f.config.worker.buildCredentialFile), 'active-build-lease.json'), '{}');
      throw Error('ACL recovery failed');
    }
    return original(exe, args, invocation);
  };
  const result = await work(f.config, a);
  assert.equal(result.outcomes[0].interrupted, true);
  assert.equal(a.counts.agent, 1); assert.equal(a.counts.pr, 0);
});

test('Effective build isolation failure stops without repair or publication', async t => {
  const f = fixture(t), a = adapter(f), original = a.run;
  a.run = async (exe, args, invocation) => {
    if (args[0] === '-NoProfile') throw Object.assign(Error('Isolation failure'), { exitCode: 74 });
    return original(exe, args, invocation);
  };
  const result = await work(f.config, a);
  assert.equal(result.outcomes[0].interrupted, true);
  assert.equal(a.counts.agent, 1); assert.equal(a.counts.pr, 0);
});
test('Unknown/high usage or enabled paid continuation prevents claims and agent invocation', async t => {
  for (const usage of [{ allowed: false, reason: '85%' }, { ...goodUsage(), extraUsageEnabled: true }, { ...goodUsage(), snapshot: { ...goodUsage().snapshot, percentage: 85 } }]) {
    const f = fixture(t), a = adapter(f, { loadUsage: () => usage }); await work(f.config, a);
    assert.equal(a.counts.agent, 0); assert.equal(readState(f.config.stateDirectory).tasks['EQ-41'].attempts, 0);
  }
});
test('One bounded repair uses a fresh usage check and remaining budget', async t => {
  const f = fixture(t); let checks = 0;
  const a = adapter(f, { failedBuilds: 1, loadUsage: () => { checks++; return goodUsage(); } });
  const result = await work(f.config, a); assert.equal(a.counts.agent, 2); assert.equal(checks, 2); assert.equal(result.outcomes[0].blocked, false);
  const calls = a.calls.filter(c => c.args[0] === '-p');
  assert.equal(calls[1].args[calls[1].args.indexOf('--max-turns') + 1], '9');
});
test('Failed tests remain a blocked draft PR after at most one repair', async t => {
  const f = fixture(t), a = adapter(f, { failedBuilds: 99 });
  const result = await work(f.config, a); assert.equal(a.counts.agent, 2); assert.equal(result.outcomes[0].blocked, true);
  assert.match(fs.readFileSync(path.join(f.config.stateDirectory, 'EQ-41-pr.md'), 'utf8'), /## Blocked/);
});
test('Usage crossed between attempts stops repair and preserves a blocked draft', async t => {
  const f = fixture(t); let checks = 0;
  const a = adapter(f, { failedBuilds: 1, loadUsage: () => ++checks === 1 ? goodUsage() : { allowed: false, reason: '85%' } });
  const result = await work(f.config, a); assert.equal(a.counts.agent, 1); assert.equal(result.outcomes[0].blocked, true);
});
test('Protected workflow edits are retained locally without build, push or PR', async t => {
  const f = fixture(t), a = adapter(f, { unsafe: true }); const result = await work(f.config, a);
  assert.equal(result.outcomes[0].interrupted, true); assert.equal(a.counts.build, 0); assert.equal(a.counts.pr, 0);
  assert.equal(readState(f.config.stateDirectory).tasks['EQ-41'].state, 'interrupted');
  assert.ok(fs.existsSync(readState(f.config.stateDirectory).tasks['EQ-41'].worktree));
});
test('Source tampering during build stops publication', async t => {
  const f = fixture(t), a = adapter(f, { changeDuringBuild: true }); const result = await work(f.config, a);
  assert.equal(result.outcomes[0].interrupted, true); assert.equal(a.counts.pr, 0);
});
test('Changing worktree Git metadata stops controller Git before publication', async t => {
  const f = fixture(t), a = adapter(f, { changeGitPointer: true }); const result = await work(f.config, a);
  if (a.counts.pointerError === 'EPERM' || a.counts.pointerError === 'EACCES') {
    assert.equal(result.outcomes[0].blocked, true);
    t.diagnostic('OS denied fixture metadata mutation; verified blocked draft instead. Pointer guard is separately tested.');
  } else { assert.equal(result.outcomes[0].interrupted, true); assert.equal(a.counts.pr, 0); }
});
test('Git pointer hash guard rejects changed content and directories independently of OS protection', t => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'matrix-pointer-test-')); t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  const file = path.join(root, 'pointer.txt'); fs.writeFileSync(file, 'gitdir: approved');
  const expected = digest(fs.readFileSync(file)); verifyGitPointer(file, expected);
  fs.writeFileSync(file, 'gitdir: changed'); assert.throws(() => verifyGitPointer(file, expected), /pointer changed/);
  assert.throws(() => verifyGitPointer(root, expected), /pointer changed/);
});
test('UI changes stay blocked pending real before/after screenshot review', async t => {
  const f = fixture(t), a = adapter(f, { ui: true }); const result = await work(f.config, a);
  assert.equal(result.outcomes[0].blocked, true);
  assert.match(fs.readFileSync(path.join(f.config.stateDirectory, 'EQ-41-pr.md'), 'utf8'), /screenshots/);
});
test('Agent interruption and uncertain PR creation never blindly rerun', async t => {
  for (const option of [{ agentFailure: true }, { prFailure: true }]) {
    const f = fixture(t), a = adapter(f, option); await work(f.config, a); const started = a.counts.agent;
    await work(f.config, a); assert.equal(a.counts.agent, started);
    assert.equal(readState(f.config.stateDirectory).tasks['EQ-41'].state, 'interrupted');
  }
});
test('Pause after agent stops builds and publication', async t => {
  const f = fixture(t), a = adapter(f, { pauseAfterAgent: true }); await work(f.config, a);
  assert.equal(a.counts.build, 0); assert.equal(a.counts.pr, 0);
});
test('Stale main prevents claims; dirty source fails before agent', async t => {
  const f = fixture(t), a = adapter(f); let scans = 0;
  a.loadSnapshot = () => ({ revision: ++scans === 1 ? f.revision : 'b'.repeat(40), files: new Map([['eq/prompts/41-fixture.md', fs.readFileSync(path.join(f.repo, 'eq/prompts/41-fixture.md'), 'utf8')]]) });
  await work(f.config, a); assert.equal(a.counts.agent, 0);
  fs.writeFileSync(path.join(f.repo, 'untracked.txt'), 'dirty'); await assert.rejects(work(f.config, a), /clean/);
});
test('Approval binds environment, exact configuration, expiry and launcher digest', t => {
  const f = fixture(t), identity = { user: 'Matrix', host: 'VM' };
  const approval = { configHash: approvalDigest(f.config), user: identity.user, host: identity.host, authorised: true, isolatedAccount: true,
    networkRestricted: true, separateBuildAccount: true, noReleaseCredentials: true, expiresAt: new Date(now + 3600000).toISOString(), buildScriptHash: digest(fs.readFileSync(f.config.worker.buildScript)) };
  atomic(f.config.worker.approvalFile, approval);
  if (process.platform !== 'win32') { assert.throws(() => approve(f.config, now, identity), /Windows/); return; }
  assert.equal(approve(f.config, now, identity).authorised, true);
  for (const mutation of [{ authorised: false }, { user: 'Other' }, { configHash: 'wrong' }, { expiresAt: new Date(now - 1).toISOString() }, { separateBuildAccount: false }]) {
    atomic(f.config.worker.approvalFile, { ...approval, ...mutation }); assert.throws(() => approve(f.config, now, identity));
  }
});
test('Config/path scope and agent tools cannot grant shell, release or credential access', t => {
  const f = fixture(t); validateWorker(f.config);
  const args = agentArguments(f.config.worker); assert.ok(args.includes('--restricted')); assert.ok(args.includes('dontAsk'));
  assert.ok(!args.join(' ').includes('Bash')); assert.ok(!args.join(' ').includes('bypassPermissions'));
  for (const file of ['VERSION', '.github/workflows/a.yml', 'tools/matrix/worker.mjs', 'eq/prompts/41-x.md', 'eq/Source/../escape.cpp', 'eq/Source/a.ps1', 'eq/Source/a\n.cpp']) assert.throws(() => validateChanges([file]));
  assert.throws(() => validateWorker({ ...f.config, worker: { ...f.config.worker, worktreeRoot: f.repo } }));
  assert.throws(() => parseAgent('{"type":"result","subtype":"error"}'));
  assert.equal(limitedEnvironment({ PATH: 'path', GH_TOKEN: 'secret', ANTHROPIC_API_KEY: 'secret' }).GH_TOKEN, undefined);
});
test('Async lock remains held across awaits and releases after failure', async t => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'matrix-async-lock-')); t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  await lockedAsync(dir, async () => { await Promise.resolve(); await assert.rejects(lockedAsync(dir, async () => {})); });
  await assert.rejects(lockedAsync(dir, async () => { throw Error('interrupted'); }));
  assert.equal(fs.existsSync(path.join(dir, 'worker.lock')), false);
});
test('Real child process timeout, output bound, cancellation and missing executable', async () => {
  await assert.rejects(runProcess(process.execPath, ['-e', 'setInterval(()=>{},1000)'], { timeoutMs: 200 }), /timed out/);
  await assert.rejects(runProcess(process.execPath, ['-e', 'process.stdout.write("x".repeat(10000))'], { maximumBytes: 10 }), /Output limit/);
  await assert.rejects(runProcess(process.execPath, ['-e', 'setInterval(()=>{},1000)'], { cancelled: () => true }), /paused/);
  await assert.rejects(runProcess(path.resolve('missing-matrix-executable'), []), /could not start/);
});

test('Scheduled build cancellation signals its controller before process termination', async t => {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'matrix-cancel-'));
  t.after(() => fs.rmSync(root, { recursive: true, force: true }));
  const marker = path.join(root, 'CANCEL_BUILD');
  const ready = path.join(root, 'ready');
  const code = 'const fs=require("node:fs");fs.writeFileSync(process.argv[2],"ready");setInterval(()=>{if(fs.existsSync(process.argv[1]))process.exit(0)},10);';
  await assert.rejects(runProcess(process.execPath, ['-e', code, marker, ready], {
    cancelled: () => fs.existsSync(ready), cancellationFile: marker, cancellationGraceMs: 3000
  }), /paused/);
  assert.match(fs.readFileSync(marker, 'utf8'), /Controller requested cancellation/);
});
