---
id: AUTO-04
status: planned
execution: scheduled
priority: medium
model: sonnet
effort: medium
dependencies: ["AUTO-01"]
conflicts: []
---

## Current execution platform — Linux container and hosted CI (9 November 2026)

The test author, implementer and independent reviewer use **separate restricted agent contexts inside the Linux development container on Matrix**, coordinated by the trusted container controller. Each agent receives only the least-privilege worktree and pinned task inputs; do not expose Docker socket, host mounts, host credentials, other containers or release/website secrets. Use a single durable queue claim across agents.

**Red/green execution contract:** perform safe Linux-local unit/DSP checks if available and explicitly supported, but use **GitHub-hosted Windows/macOS Actions on pinned PR head SHAs** for native VST3/AU/installer builds and cross-platform regression tests. Test-writing agents must not receive GitHub workflow-editing or bypass rights. Passing Linux tests does not imply Windows/macOS success, and slow CI must be handled with durable checkpoints rather than unbounded agent polling.

The older instructions below referring to the separate Windows build account, Windows Claude CLI and Windows job queue are **legacy context only**. For current implementation, adapt `tools/matrix/linux-worker.mjs` and compare external stage orchestration against saved Claude workflows under Linux. No new agent launches from this backlog edit.

# AUTO-04 — Independent test-writing agent and test-first (TDD) workflow

**Status:** Planned only; do not activate or spawn a second agent yet. **Priority:** Medium — follow the safe AUTO-01 single-agent pilot. **Recommended model:** Sonnet for initial implementation; architecture/security review if adding a second trust boundary. **Effort:** Medium. **Dependency:** [AUTO-01](AUTO-01-matrix-autonomous-development.md) must support safe, bounded development and test execution first.

## Rationale
"It's important to get another agent to write the tests TDD style if you've got time." The intent is to avoid the implementing agent validating its own assumptions and to catch regressions earlier. **Independence matters more than running two agents in parallel**: a separate role/context should derive acceptance tests from the canonical backlog specification and existing code/contracts, without looking at the implementer's solution first.

## Scope / workflow
1. **Optional, budget-aware gate:** Enable only for suitable tasks when independent tests will materially improve confidence and both agent usage/budget limits allow. Retain a low-cost single-agent path for tiny UI/copy changes or when allowance is tight. The current 85% Claude weekly cutoff and paid-extra-usage prohibition apply independently before every agent invocation. No implicit paid API fallback. If no trustworthy current allowance is available, skip the optional agent rather than weakening AUTO-01.
2. **Test agent first (red):** Given a pinned prompt, acceptance criteria, existing public interfaces and tests, an independently scoped **test author** drafts targeted failing tests before implementation. Require measurable behaviour and boundary/negative cases; avoid brittle tests that only reproduce prose or implementation details. Run them against the unchanged base where safe to demonstrate the correct, expected failure. Distinguish genuine red tests from pre-existing failures or invalid tests; review or reject invalid test designs.
3. **Implementer second (green):** Pass the verified acceptance-test contract and relevant failing output to a separate implementation agent/context. The implementation agent changes production code to satisfy the tests, while preserving existing plugin/session compatibility, DSP and real-time audio constraints. It must not silently edit/disable/delete independently authored tests to obtain green results.
4. **Independent review/refactor:** Ask the test agent (or a fresh reviewer) to assess whether tests cover the original requirements and useful edge cases; allow narrowly reviewed test improvements. Run the full established Windows/macOS CI gates, audio numerical tests and regressions. Neither agent can self-approve unsafe merges, bypass tests, change release secrets or ship a release.
5. **Orchestration and isolation:** Use separate roles/contexts and a single durable task owner, with pinned source revision, prompt hash, bounded steps, retries, wall-time and resource budgets. The test writer should not inherit implementation-agent scratchpads or untrusted instructions from generated code. Limit filesystem access and agent tools consistently with AUTO-01; generated tests/build inputs remain untrusted executable code and must run only through the approved isolated Linux checks and trusted hosted CI build setup.
6. **Traceability:** Preserve test-author and implementer provenance (provider/model if allowed, task ID, prompt/base commit, red/green test evidence, costs/limits and reasons for skipping the independent role) in local structured logs and a concise PR summary. Do not publish private usage or account information.
7. **Provider-neutral design:** Do not assume the second role is a different vendor or that Codex fallback [AUTO-02](AUTO-02-codex-fallback.md) is active. Separate agent context can be the same permitted provider; support a second provider only after AUTO-02 permissions/allowance checks have independently passed.

## Important exceptions
- Pure visual changes such as [EQ-38](../../eq/prompts/38-frequency-guide-instrument-icons.md) may need screenshots, human UI inspection and rendering/layout checks; do not claim that TDD unit tests alone prove visual quality.
- For low-level audio DSP, require deterministic numerical/reference tests and real-time safety/performance validation, not merely mocked happy paths.
- If test-first work would exceed the allowed budget or delay a small task disproportionately, record `independent-tests-skipped` with reason and use the normal review/CI path. Do not treat that as a blocker unless the task is explicitly safety-critical.

## Acceptance criteria
- Demonstrate an eligible DSP or behaviour change with a reproducible red test against pre-change main, green after implementation and independent review of tests; preserve a known-good baseline.
- Confirm the implementer cannot mutate verified tests without a recorded review and that tests cannot modify protected files or bypass the restricted build launcher.
- Automated orchestration tests cover optional-role eligibility, 85% threshold/unknown usage, timeouts, invalid tests, agent failure, cancellation/retry, duplicate claims and cost caps.
- PR traces author/reviewer roles, test evidence and any skipped second-agent decision. Both usual CI and release-integrity gates remain mandatory; no automatic publication initiated by this backlog-only change.

