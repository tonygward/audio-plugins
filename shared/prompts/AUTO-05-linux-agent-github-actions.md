---
id: AUTO-05
status: blocked
execution: deferred
priority: high
model: sonnet
effort: high
dependencies: []
conflicts: []
---

# AUTO-05 — Isolated Linux development agent and hosted native builds

**Tracking issue:** [#166](https://github.com/tonygward/audio-plugins/issues/166)
**Status:** Parent tracking epic, not a duplicate implementation job. Its child milestones own the remaining acceptance evidence.
**Model:** Sonnet for implementation/review; Opus only for unresolved isolation or orchestration architecture, with the reason recorded and a return to Sonnet.
**Policy:** [Current execution and delivery policy](../../docs/songsmith-execution-policy.md).

## Objective and adopted architecture
Matrix owns an isolated, non-root Linux-container development controller. Claude edits scoped branches/worktrees. GitHub-hosted Windows/macOS Actions validate native plugin binaries, tests and the two suite installers. Trusted automation owns automatic approval/merge, build-number increments, release publication, private R2 and beta website deployment. The coding agent never holds publishing credentials.

The code foundation exists in tools/matrix/linux-worker.mjs. Real implementation PRs #174 and #177 demonstrate live execution has occurred; this does not certify every runtime isolation, recovery or distribution check. Retired Windows multi-account, DPAPI and local build-broker designs are historical; do not reactivate them.

## Current behaviour and desired completion
The existing Markdown selector is not a verified board reader. [AUTO-11](AUTO-11-board-driven-pickup.md) specifies the board integration: actual visible manual order decides selection, regardless of issue/prompt repository or prefix. No priority/ID/repository fallback; fail closed if manual rank cannot be verified.

Use one task per invocation and one active task total. Stop new pickup at >=60% verified weekly Claude usage, including scoped meters; unknown/stale telemetry blocks pickup. Infrastructure/website tasks require the correct scoped execution path, not a location-based refusal.

## Delivery milestones
- [AUTO-06](done/AUTO-06-matrix-queue-and-usage-foundation.md): merged queue and first-party usage foundations.
- [AUTO-07](done/AUTO-07-linux-ci-worker-code-foundation.md): merged Linux-worker/hosted-CI code foundation.
- [AUTO-08](AUTO-08-linux-container-deployment-validation.md): deployed image, mounts, capabilities, credentials, scheduler, durable recovery, pause and negative-isolation evidence.
- [AUTO-09](AUTO-09-container-development-ci-and-merge-pilot.md): genuine single-task development, exact-head native CI, bounded repair and trusted automatic merge evidence.
- [AUTO-10](AUTO-10-verified-beta-distribution-e2e.md): qualifying build → automatic beta build/patch increment → exactly two release installers → verified private R2 → deployed beta website and authenticated downloads.
- [AUTO-03](AUTO-03-matrix-remote-heartbeat.md), [AUTO-04](AUTO-04-independent-tdd-test-agent.md) and [AUTO-02](AUTO-02-codex-fallback.md) remain separate enhancements.

## Acceptance, tests and lifecycle
Reconcile and accept AUTO-08, AUTO-09 with real run/PR/CI/deployment evidence; do not infer their completion from code or earlier prose. Test isolation, restart/idempotence, missing/stale usage, 59.9%/60% limits, one-task claims, failed/stale native CI and failed distribution. Preserve unrelated Matrix workloads.

Automatically approve/merge authorised beta work only after exact-head required checks pass. Automatically increment the third beta version component; major/minor increments are deferred. Comment on the issue at actual pickup, progress, failure, merge and release blockers. Keep it open until beta deployment/download verification, post the final completion comment, then close it and finally set board Done. Archiving a merged implementation prompt is not verified delivery.

This epic records scope and acceptance; editing it does not activate services or grant credentials. Use the child prompts for implementation; no duplicate umbrella job, synthetic release or unrelated version bump.


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the prompt is deferred; once picked up it is claimed with a start comment. Needs attention is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Needs attention. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This specification update does not activate Matrix or demonstrate operational board integration.
