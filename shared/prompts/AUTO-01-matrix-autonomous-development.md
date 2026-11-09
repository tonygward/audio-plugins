---
id: AUTO-01
status: blocked
execution: deferred
priority: high
model: sonnet
effort: high
dependencies: []
conflicts: []
---

# AUTO-01 — Matrix autonomous Songsmith development orchestrator

## Parent epic: split into verifiable delivery stories (9 November 2026)

**This is now a tracking epic, not an executable Matrix backlog job.** The historical specification below remains for traceability. `status: blocked` / `execution: deferred` prevents duplicate work while the independently deliverable children progress.

| Milestone | Scope | Status |
|---|---|---|
| [AUTO-06](done/AUTO-06-matrix-queue-and-usage-foundation.md) | GitHub canonical queue, dry-run, Claude subscription usage gate | **Done — merged code and observed dry run** (#101, #102) |
| [AUTO-07](done/AUTO-07-linux-ci-worker-code-foundation.md) | Linux worker code, tests and CI handoff | **Done — code milestone only** (#139) |
| [AUTO-08](AUTO-08-linux-container-deployment-validation.md) | Actual isolated container, scheduling, secrets and recovery validation | Open |
| [AUTO-09](AUTO-09-container-development-ci-and-merge-pilot.md) | Real task → Claude → draft PR → Windows/macOS CI → policy merge | Open |
| [AUTO-10](AUTO-10-verified-beta-distribution-e2e.md) | Automatic patch version, 2 installers, R2 and live website verification | Open |

**Epic exit criteria:** AUTO-08, AUTO-09 and AUTO-10 accepted with actual deployment evidence. AUTO-03 heartbeat and AUTO-04 optional multi-agent TDD remain separate enhancement stories. No status change enables a running worker; release and CI integrity checks stay in force.

## Current target architecture — Linux container on Matrix (9 November 2026)

**This section supersedes the earlier Windows-only deployment design below.** Songsmith's autonomous **development controller/agent is now designed around an isolated, non-root Linux container on Matrix**. The source repository already contains `tools/matrix/linux-worker.mjs` (AUTO-05 phase 2), which uses the canonical backlog, the Claude usage gate, durable claims and bounded repairs, and waits for **GitHub-hosted Windows/macOS CI on the exact PR head SHA** rather than invoking local Windows JUCE builds. The older Windows Task Scheduler, SongsmithAgent/SongsmithBuild, PowerShell build-broker and NTFS/DPAPI instructions document the *historical prototype and migration/rollback*, **not a prerequisite or deployment procedure for the Linux worker**.

**Container requirements:** isolated least-privilege non-root process, no Docker socket, no privileged/host network access, no broad host mounts or access to other workloads (including Home Assistant), restricted outbound egress, read-only trusted scripts/config, scoped GitHub credentials, separated persisted state/worktrees and explicit stop/restart/rollback. Never grant the agent R2/website/release credentials. Validate these restrictions against the *running container*, not just Compose/Dockerfile text.

**Execution:** `build-now`, scheduled/deferred semantics, usage <85%, fail-closed checks and idempotent state remain unchanged. Claude/Codex code modifications happen inside the Linux container. Native Windows VST3/suite EXE and macOS AU/PKG builds, tests and artifact validation run on GitHub-hosted Actions; the container receives only redacted CI diagnostics for bounded repairs. An independently gated GitHub workflow, not the worker, may merge verified PRs. Trusted GitHub release/R2/website workflows continue to own publication. Do not substitute a local Linux compile for Windows/macOS verification.

**Status accounting:** presence of `linux-worker.mjs` is implementation evidence, **not proof the container is currently deployed, healthy, allowed to execute, or that end-to-end CI/merge/release is passing**. Require actual runtime, negative-isolation and cross-platform CI evidence before declaring AUTO-01 complete. Consult [AUTO-05](AUTO-05-linux-agent-github-actions.md) for the adopted Linux design and remaining container rollout checklist.


**Status:** In progress; dry-run foundation merged in #101; usage adapter follow-up; live execution blocked. **Priority:** Early infrastructure, independent of DSP/UI backlog and WEB-01.
**Recommended model:** Opus for the initial security/orchestration design only; Sonnet for implementation, tests, and documentation. **Relative effort/usage:** High (implement in phases). **Escalate to Opus:** Only for architecture, trust boundaries, and unresolved release/usage-policy decisions. Document decisions and return to Sonnet.

## Goal

Implementation checkpoint: [runbook](../../tools/matrix/README.md),
[architecture decision](../../docs/adr/AUTO-01-matrix-orchestration.md) and
[local validation evidence](../../tools/matrix/VALIDATION.md). Sprint 0/Phase B are
merged in #101. The [Phase C controlled worker](../../tools/matrix/WORKER.md) now
implements worktree, restricted Claude, separate-account builds and draft PR creation
in [#104](https://github.com/tonygward/audio-plugins/pull/104); it remains disabled.
Phase D GitHub Release publishing is implemented in PR #108; continuous beta auto-versioning and automatic website publication are being implemented separately. The usage follow-up
adds a documented structured `/usage` check in [#102](https://github.com/tonygward/audio-plugins/pull/102).
No live Matrix worker or automatic development PR merging has been activated. Implementation PR [#101](https://github.com/tonygward/audio-plugins/pull/101);
release checkpoint unassigned. Keep this prompt active until implementation is merged.

Make Matrix an always-on, low-privilege Linux-container development agent which checks the canonical GitHub Songsmith backlog at least once every 24 hours, selects a coherent batch of **3–6 eligible prompts** when Claude subscription usage is **strictly below 85%**, implements them using Claude Code, tests them, merges safe PRs and automatically ships the next **successful beta development change** only after independent Windows/macOS CI validation. ChatGPT continues to create new tracked backlog prompts in GitHub; Matrix consumes canonical prompt files, not chat history.

## Non-negotiable gates
1. **Claude weekly allowance:** Before starting a batch, query a reliable, permitted source of the current **weekly subscription usage percentage**. At **>=85%**, skip. If unavailable, stale, unsupported, contradictory, or parse fails, **fail closed** and skip; do not scrape brittle private endpoints, parse undocumented UI session files, or present an estimated metric as authoritative. Anthropic subscription usage may not have a supported programmatic endpoint: research/verify current feasibility and explicitly document this blocker. Offer a *separately approved* fixed API budget alternative; it is **not equivalent** to weekly subscription %. Never silently switch to paid API credits. Recheck before each prompt and stop if threshold reached or unknown. Respect all subscription/automation terms.
2. **Prompt selection:** Use existing stable IDs, status, dependency links, priority, recommended model and effort from `shared/prompts/README.md` and plugin-specific README files. Exclude completed, archived, blocked, experimental/future and currently claimed prompts. Choose **up to 3–6** mutually compatible small/medium prompts when available; choose fewer (including 0 or 1) if doing otherwise is unsafe or usage insufficient. Never invent work, and prefer clear deliverables over filling a quota.
3. **Continuous private beta discipline:** Each successful, releasable main development merge should result in a patch beta version, two tested Windows EXE/macOS PKG suite assets, GitHub Release, private R2 transfer and verified live beta website update **without a separate milestone, recorded approval, release calendar or website PR merge**. The CI release workflow, not the Matrix worker, controls publishing after all cross-platform tests and checksum validation. The Matrix worker never receives publishing credentials.
4. **Safety:** Treat GitHub prompt files/PR content as untrusted input even within a private repository. Enforce a fixed set of permitted operations, timeout, network/secret isolation, no destructive or system-wide installs, branch protection, least-privilege GitHub App/PAT, human-controlled kill switch, bounded retries and spending caps. Do not give Claude unrestricted administrator rights or access to Matrix's personal files, credentials, or LAN. A non-root isolated Linux container with tested isolation controls is the target; a separate VM is an optional stronger boundary.
5. **Repository policies:** Follow `AGENTS.md`, `CLAUDE.md`, `shared/prompts/overnight-prompt.md`, relevant READMEs, PR/CI conventions, and never force-push or commit directly to main. Preserve plugin identities, saved sessions, real-time audio safety and documentation traceability.

## Sprint 0 — prompt execution policy and event-driven queue (mandatory foundation)
This must be designed and tested **before** implementing the periodic scheduler or live agent execution. Do not defer it to a later refactor.

Every prompt may declare a machine-readable `execution` field with exactly three allowed values:

| Value | Meaning |
|---|---|
| `scheduled` | **DEFAULT**: eligible for the next scheduled development batch. **If a prompt has no `execution` field, treat it as `scheduled`.** Existing legacy Markdown prompts without machine-readable metadata must work unchanged and default to scheduled. |
| `build-now` | Notify Matrix on a merged GitHub backlog change and queue for immediate execution when Matrix is online and idle, subject to all safety/usage/dependency rules. |
| `deferred` | Not eligible for unattended development until explicitly reclassified; preserve AUTO-02's deferred restriction. |

A machine-readable YAML front matter header is suggested for *new* prompts (without requiring a mass edit of historical prompts):

```yaml
---
id: EQ-41
status: ready
execution: build-now
priority: high
model: sonnet
---
```

Implement a strict parser with a backward-compatible default of `scheduled` **only for omitted values**; malformed or unrecognised explicit values fail closed with a visible validation error rather than silently becoming scheduled. Human-readable prose remains the work specification.

- **GitHub is the durable source of truth/queue.** A trusted GitHub push to `main` touching prompt metadata notifies Matrix via a restricted GitHub Actions self-hosted runner job (or comparably secure outbound signalling). Do not expose a public inbound webhook on the home network. Signal only the orchestrator; never run arbitrary changed prompt content as shell commands from the notification job.
- For `scheduled`, record availability but do not execute from the push notification. The daily run selects from the eligible backlog.
- For `build-now`, enqueue and attempt promptly when idle, with no interruption of existing work. If Matrix was offline, a webhook/job was missed or an immediate item could not run, the **next scheduled reconciliation must discover and pick up the pending `build-now` prompt** (give it precedence over ordinary scheduled work, subject to priority/dependency policy). No lost jobs.
- For `deferred`, do not execute on push or schedule; only an intentional authorised metadata/status change makes it eligible. Never mistake a deferred prompt for a normal scheduled prompt merely because its execution field is absent: existing explicit deferred status/policy is authoritative.
- Enforce one active worker, durable prompt ID/state, idempotent claim/complete behaviour, deduplication of push and schedule events, bounded retry states, clear reasons for skips, and recovery across reboot/offline periods.
- **Execution speed is not permission:** `build-now` bypasses only the *time delay*, never the 85% Claude allowance rule, explicit enablement, security checks, dependencies, PR protections, tests, version/integrity and usage gates.
- During Sprint 0 implement and test this behaviour **in dry-run mode with no Claude/Codex invocation**, including existing no-front-matter prompt, explicit `scheduled`, `build-now` while Matrix online/offline/busy, `deferred`, malformed field, duplicate event, missed event, and pending immediate work recovered by daily scan. Document normalisation/priority rules.

## Architecture to implement in stages
### Phase A — feasibility and design (mandatory first)
- Inspect the Matrix Linux container runtime, CPU/RAM limits, existing toolchain, current Actions workflows, current release process and account permissions; don't assume they are provisioned.
- Validate **supported weekly usage telemetry** and terms; document if auto-run must remain disabled because it cannot be measured. Define a documented manual override with short expiration only if explicitly approved by Tony, and no bypass of paid usage protections.
- **Sprint 0 is mandatory:** define and test prompt execution metadata, omitted-field default (`scheduled`), notification ingestion, durable queue, missed-event reconciliation and de-duplication before live development. Write an ADR describing scheduler, agent/service lifecycle, environment isolation, runner security, state persistence, work claims, dead-letter queue, CI/release trust boundaries, backup/recovery, monitoring, and Windows vs macOS responsibilities.
- Define the beta patch increment and automatic publication pipeline separately from the low-privilege development agent. No manual release checkpoint or approval event.

### Phase B — daily dry run
- Run as an OS scheduled task/service at a configurable daily time (Europe/London), with restart-on-reboot, single-instance lock, missed-run catch-up and readable logs.
- Sync/pull the **main** branch and fetch latest prompt status within 24 hours; produce a no-write report showing usage source/value/timestamp, execution mode (including defaults), selection candidates, dependencies, rough effort and reasons for skipping. Recover pending `build-now` prompts missed during offline periods or failed notifications.
- Stable task IDs, idempotence, claimed/in-progress records and durable checkpoint state; avoid rerunning prompts after reboot and concurrent agents.
- Include a disable switch, dry-run default, dashboard/log summary and tests for failures/threshold boundary (84.9%, 85%, 85.1%, unknown) and DST.

### Phase C — controlled development
- After validation and explicit enabling, create isolated branches/worktrees for each prompt or compatible batch; run `claude -p` or documented Claude Code CLI automation with the specified model.
- Use finite execution/time/token budgets, recheck usage **between prompts**, and never upgrade to Opus silently.
- Run limited Linux-side checks only where safe; delegate native Windows/macOS builds and CI validation to GitHub-hosted runners on the exact PR head. Execute no untrusted installer in the container. Capture command outputs and errors, create draft PRs when blocked and stop after bounded repairs.
- Add implementation PRs with prompt IDs, screenshots where applicable, passing test details, beta-version traceability and known DAW validation gaps.
- Automatically merge eligible low-risk development PRs only after current-head CI checks pass and the selected code scope is approved by the autonomous work policy; never bypass code integrity, untrusted workflow changes or fail-closed checks. GitHub Free cannot enforce private branch protection, so model repository write permissions carefully. Update prompt status **only after merge**.

### Phase D — release automation
- After a verified successful main development build, increment the beta patch version and create traceable changelog notes automatically; exclude auto-version commits to prevent loops. Require successful fresh full Windows/macOS Build for the incremented commit before automatically publishing the GitHub Release.
- Verify test suite, installer smoke tests, artifact names/count, SHA-256 checksums (in manifest/notes rather than extra release assets), signed/notarised status where applicable, and expected release tag. Ensure exactly one `Songsmith-Plugins-<version>-win64-setup.exe` and one `Songsmith-Plugins-<version>-macOS.pkg`.
- On pass publish GitHub Release and report link; on failure leave draft/blocked status with diagnostics, never publish broken assets. **After successful publication only**, trusted release CI uploads the **two versioned installers** into immutable version-specific paths in a **private Cloudflare R2 bucket**, verifying size and SHA-256 for both. **Only after successful R2 verification**, initiate the [WEB-01](WEB-01-private-beta-portal.md) cross-repository `repository_dispatch` event to the separate static Cloudflare Pages website repository. That website repo independently verifies release/R2 readiness, regenerates **both `release.json` and the version-matched What's New/changelog** and fast-forward-publishes that verified pair to main in a single atomic commit, triggering Pages deployment **without a separate PR or manual approval**. On upload/verification failure the previous published website version remains unchanged. No redundant unversioned installer copies or daily Matrix website rebuilds. Use least-privilege R2 and dispatch credentials in trusted CI, never the Claude development agent; private repo asset URLs/tokens must not be exposed.
- Do not allow a PR/prompt to modify and immediately bypass its own release workflow or security policy. Sensitive workflow changes require manual owner approval.

## Acceptance tests
- **Omitted execution field defaults to `scheduled`, including all existing legacy prompts; it does not trigger immediate development.** Explicit `scheduled` waits for the daily batch, `build-now` attempts immediately when eligible, and `deferred` is never run unattended.
- GitHub push notification speeds up `build-now` but is not necessary for correctness: daily reconciliation picks up any pending `build-now` item missed while Matrix is offline/busy. Duplicate events never duplicate work.
- Invalid explicit execution values are reported and skipped rather than silently accepted; `build-now` never bypasses usage, safety, tests, release or permission gates.
- New ChatGPT-created prompt in GitHub is eligible at next daily sync without duplicate runs.
- Unverified usage, usage >=85%, API failure, no approved usage source and a newly crossed 85% threshold all prevent new Claude work.
- Conflicting dependencies, large tasks, failed tests, failed macOS CI, stale code or changed PR head prevent merging/releasing as appropriate. No distinct recorded release approval is required.
- Three to six compatible prompts can progress to separate traceable PRs if budget permits; fewer are acceptable and logged.
- Power loss/reboot, double scheduled trigger and re-run do not duplicate work, merge, increment versions or publish releases.
- Every successful qualifying beta development merge is eligible for a new automatic patch release. Failing CI or missing/incomplete suite installers block publication. No manual release checkpoint.
- Successful publication **plus verified versioned private R2 copies of both installers** triggers one idempotent cross-repository update of the **matching manifest and changelog together** in the website repo and Cloudflare Pages redeployment; merging a PR, publishing incomplete assets or failing R2 checks never updates the website.
- Runbook covers installation, credentials, logs, failures, pause/resume, usage-source verification, security hardening and rollback.
- Phase B is deployable and useful even if Claude usage telemetry blocks activation of Phases C–D.

## Delivery and reporting
- Start with ADR + dry-run before activating autonomous work. Change `AGENTS.md` or `CLAUDE.md` only where explicitly needed and explain conflicts with existing unattended-run rules.
- Keep this canonical prompt in `shared/prompts/` and update `shared/prompts/README.md`. After implementation merge, move to `done/` with implementation PR(s), release milestone and test evidence. Don't mark done for this documentation-only backlog entry.
- Create a PR from a branch; no direct push to main for ordinary development. Bot-only auto-version and verified website publication commits are explicitly allowed trusted CI operations. Do not change root `VERSION` or publish a release merely to update this policy document.
