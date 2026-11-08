import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
import { digest, scan, select } from './backlog.mjs';
import { atomic, lockedAsync, readState, reconcile, claim, finish, record } from './state.mjs';
import { checkClaudeUsage } from './usage.mjs';
import { usageGate, dailyDue, londonDay } from './policy.mjs';
import { buildPlan, worktreePlan } from './validation.mjs';
import { runProcess, limitedEnvironment } from './process.mjs';

const plugins = ['eq', 'analog-compressor', 'maximiser'];
const within = (parent, child) => { const relative = path.relative(path.resolve(parent), path.resolve(child)); return relative !== '' && !relative.startsWith('..') && !path.isAbsolute(relative); };
const native = p => typeof p === 'string' && path.isAbsolute(p) && !/\.(cmd|bat|ps1)$/i.test(p);
export function validateWorker(config) {
  const w = config.worker;
  if (!w || w.enabled === false) return;
  if (w.buildMode !== undefined && w.buildMode !== 'scheduled') throw Error('Unknown build mode');
  if (w.enabled !== true || config.autoMerge !== false || config.releasePublication !== false || !config.claudeUsage?.enabled) throw Error('Worker requires enabled subscription checks and disabled merge/release');
  for (const key of ['gitExecutable', 'ghExecutable', 'powershellExecutable']) if (!native(w[key])) throw Error(`Absolute native ${key} required`);
  for (const key of ['worktreeRoot', 'approvalFile', 'juceDirectory', 'buildScript', 'buildCredentialFile']) if (typeof w[key] !== 'string' || !path.isAbsolute(w[key])) throw Error(`Absolute ${key} required`);
  if (!/^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/.test(w.repositorySlug)) throw Error('Invalid GitHub repository');
  if (!Number.isInteger(w.maximumPrompts) || w.maximumPrompts < 1 || w.maximumPrompts > 6 || !Number.isInteger(w.maximumTurns) || w.maximumTurns < 1 || w.maximumTurns > 30 || !Number.isFinite(w.maximumBudgetUsd) || w.maximumBudgetUsd <= 0 || w.maximumBudgetUsd > 20) throw Error('Invalid worker budget');
  if (!Number.isInteger(w.timeoutSeconds) || w.timeoutSeconds < 60 || w.timeoutSeconds > 1800) throw Error('Invalid agent timeout');
  for (const target of [w.worktreeRoot, config.stateDirectory]) {
    if (within(config.repository, target) || within(target, config.repository) || path.resolve(target) === path.resolve(config.repository)) throw Error('Worker storage must be separate from source checkout');
  }
  if (within(w.worktreeRoot, config.stateDirectory) || within(config.stateDirectory, w.worktreeRoot) || path.resolve(w.worktreeRoot) === path.resolve(config.stateDirectory)) throw Error('State and worktree roots must be separate');
  for (const target of [w.approvalFile, w.buildScript, w.buildCredentialFile]) if ([config.repository, w.worktreeRoot, config.stateDirectory].some(root => within(root, target))) throw Error('Approval, build launcher and credentials must be outside writable roots');
}

export function approvalDigest(config) {
  return digest(JSON.stringify({ repository: config.repository, stateDirectory: config.stateDirectory, claudeUsage: config.claudeUsage, worker: config.worker }));
}

export function approve(config, now = Date.now(), identity = { user: os.userInfo().username, host: os.hostname() }) {
  if (process.platform !== 'win32') throw Error('Production build launcher requires Windows');
  const file = config.worker.approvalFile;
  if (fs.lstatSync(file).isSymbolicLink()) throw Error('Approval cannot be a link');
  const a = JSON.parse(fs.readFileSync(file, 'utf8'));
  if (a.configHash !== approvalDigest(config) || a.user !== identity.user || a.host !== identity.host || a.authorised !== true || a.isolatedAccount !== true || a.networkRestricted !== true || a.separateBuildAccount !== true || a.noReleaseCredentials !== true) throw Error('Owner deployment approval does not match verified environment');
  const expires = Date.parse(a.expiresAt);
  if (!Number.isFinite(expires) || expires <= now || expires > now + 86400000) throw Error('Approval expired or exceeds 24 hours');
  if (a.buildScriptHash !== digest(fs.readFileSync(config.worker.buildScript))) throw Error('Build launcher changed since approval');
  return a;
}

