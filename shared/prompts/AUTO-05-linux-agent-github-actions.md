---
id: AUTO-05
status: planned
execution: deferred
priority: high
model: sonnet
effort: medium
dependencies: ["AUTO-01"]
conflicts: []
---

# AUTO-05 — Isolated Linux development agent with GitHub Actions native builds

**Status:** Architecture/design backlog only; not authorised for autonomous execution.
**Priority:** High infrastructure evaluation, after or alongside the supervised AUTO-01 pilot.
**Recommended model:** Sonnet for repository audit, implementation plan, CI integration and tests. **Escalate to Opus:** Only for unresolved security/trust-boundary or deployment architecture decisions; document the decision, then return to Sonnet. **Relative effort/usage:** Medium for design; implementation effort to be estimated after audit.

## Goal

Evaluate and, after separate owner approval, migrate the Songsmith autonomous development controller to an isolated **Linux VM or non-privileged container on MATRIX**, using **GitHub-hosted Actions runners** to compile and test native Windows/macOS plugin targets. Reuse the existing backlog/usage logic and GitHub CI/release pipeline instead of maintaining a complex local Windows cross-account build environment.

**This prompt requests an architecture decision and phased implementation plan only. Do not activate the migration or live worker.** Maintain the existing scheduled dry-run scanner and current AUTO-01 state while investigating.

## Context and hypothesis

- MATRIX already hosts Docker workloads, including Home Assistant; no change may interrupt these services.
- AUTO-01 currently implements a disabled Windows controller/worker with a dedicated source checkout, SongsmithAgent and SongsmithBuild accounts, trusted local build launcher, short-lived approval and usage gates.
- The current Windows approach has substantial NTFS ACL, identity, build-credential and worktree-isolation complexity. These safeguards must not be weakened merely to make the pilot work.
- The existing GitHub Actions workflows, particularly `.github/workflows/build.yml` and `.github/workflows/matrix-tests.yml`, reportedly already cover native Windows and macOS builds and tests. Verify actual triggers, targets, prerequisites, installer outputs, required checks and whether they execute on PRs before relying on them.
- A standard Linux container cannot use the host's Windows Visual Studio installation to produce native Windows JUCE/VST3 binaries. Use hosted Windows/macOS jobs for those targets; Linux builds/tests only where supported.
- A Linux VM and a Docker container are alternatives, not equivalent security boundaries. A hardened VM may offer stronger separation from the host and existing Docker workloads; compare operational cost and security explicitly.

## Proposed workflow (subject to audit)

1. Isolated Linux agent polls/synchronises the canonical GitHub prompt backlog, respects `scheduled`, `build-now`, `deferred`, dependencies, claims and the authoritative 85% Claude usage gate from AUTO-01. Unknown or invalid usage fails closed.
2. Agent uses Claude Code with bounded runtime, turns, retries and spending under a least-privilege identity, implements one controlled backlog item and opens/updates a **draft PR**.
3. Existing GitHub Actions build/test on Windows and macOS hosted runners, with Linux unit/DSP checks where useful. Do not expose secrets to untrusted PR code.
4. Agent observes only the current-head CI result, retrieves redacted actionable diagnostics, performs bounded repairs and pushes updates; stale checks never authorise a merge.
5. After the separately approved automated merge policy and all required independent checks, the existing trusted release workflow alone can publish verified beta installers to GitHub Release/R2 and update the private beta website. Agent receives **no publishing credentials**.

## Requirements and investigation

- Read `AGENTS.md`, `CLAUDE.md`, AUTO-01 prompt, `tools/matrix/WORKER.md`, relevant ADRs, `.github/workflows/build.yml`, `.github/workflows/matrix-tests.yml`, and release workflows. Document the actual current capabilities and gaps; do not assume green CI implies DAW compatibility.
- Compare (A) Linux VM, (B) rootless/non-privileged Docker container, and (C) existing Windows accounts against isolation, maintainability, recovery, resource consumption, licensing, Claude/GitHub authentication, secret handling, attack surface, screenshot validation and CI costs/latency.
- Keep Docker socket inaccessible to the agent; no privileged mode, host networking, arbitrary host mounts, Home Assistant secrets, unrelated containers, host filesystem/LAN discovery or publish credentials. Prefer disposable workspaces and read-only trusted configuration.
- Consider that untrusted source code, package scripts and build/test processes run with the permissions of CI runners; minimise GitHub token permissions and follow GitHub's fork/PR workflow security model. Do not execute unreviewed workflow changes with privileged credentials.
- Reuse the existing dry-run queue, missed-event reconciliation, prompt IDs, model choices, usage gate, timeout/repair budgets and task-state telemetry. Address idempotence and reconciling PRs, retries and CI after reboot/offline periods.
- Validate Windows VST3/installer, macOS PKG/installer, notarisation/signing status, Linux portable DSP tests and screenshot/GUI/DAW evidence. State which tests require interactive Windows/macOS hosts rather than generic CI.
- Estimate hosted-runner usage minutes and plan constraints rather than assuming unlimited free CI. Check current GitHub policy/billing separately at implementation time.
- Provide migration and rollback steps; identify which Windows ACL/account/build-launcher pieces could be retired **only after** the replacement passes a supervised pilot.
- Preserve the existing AUTO-01 worker/scanner and all active Docker services. No system-wide installs or changes to VM/container settings without explicit owner approval.

## Deliverables

1. Short architecture decision record (trust-boundary diagram, tradeoffs, recommended option and why).
2. Gap analysis of current workflows versus draft-PR → CI → bounded repair → merge → beta release lifecycle.
3. Phased migration plan with dependencies, acceptance tests, approximate effort/cost and rollback.
4. Security model and credentials matrix; demonstrate that the developer agent cannot read publishing secrets or control other Docker containers.
5. Recommendation to replace or supplement AUTO-01, including whether the current Windows build account remains useful.

## Acceptance criteria

- Repository-supported architecture, not a speculative replacement.
- Current dry-run scanner remains operational; existing Home Assistant and Docker workloads untouched.
- No claims of complete isolation without effective negative/positive tests.
- No auto-merges, releases, scheduled worker activation, secrets rotation or infrastructure changes caused by this backlog addition.
- Open a **draft PR** for the design/implementation proposal; await separate owner authorisation to change runtime architecture.

**Execution policy:** `deferred` until Tony explicitly promotes it. An architectural evaluation alone is not approval for autonomous runtime migration.
