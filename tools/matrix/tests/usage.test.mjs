import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { parseUsage, checkClaudeUsage, validateUsageConfig, USAGE_ARGS } from '../usage.mjs';
import { tick, readConfig } from '../matrix.mjs';
const now = Date.parse('2026-11-06T20:00:00Z');
const reset = new Date(now + 3600000).toISOString();
const executable = path.resolve('trusted', process.platform === 'win32' ? 'claude.exe' : 'claude');
const enabled = { enabled: true, executable };
function rows(percent = 84.9) { return [
  { kind: 'session', group: 'session', percent: 0, resets_at: null },
  { kind: 'weekly_all', group: 'weekly', percent, resets_at: reset, scope: null }
]; }
function events(percent = 84.9) { return [
  { type: 'system', subtype: 'init' },
  { type: 'assistant', message: { content: [{ type: 'text', text: 'Current week (all models)' }] }, usage_report: {
    session: { total_cost_usd: 0, total_api_duration_ms: 0, model_usage: {} },
    rate_limits: { limits: rows(percent), extra_usage: { is_enabled: false } }
  } },
  { type: 'result', subtype: 'success', is_error: false, num_turns: 0, total_cost_usd: 0 }
]; }
const stream = items => items.map(item => JSON.stringify(item)).join('\n') + '\n';
const parse = items => parseUsage(stream(items), { startedAt: now, now });
function fakeRun(options = {}) {
  const calls = [];
  const run = (exe, args, invocation) => {
    calls.push({ exe, args, invocation });
    if (options.throw) throw new Error('provider output with secret');
    if (options.failure) return { status: 1, stdout: '', stderr: 'private token data' };
    if (options.timeout) return { status: null, error: { code: 'ETIMEDOUT' }, stdout: '', stderr: '' };
    const text = args[0] === '--version' ? options.version ?? '2.1.319 (Claude Code)\n'
      : args[0] === 'auth' ? JSON.stringify(options.auth ?? { loggedIn: true, authMethod: 'claude.ai', apiProvider: 'firstParty' })
      : options.output ?? stream(events());
    return { status: 0, stdout: text, stderr: options.stderr ?? '' };
  };
  return { run, calls };
}
for (const [percent, allowed] of [[0, true], [84.9, true], [85, false], [85.1, false], [96, false], [100, false]]) test(`Structured weekly percentage ${percent}`, () => {
  const result = parse(events(percent)); assert.equal(result.allowed, allowed); assert.equal(result.snapshot.percentage, percent);
});
test('Extra usage enabled, absent, null and malformed all block below threshold', () => {
  for (const extra of [undefined, null, {}, { is_enabled: true }, { is_enabled: 'false' }]) {
    const e = events(); e[1].usage_report.rate_limits.extra_usage = extra;
    const result = parse(e); assert.equal(result.allowed, false); assert.equal(result.snapshot.percentage, 84.9);
  }
});
test('Weekly and extra-usage reasons are reported together', () => {
  const e = events(96); e[1].usage_report.rate_limits.extra_usage.is_enabled = true;
  const result = parse(e); assert.equal(result.reasons.length, 2); assert.equal(result.extraUsageEnabled, true);
});
test('Missing, null, empty and duplicate weekly meters fail closed', () => {
  for (const limits of [undefined, null, [], rows().slice(0, 1), [...rows(), rows()[1]]]) {
    const e = events(); e[1].usage_report.rate_limits.limits = limits; assert.throws(() => parse(e));
  }
  const e = events(); e[1].usage_report.rate_limits = null; assert.throws(() => parse(e));
});
test('Malformed percentages, unknown meter and invalid weekly identity fail closed', () => {
  for (const change of [{ percent: '84' }, { percent: null }, { percent: -1 }, { percent: 101 }, { percent: NaN }, { group: 'session' }, { scope: { model: { display_name: 'Sonnet' } } }, { kind: 'new_unknown_meter' }]) {
    const e = events(); Object.assign(e[1].usage_report.rate_limits.limits[1], change); assert.throws(() => parse(e));
  }
});
test('Weekly reset must be valid, future and within eight days', () => {
  for (const resets_at of [null, 'invalid', new Date(now).toISOString(), new Date(now + 9 * 86400000).toISOString()]) {
    const e = events(); e[1].usage_report.rate_limits.limits[1].resets_at = resets_at; assert.throws(() => parse(e));
  }
});
test('Cache/fetch failure notices invalidate otherwise plausible structured data', () => {
  for (const text of ['Showing last-known usage', 'Showing cached usage', 'stale data', 'usage endpoint is rate limited', 'Unable to retrieve usage', 'Could not fetch plan limits', 'Press r to retry']) {
    const e = events(); e[1].message.content[0].text = text; assert.throws(() => parse(e));
  }
  const e = events(); e[1].usage_report.cached = true; assert.throws(() => parse(e));
});
test('No model calls permitted and successful terminal result is mandatory', () => {
  for (const change of [{ num_turns: 1 }, { total_cost_usd: 0.001 }, { subtype: 'error' }, { is_error: true }]) {
    const e = events(); Object.assign(e[2], change); assert.throws(() => parse(e));
  }
  const e = events(); e[1].usage_report.session.model_usage = { sonnet: {} }; assert.throws(() => parse(e));
  assert.throws(() => parse(events().slice(0, 2)));
  assert.throws(() => parse([...events(), events()[2]]));
});
test('Ambiguous/invalid streams and tool activity fail closed', () => {
  assert.throws(() => parseUsage('not json', { startedAt: now, now }));
  assert.throws(() => parseUsage('null\n', { startedAt: now, now }));
  assert.throws(() => parseUsage('x'.repeat(1024 * 1024 + 1), { startedAt: now, now }));
  assert.throws(() => parse([events()[0], events()[1], events()[1], events()[2]]));
  const e = events(); e[1].message.content.push({ type: 'tool_use', name: 'Bash' }); assert.throws(() => parse(e));
});
test('Fresh request does not become reusable evidence after timeout or clock reversal', () => {
  for (const startedAt of [now + 1, now - 60001, NaN]) assert.throws(() => parseUsage(stream(events()), { startedAt, now }));
});
test('Scoped weekly exhaustion and session exhaustion also stop work', () => {
  const e = events(); e[1].usage_report.rate_limits.limits.push({ kind: 'weekly_scoped', group: 'weekly', percent: 85, resets_at: reset });
  assert.equal(parse(e).allowed, false);
  const s = events(); s[1].usage_report.rate_limits.limits[0].percent = 100; assert.equal(parse(s).allowed, false);
});
test('Adapter uses absolute executable, empty temporary cwd, bounded fixed commands and cleans up', () => {
  const fake = fakeRun();
  const result = checkClaudeUsage(enabled, { run: fake.run, environment: {}, clock: () => now });
  assert.equal(result.allowed, true); assert.equal(result.cliVersion, '2.1.319');
  assert.deepEqual(fake.calls.map(c => c.args), [['--version'], ['auth', 'status', '--json'], [...USAGE_ARGS]]);
  for (const c of fake.calls) { assert.equal(c.exe, executable); assert.equal(c.invocation.shell, false); assert.ok(c.invocation.timeout <= 30000); assert.equal(fs.existsSync(c.invocation.cwd), false); }
});
test('Missing configuration never invokes Claude; invalid paths/wrappers are rejected', () => {
  for (const config of [undefined, { enabled: false }, { enabled: true, executable: 'claude' }, { enabled: true, executable: path.resolve('claude.cmd') }, { enabled: 'true', executable }]) {
    const fake = fakeRun(); assert.equal(checkClaudeUsage(config, { run: fake.run }).allowed, false); assert.equal(fake.calls.length, 0);
  }
  assert.throws(() => validateUsageConfig({ ...enabled, arbitraryArgs: ['/do-work'] }));
});
test('Alternative credentials/provider overrides are rejected without leaking values', () => {
  for (const name of ['ANTHROPIC_API_KEY', 'ANTHROPIC_AUTH_TOKEN', 'ANTHROPIC_BASE_URL', 'CLAUDE_CODE_USE_BEDROCK', 'CLAUDE_CONFIG_DIR']) {
    const fake = fakeRun(), result = checkClaudeUsage(enabled, { run: fake.run, environment: { [name]: 'secret-value' } });
    assert.equal(result.allowed, false); assert.equal(fake.calls.length, 0); assert.equal(JSON.stringify(result).includes('secret-value'), false);
  }
});
test('Subscription login is verified before issuing /usage', () => {
  for (const auth of [{}, { loggedIn: false }, { loggedIn: true, authMethod: 'api_key', apiProvider: 'firstParty' }, { loggedIn: true, authMethod: 'claude.ai', apiProvider: 'thirdParty' }]) {
    const fake = fakeRun({ auth }); const result = checkClaudeUsage(enabled, { run: fake.run, environment: {} });
    assert.equal(result.allowed, false); assert.equal(fake.calls.length, 2);
  }
});
test('Unsupported CLI versions stop before any account or usage query', () => {
  for (const version of ['2.1.318 (Claude Code)', '2.2.0 (Claude Code)', 'unknown']) {
    const fake = fakeRun({ version }); assert.equal(checkClaudeUsage(enabled, { run: fake.run, environment: {} }).allowed, false); assert.equal(fake.calls.length, 1);
  }
});
test('Timeouts, failed exits and stderr return redacted unknown usage', () => {
  for (const options of [{ timeout: true }, { failure: true }, { stderr: 'private token data' }, { throw: true }]) {
    const fake = fakeRun(options); const result = checkClaudeUsage(enabled, { run: fake.run, environment: {} });
    assert.equal(result.allowed, false); assert.equal(result.snapshot, null); assert.equal(JSON.stringify(result).includes('private token data'), false);
    assert.equal(JSON.stringify(result).includes('secret'), false);
  }
});
test('Dry-run reports show live usage and blockers without claiming work', t => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'matrix-usage-report-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  const config = { repository: dir, stateDirectory: path.join(dir, 'state'), enabled: true, batchLimit: 6, dailyTime: '02:00', liveExecution: false, autoMerge: false, releasePublication: false, claudeUsage: enabled };
  for (const percent of [84.9, 96]) {
    const report = tick(config, { loadSnapshot: () => ({ revision: 'a'.repeat(40), files: new Map() }), loadUsage: () => parse(events(percent)) });
    assert.equal(report.usage.allowed, percent < 85); assert.equal(report.usage.snapshot.percentage, percent); assert.equal(report.liveExecution, false);
    assert.equal(report.blockers.length, percent < 85 ? 1 : 2);
  }
  const file = path.join(dir, 'config.json'); fs.writeFileSync(file, JSON.stringify(config)); assert.equal(readConfig(file).claudeUsage.enabled, true);
  config.claudeUsage = { ...enabled, executable: 'relative' }; fs.writeFileSync(file, JSON.stringify(config)); assert.throws(() => readConfig(file));
});
test('Paused and idle workers never query account usage', t => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'matrix-usage-pause-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true }));
  const config = { repository: dir, stateDirectory: path.join(dir, 'state'), enabled: false, claudeUsage: enabled };
  assert.equal(tick(config, { loadUsage: () => { throw Error('must not invoke'); } }).paused, true);
});
