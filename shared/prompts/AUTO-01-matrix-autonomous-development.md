---
id: AUTO-01
priority: high
model: sonnet
effort: high
dependencies: []
conflicts: []
---

# AUTO-01 — Autonomous development orchestrator

**Tracking issue:** [#162](https://github.com/tonygward/audio-plugins/issues/162)
**Model:** Sonnet for implementation/review; Opus only for unresolved isolation or orchestration architecture, with the reason recorded and a return to Sonnet.
**Policy:** [Current execution and delivery policy](../../docs/songsmith-execution-policy.md).

## Objective and adopted architecture
Matrix owns an isolated, non-root Linux-container development controller. Claude edits scoped branches/worktrees. GitHub-hosted Windows/macOS Actions validate native plugin binaries, tests and the two suite installers. Trusted automation owns automatic approval/merge, build-number increments, release publication, private R2 and beta website deployment. The coding agent never holds publishing credentials.

The code foundation exists in tools/matrix/linux-worker.mjs. Real implementation PRs #174 and #177 demonstrate live execution has occurred; this does not certify every runtime isolation, recovery or distribution check. Retired Windows multi-account, DPAPI and local build-broker designs are historical; do not reactivate them.

## Current behaviour and desired completion
Board pickup is live ([AUTO-11](done/AUTO-11-board-driven-pickup.md)): the board's actual order decides selection, with no fallback ordering.

Use one task per invocation and one active task total. Stop new pickup at >=60% verified weekly Claude usage, including scoped meters; unknown/stale telemetry blocks pickup. Infrastructure/website tasks require the correct scoped execution path, not a location-based refusal.

## Delivery milestones
- [AUTO-06](done/AUTO-06-matrix-queue-and-usage-foundation.md): merged queue and first-party usage foundations.
- [AUTO-07](done/AUTO-07-linux-ci-worker-code-foundation.md): merged Linux-worker/hosted-CI code foundation.
- [AUTO-08](AUTO-08-linux-container-deployment-validation.md): deployed image, mounts, capabilities, credentials, scheduler, durable recovery, pause and negative-isolation evidence.
- [AUTO-09](AUTO-09-container-development-ci-and-merge-pilot.md): genuine single-task development, exact-head native CI, bounded repair and trusted automatic merge evidence.
- [AUTO-04](AUTO-04-independent-tdd-test-agent.md) and [AUTO-02](AUTO-02-codex-fallback.md) remain separate enhancements.

## Acceptance, tests and lifecycle
Reconcile and accept AUTO-08 and AUTO-09 with real run/PR/CI/deployment evidence; do not infer their completion from code or earlier prose. Test isolation, restart/idempotence, missing/stale usage, 59.9%/60% limits, one-task claims, failed/stale native CI and failed distribution. Preserve unrelated Matrix workloads.

Automatically approve/merge authorised beta work only after exact-head required checks pass. Automatically increment the third beta version component; major/minor increments are deferred. Comment on the issue at actual pickup, progress, failure, merge and release blockers. After the beta release, Matrix posts the final comment (with the download link), closes the issue and sets the card Done. Archiving a merged implementation prompt is not verified delivery.

This epic records scope and acceptance; editing it does not activate services or grant credentials. Use the child prompts for implementation; no duplicate umbrella job, synthetic release or unrelated version bump.

## Delivery

Follow the [execution policy](../../docs/songsmith-execution-policy.md). The board decides when this runs; Sonnet is the default model.
