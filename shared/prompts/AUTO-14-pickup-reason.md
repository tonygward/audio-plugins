---
id: AUTO-14
status: deferred
execution: deferred
priority: medium
model: sonnet
effort: low
dependencies: []
conflicts: []
---

# AUTO-14 — Record the reason for each Matrix issue pickup

**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/208  
**Related:** [AUTO-11 board selection/claim](https://github.com/tonygward/audio-plugins/issues/175), [AUTO-12 issue lifecycle](https://github.com/tonygward/audio-plugins/issues/180).  
**Example:** [ACOMP-11 issue #199](https://github.com/tonygward/audio-plugins/issues/199), pickup job `matrix-20261110213641`.  
**Policy:** [Songsmith execution policy](../../docs/songsmith-execution-policy.md).  
**Workstream:** Matrix infrastructure. **Execution:** Deferred; this prompt does not queue or start work.

## Objective and user value

Every Matrix startup comment must explain **why that issue was selected**: the scheduled Todo queue, an unclaimed Doing/run-now request, or an explicit resume. Make the reason auditable from the issue without requiring access to private worker logs. Do not confuse a selection trigger with proof that a human manually moved the card.

## Existing behaviour

The ACOMP-11 initial comment identifies the Matrix job, prompt revision, repository and limits, but not the pre-claim project status or selection path. Since Matrix can move a card to Doing when claiming it, reading the status after pickup cannot reconstruct the trigger. Review the current worker code, persisted claim/job schema and comment rendering before editing.

## Desired behaviour

Capture structured, immutable provenance **at candidate selection, before the board mutation**. Persist it in the durable job/claim and render in the initial issue comment.

- **Scheduled (Todo):** issue selected from the Todo queue at the first permitted daily pickup (Europe/London). Show original status, selection timestamp, London schedule date, and ordering/view metadata where reliably available; this consumes the one scheduled Todo pickup for the London day.
- **Run now (Doing):** issue was unclaimed in Doing prior to Matrix claiming it. Show original status and worker poll/selection timestamp. This does not consume the daily Todo slot. Say “Run now request” rather than “manually moved by Tony”, unless separately verified GitHub event data identifies an actor.
- **Resumed (Todo or Doing):** explicit requeue of previously Blocked/interrupted work. Show the triggering status, original Matrix job or PR, persisted checkpoint/stage and resume intent when available; do not create duplicate implementation work.
- **Unknown:** if provenance cannot be established reliably, state “Pickup reason: Unknown (provenance unavailable)” and log a diagnostic. Never infer from the post-claim status or fabricate manual actor information.

Suggested start-comment excerpt:

```text
Matrix has picked up ACOMP-XX.
- Pickup reason: Run now (Doing)
- Before claim: Doing
- Selected at: 2026-11-10 21:36 Europe/London
- Initiator: Unknown (board status requested immediate processing)
- Matrix job: matrix-...
...
```

Use consistent exact labels across the three reason types; keep existing job, prompt revision, repository, limits and status wording.

## Implementation guidance

1. Identify the selection boundary in AUTO-11 and existing claim/start-comment integration in AUTO-12. Add a small typed or validated provenance record (e.g., `pickupReason`, `preClaimStatus`, `selectedAt`, `boardView`, `scheduleDate`, `resumedFrom`) without introducing another orchestrator or competing job record.
2. Read pre-claim board state from the same validated candidate snapshot used for selection. If the snapshot is stale or inconsistent, follow the existing fail-closed selection policy; do not substitute guessed provenance.
3. Persist the provenance atomically with or durably before claim transition and use the persisted copy for comment generation. Handle partial claim/status/comment failures via reconciliation, preserving exactly one accurate startup comment per claim.
4. Where reliable GitHub event history explicitly identifies a status-changing actor, it may be displayed separately as verified evidence; otherwise mark the initiator unknown. Never equate Doing with a proven manual action.
5. Preserve existing board ordering, one global active job, Todo daily slot, Doing priority, 60% weekly usage gate, scoped permissions and deferred/Separate exclusions. Do not change any scheduling or user-visible status semantics.
6. On crash recovery, keep the original reason and timestamp, not the status observed on restart. Resume only from the persisted checkpoint without reimplementation.
7. Restrict privileged comment writes to the trusted controller. Avoid disclosing secrets, tokens or private runtime paths in issues.

## Acceptance criteria and automated tests

- **Scheduled Todo:** controlled selection yields `Pickup reason: Scheduled (Todo)`, records pre-claim Todo and London date; consumes the scheduled slot once.
- **Run-now Doing:** controlled unclaimed Doing selection yields `Pickup reason: Run now (Doing)`; does not consume a Todo slot or falsely attribute a human actor.
- **Resume:** explicit retry after Blocked identifies resume, pre-claim status and linked prior job/PR/checkpoint when available.
- **Persistence:** startup comment continues to show the original pre-claim status after Matrix changes board status to Doing and after worker restart.
- **Idempotency:** duplicate polls, worker crash, partial comment API failure and reconciliation do not create contradictory or duplicate pickup comments.
- **Negative cases:** stale/partial board snapshots fail closed; unknown actor is labelled unknown; deferred/Separate cards are not claimed; invalid provenance never silently becomes “manual”.
- **Regression:** ordering, usage threshold (59.9% versus 60%/unknown), one active job, scheduled slot, cross-repo claims and normal comments remain correct.
- **Live verification:** one controlled Matrix pickup proves the persisted reason matches the starting issue comment. Do not rewrite ACOMP-11's historical reason without genuine saved evidence.

## Dependencies, risks and completion

Depends on existing AUTO-11 selection and AUTO-12 comment/claim infrastructure; adapt to the actual code. Main risks are post-claim status confusion, historical provenance gaps and duplicate start comments after interruption.

Run Matrix/controller automated tests, review exact-head CI and demonstrate deployed worker acceptance. Follow the standard scoped trusted merge process. **No unrelated plugin VERSION increment, installer build or beta publication is required for this infrastructure-only change.** Comment with evidence, close issue, then mark board Done only after verified completion. Keep this item deferred until separately scheduled.
