---
id: AUTO-12
model: sonnet
effort: high
dependencies: ["AUTO-08"]
conflicts: []
---

# AUTO-12 — Bounded Claude agentic loop and resumable issue lifecycle

**Tracking issue:** [#180](https://github.com/tonygward/audio-plugins/issues/180)
**Split from:** [AUTO-11](AUTO-11-board-driven-pickup.md)
**Policy:** [Execution and delivery](../../../docs/songsmith-execution-policy.md)
**Status:** Done (11 November 2026). Merged in PRs #193 (Blocked checkpoint, explicit resume, run budget), #209 (in-flight CI recovery after a restart), #218 (resume of committed work, named limits, board Execution releases deferred-looking prompts) and #221 (a resumed task rebuilds its issue-generated prompt). Live evidence: ACOMP-12 (job `matrix-20261111081822`) ran from a Doing card through the starting comment, an agent run, pull request #215, hosted CI, merge and verified beta delivery, and Matrix posted the final comment and closed the issue itself. A controlled test card (TEST-01, #219, pull request #220, closed without merging) then proved on the real board: a Claude turn limit stops the task with one Blocked comment that names the limit and the card moves to Blocked; moving it back to Doing resumes from a clean worktree with a fresh budget; a hosted Build that never starts parks the task as Blocked with the pull request and commit kept; and moving that card back to Doing resumes on the saved branch and the same pull request with no agent run and no second pull request. One Blocked comment attempt failed once (`Process failed`) and succeeded on the next tick with no duplicate. Not exercised live: a kill with a job in flight (covered by the recovery tests, #209) and an agent repair on a resumed failed Build (covered by tests only).

## Objective and user value
One immediate Doing request is first claimed with durable evidence, then enters the standard Claude loop and reaches verified delivery or a clear Blocked outcome. The board remains a truthful high-level view.

## Existing and desired behaviour
Inspect tools/matrix/linux-worker.mjs, existing repair limits, job state and trusted CI/merge/publish workflows first. Reuse those components. Matrix selects and claims; Claude implements/tests/reviews and coordinates trusted merge/publication. Do not build another scheduler or give the coding agent publication credentials.

## Implementation
- Accept exactly one durable task context from AUTO-11: issue, task ID, prompt revision, implementation repository, claim/lease and scheduling intent. Direct pilot execution may supply the same contract without a live board reader.
- Before coding, verify claim, Doing and successful start comment with job and limits. Persist and reconcile partial failures.
- Use configurable defaults of 120 minutes elapsed and three repair iterations total across code/review/CI. Configure an explicit finite Claude turn/token budget appropriate to the installed CLI/SDK; record effective limits at start. Never claim a CLI option exists without verifying installed support.
- Check the first-party weekly/scoped usage before pickup and further Claude invocations: >=60% or unknown/stale readings stop new agent work. Keep one task/run, one active job globally.
- Count wait time towards elapsed budget. Persist start time/counters; restart cannot reset the same run's budget. Terminate further agent calls at the first limit; in-flight trusted workflows may finish and are reconciled before resume.
- Implement, run appropriate tests, independently review where practical, fix within budget, submit PR, require exact-head checks/relevant native CI, invoke trusted automatic approval/merge and follow AUTO-10 through delivery.
- At limit/blocker: checkpoint branch/SHA/PR/check results and delivery stage, comment with progress, actual reason/limit, remaining work and resume action, leave open, Blocked. No automatic next-poll retry.
- Explicit return to Todo or authorised run-now resumes saved state after reconciliation. Never recode merged work or duplicate PR/release/version bumps.
- Final task-specific verification and evidence comment must precede close and board Done. Use task-appropriate delivery: plugin installers/site, website deployment, or infrastructure runtime evidence. A failed comment blocks completion; failed board update after close retries finalisation only.
- AUTO-03 provides liveness between comments; no heartbeat comment spam.

## Acceptance and tests
Prove time/repair/turn-budget boundaries, usage 59.9/60/unknown, kill/restart with retained counters, concurrent claim protection, start/final comment and board API failures, stale exact-head checks, failed release and explicit resume from merged state. One real functional task must reach verified delivery; one controlled limit must produce Blocked and resume without duplicate work. Record run, PR, CI and deployment links.

## Dependencies and risks
AUTO-08 runtime validation; integrate with AUTO-11 selection, AUTO-09 pilot and AUTO-10 delivery. Reconcile existing implementation before changing it. No dependency cycle: direct controlled pilot can validate the loop before board rollout. Preserve unrelated Matrix workloads.

## Build and release
Sonnet default; Opus only for difficult architecture/debugging with recorded reason. Infrastructure changes get appropriate tests and deployed-runtime evidence, no plugin VERSION bump. Qualifying plugin main builds increment only the beta third component; major/minor deferred. Documentation is not activation.
