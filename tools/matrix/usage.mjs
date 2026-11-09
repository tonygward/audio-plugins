import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { TELEMETRY_SOURCE, WEEKLY_LIMIT_PERCENT, usageGate } from './policy.mjs';

export const USAGE_ARGS = Object.freeze(['-p', '/usage', '--safe-mode', '--tools', '', '--max-turns', '1',
  '--output-format', 'stream-json', '--verbose', '--no-session-persistence', '--strict-mcp-config',
  '--mcp-config', '{"mcpServers":{}}', '--setting-sources', '']);
class UsageError extends Error {}
const unknown = reason => ({ allowed: false, reason, reasons: [reason], snapshot: null });

export function validateUsageConfig(config) {
  if (config === undefined) return;
  if (!config || typeof config !== 'object' || typeof config.enabled !== 'boolean' || Object.keys(config).some(k => !['enabled', 'executable'].includes(k))) throw new UsageError('Invalid claudeUsage configuration');
  if (config.enabled && (typeof config.executable !== 'string' || !path.isAbsolute(config.executable) || /\.(?:cmd|bat|ps1)$/i.test(config.executable))) throw new UsageError('claudeUsage requires an absolute native Claude executable path');
}

function finitePercent(n) { return typeof n === 'number' && Number.isFinite(n) && n >= 0 && n <= 100; }

export function parseUsage(stdout, { startedAt, now = Date.now() }) {
  if (typeof stdout !== 'string' || !stdout.trim() || Buffer.byteLength(stdout) > 1024 * 1024) throw new UsageError('Missing or oversized usage response');
  let messages;
  try { messages = stdout.trim().split(/\r?\n/).filter(line => line.trim()).map(line => JSON.parse(line)); }
  catch { throw new UsageError('Malformed usage event stream'); }
  if (messages.some(m => !m || typeof m !== 'object' || Array.isArray(m))) throw new UsageError('Invalid usage event');
  if (messages.some(m => !['system', 'assistant', 'result'].includes(m.type)) || messages.filter(m => m.type === 'system').some(m => m.subtype !== 'init')) throw new UsageError('Unsupported usage event');
  const results = messages.filter(m => m.type === 'result');
  if (results.length !== 1 || messages.at(-1) !== results[0] || results[0].subtype !== 'success' || results[0].is_error !== false || results[0].num_turns !== 0 || results[0].total_cost_usd !== 0) throw new UsageError('Usage command failed or performed model work');
  if (messages.some(m => m.is_error || m.error || ['tool_use', 'thinking'].some(type => m.message?.content?.some(c => c.type === type)))) throw new UsageError('Unexpected error or model activity');
  // /usage can return cached rows on network failure. Never treat local receipt time
  // as proof that these rows were refreshed. Reject documented fallback notices.
  const notices = messages.flatMap(m => [m.result ?? '', ...(m.message?.content ?? []).filter(c => c.type === 'text').map(c => c.text)]).join('\n');
  if (/last[-\s]known|\bcached\b|\bstale\b|rate[-\s]?limited|could(?:n.t| not) (?:fetch|get|retrieve)|(?:failed|unable) to (?:fetch|get|retrieve)|\bretry\b|\bunavailable\b/i.test(notices)
      || messages.some(m => m.cached === true || m.is_cached === true || m.usage_report?.cached === true || m.usage_report?.is_cached === true)) throw new UsageError('Usage is cached or retrieval failed');
  const reports = messages.filter(m => m.type === 'assistant' && m.usage_report);
  if (reports.length !== 1 || messages.filter(m => m.type === 'assistant').length !== 1) throw new UsageError('Missing or ambiguous structured usage report');
  const report = reports[0].usage_report;
  if (!report.session || report.session.total_cost_usd !== 0 || report.session.total_api_duration_ms !== 0 || !report.session.model_usage || Object.keys(report.session.model_usage).length) throw new UsageError('Usage report indicates model work');
  const limits = report.rate_limits?.limits;
  if (!Array.isArray(limits) || limits.length === 0 || limits.some(row => !row || typeof row !== 'object' || typeof row.kind !== 'string' || !finitePercent(row.percent))) throw new UsageError('Missing or invalid plan limits');
  const weekly = limits.filter(row => row.kind === 'weekly_all');
  if (weekly.length !== 1 || weekly[0].group !== 'weekly' || weekly[0].scope != null) throw new UsageError('Missing or contradictory all-model weekly meter');
  if (!Number.isFinite(startedAt) || now < startedAt || now - startedAt > 60000) throw new UsageError('Usage request is stale or clock changed');
  // Keep all reported weekly scoped/session meters conservative. Unknown kinds block
  // until reviewed instead of assuming that a new provider limit cannot matter.
  if (limits.some(row => !['session', 'weekly_all', 'weekly_scoped'].includes(row.kind))) throw new UsageError('Unsupported usage meter');
  for (const row of limits.filter(row => row.kind.startsWith('weekly'))) {
    const reset = Date.parse(row.resets_at);
    if (!Number.isFinite(reset) || reset <= now || reset > now + 8 * 86400000) throw new UsageError('Expired or invalid weekly reset');
  }
  const snapshot = { source: TELEMETRY_SOURCE, percentage: weekly[0].percent, observedAt: new Date(startedAt).toISOString(), resetsAt: weekly[0].resets_at };
  const gate = usageGate(snapshot, now), reasons = gate.allowed ? [] : [gate.reason];
  if (limits.some(row => row.kind === 'weekly_scoped' && row.percent >= WEEKLY_LIMIT_PERCENT)) reasons.push(`Scoped weekly usage at or above ${WEEKLY_LIMIT_PERCENT}%`);
  if (limits.some(row => row.kind === 'session' && row.percent >= 100)) reasons.push('Session allowance exhausted');
  const extra = report.rate_limits.extra_usage;
  // Missing/null is unknown, not proof that paid continuation is disabled.
  if (!extra || extra.is_enabled !== false) reasons.push(extra?.is_enabled === true ? 'Paid extra usage is enabled' : 'Paid extra-usage status is unknown');
  return { ...gate, allowed: reasons.length === 0, reason: reasons.join('; ') || gate.reason, reasons,
    extraUsageEnabled: typeof extra?.is_enabled === 'boolean' ? extra.is_enabled : null,
    meters: limits.map(row => ({ kind: row.kind, percentage: row.percent, resetsAt: row.resets_at ?? null })) };
}

