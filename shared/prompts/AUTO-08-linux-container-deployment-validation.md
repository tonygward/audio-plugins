---
id: AUTO-08
priority: high
model: sonnet
effort: medium
dependencies: ["AUTO-07"]
conflicts: []
---

**GitHub tracking issue:** [#155](https://github.com/tonygward/audio-plugins/issues/155).

# AUTO-08 — Validate isolated Linux container deployment on Matrix

**Split from:** AUTO-05. It requires runtime evidence. **Code foundation:** [AUTO-07](done/AUTO-07-linux-ci-worker-code-foundation.md).

## Goal
Prove the existing Linux-container development controller is deployed and safely operational as a *controller*, while unattended coding remains disabled until approved. Do not reactivate legacy Windows accounts, Windows Task Scheduler worker or local PowerShell build broker.

## Work
1. Record version-pinned container image, non-root UID, effective capabilities, read-only trusted mounts, least-privilege writable state/worktree volumes, user/network namespace and host resource limits. Verify no privileged/host network, Docker socket, broad host/LAN mounts, Home Assistant secrets or publishing credentials. Test prohibited access **from inside the actual container**.
2. Verify container can fetch canonical GitHub main using narrowly scoped read access, persist/recover queue state, and access only allowed outbound endpoints through the configured proxy. Document where Claude subscription authentication and GitHub PR credentials reside and ensure they are inaccessible to untrusted worktrees/build code.
3. Prove the correct scheduled reconciliation, restart policy, stale-queue recovery, duplicate-claim prevention, explicit pause/kill switch, reboot persistence and clean rollback without affecting unrelated Matrix workloads.
4. Demonstrate a **dry-run** `build-now` item with current revision, selection reason, usage gate and no Claude coding invocation. Separate controller health from agent availability.
5. Provide executable runbook with exact image/config references and deployment evidence stripped of identifying paths, tokens and secret values.

## Acceptance
- Repeatable dry-run under deployed non-root container; passed positive and negative isolation tests.
- No credentials leaked through `env`, logs, PRs, images or tests.
- Durable queue survives restart and does not duplicate the selected task.
- Existing hosted Windows/macOS GitHub Actions remain intact; no uncontrolled release.
- Evidence attached to PR and deployment runbook, then mark done.

**No flag changes or worker activation are authorised merely by this prompt.**

## Delivery

Follow the [execution policy](../../docs/songsmith-execution-policy.md). The board decides when this runs; Sonnet is the default model.
