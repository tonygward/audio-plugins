---
id: AUTO-06
priority: high
model: sonnet
effort: medium
dependencies: []
conflicts: []
---

# AUTO-06 — Canonical backlog queue and Claude subscription usage gate

**Milestone:** Completed implementation (not live agent activation). **Split from:** [AUTO-01](../AUTO-01-matrix-autonomous-development.md). **Evidence:** [PR #101](https://github.com/tonygward/audio-plugins/pull/101) dry-run queue foundation and [PR #102](https://github.com/tonygward/audio-plugins/pull/102) first-party subscription-usage adapter.

## Delivered scope
- Read canonical merged GitHub backlog with stable prompt IDs and `scheduled` (default), `build-now`, and `deferred` execution policies; dependency and conflict filtering, durable pending/claimed/completed states and missed-notification reconciliation.
- Read-only subscription allowance check; fail closed at 85% weekly Claude usage, or when unavailable/unsafe. Never silently enable paid extra usage.
- Explicit dry-run status, decisions, skipped-task reasons and zero agent execution.
- Repository source/test evidence: `tools/matrix/backlog.mjs`, `policy.mjs`, `state.mjs`, `usage.mjs`, `matrix.mjs` and related tests.

## Acceptance evidence and boundaries
Merged PRs #101/#102 and the recorded EQ-38 `build-now` dry-run showing `pending`, zero attempts, usage blocked constitute the *queue/usage foundation* acceptance. This does **not** establish that the new Linux container polls on a schedule, launches Claude, creates PRs or merges/releases anything. Do not use this milestone to bypass runtime deployment checks.

**Archived as a completed, tightly scoped implementation milestone.** Parent AUTO-01 remains open.
