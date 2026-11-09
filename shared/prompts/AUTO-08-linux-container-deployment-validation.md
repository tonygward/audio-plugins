---
id: AUTO-08
status: planned
execution: deferred
priority: high
model: sonnet
effort: medium
dependencies: ["AUTO-07"]
conflicts: []
---

**Current policy:** [Board ordering, one task, 60% usage gate, automatic beta builds and verified issue completion](../../docs/songsmith-execution-policy.md). This policy supersedes historical batches, folder/ID ordering and merge-only completion in older design notes. Automatic approval/merge requires exact-head green CI; issue Done requires beta deployment/download verification and a final issue comment.

**GitHub tracking issue:** [#155](https://github.com/tonygward/audio-plugins/issues/155).

**Execution policy:** Follow [current selection and delivery rules](../../docs/songsmith-execution-policy.md). Board order decides selection regardless of prefix or issue/prompt location. This deferred task needs its appropriate scoped infrastructure execution path; board placement does not grant new privileges. One task only; 60% weekly usage stop threshold. Documentation does not activate a worker.


# AUTO-08 — Validate isolated Linux container deployment on Matrix

**Split from:** [AUTO-05](AUTO-05-linux-agent-github-actions.md). **Status:** Open: requires runtime evidence. **Code foundation:** [AUTO-07](done/AUTO-07-linux-ci-worker-code-foundation.md).

## Goal
Prove the existing Linux-container development controller is deployed and safely operational as a *controller*, while unattended coding remains disabled until approved. Do not reactivate legacy Windows accounts, Windows Task Scheduler worker or local PowerShell build broker.

## Work
1. Record version-pinned container image, non-root UID, effective capabilities, read-only trusted mounts, least-privilege writable state/worktree volumes, user/network namespace and host resource limits. Verify no privileged/host network, Docker socket, broad host/LAN mounts, Home Assistant secrets or publishing credentials. Test prohibited access **from inside the actual container**.
2. Verify container can fetch canonical GitHub main using narrowly scoped read access, persist/recover queue state, and access only allowed outbound endpoints through the configured proxy. Document where Claude subscription authentication and GitHub PR credentials reside and ensure they are inaccessible to untrusted worktrees/build code.
3. Prove the correct scheduled reconciliation, restart policy, stale-queue recovery, duplicate-claim prevention, explicit pause/kill switch, reboot persistence and clean rollback without affecting unrelated Matrix workloads.
4. Demonstrate a **dry-run** `build-now` item with current revision, selection reason, usage gate and no Claude coding invocation. Separate controller health from agent availability; AUTO-03 will supply remote heartbeat reporting.
5. Provide executable runbook with exact image/config references and deployment evidence stripped of identifying paths, tokens and secret values.

## Acceptance
- Repeatable dry-run under deployed non-root container; passed positive and negative isolation tests.
- No credentials leaked through `env`, logs, PRs, images or tests.
- Durable queue survives restart and does not duplicate the selected task.
- Existing hosted Windows/macOS GitHub Actions remain intact; no uncontrolled release.
- Evidence attached to PR and deployment runbook, then mark done.

**No flag changes or worker activation are authorised merely by this prompt.**
