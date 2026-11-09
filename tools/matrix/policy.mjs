export const WEEKLY_LIMIT_PERCENT = 70;
export const TELEMETRY_SOURCE = 'claude-code-usage-report-weekly-all';

export function usageGate(snapshot, now = Date.now()) {
  if (!snapshot || snapshot.source !== TELEMETRY_SOURCE) return { allowed: false, reason: 'No verified weekly subscription telemetry', snapshot: null };
  const { percentage, observedAt, resetsAt } = snapshot;
  const age = now - Date.parse(observedAt), reset = Date.parse(resetsAt);
  if (typeof percentage !== 'number' || !Number.isFinite(percentage) || percentage < 0 || percentage > 100 || !Number.isFinite(age) || age < 0 || age > 60000 || !Number.isFinite(reset) || reset <= now || reset > now + 8 * 86400000) return { allowed: false, reason: 'Malformed, stale or contradictory weekly telemetry', snapshot: null };
  return { allowed: percentage < WEEKLY_LIMIT_PERCENT, reason: percentage < WEEKLY_LIMIT_PERCENT ? `Weekly usage below ${WEEKLY_LIMIT_PERCENT}%` : `Weekly usage at or above ${WEEKLY_LIMIT_PERCENT}%`, snapshot };
}

export function londonDay(now = new Date()) {
  return new Intl.DateTimeFormat('en-CA', { timeZone: 'Europe/London', year: 'numeric', month: '2-digit', day: '2-digit' }).format(now);
}

export function dailyDue(lastDay, time = '02:00', now = new Date()) {
  if (!/^([01]\d|2[0-3]):[0-5]\d$/.test(time)) throw new Error('Invalid dailyTime');
  const clock = new Intl.DateTimeFormat('en-GB', { timeZone: 'Europe/London', hour: '2-digit', minute: '2-digit', hourCycle: 'h23' }).format(now);
  return lastDay !== londonDay(now) && clock >= time;
}

// Preview only. Live invocation deliberately cannot be enabled by a repository edit.
export function claudePlan(prompt) {
  if (prompt.model !== 'sonnet') throw new Error('Explicit owner review required for model');
  return { executable: 'claude', args: ['-p', '--model', 'sonnet', '--max-turns', '30', '--output-format', 'json'], timeoutSeconds: 1800,
    blocker: 'Live adapter not activated: preflight telemetry refresh, isolated account/network, subscription-only billing and owner approval are not verified' };
}

export function releaseAssets(version, names) {
  if (!/^\d+\.\d+\.\d+$/.test(version)) throw new Error('Invalid version');
  const expected = [`Songsmith-Plugins-${version}-win64-setup.exe`, `Songsmith-Plugins-${version}-macOS.pkg`];
  if (names.length !== 2 || expected.some(n => !names.includes(n))) throw new Error('Release requires exactly the two versioned suite installers');
  return expected;
}
