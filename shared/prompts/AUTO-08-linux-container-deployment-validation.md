---
id: AUTO-08
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

**Split from:** AUTO-05. **Status:** Open: requires runtime evidence. **Code foundation:** [AUTO-07](done/AUTO-07-linux-ci-worker-code-foundation.md).

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


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the prompt is deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This specification update does not activate Matrix or demonstrate operational board integration.
