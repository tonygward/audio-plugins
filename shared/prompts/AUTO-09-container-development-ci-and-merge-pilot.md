---
id: AUTO-09
status: planned
execution: deferred
priority: high
model: sonnet
effort: high
dependencies: ["AUTO-08"]
conflicts: []
---

**Current policy:** [Board ordering, one task, 60% usage gate, automatic beta builds and verified issue completion](../../docs/songsmith-execution-policy.md). This policy supersedes historical batches, folder/ID ordering and merge-only completion in older design notes. Automatic approval/merge requires exact-head green CI; issue Done requires beta deployment/download verification and a final issue comment.

**GitHub tracking issue:** [#156](https://github.com/tonygward/audio-plugins/issues/156).

**Execution policy:** Follow [current selection and delivery rules](../../docs/songsmith-execution-policy.md). Board order decides selection regardless of prefix or issue/prompt location. This deferred task needs its appropriate scoped infrastructure execution path; board placement does not grant new privileges. One task only; 60% weekly usage stop threshold. Documentation does not activate a worker.


# AUTO-09 — One Linux-container task through PR, native CI and verified merge

**Split from:** [AUTO-01](AUTO-01-matrix-autonomous-development.md) and [AUTO-05](AUTO-05-linux-agent-github-actions.md). **Status:** Open: an end-to-end operational pilot, not new worker scaffolding.

## Goal
Show one eligible canonical backlog item is developed by the isolated Linux agent, tested on **hosted Windows and macOS GitHub Actions at the current PR head SHA**, repaired in a bounded way if needed, and merged through the independently gated workflow.

## Work
1. Only after AUTO-08 passes, intentionally enable a **single-task supervised pilot** in the container with confirmed first-party Claude usage **strictly below 60%**, extra paid usage off, scoped credentials and explicit owner activation. Preserve a no-run mode while blocked/unknown.
2. From a pinned prompt ID and main commit, create isolated worktree/branch, launch bounded Claude CLI, capture provenance, changed-file allowlist and test results, and open draft PR. Do not let the agent edit protected workflows, approval rules or publishing secrets.
3. Verify the hosted Windows VST3/EXE suite build and macOS AU/PKG suite build/test jobs run against **exactly the latest PR head SHA**. Never treat stale or missing checks as green. Collect redacted failures and perform at most the configured repair attempts.
4. Verify an independent trusted `matrix-auto-merge.yml` run can merge only a passing, policy-compliant current-head PR and archive/mark the corresponding stable prompt ID done **after** merge. Validate failure/timeout, interruption/restart, duplicate notification, pause and missing-allowance paths.
5. Capture one real pilot run with PR URL, head SHA, CI URLs, test outcomes, merge/archive evidence and any manual DAW/UI checks still outstanding.

## Acceptance
A fully traced real task reaches green Windows/macOS CI and a policy-valid merge with no manual developer code intervention; when any gate fails it remains pending/draft without merge. A supervised *pilot* is not proof of unattended reliability: record and test a separate controlled unattended repeat before closing AUTO-01 globally.

**This prompt does not directly grant permission to activate live execution.**


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the prompt is deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This specification update does not activate Matrix or demonstrate operational board integration.
