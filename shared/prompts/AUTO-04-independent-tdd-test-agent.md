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

# AUTO-04 — Independent test-writing agent and test-first (TDD) workflow

**Status:** Planned only; do not activate or spawn a second agent yet. **Priority:** Medium — follow the safe AUTO-01 single-agent pilot. **Recommended model:** Sonnet for initial implementation; architecture/security review if adding a second trust boundary. **Effort:** Medium. **Dependency:** [AUTO-01](AUTO-01-matrix-autonomous-development.md) must support safe, bounded development and test execution first.

## Rationale
"It's important to get another agent to write the tests TDD style if you've got time." The intent is to avoid the implementing agent validating its own assumptions and to catch regressions earlier. **Independence matters more than running two agents in parallel**: a separate role/context should derive acceptance tests from the canonical backlog specification and existing code/contracts, without looking at the implementer's solution first.

## Scope / workflow
1. **Optional, budget-aware gate:** Enable only for suitable tasks when independent tests will materially improve confidence and both agent usage/budget limits allow. Retain a low-cost single-agent path for tiny UI/copy changes or when allowance is tight. The current 85% Claude weekly cutoff and paid-extra-usage prohibition apply independently before every agent invocation. No implicit paid API fallback. If no trustworthy current allowance is available, skip the optional agent rather than weakening AUTO-01.
2. **Test agent first (red):** Given a pinned prompt, acceptance criteria, existing public interfaces and tests, an independently scoped **test author** drafts targeted failing tests before implementation. Require measurable behaviour and boundary/negative cases; avoid brittle tests that only reproduce prose or implementation details. Run them against the unchanged base where safe to demonstrate the correct, expected failure. Distinguish genuine red tests from pre-existing failures or invalid tests; review or reject invalid test designs.
3. **Implementer second (green):** Pass the verified acceptance-test contract and relevant failing output to a separate implementation agent/context. The implementation agent changes production code to satisfy the tests, while preserving existing plugin/session compatibility, DSP and real-time audio constraints. It must not silently edit/disable/delete independently authored tests to obtain green results.
4. **Independent review/refactor:** Ask the test agent (or a fresh reviewer) to assess whether tests cover the original requirements and useful edge cases; allow narrowly reviewed test improvements. Run the full established Windows/macOS CI gates, audio numerical tests and regressions. Neither agent can self-approve unsafe merges, bypass tests, change release secrets or ship a release.
5. **Orchestration and isolation:** Use separate roles/contexts and a single durable task owner, with pinned source revision, prompt hash, bounded steps, retries, wall-time and resource budgets. The test writer should not inherit implementation-agent scratchpads or untrusted instructions from generated code. Limit filesystem access and agent tools consistently with AUTO-01; generated tests/build inputs remain untrusted executable code and must run only inside the already approved isolated build setup.
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

## Delivery
Implement as a separate PR once AUTO-01 live execution has been validated. Do not enable the extra agent merely by adding this prompt to the backlog.