export function checkClaudeUsage(config, { run = spawnSync, environment = process.env, clock = Date.now } = {}) {
  try { validateUsageConfig(config); } catch { return unknown('Invalid Claude usage configuration'); }
  if (!config?.enabled) return unknown('Claude usage check is disabled or not configured');
  // Do not read or export token values. Refuse alternate provider/auth overrides.
  if (Object.entries(environment).some(([key, value]) => value && /^(ANTHROPIC_|CLAUDE_CODE_USE_|CLAUDE_CODE_OAUTH_TOKEN|CLAUDE_CONFIG_DIR)/i.test(key))) return unknown('Alternate Claude authentication/provider environment is configured');
  let directory;
  try {
    directory = fs.mkdtempSync(path.join(os.tmpdir(), 'matrix-usage-'));
    function invoke(args, timeout) {
      const result = run(config.executable, args, { cwd: directory, env: { ...environment, NO_COLOR: '1' }, encoding: 'utf8', timeout,
        killSignal: 'SIGKILL', maxBuffer: 1024 * 1024, windowsHide: true, shell: false, stdio: ['ignore', 'pipe', 'pipe'] });
      // No stderr/raw provider output is persisted; it may contain account information.
      if (result.error || result.signal || result.status !== 0 || result.stderr?.trim()) throw new UsageError('Claude usage process failed, timed out or reported diagnostics');
      return result.stdout;
    }
    const version = /^2\.1\.(\d+)\s+\(Claude Code\)\s*$/.exec(invoke(['--version'], 5000));
    if (!version || Number(version[1]) < 319) throw new UsageError('Unsupported Claude Code version; requires verified 2.1.319 or later 2.1.x');
    let auth;
    try { auth = JSON.parse(invoke(['auth', 'status', '--json'], 15000)); } catch { throw new UsageError('Claude subscription authentication could not be verified'); }
    if (auth.loggedIn !== true || auth.authMethod !== 'claude.ai' || auth.apiProvider !== 'firstParty') throw new UsageError('First-party Claude subscription login required');
    const startedAt = clock();
    const result = parseUsage(invoke([...USAGE_ARGS], 30000), { startedAt, now: clock() });
    return { ...result, cliVersion: `2.1.${version[1]}` };
  } catch (error) {
    // All thrown messages are fixed adapter diagnostics, never subprocess stderr.
    return unknown(error instanceof UsageError ? error.message : "Claude usage check failed");
  } finally {
    // Only remove the fresh, owned directory beneath the resolved OS temp root.
    if (directory && path.dirname(path.resolve(directory)) === path.resolve(os.tmpdir()) && path.basename(directory).startsWith('matrix-usage-')) fs.rmSync(directory, { recursive: true, force: true });
  }
}
