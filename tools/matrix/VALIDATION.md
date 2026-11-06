# AUTO-01 validation evidence

Local validation on the Windows development host, 6 November 2026:

| Check | Result |
|---|---|
| `node --test tools/matrix/tests/*.test.mjs` | 49 passed, 0 failed |
| PowerShell AST parse of both setup/notification scripts | No syntax errors |
| Real tracked-HEAD dry run | No active metadata errors; no queue-state commit |
| Real fetched-main daily reconciliation | Successful; revision `2f10cfbecfec6d501596e265cf4d3bf23f06e4c0` |
| Daily candidates | ACOMP-06, EQ-38, MAX-06 |
| Weekly usage | Unknown; fail closed; no Claude invocation |
| Historical archive warning | `shared/prompts/done/S7-one-version-number.md` lacks a stable canonical ID; SUI-07 remains canonical |
| `git diff --check` | Passed |

Tests include legacy/default and all explicit execution modes, malformed metadata,
deferred AUTO-02, dependency/claim/conflict gates, duplicate/offline/busy signals,
missed-event daily recovery, real cross-process exclusion, interruption/dead-letter
states, corrupt state, usage boundaries/staleness/reset checks, London DST, pause,
fetch failure, local/canonical separation, timer idempotence, real backlog parsing,
exact-head protected merge validation, two-asset release approval checks and verified
R2/publication requirements before producing a website-event plan.

The real report/state/log files are in ignored `build/matrix-validation/`; they
contain no credentials and are not installed operational configuration.

Not verified: Linux/macOS CI (new workflow will run on the PR), actual Windows task
registration/reboot, restricted notification runner/ACLs, Matrix hardware, subscription
authentication/fresh pre-start telemetry, isolated Claude execution, builds/DAW checks,
authenticated PR/merge adapters, live release/R2 upload or website receiver. The tests
use synthetic evidence for those policy contracts. No scheduler/runner, AI execution,
auto-merge, installer or release was activated. No audio/plugin source changed;
plugin builds were not rerun. Not yet verified in a DAW.
