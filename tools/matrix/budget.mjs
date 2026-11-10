// Run budget (AUTO-12 first slice): total elapsed time for one run, counted from the first claim and kept in the saved state,
// so a restart cannot reset it. Only an explicit resume starts a new budget. CI and publication waiting count.
export const DEFAULT_RUN_MINUTES = 120;

export const runMinutes = worker => worker?.maximumRunMinutes ?? DEFAULT_RUN_MINUTES;

/** Returns the reason the run must stop, or null while there is budget left. */
export function budgetStop(task, worker, nowMs) {
  const started = Date.parse(task?.runStartedAt);
  if (!Number.isFinite(started)) return null;
  const limit = runMinutes(worker);
  return nowMs - started >= limit * 60000 ? `Run time limit of ${limit} minutes reached (counted from the first claim, including CI waiting)` : null;
}
