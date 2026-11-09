---
id: AUTO-09
status: planned
execution: scheduled
priority: high
model: sonnet
effort: high
dependencies: ["AUTO-08"]
conflicts: []
---

# AUTO-09 — One Linux-container task through PR, native CI and verified merge

**Split from:** [AUTO-01](AUTO-01-matrix-autonomous-development.md) and [AUTO-05](AUTO-05-linux-agent-github-actions.md). **Status:** Open: an end-to-end operational pilot, not new worker scaffolding.

## Goal
Show one eligible canonical backlog item is developed by the isolated Linux agent, tested on **hosted Windows and macOS GitHub Actions at the current PR head SHA**, repaired in a bounded way if needed, and merged through the independently gated workflow.

## Work
1. Only after AUTO-08 passes, intentionally enable a **single-task supervised pilot** in the container with confirmed first-party Claude usage **strictly below 85%**, extra paid usage off, scoped credentials and explicit owner activation. Preserve a no-run mode while blocked/unknown.
2. From a pinned prompt ID and main commit, create isolated worktree/branch, launch bounded Claude CLI, capture provenance, changed-file allowlist and test results, and open draft PR. Do not let the agent edit protected workflows, approval rules or publishing secrets.
3. Verify the hosted Windows VST3/EXE suite build and macOS AU/PKG suite build/test jobs run against **exactly the latest PR head SHA**. Never treat stale or missing checks as green. Collect redacted failures and perform at most the configured repair attempts.
4. Verify an independent trusted `matrix-auto-merge.yml` run can merge only a passing, policy-compliant current-head PR and archive/mark the corresponding stable prompt ID done **after** merge. Validate failure/timeout, interruption/restart, duplicate notification, pause and missing-allowance paths.
5. Capture one real pilot run with PR URL, head SHA, CI URLs, test outcomes, merge/archive evidence and any manual DAW/UI checks still outstanding.

## Acceptance
A fully traced real task reaches green Windows/macOS CI and a policy-valid merge with no manual developer code intervention; when any gate fails it remains pending/draft without merge. A supervised *pilot* is not proof of unattended reliability: record and test a separate controlled unattended repeat before closing AUTO-01 globally.

**This prompt does not directly grant permission to activate live execution.**
