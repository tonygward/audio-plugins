---
id: AUTO-13
status: deferred
execution: deferred
priority: medium
model: sonnet
effort: small
dependencies: []
conflicts: []
---

# AUTO-13 — Comment on issue when PR CI passes

**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/196
**Related:** [AUTO-11 board pickup](https://github.com/tonygward/audio-plugins/issues/175), [AUTO-12 agentic lifecycle](https://github.com/tonygward/audio-plugins/issues/180).
**Policy:** [Songsmith execution policy](../../docs/songsmith-execution-policy.md).
**Execution:** Deferred infrastructure improvement; does not start Matrix, change board order or interrupt ACOMP-07.

## Objective and value
When all required PR-head CI checks pass, Matrix posts a concise, auditable progress comment on the **original functional GitHub issue**, so the board's Doing card explains that testing passed and work is progressing towards merge and beta publication.

## Existing behaviour
ACOMP-07 [issue #160](https://github.com/tonygward/audio-plugins/issues/160) received a Matrix pickup comment but did not immediately receive a CI-passed comment when [PR #195](https://github.com/tonygward/audio-plugins/pull/195) passed Windows/macOS checks (Build #269). The issue remained open while the PR merged and subsequent release work proceeded. Inspect actual Matrix lifecycle and trusted CI/PR callbacks before implementing; don't infer missing functionality from an old screenshot.

## Required behaviour
1. Detect **all required checks successful for the exact current PR head SHA**, including relevant Windows/macOS plugin jobs and required security/quality gates. Intentional skipped jobs do not count as failures; missing, pending, stale or failed required checks do not count as success.
2. Resolve the uniquely linked original tracking issue from the durable Matrix job context / canonical task ID, even when implementation, issue and prompt reside in different repositories. Never comment on the PR instead of the issue, or guess from a branch title alone.
3. Post one progress comment on the original issue only after verified CI success. Include task ID, PR link, CI workflow run link, exact head SHA (abbreviated is fine), Windows/macOS result, and what comes next (trusted auto-merge and beta publication). Example: “✅ CI passed for ACOMP-07 — PR #195, Build #269. Windows and macOS required checks green at <SHA>. Awaiting automatic merge and beta distribution.”
4. Make comments **idempotent per task + PR + head SHA + CI result** using a durable marker and reconciliation. Workflow reruns, repeated polls or controller restarts must not spam duplicate comments. If code is amended and a new head SHA passes, a new comment may describe the new validation.
5. If posting fails, persist an actionable retry state; do not pretend a comment was posted. Retry safely without recoding. Do not block trusted merge solely because a non-final progress comment fails; final evidence comment remains mandatory before issue closure.
6. Keep existing start, blocker, merge, release and final comments; do not mark the issue Done or claim deployment merely because CI passes. Board stays Doing until verified task-specific delivery or goes Blocked if legitimately stalled.
7. Scope permission narrowly to issue-comment write access; do not give the Claude coding process elevated GitHub credentials. Trusted Matrix controller/approved workflow writes the comment.

## Acceptance tests
- Required exact-head Windows and macOS CI green -> exactly one CI-passed comment on the linked issue, with PR/CI links and SHA.
- Pending, failed, missing or stale checks -> no success comment.
- Intentionally skipped unrelated plugin checks -> not a false failure; required checks still enforced.
- Retries, duplicate events, controller restarts and repeated polling -> no duplicate comment.
- Amended PR head -> success comment only for its new validated SHA.
- Cross-repository tracking issue and PR -> comment lands on original issue.
- Permission/API failures -> retryable state, accurate diagnostics, no false Done.
- One live functional Matrix task demonstrates the resulting comment and later delivery trace.

## Dependencies, risks and delivery
Implement as a small independent extension of existing AUTO-12 / CI observability, rather than a new scheduler. Follow the current <60% weekly Claude usage gate, one active Matrix task and bounded agent workflow. Infrastructure-only change requires Matrix orchestration tests and deployed runtime evidence; **no plugin beta version bump or installer release**. Do not introduce a separate manual approval gate. Avoid heartbeat-style comment spam.