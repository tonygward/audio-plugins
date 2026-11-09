---
id: AUTO-11
status: deferred
execution: deferred
priority: 1
model: sonnet
effort: medium
dependencies: ["AUTO-08", "AUTO-09"]
conflicts: []
---

# AUTO-11 — Matrix selects from the Songsmith board in manual card order

**Tracking issue:** [audio-plugins #175](https://github.com/tonygward/audio-plugins/issues/175)
**Board:** [Songsmith Development](https://github.com/users/tonygward/projects/2)
**Status:** Design and implementation prompt drafted; **deferred infrastructure work**. Do not run automatically as functional overnight work. No live execution is authorised by this file.

## Objective
Give the owner direct control of Matrix priorities by **dragging cards up and down in the GitHub Project's Todo column**. On an eligible overnight run, Matrix selects the first eligible Todo card **in the manually displayed top-to-bottom order**, reads its existing canonical Markdown implementation prompt and executes **at most one task**.

## Existing behaviour
Matrix selects prompt files using `tools/matrix/backlog.mjs`, ordered by `build-now`, priority, and task ID; the project board is currently a dashboard and not a worker input. The Linux worker config sets `batchLimit: 1` and `maximumPrompts: 1`.

## Proposed behaviour and implementation
1. Discover the private Songsmith Project #2 and read board cards, status and **manual ranking as displayed in the specific Board view**, using GitHub Projects v2's authorised API. **First test if this visible per-column order is actually exposed**; a generic GraphQL `items` result is not automatically equivalent. Verify by moving two Todo cards and inspecting the order.
2. **Todo:** ordered overnight queue. Select the highest visible, *eligible* card. **Doing:** owner-requested immediate task; serialise behind a currently active task. **Backlog/Done:** excluded. A Doing card without a Matrix Job means requested but not proof of execution.
3. Map each card to exactly one canonical prompt ID and path; do not treat implementation PR cards as independent jobs. On initial rollout support `audio-plugins` functional prompts only; match cross-repository issue IDs to their actual implementation scope, never blindly refuse solely because the issue is tracked in another repository if the canonical prompt belongs to audio-plugins.
4. Preserve allowlist excluding `AUTO-`, `WEB-` and unknown scopes from functional worker selection; check merged prompt presence, `done/` status, dependencies, Sonnet suitability, task claim/CI state, subscription allowance, non-root isolation, credential limits and existing release controls. Skip ineligible cards with clear reasons, then check the next eligible card. Never select two in one invocation, start parallel agents or weaken gates.
5. Atomically claim work in durable state. Add the actual Matrix Job identifier and a pickup comment once started. Reflect verified Doing on genuine worker pickup. **Done requires both** (a) verified PR merge and (b) successful beta release publication with the matching Windows and macOS setup/installer downloads accessible and verified on beta.songsmith.uk. Do not mark Done on merge alone. Keep Doing or explicitly release-pending when publication has not passed verification. On refused/failed work leave actionable diagnostics on the issue and move to Backlog as the owner specified, avoiding retry loops.
6. Poll the personal-account board on an independently configured interval, respecting GitHub rate limits, permissions and recoverability. Do not activate an unvalidated worker or give it release/Cloudflare secrets.
7. **Fail closed if manual order cannot be demonstrated through the API**. Do not claim drag order works or silently substitute API pagination order. Propose a numeric `Queue Order` project field for explicit owner approval; only then implement a deterministic alternative.
8. Before enabling, reconcile existing Doing cards and PRs (e.g. VER-01, SUI-15 from the issue history) to avoid duplicate execution.

## Acceptance criteria and automated tests
- Dragging A above B in Todo selects A first; reversing them selects B first, both in a verified live board snapshot.
- A blocked/ineligible top card is skipped with a clear reason and the next eligible card selected; one task maximum.
- Existing worker claim prevents duplicate execution even if a card remains Doing.
- Board access errors, uncertain manual ranks and stale/partial snapshots do not trigger work.
- Automatic issue comments and board status transitions happen only with verifiable states; failures return to Backlog with reasons, while merged work awaiting release remains Doing/release-pending.
- A merged implementation PR without updated installers on the beta website **must not** mark its card Done. After successful Windows and macOS builds, GitHub Release publication, private R2 transfer and Access-protected website update, verify the site presents the expected release and installer links resolve to the matching artifacts. Record PR, commit SHA, release/tag, installer artifact identities and verification evidence before Done.
- Failed, delayed, inaccessible or mismatched beta downloads block Done even if PR/CI succeeded; never create a second implementation job for work awaiting publication.
- Tests cover metadata mapping, ordering, safe filters, usage thresholds, API failures, duplicate cards and cross-repo issue links.
- CI and review pass. The end-to-end release gate has verified installers and website links; no release or deployment claims without separate verification.

## Dependencies and risk
AUTO-08 (deployed and validated Linux runtime), AUTO-09 (real end-to-end worker/CI pilot); GitHub Projects view ordering API capability and scoped credentials. The board is a remote code-execution control, so authentication, auditability, one-at-a-time claims and fail-closed behaviour are mandatory.

## Build, version and release
Infrastructure change only; do not perform this during scheduled functional runs. Implement on a branch with tests and PR, with human approval of any new trigger/credential scope. Preserve the current product version and existing automated release rules.
