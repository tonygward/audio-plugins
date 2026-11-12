
**Current policy:** [Board ordering, one task, 60% usage gate, automatic beta builds and verified issue completion](../../docs/songsmith-execution-policy.md). This policy supersedes historical batches, folder/ID ordering and merge-only completion in older design notes. Automatic approval/merge requires exact-head green CI; issue Done requires beta deployment/download verification and a final issue comment.
# AUTO-02 — Codex fallback when Claude allowance is exhausted

## Current execution platform — Linux container on Matrix (9 November 2026)

**Target platform:** Codex fallback, when separately authorised, runs inside the **same isolated, non-root Linux development container boundary** and canonical queue/claim ownership model as the primary Claude worker; it is not a second Windows desktop process. Verify the Linux Codex CLI authentication, licensing/usage telemetry, bounded `codex exec`, credential separation, egress policy, token redaction, no Docker socket and no host/release secrets. A provider switch must never bypass the container's preflight, the Claude/Codex independent usage gates or the persisted claims.

**Build handoff:** Agents edit branches/worktrees inside the container; **GitHub-hosted Windows/macOS Actions** perform native plugin/installer validation at the exact PR head SHA. An independently trusted CI/auto-merge path owns merges; verified release/R2/website workflows own distribution. No local Windows build account, Windows task or DPAPI credential is required for this feature.

This documentation change does not enable the fallback; hold its card back with the board's Execution value Deferred until it is intentionally promoted. Reconcile this story with AUTO-05 and the existing `tools/matrix/linux-worker.mjs` provider boundary at implementation time.


**Priority:** Future enhancement after AUTO-01 is operational and tested.  
**Dependency:** [AUTO-01 Matrix autonomous development orchestrator](AUTO-01-matrix-autonomous-development.md).  
**Recommended model:** Sonnet for implementation and tests; Opus only if agent orchestration/security architecture requires a new design decision.  
**Relative effort/usage:** Medium. No automatic release checkpoint or version assignment.

## Goal
Allow Matrix to switch from Claude Code to **OpenAI Codex CLI** when Claude has exhausted its weekly allowance or reaches the configured **60% stop threshold**, while using exactly the same GitHub Songsmith backlog, project standards, testing and CI/release pipeline.

This is a **backlog-only proposal**. Recording this requirement must not enable Codex, alter credentials, install an agent, schedule execution, change existing Claude policies, or trigger builds/releases.

## Requirements for later implementation
1. **Agent selection:** Claude Code remains preferred while an authoritative and permitted weekly usage measurement is below 60%. When it is >=60%, Claude is exhausted, or Claude is otherwise unavailable, consider Codex **only when Codex has been independently authenticated, explicitly enabled by Tony and verified to be within its own usage/budget constraints**.
2. **Fail closed:** Do not guess subscription allowance. If either provider's usage cannot be reliably established, apply its separately approved conservative policy; never silently fall back to separately billed API credits. If neither agent is safe and available, skip development and record why. A Claude usage-measurement failure must not be misrepresented as credit exhaustion.
3. **Shared instructions:** Treat root `AGENTS.md` and repository prompt/backlog rules as the common, provider-neutral contract. Claude-specific rules in `CLAUDE.md` must be harmonised with shared rules; Codex's normal instruction loading does not imply it reads both files. Explicitly review discrepancies without duplicating or contradicting instructions. Preserve existing prompt IDs and model recommendations, translating provider-specific model instructions only when authorised.
4. **Common toolchain:** Both agents must use the same JUCE/CMake compiler toolchain, isolated Linux container workspace, CI tests, PR templates, branch protection, release checkpoints, and traceability. Preserve root `VERSION`, installer format and the rule of exactly one Windows EXE and one macOS PKG per validated release.
5. **Automation interface:** Introduce a narrow provider adapter (Claude Code vs `codex exec`) for non-interactive jobs, return codes, logs, timeouts, cancellation, allowed filesystem/network access, token/usage caps and error reporting. Prevent simultaneous ownership of the same prompt. Recheck budget/availability between prompts.
6. **Security and costs:** Keep separate account credentials, secrets, permissions, rate limits and usage policies; never expose them in prompts, logs, CI output or pull requests. Never use unattended unrestricted shell/administrative privileges. Research current Codex CLI authentication, subscription-use terms, usage visibility and automation constraints at implementation time rather than assuming current behaviour persists.
7. **Fallback and recovery:** Track which agent handled each prompt, allow deliberate manual selection, prevent duplicate PRs or repeated work across provider switches, and keep blocked PRs for review. If a provider stops part-way through a task, preserve the workspace and explicit progress before reassignment; do not blindly resume incompatible sessions.
8. **Gatekeeping unchanged:** Neither agent may bypass required tests, protected PR merges, macOS CI, validated CI/integrity checks or WEB-01 verified release update boundaries. The trusted GitHub release workflow, not either agent, owns publication credentials.

## Implementation phases when explicitly scheduled
- Document the current provider capabilities and approved limits; review `AUTO-01` integration points.
- Build a dry-run provider selector and unit tests around the 60% boundary, unknown usage and dual-provider exhaustion.
- Enable Codex only after explicit approval and successful sandbox test; verify single-prompt development then 1-prompt batches.
- Record agent provenance and add operator pause/override instructions.

## Acceptance criteria
- Claude below 60% → select Claude when available; >=60% or exhausted → select enabled Codex only if its independent allowance/budget passes.
- Unknown Claude usage is not treated as confirmed exhaustion; any fallback on such a case requires an explicit configured policy.
- Codex exhausted or unknown, both providers unavailable, or policy disabled → no unattended coding.
- Same canonical backlog, shared instructions, build/tests, traceable PRs and independent release gates regardless of chosen provider.
- No duplicate prompt execution during restart, reassignment, or concurrent agent runs.
- No unapproved paid API consumption.
- Documentation and tests evidence precede activation.

**Do not implement AUTO-02 merely because this prompt exists.** It is intentionally excluded from AUTO-01's autonomous eligibility until Tony explicitly promotes it to an active task. Once later implemented and merged, move this same ID to `shared/prompts/done/` and update the roadmap traceability.


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the card's board Execution is Deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This specification update does not activate Matrix or demonstrate operational board integration.