## Grounding in Claude Code dynamic workflows (reviewed 7 November 2026)

**Primary reference:** [Anthropic — Orchestrate subagents at scale with dynamic workflows](https://code.claude.com/docs/en/workflows). Consult sections "When to use a workflow", "Where the keyword works", "Approve the plan before it runs", "Save the workflow for reuse", "What the saved script looks like", "Behavior and limits", "Resume after a pause", "When a run hits your usage limit" and "Cost".

**Architecture recommendation:** Evaluate a **small, explicit, saved project workflow** (`.claude/workflows/songsmith-tdd.js`) as an optional implementation primitive for this requirement, *not* blanket `/effort ultracode` or unconstrained parallel subagents. A Claude workflow is script-controlled orchestration: `agent()` runs one isolated role, `phase()` groups the display, and `parallel()`/`pipeline()` provide fan-out. We need a **sequential** red → green → review sequence, not simultaneous editing of the same tests. Use a source-controlled, human-reviewed script with stable phases, input `args` (pinned task ID, prompt SHA, base commit), strict structured per-role outputs and bounded time/cost. This is a **proposed design**, not a capability already activated in Matrix.

### Proposed staged workflow
1. **`phase("Red: independently author acceptance tests")`** — one test-author subagent sees only the canonical task, baseline code/interfaces and test conventions; returns structured test scope, files, expected failures and evidence. Do not show it the implementer's future patch.
2. **Trusted external gate:** The Matrix controller/build launcher verifies and runs the new tests on the unchanged base; confirm a meaningful red failure, not a compiler/configuration/pre-existing failure. The documented workflow script has **no direct filesystem or shell access**: its subagents use tools. AUTO-01's legacy Windows worker disallows shell/subagent tools and builds only through a separate Windows build account; the Linux worker similarly restricts agent tools and uses hosted CI, so **the existing permission/tool model must be evaluated and safely adapted** before claiming this can run. It may be better to orchestrate red/green via separate `claude -p` invocations in Matrix's trusted controller, using a workflow only for read-only review.
3. **`phase("Green: implement")`** — a separate implementer context receives the validated failing test contract, changes production source, and may not silently modify approved tests.
4. **`phase("Review: independent verification")`** — new reviewer/test-agent context checks requirement coverage, regressions and edge cases; independently run the fixed full test suite, and create a PR with traceable red/green evidence. Any authorised test changes require explicit recorded review. No agent receives release credentials or direct merge permission.

### Documented constraints that matter on Matrix
- **Never rely on `ultracode` keyword in a backlog prompt:** Anthropic says its opt-in keyword does not start workflows from `claude -p`, normal Agent SDK prompts, scheduled tasks, webhooks or PR comments. Explicitly invoke a saved workflow via a supported, permission-approved mechanism, or retain deterministic Matrix-side stage orchestration.
- **Non-interactive permissions:** In `claude -p` / Agent SDK the workflow has no interactive plan-approval prompt. Anthropic documents permission rules such as `Workflow(<name>)`, host callbacks and normal permission evaluation. The existing controlled worker uses `dontAsk` plus `Read,Edit,Write,Glob,Grep` without a Workflow tool permission. Adding Workflow is **a new capability requiring a security review**; do not enable `bypassPermissions` merely to get it running. Subagents inherit constrained tool rules, and code/test execution remains on the trusted build launcher.
- **Agent-count and credit discipline:** Anthropic warns multi-agent workflows consume substantially more tokens; default concurrency can be as high as 16 agents, with documented configurable caps. Use at most 2–3 agents *sequentially* initially, no unlimited `ultracode`, and fresh external usage checks before adding work. Do not presume a saved workflow internally enforces AUTO-01's 85% cutoff.
- **Resumption is not a durable Linux-container job queue:** Workflow progress can be resumed within a session, but restarted/failed downstream agents can repeat work. Non-interactive `claude -p` / SDK workflow runs do **not** automatically wait for a subscription quota reset in the way qualifying interactive workflows can. Matrix's persisted claims, Git SHAs, idempotency and explicit retries remain authoritative.
- **Script capabilities:** Workflow JS supports `agent()`, `phase()`, `parallel()`, `pipeline()`, `log()` and `args`, but no module imports or direct filesystem/shell. Do not put `Date.now()` or randomness inside the script; supply required timestamps/IDs through `args` for deterministic replay. Use `/workflow-authoring` to design/edit a saved workflow, and confirm the installed CLI version supports that skill (the guide says v2.1.248+).
- **Proof before rollout:** Verify documented workflow access with the deployed Linux-container Claude Code CLI and an explicit no-side-effect pilot. Record denial/failure behaviour when workflow permission is missing, and prove external Matrix ownership, test isolation and full CI remain intact. Claude's example scripts are illustrations, not a production-safe TDD recipe.

### Specific acceptance additions
- Compare three choices in a design note: (a) Matrix-orchestrated independent `claude -p` role invocations, (b) explicit saved Claude Code workflow, (c) ordinary subagents. Choose the smallest safe approach; no requirement to adopt dynamic workflows merely because they exist.
- Test no-agent/usage-blocked behaviour, workflow-permission denial, failed `agent()` returning `null`, stalled/retried agents, duplicate work avoidance and traceability of each stage. Show that failed or restarted workflow phases cannot create duplicate commits/PRs or silently mutate red tests.
- Document the opt-in and operational workflow commands (`/workflows`, saved `/<name>`) **only for supervised interactive usage**, and a verified explicit non-interactive invocation path **if** AUTO-01's permission model permits it.

**Documentation is a capability reference, not permission to enable multi-agent execution or override existing security controls.**

## Delivery
Implement as a separate PR once AUTO-01 live execution has been validated. Do not enable the extra agent merely by adding this prompt to the backlog.
