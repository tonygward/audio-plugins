# AUTO-01 — Matrix autonomous Songsmith development orchestrator

**Status:** Planned, not implemented. **Priority:** Early infrastructure, independent of DSP/UI backlog and WEB-01.  
**Recommended model:** Opus for the initial security/orchestration design only; Sonnet for implementation, tests, and documentation. **Relative effort/usage:** High (implement in phases). **Escalate to Opus:** Only for architecture, trust boundaries, and unresolved release/usage-policy decisions. Document decisions and return to Sonnet.

## Goal
Make Matrix an always-on, low-privilege Windows development agent which checks the canonical GitHub Songsmith backlog at least once every 24 hours, selects a coherent batch of **3–6 eligible prompts** when Claude subscription usage is **strictly below 85%**, implements them using Claude Code, tests them, merges safe PRs and automatically publishes an **agreed release checkpoint** only after independent Windows/macOS CI validation. ChatGPT continues to create new tracked backlog prompts in GitHub; Matrix consumes canonical prompt files, not chat history.

## Non-negotiable gates
1. **Claude weekly allowance:** Before starting a batch, query a reliable, permitted source of the current **weekly subscription usage percentage**. At **>=85%**, skip. If unavailable, stale, unsupported, contradictory, or parse fails, **fail closed** and skip; do not scrape brittle private endpoints, parse undocumented UI session files, or present an estimated metric as authoritative. Anthropic subscription usage may not have a supported programmatic endpoint: research/verify current feasibility and explicitly document this blocker. Offer a *separately approved* fixed API budget alternative; it is **not equivalent** to weekly subscription %. Never silently switch to paid API credits. Recheck before each prompt and stop if threshold reached or unknown. Respect all subscription/automation terms.
2. **Prompt selection:** Use existing stable IDs, status, dependency links, priority, recommended model and effort from `shared/prompts/README.md` and plugin-specific README files. Exclude completed, archived, blocked, experimental/future and currently claimed prompts. Choose **up to 3–6** mutually compatible small/medium prompts when available; choose fewer (including 0 or 1) if doing otherwise is unsafe or usage insufficient. Never invent work, and prefer clear deliverables over filling a quota.
3. **Release discipline:** A successful prompt merge is **not** automatically a release. Respect root `VERSION`, versioning rules, agreed release checkpoints, tests and exactly **two release assets** (one Windows EXE and one macOS PKG). Only the isolated, authorised release workflow publishes after required checks pass on both platforms; no unpublished CI artifact or speculative build gets promoted. The orchestrator must not have release credentials.
4. **Safety:** Treat GitHub prompt files/PR content as untrusted input even within a private repository. Enforce a fixed set of permitted operations, timeout, network/secret isolation, no destructive or system-wide installs, branch protection, least-privilege GitHub App/PAT, human-controlled kill switch, bounded retries and spending caps. Do not give Claude unrestricted administrator rights or access to Matrix's personal files, credentials, or LAN. A separate isolated VM/agent account is preferred.
5. **Repository policies:** Follow `AGENTS.md`, `CLAUDE.md`, `shared/prompts/overnight-prompt.md`, relevant READMEs, PR/CI conventions, and never force-push or commit directly to main. Preserve plugin identities, saved sessions, real-time audio safety and documentation traceability.

## Architecture to implement in stages
### Phase A — feasibility and design (mandatory first)
- Inspect Matrix's actual OS, CPU/RAM, existing toolchain, current Actions workflows, current release process and account permissions; don't assume they are provisioned.
- Validate **supported weekly usage telemetry** and terms; document if auto-run must remain disabled because it cannot be measured. Define a documented manual override with short expiration only if explicitly approved by Tony, and no bypass of paid usage protections.
- Write an ADR describing scheduler, agent/service lifecycle, environment isolation, runner security, state persistence, work claims, dead-letter queue, CI/release trust boundaries, backup/recovery, monitoring, and Windows vs macOS responsibilities.
- Define release checkpoints and approval contract separately from prompt completion.

### Phase B — daily dry run
- Run as an OS scheduled task/service at a configurable daily time (Europe/London), with restart-on-reboot, single-instance lock, missed-run catch-up and readable logs.
- Sync/pull the **main** branch and fetch latest prompt status within 24 hours; produce a no-write report showing usage source/value/timestamp, selection candidates, dependencies, rough effort and reasons for skipping.
- Stable task IDs, idempotence, claimed/in-progress records and durable checkpoint state; avoid rerunning prompts after reboot and concurrent agents.
- Include a disable switch, dry-run default, dashboard/log summary and tests for failures/threshold boundary (84.9%, 85%, 85.1%, unknown) and DST.

### Phase C — controlled development
- After validation and explicit enabling, create isolated branches/worktrees for each prompt or compatible batch; run `claude -p` or documented Claude Code CLI automation with the specified model.
- Use finite execution/time/token budgets, recheck usage **between prompts**, and never upgrade to Opus silently.
- Compile/test on Matrix only in sandboxed build directories; execute no untrusted installer. Capture command outputs and errors, create draft PRs when blocked and stop after bounded repairs.
- Add implementation PRs with prompt IDs, screenshots where applicable, passing test details, release-checkpoint mapping, and known DAW validation gaps.
- Merge only through protected GitHub PR rules after required checks; no self-approval bypass. Update prompt locations/status and traceability **only after merge**.

### Phase D — release automation
- Use separate GitHub CI release workflow with explicit milestone marker/approved release scope, macOS hosted CI, Windows CI (self-hosted Matrix only if safe/isolated) and version validation.
- Verify test suite, installer smoke tests, artifact names/count, SHA-256 checksums (in manifest/notes rather than extra release assets), signed/notarised status where applicable, and expected release tag. Ensure exactly one `Songsmith-Plugins-<version>-win64-setup.exe` and one `Songsmith-Plugins-<version>-macOS.pkg`.
- On pass publish GitHub Release and report link; on failure leave draft/blocked status with diagnostics, never publish broken assets. Integrate future WEB-01 protected portal without publicising private repository download URLs.
- Do not allow a PR/prompt to modify and immediately bypass its own release workflow or security policy. Sensitive workflow changes require manual owner approval.

## Acceptance tests
- New ChatGPT-created prompt in GitHub is eligible at next daily sync without duplicate runs.
- Unverified usage, usage >=85%, API failure, no approved usage source and a newly crossed 85% threshold all prevent new Claude work.
- Conflicting dependencies, large task, failed tests, failed macOS CI, stale code, missing release approval or changed PR head prevent merging/releasing as appropriate.
- Three to six compatible prompts can progress to separate traceable PRs if budget permits; fewer are acceptable and logged.
- Power loss/reboot, double scheduled trigger and re-run do not duplicate work, merge, increment versions or publish releases.
- Each release is a validated checkpoint with exactly two installers; no automatic publication merely because a batch finished.
- Runbook covers installation, credentials, logs, failures, pause/resume, usage-source verification, security hardening and rollback.
- Phase B is deployable and useful even if Claude usage telemetry blocks activation of Phases C–D.

## Delivery and reporting
- Start with ADR + dry-run before activating autonomous work. Change `AGENTS.md` or `CLAUDE.md` only where explicitly needed and explain conflicts with existing unattended-run rules.
- Keep this canonical prompt in `shared/prompts/` and update `shared/prompts/README.md`. After implementation merge, move to `done/` with implementation PR(s), release milestone and test evidence. Don't mark done for this documentation-only backlog entry.
- Create a PR from a branch; no direct push to main. Docs-only PR may merge under repository rules. Do not change root `VERSION` or publish a release as part of adding this prompt.
