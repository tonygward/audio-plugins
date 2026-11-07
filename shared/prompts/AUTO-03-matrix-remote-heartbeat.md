---
id: AUTO-03
status: planned
execution: scheduled
priority: high
model: sonnet
effort: medium
dependencies: []
conflicts: []
---

# AUTO-03 — Matrix remote heartbeat and backlog observability

**Status:** Planned; backlog specification only. **Priority:** High — enable remote diagnostics while AUTO-01 remains in dry-run mode. **Recommended model:** Sonnet. **Effort:** Medium. **Dependency:** Existing [AUTO-01 infrastructure](AUTO-01-matrix-autonomous-development.md); heartbeat must be usable before live execution is enabled.

## Motivation
Matrix is running a Windows Task Scheduler job roughly every five minutes, but successful Windows task exit codes are not proof that it fetched the latest GitHub backlog. The latest local `status` report may reflect the last daily reconciliation (02:00 Europe/London); the optional GitHub `Notify Matrix backlog` workflow can be skipped. ChatGPT's connected GitHub integration cannot read local `C:/Scheduled/Songsmith-Matrix/state` files. Provide a safe GitHub-visible heartbeat so an owner or remotely connected assistant can tell **scheduler alive**, **new GitHub revision fetched**, **task queued**, and **development blocked** apart.

## Requirements
1. Keep the authoritative queue/state and full diagnostic logs local. Produce a **minimal, sanitised remote summary** including schema version, observed UTC time, last scheduler tick, last successful canonical fetch time, observed main commit SHA, mode (`dry-run` / live), pause flag, queue counts and **ID/state/execution mode** for relevant pending tasks (e.g. `EQ-38: pending, build-now`). Include an execution outcome and machine-readable blocker codes (`usage-threshold`, `unknown-usage`, `live-disabled`, `paused`, `fetch-failed`); do not misreport a dry-run as a failed live attempt.
2. Clearly distinguish *last scheduler tick* from *last successful GitHub fetch*. A successful Windows Task Scheduler exit status alone must never imply the canonical revision was refreshed. Include a stale/no-update threshold and make missing/expired heartbeats visibly **unknown/stale**, not `healthy`.
3. Choose a GitHub-readable destination that ChatGPT's existing GitHub connector can access, **preferably a private restricted repository or private workflow artifact** rather than a public status file. Investigate connector read capabilities before choosing. If the status must be public, require explicit approval and publish only a strict minimal allowlist of non-sensitive fields. Never push machine/user names, home paths, source contents, prompts, diagnostics, credentials, tokens, usage account details, or raw logs.
4. Use a dedicated narrowly scoped GitHub credential or GitHub App with access only to the heartbeat destination and the minimum write permission. No reuse of release/website/R2 PATs; keep credentials outside checkout and never expose them to AI agent tools, builds or logs. Bound HTTP retries/timeouts, deduplicate unchanged payloads and rate-limit publication (e.g. one material update and periodic liveness signal) to avoid unnecessary commits/actions/API calls.
5. Emit telemetry from the trusted scheduler/controller, not from untrusted backlog text or Claude. Preserve owner-controlled configuration and update deployment/runbook guidance. A heartbeat failure must not corrupt the queue, claim a prompt or block normal dry-run scanning. Report publication failures locally.
6. Provide a read-only `status` or equivalent view showing the exact remote destination/revision and publication outcome, plus the local status. Make the remote record easy to inspect using GitHub web/API and the connected GitHub app.
7. Reconcile the current five-minute timer with the daily GitHub fetch and optional notify trigger; document what each actually does and why `build-now` may remain pending until reconciliation. Do not enable unattended development, auto-merge, releases or remote inbound access as part of this change.

## Acceptance / verification
- Automated tests for heartbeat schema/allowlist, redaction (including paths/tokens), stale timestamps, throttling, failed auth/network, partial GitHub publication and idempotence; no secrets in mock outputs or test snapshots.
- Demonstrate a dry-run for a queued `build-now` prompt: the remote summary reports the **new** main SHA and `EQ-38: pending`, with an accurate usage-gate reason and `liveExecution=false`; no Claude coding invocation or attempt increment.
- Verify Task Scheduler firing alone does not advance the last-successful-fetch timestamp. A failed fetch retains the last known successful revision and flags staleness accurately.
- Document how Tony and ChatGPT can check it remotely, secure key rotation/revocation, and how to disable heartbeat without impacting local Matrix scanning.
- PR includes test evidence and a sample **synthetic** sanitised status record, no real identifying machine details.

## Grounding in Claude Code dynamic workflows (reviewed 7 November 2026)

**Primary reference:** [Anthropic — Orchestrate subagents at scale with dynamic workflows](https://code.claude.com/docs/en/workflows), especially "When to use a workflow", "Where the keyword works", "Manage runs", "When a run hits your usage limit", and "Behavior and limits".

The documented Claude workflow is a **JavaScript orchestration script for agents**, not a persistent Windows service, scheduler, durable inter-session task queue, or remote machine heartbeat. Its `/workflows` view shows progress **inside a Claude session**; a saved `.claude/workflows/` script and the session's workflow artifacts do **not** provide a GitHub-readable remote status endpoint. Do not implement AUTO-03 by launching Claude to report its own status.

- Heartbeat must be emitted by **trusted Matrix Node.js/Task Scheduler code** even if no Claude session exists, Claude is signed out, the 85% usage gate blocks new work, or live execution is disabled. Use no agent tokens to publish liveness.
- When AUTO-04 or another explicit Claude workflow is eventually enabled, add a **sanitised optional workflow lifecycle** field (`not-started/running/paused/failed/completed/unknown`, task ID, count, last observation) derived from the trusted controller's observations; distinguish it from the actual scheduler/fetch heartbeat. Do not copy workflow transcripts, agent prompts, session-local paths, token/account details, or raw `/workflows` output to GitHub.
- Anthropic documents that workflows can be resumed within a session, that results live in session directories, and that paused/restarted agents can rerun. Therefore remote status must be **observation-based and stale-aware**, never infer completion from a previously published `running` record.
- Anthropic's usage-limit auto-resume is only for qualifying **interactive subscription** workflows and explicitly excludes non-interactive `claude -p` / Agent SDK and background sessions. Never advertise automatic resumption after a quota reset for Matrix's scheduled worker.
- Keep the destination/privacy/credential requirements above. This reference informs observability semantics; it does **not** remove AUTO-01's deployment prerequisites or give permission to run Claude autonomously.

**Source validation at implementation:** Check the deployed Claude CLI version against the current documentation; test status publication on Matrix's actual Windows task, not a demo interactive workflow. The page describes workflow support in non-interactive mode but does not document a built-in remote heartbeat service.

## Delivery
Implement as an independent small infrastructure PR. Keep backlog prompt pending until merged, tested, and documented. No VERSION bump or release pipeline modification for this task.
