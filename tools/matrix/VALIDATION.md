# AUTO-01 validation evidence

## Controlled worker follow-up

Local `node --test tools/matrix/tests/*.test.mjs`: **91 passed, 0 failed**.
All Matrix PowerShell scripts parsed without syntax errors. The actual disabled
`work` CLI returned `liveExecution: false` without an agent or network operation.
`git diff --check` passed. Windows denied the integration fixture's attempted Git
pointer write; that case verified a blocked draft instead, while a separate
file/hash unit test verified metadata-change detection directly. Cross-platform CI
will exercise the same integration case where fixture metadata can be changed.

The worker integration suite uses real temporary Git repositories, branch/worktree
creation, commits and pushes into local bare remotes. A real simulated-agent child
process edits fixture source; build and GitHub responses are simulated. Cases cover
draft-PR success, duplicate prevention, high/unknown usage, per-repair usage rechecks,
bounded repairs, failed builds, protected edits, source/Git-pointer tampering, pause,
stale main, uncertain PR creation, approval/paths and real child timeout/cancellation/
output limits. No subscription development turn, public remote or real installer is
used in tests. PowerShell launcher/task scripts are syntax-checked only.

Actual worker live execution remains unverified/unactivated. Separate-account Windows
credential/ACL/network behaviour, restricted Claude file edits, JUCE builds, actual
GitHub draft creation, reboot scheduling and screenshot capture need the documented
supervised pilot. See [worker guide](WORKER.md). No automatic merge or release is
implemented, and VERSION is unchanged. Not yet verified in a DAW.

## Usage adapter follow-up

Local tests: 73 passed, 0 failed. New cases cover structured weekly boundaries,
extra usage, cached fallback notices, unknown/duplicate meters, resets, zero-turn
requirements, subscription auth, version checks, provider overrides, redacted errors,
timeouts, fixed commands/temporary-directory cleanup, report integration and pause.
Existing 49 foundation tests remain passing. No account access is used by tests.

The actual adapter (`node tools/matrix/matrix.mjs usage` with an ignored test config)
was checked against installed Claude Code 2.1.319: 96% weekly usage, future weekly
reset, extra usage enabled; eligibility correctly false for both reasons. The
preceding direct CLI verification confirmed zero model turns, zero API duration and
zero reported cost. Sandbox network denial produced missing limits and was rejected;
outside-sandbox read-only verification retrieved the report. Account settings were
not changed and no development task ran. Raw private CLI output is not retained.

Real provider-side cached fallback and Matrix's dedicated account are not verified;
cache/error behaviour is simulated in tests. The structured report is experimental
and does not expose a server fetch timestamp; see runbook freshness limitations.
No live execution, task/runner activation, merge or publication is enabled by this
adapter. Suite VERSION is unchanged. Not yet verified in a DAW.

## Original foundation checkpoint

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
| [Orchestration CI run 42285585425](https://github.com/tonygward/audio-plugins/actions/runs/42285585425) | All 49 tests passed on Windows, Ubuntu and macOS, Node 24, implementation commit `662f829` |

Tests include legacy/default and all explicit execution modes, malformed metadata,
deferred AUTO-02, dependency/claim/conflict gates, duplicate/offline/busy signals,
missed-event daily recovery, real cross-process exclusion, interruption/dead-letter
states, corrupt state, usage boundaries/staleness/reset checks, London DST, pause,
fetch failure, local/canonical separation, timer idempotence, real backlog parsing,
exact-head protected merge validation, two-asset release approval checks and verified
R2/publication requirements before producing a website-event plan.

The real report/state/log files are in ignored `build/matrix-validation/`; they
contain no credentials and are not installed operational configuration.

Not verified: broader plugin Build CI (queued when checked), actual Windows task
registration/reboot, restricted notification runner/ACLs, Matrix hardware, subscription
authentication/fresh pre-start telemetry, isolated Claude execution, builds/DAW checks,
authenticated PR/merge adapters, live release/R2 upload or website receiver. The tests
use synthetic evidence for those policy contracts. No scheduler/runner, AI execution,
auto-merge, installer or release was activated. No audio/plugin source changed;
plugin builds were not rerun. Not yet verified in a DAW.