export function agentArguments(w, turns = w.maximumTurns, budget = w.maximumBudgetUsd) {
  return ['-p', '--model', 'sonnet', '--max-turns', String(turns), '--max-budget-usd', String(budget),
    '--output-format', 'json', '--safe-mode', '--restricted', '--permission-mode', 'acceptEdits',
    '--tools', 'Read,Edit,Write,Glob,Grep', '--allowedTools', 'Read,Edit,Write,Glob,Grep',
    '--strict-mcp-config', '--mcp-config', '{"mcpServers":{}}', '--setting-sources', '', '--no-session-persistence'];
}

export function validateChanges(files) {
  if (!files.length) throw Error('Agent produced no changes');
  for (const file of files) {
    if (!file || /[\\\x00-\x1f]/.test(file) || file.split('/').some(p => p === '..' || p.startsWith('.'))
      || !/^(eq|analog-compressor|maximiser|shared)\/(Source|Tests|Resources|docs)\//.test(file) && !/^(eq|analog-compressor|maximiser)\/README\.md$/.test(file)
      || /\.(exe|dll|cmd|bat|ps1|sh|cmake)$/i.test(file)) throw Error('Change outside permitted source/test/documentation scope');
  }
}

export function parseAgent(text) {
  let result; try { result = JSON.parse(text); } catch { throw Error('Invalid Claude result'); }
  if (result.type !== 'result' || result.subtype !== 'success' || result.is_error !== false || !Number.isInteger(result.num_turns) || result.num_turns < 1 || !Number.isFinite(result.total_cost_usd) || result.total_cost_usd < 0) throw Error('Claude stopped or returned unsuccessful result');
  return { turns: result.num_turns, estimatedCostUsd: result.total_cost_usd };
}

export function verifyGitPointer(file, expectedHash) {
  if (!fs.lstatSync(file).isFile() || digest(fs.readFileSync(file)) !== expectedHash) throw Error('Worktree Git pointer changed');
}

