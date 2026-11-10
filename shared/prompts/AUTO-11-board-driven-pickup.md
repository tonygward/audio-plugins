---
id: AUTO-11
status: deferred
execution: deferred
priority: 1
model: sonnet
effort: medium
dependencies: ["AUTO-08", "AUTO-09"]
conflicts: []
---

# AUTO-11 — Board configuration, ordered pickup and atomic task handoff

**Tracking issue:** [#175](https://github.com/tonygward/audio-plugins/issues/175)
**Board:** [Songsmith Development](https://github.com/users/tonygward/projects/2)
**Policy:** [Execution and delivery](../../docs/songsmith-execution-policy.md)
**Split:** Task execution/limits/resume belong to [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180); verified delivery belongs to [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157).
**Status:** Specification; live board integration unverified.

## Objective and existing behaviour
Let board management determine the next task. Existing Markdown selection is not a verified board reader. Keep this task focused on board configuration, selection, claims and handoff, without duplicating the agent loop or publishing implementation.

## Requirements
1. Reconcile actual project fields/views first. Use Backlog, Todo (ready queue), Doing (run now, then claimed), Blocked (blocked/limit) and Done. Add Blocked if missing; retain Todo rather than creating a duplicate Ready column. Preserve order and unrelated fields/views.
2. Doing is a run-now request and is offered first, then Todo, each in the specific view's visible manual top-to-bottom order. A Doing card whose prompt is deferred is skipped and never blocks. Pickup is serialised behind any active job and still needs a durable claim and all gates. Backlog/Blocked/Done do not auto-run.
3. Validate actual order with real A/B drag-and-drop tests. Generic GraphQL pagination is not proof. If rank unavailable, fail closed with an actionable reason; no priority/ID/repository fallback. Numeric Queue Order requires owner decision.
4. Resolve exactly one issue, canonical merged prompt, stable task ID and implementation repository. Never reject by prefix or location. Exclude parent epics, historical work-order trackers and PR cards as duplicate execution jobs.
5. Check dependencies, claims, existing branches/PRs/releases, execution scope and verified weekly/scoped usage (<60%). Unknown/stale usage blocks new work globally. Infrastructure/website tasks require their scoped path; no new privileges from board placement. Batch size one and one active task globally.
6. Atomically persist claim/lease and task context, move to Doing and post start comment before development (job/run, prompt revision, repository, effective limits). Reconcile partial failures; never code without recorded claim and successful start comment.
7. Hand durable context to AUTO-12; observe its stages. Bounded loop and release finalisation are owned by AUTO-12/AUTO-10, not separately reimplemented here. Sync Blocked and Done only from verified lifecycle evidence.
8. Reconcile existing Doing cards before rollout: active claimed work stays Doing; an unclaimed Doing card is a run-now request and stays; a deferred one is left alone; interrupted/blocked work goes Blocked with explanation. Merged work resumes delivery only. Persist API failures and avoid duplicate comments/claims.
9. Explicit return from Blocked to Todo or Doing permits resumption after reconciliation, never an automatic poll retry. Finalisation obeys successful evidence comment → close issue → Done; handle partial updates idempotently.

## Acceptance and tests
- Actual project has intended statuses; order preserved.
- A/B board reorder changes selection; cross-repository/prefix cases obey order.
- One atomic claim under concurrency/restart/duplicate cards.
- A Doing card with no claim is picked up ahead of Todo (run now); a Doing card with a deferred prompt is never picked up and does not block other work.
- Usage 59.9 permits, 60/unknown/stale blocks. Missing order, partial snapshots and API errors start no work.
- Blocked task records reason; Blocked is not immediately requeued.
- Handoff to bounded loop, correct start comments and recovery from partial status/comment failures.
- Final Done only from verified completion, including no duplicate coding for merged/release-pending tasks.

## Dependencies, risks, build and delivery
AUTO-08 runtime and AUTO-09 pilot evidence; integrate AUTO-12 and AUTO-10 for complete rollout. Do not create a dependency cycle: loop/pilot can be tested with a controlled direct task context before enabling board pickup. Test scoped board access and live view order. Infrastructure-only changes do not bump plugin VERSION or manufacture plugin releases. Record deployed controller and live board evidence before claiming operational.
