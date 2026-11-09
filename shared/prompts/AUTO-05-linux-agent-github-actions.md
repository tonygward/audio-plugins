---
id: AUTO-05
status: blocked
execution: deferred
priority: high
model: sonnet
effort: medium
dependencies: ["AUTO-01"]
conflicts: []
---

## Parent epic: Linux deployment work split (9 November 2026)

**Tracking only — not an eligible autonomous prompt.** The Linux-container direction was selected and the code foundation was merged in [AUTO-07](done/AUTO-07-linux-ci-worker-code-foundation.md), [PR #139](https://github.com/tonygward/audio-plugins/pull/139). That completes the **architecture decision and implementation-code milestones**, *not* a verified running container.

Remaining delivery is tracked separately: [AUTO-08 container security and deployment validation](AUTO-08-linux-container-deployment-validation.md) and [AUTO-09 real task/hosted-CI/merge pilot](AUTO-09-container-development-ci-and-merge-pilot.md). [AUTO-10](AUTO-10-verified-beta-distribution-e2e.md) verifies downstream releases. The legacy discussion below is retained for provenance; those subtask stories now own execution and acceptance evidence.

Close this epic only after AUTO-08 and AUTO-09 are proven in the actual Linux container. Deferred status prevents Matrix selecting this umbrella story alongside its child tasks.

## Adopted direction and implementation checkpoint — 9 November 2026

**Architecture decision supplied by the owner:** Songsmith development now targets an **isolated Linux container on Matrix**, not the retired Windows multi-account autonomous worker. The source contains `tools/matrix/linux-worker.mjs` labelled AUTO-05 phase 2, with non-root/no-Docker-socket/capability/egress preflight, durable queue/usage handling and hosted GitHub Windows/macOS CI validation at pinned PR head SHA. This story is now the **container implementation, deployment validation and rollback backlog**, rather than a request to choose Windows vs Linux. The Windows worker remains historical/rollback context; don't reactivate it while migrating.

**Still verify:** actual container image/config and volume mounts, least-privilege secret delivery, authentic first-party subscription usage within container, GitHub scoped PR token and egress, polling/scheduling and restart, integration with CI repair and auto-merge policies, native Windows/macOS CI gates, isolation negative tests, durable state/recovery and the remotely observable heartbeat. Neither the file's existence nor this architecture decision proves that the production container is running or fully validated.

**Scope discipline:** Container migration is the chosen direction, but *live unattended execution remains a separate operational activation decision* after these checks. Preserve Matrix's unrelated Docker workloads and existing release credentials boundary. Keep the story in the backlog until deployed evidence and end-to-end tests are recorded.

# AUTO-05 — Isolated Linux development agent with GitHub Actions native builds

**Status:** Linux-container architecture selected; implementation/validation backlog open. Not authorisation to enable unattended execution.
**Priority:** High infrastructure evaluation, after or alongside the supervised AUTO-01 pilot.
**Recommended model:** Sonnet for repository audit, implementation plan, CI integration and tests. **Escalate to Opus:** Only for unresolved security/trust-boundary or deployment architecture decisions; document the decision, then return to Sonnet. **Relative effort/usage:** Medium for design; implementation effort to be estimated after audit.

## Goal

Implement and validate the owner-selected migration of the Songsmith autonomous development controller to an isolated **non-root Linux container on MATRIX**, using **GitHub-hosted Actions runners** to compile and test native Windows/macOS plugin targets. Reuse the existing backlog/usage logic and GitHub CI/release pipeline instead of maintaining a complex local Windows cross-account build environment.

**The container is the selected direction. Complete its deployment validation without automatically activating the live worker.** Maintain the existing scheduled dry-run scanner and current AUTO-01 state while investigating.

## Context and hypothesis

- MATRIX already hosts Docker workloads, including Home Assistant; no change may interrupt these services.
- AUTO-01 currently implements a disabled Windows controller/worker with a dedicated source checkout, SongsmithAgent and SongsmithBuild accounts, trusted local build launcher, short-lived approval and usage gates.
- The current Windows approach has substantial NTFS ACL, identity, build-credential and worktree-isolation complexity. These safeguards must not be weakened merely to make the pilot work.
- The existing GitHub Actions workflows, particularly `.github/workflows/build.yml` and `.github/workflows/matrix-tests.yml`, reportedly already cover native Windows and macOS builds and tests. Verify actual triggers, targets, prerequisites, installer outputs, required checks and whether they execute on PRs before relying on them.
- A standard Linux container cannot use the host's Windows Visual Studio installation to produce native Windows JUCE/VST3 binaries. Use hosted Windows/macOS jobs for those targets; Linux builds/tests only where supported.
- The Linux container is the selected design; a VM is a documented fallback if isolation tests reveal unacceptable shared-kernel risk.

## Proposed workflow (subject to audit)

1. Isolated Linux agent polls/synchronises the canonical GitHub prompt backlog, respects `scheduled`, `build-now`, `deferred`, dependencies, claims and the authoritative 85% Claude usage gate from AUTO-01. Unknown or invalid usage fails closed.
2. Agent uses Claude Code with bounded runtime, turns, retries and spending under a least-privilege identity, implements one controlled backlog item and opens/updates a **draft PR**.
3. Existing GitHub Actions build/test on Windows and macOS hosted runners, with Linux unit/DSP checks where useful. Do not expose secrets to untrusted PR code.
4. Agent observes only the current-head CI result, retrieves redacted actionable diagnostics, performs bounded repairs and pushes updates; stale checks never authorise a merge.
5. After the separately approved automated merge policy and all required independent checks, the existing trusted release workflow alone can publish verified beta installers to GitHub Release/R2 and update the private beta website. Agent receives **no publishing credentials**.

## Requirements and investigation

- Read `AGENTS.md`, `CLAUDE.md`, AUTO-01 prompt, `tools/matrix/WORKER.md`, relevant ADRs, `.github/workflows/build.yml`, `.github/workflows/matrix-tests.yml`, and release workflows. Document the actual current capabilities and gaps; do not assume green CI implies DAW compatibility.
- Validate the selected non-root Linux container against isolation, maintainability, recovery, resource consumption, Claude/GitHub authentication, secret handling, screenshot validation and CI costs/latency; retain VM/Windows options as rollback comparisons only.
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

**Execution policy:** `scheduled` for implementation planning and validation; this does **not** enable live container execution or authorise unattended agent work. Operational activation requires validated isolation and independent approval.