export async function work(config, { trigger = 'daily', run = runProcess, loadSnapshot, loadUsage = checkClaudeUsage,
  loadApproval = approve, clock = Date.now } = {}) {
  if (!config.enabled || !config.worker?.enabled || config.liveExecution !== true) return { liveExecution: false, reason: 'Controlled worker is disabled' };
  validateWorker(config);
  const w = config.worker, dir = config.stateDirectory, paused = () => fs.existsSync(path.join(dir, 'PAUSED'));
  const leaseJournal = path.join(path.dirname(w.buildCredentialFile), 'active-build-lease.json');
  const verifyLeaseClosed = () => { if (fs.existsSync(leaseJournal)) throw Error('Outstanding build ACL lease requires operator recovery'); };
  verifyLeaseClosed();
  return lockedAsync(dir, async () => {
    if (paused()) return { liveExecution: false, reason: 'Worker paused' };
    let approval = loadApproval(config, clock());
    const state = readState(dir), save = () => atomic(path.join(dir, 'state.json'), state);
    const deadline = clock() + 4 * 3600000;
    const cancelled = () => {
      try { approval = loadApproval(config, clock()); }
      catch { return true; }
      return paused() || clock() >= deadline || clock() >= Date.parse(approval.expiresAt);
    };
    const env = limitedEnvironment();
    const gitPointers = new Map();
    const command = async (exe, args, cwd = config.repository, extra = {}) => {
      if (cancelled()) throw Error('Worker paused, deadline reached or approval expired');
      return run(exe, args, { cwd, env, timeoutMs: 60000, cancelled, ...extra });
    };
    const git = async (args, cwd) => {
      if (cwd && gitPointers.has(cwd)) {
        const pointer = path.join(cwd, '.git');
        verifyGitPointer(pointer, gitPointers.get(cwd));
      }
      return (await command(w.gitExecutable, ['-c', `core.hooksPath=${process.platform === 'win32' ? 'NUL' : '/dev/null'}`, '-c', 'commit.gpgSign=false', ...args], cwd)).stdout.trimEnd();
    };
    async function getSnapshot() {
      if (loadSnapshot) return await loadSnapshot(config.repository);
      await git(['fetch', '--no-tags', 'origin', 'main']);
      const revision = await git(['rev-parse', '--verify', 'refs/remotes/origin/main']);
      if (!/^[a-f0-9]{40,64}$/.test(revision)) throw Error('Invalid main revision');
      const files = new Map();
      for (const file of (await git(['ls-tree', '-r', '--name-only', revision])).split('\n')) {
        if (/^(eq|analog-compressor|maximiser|shared)\/prompts\/.+\.md$/.test(file)) files.set(file, await git(['show', `${revision}:${file}`]));
      }
      return { revision, files };
    }
    const log = (id, stage, details = {}) => {
      const entry = { time: new Date(clock()).toISOString(), id, stage, ...details };
      fs.mkdirSync(path.join(dir, 'logs'), { recursive: true });
      fs.appendFileSync(path.join(dir, 'logs', 'worker.jsonl'), JSON.stringify(entry) + '\n'); record(state, entry); save();
    };
    if (trigger === 'timer') {
      const inbox = path.join(dir, 'inbox');
      const events = fs.existsSync(inbox) ? fs.readdirSync(inbox).filter(n => /^[a-f0-9]{40,64}\.json$/.test(n)) : [];
      if (dailyDue(state.lastWorkerDaily, config.dailyTime, new Date(clock()))) trigger = 'daily';
      else if (events.some(e => !state.workerEvents?.includes(e))) trigger = 'notify';
      else return { liveExecution: false, idle: true };
    }
    if (!['daily', 'notify'].includes(trigger)) throw Error('Invalid worker trigger');
    if ((await git(['status', '--porcelain'])).trim()) throw Error('Source checkout must be clean');
    const remote = await git(['remote', 'get-url', 'origin']);
    if (remote !== `https://github.com/${w.repositorySlug}` && remote !== `https://github.com/${w.repositorySlug}.git`) throw Error('Origin does not match approved GitHub repository');
    const initial = await getSnapshot(), backlog = scan(initial.files);
    if (backlog.errors.length) throw Error('Backlog validation failed');
    reconcile(state, backlog.prompts, initial.revision); save();
    const selected = select(backlog.prompts, state.tasks, trigger, Math.min(config.batchLimit, w.maximumPrompts)).selected;
    const outcomes = [];
    for (const p of selected) {
      if (cancelled()) break;
      approval = loadApproval(config, clock());
      const fresh = await getSnapshot();
      if (fresh.revision !== initial.revision) { log(p.id, 'stopped', { reason: 'Main changed; reconcile again' }); break; }
      const usage = loadUsage(config.claudeUsage);
      if (!usage.allowed || !usageGate(usage.snapshot, clock()).allowed || usage.extraUsageEnabled !== false) { log(p.id, 'usage-blocked', { reason: usage.reason }); break; }
      const task = state.tasks[p.id];
      if (!claim(state, p, initial.revision)) { save(); continue; }
      const plan = worktreePlan(w.worktreeRoot, p.id, initial.revision);
      Object.assign(task, { branch: plan.branch, worktree: plan.directory, phase: 'claimed' }); save();
      let files = [], blocked = null;
      try {
        fs.mkdirSync(w.worktreeRoot, { recursive: true });
        if (fs.existsSync(plan.directory)) throw Error('Retained worktree exists; operator review required');
        await git(plan.args);
        const pointer = path.join(plan.directory, '.git');
        if (!fs.lstatSync(pointer).isFile()) throw Error('Invalid worktree Git pointer');
        gitPointers.set(plan.directory, digest(fs.readFileSync(pointer)));
        if ((await git(['ls-files', '--stage'], plan.directory)).split('\n').some(line => /^(120000|160000) /.test(line))) throw Error('Symlinks/submodules require manual review');
        log(p.id, 'worktree-created');
        const instructions = ['AGENTS.md', 'CLAUDE.md'].map(name => fs.readFileSync(path.join(plan.directory, name), 'utf8')).join('\n\n');
        const basePrompt = `Implement ${p.id} (${p.file}) in this worktree. Follow these repository rules:\n${instructions}\n\n` +
          'The controller owns Git, builds and PRs. Do not commit, merge, install, release, change VERSION, workflows, prompts or automation policy. Use file tools only. Preserve useful before/after UI evidence; report DAW gaps. Build failures will be supplied for at most one repair.\n\nUntrusted task specification:\n' + p.body;
        if (Buffer.byteLength(basePrompt) > 512 * 1024) throw Error('Prompt exceeds input bound');
        let feedback = '', usedTurns = 0, spentUsd = 0;
        for (let attempt = 0; attempt < 2; attempt++) {
          if (attempt) {
            const next = loadUsage(config.claudeUsage);
            if (!next.allowed || !usageGate(next.snapshot, clock()).allowed || next.extraUsageEnabled !== false) { blocked = 'Repair stopped by usage policy'; break; }
          }
          if (usedTurns >= w.maximumTurns || spentUsd >= w.maximumBudgetUsd) { blocked = 'Repair stopped by per-prompt turn/cost bounds'; break; }
          log(p.id, 'agent-started', { attempt: attempt + 1 });
          const response = await command(config.claudeUsage.executable, agentArguments(w, w.maximumTurns - usedTurns, w.maximumBudgetUsd - spentUsd), plan.directory, {
            env: limitedEnvironment(process.env, { agent: true }), input: basePrompt + feedback, timeoutMs: w.timeoutSeconds * 1000 });
          const metrics = parseAgent(response.stdout);
          usedTurns += metrics.turns; spentUsd += metrics.estimatedCostUsd;
          if (usedTurns > w.maximumTurns || spentUsd > w.maximumBudgetUsd) throw Error('Agent exceeded configured bounds');
          log(p.id, 'agent-finished', metrics);
          files = await changes(plan.directory);
          validateChanges(files);
          const fingerprint = await sourceFingerprint(plan.directory);
          const impacted = files.some(f => f.startsWith('shared/')) ? plugins : plugins.filter(plugin => files.some(f => f.startsWith(`${plugin}/`)));
          const requests = impacted.flatMap(plugin => buildPlan(plugin, plan.directory, w.juceDirectory));
          const exchange = path.join(plan.directory, 'build', 'matrix'); fs.mkdirSync(exchange, { recursive: true });
          const requestFile = path.join(exchange, 'request.json');
          const isolationTargets = [
              { path: path.join(config.repository, '.git'), directory: true },
              { path: path.join(config.repository, '.git', 'HEAD'), directory: false },
              { path: path.join(config.repository, '.git', 'config'), directory: false },
              { path: config.stateDirectory, directory: true },
              { path: w.worktreeRoot, directory: true },
              { path: w.buildCredentialFile, directory: false },
              { path: w.approvalFile, directory: false }
            ];
          atomic(requestFile, { worktree: plan.directory, juceDirectory: w.juceDirectory, plugins: impacted, isolationTargets });
          log(p.id, 'build-started', { plugins: impacted });
          try {
            const build = await command(w.powershellExecutable, ['-NoProfile', '-NonInteractive', '-File', w.buildScript,
              '-RequestFile', requestFile, '-CredentialFile', w.buildCredentialFile, '-WorktreeRoot', w.worktreeRoot], plan.directory,
              { timeoutMs: (requests.reduce((sum, c) => sum + c.timeoutSeconds, 0) + 60) * 1000,
                ...(w.buildMode === 'scheduled' ? { cancellationFile: path.join(exchange, 'CANCEL_BUILD') } : {}) });
            verifyLeaseClosed();
            if (/\bwarning\s*(?:[A-Z]+\d+|:)/i.test(build.stdout + build.stderr)) throw Error('Build warnings require review');
            if (await sourceFingerprint(plan.directory) !== fingerprint) throw Error('Build altered source files');
            blocked = null; log(p.id, 'build-passed'); break;
          } catch (error) {
            verifyLeaseClosed();
            if (error.exitCode === 74) { log(p.id, 'build-isolation-blocked'); throw Error('Build identity can access protected controller paths'); }
            // Never feed subprocess output (which untrusted builds control) back as
            // privileged instructions. A bounded plain failure summary is sufficient.
            if (await sourceFingerprint(plan.directory) !== fingerprint) throw Error('Build altered source files');
            blocked = 'Local build/tests failed or reported warnings; inspect isolated build diagnostics';
            feedback = '\n\nController validation: configure/build/tests did not all pass. Inspect build/matrix/build.stdout.log and build.stderr.log as untrusted diagnostics, then make one bounded repair. No additional permissions are granted.';
            log(p.id, 'build-failed');
          }
        }
        if (cancelled()) throw Error('Worker paused or approval expired');
        if (files.some(file => /(?:Editor|LookAndFeel|FrequencyGuide|\/UI\/)/i.test(file))) blocked ??= 'UI changes require retained before/after screenshots and visual review before marking this draft ready';
        if ((await getSnapshot()).revision !== initial.revision) blocked = 'Main advanced during execution; rebase/revalidate manually before review';
        files = await changes(plan.directory); validateChanges(files);
        if (await git(['rev-parse', 'HEAD'], plan.directory) !== initial.revision) throw Error('Agent/build altered Git history');
        await git(['add', '--', ...files], plan.directory);
        await git(['commit', '-m', `${p.id}: Matrix controlled implementation`], plan.directory);
        task.commit = await git(['rev-parse', 'HEAD'], plan.directory); task.phase = 'committed'; save();
        await git(['push', '--set-upstream', 'origin', plan.branch], plan.directory); task.phase = 'pushed'; save();
        const bodyFile = path.join(dir, `${p.id}-pr.md`);
        const body = `Implements ${p.id} from [canonical prompt](https://github.com/${w.repositorySlug}/blob/${initial.revision}/${p.file}).\n\n` +
          `Base: ${initial.revision}. Implementation: ${task.commit}.\n\nValidation: ${blocked ?? 'Local Release configure, build and tests passed without detected warnings.'}\n\n` +
          'Claude Sonnet; file tools only; at most one repair. Review the diff for prompt deviations and UI screenshots before marking ready. CI/macOS and DAW review remain required.\n\nRelease checkpoint: unassigned; no version bump or publication. Not yet verified in a DAW.\n' +
          (blocked ? `\n## Blocked\n\n${blocked}\n` : '');
        fs.writeFileSync(bodyFile, body);
        // Reconcile PR side effects by exact head branch before attempting creation.
        const existing = JSON.parse((await command(w.ghExecutable, ['pr', 'list', '--repo', w.repositorySlug, '--head', plan.branch, '--state', 'all', '--json', 'url,headRefOid,state'], config.repository)).stdout);
        if (!Array.isArray(existing) || existing.length > 1 || existing.some(pr => pr.headRefOid !== task.commit || pr.state !== 'OPEN')) throw Error('Existing PR requires operator review');
        const url = existing[0]?.url ?? (await command(w.ghExecutable, ['pr', 'create', '--repo', w.repositorySlug, '--base', 'main', '--head', plan.branch, '--draft', '--title', `${p.id}: Matrix implementation`, '--body-file', bodyFile], config.repository)).stdout.trim();
        if (!new RegExp(`^https://github\\.com/${w.repositorySlug.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}/pull/\\d+$`).test(url)) throw Error('Unverified draft PR response; inspect retained branch');
        finish(state, p.id, 'awaiting-review', { pr: url, reason: blocked, phase: 'draft-pr' }); log(p.id, 'draft-pr-created', { pr: url, blocked: !!blocked });
        outcomes.push({ id: p.id, pr: url, blocked: !!blocked });
      } catch (error) {
        finish(state, p.id, 'interrupted', { phase: 'interrupted', reason: 'Execution interrupted; inspect retained worktree, command logs and remote branch/PR before retry' });
        // Local diagnostic only: a short error message so an interruption can be explained.
        log(p.id, 'interrupted', { error: String(error?.message ?? error).slice(0, 300) }); outcomes.push({ id: p.id, interrupted: true }); break;
      }
    }
    if (trigger === 'daily') state.lastWorkerDaily = londonDay(new Date(clock()));
    const inbox = path.join(dir, 'inbox');
    if (fs.existsSync(inbox)) state.workerEvents = fs.readdirSync(inbox).filter(n => /^[a-f0-9]{40,64}\.json$/.test(n));
    save();
    const report = { time: new Date(clock()).toISOString(), liveExecution: true, outcomes, merged: false, released: false };
    atomic(path.join(dir, 'latest-worker-report.json'), report);
    return report;

    async function changes(cwd) {
      const staged = await git(['diff', '--name-only', '-z', 'HEAD'], cwd);
      const untracked = await git(['ls-files', '--others', '--exclude-standard', '-z'], cwd);
      return [...new Set((staged + '\0' + untracked).split('\0').filter(Boolean))];
    }
    async function sourceFingerprint(cwd) {
      const files = await changes(cwd); validateChanges(files);
      return digest(JSON.stringify(files.sort().map(file => {
        const target = path.resolve(cwd, file);
        if (!within(cwd, target) || fs.existsSync(target) && !fs.lstatSync(target).isFile()) throw Error('Invalid changed file');
        return [file, fs.existsSync(target) ? digest(fs.readFileSync(target)) : 'deleted'];
      })));
    }
  });
}
