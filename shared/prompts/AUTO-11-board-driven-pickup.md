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

# AUTO-11 — Board-ordered Matrix pickup and verified delivery lifecycle

**Tracking issue:** [audio-plugins #175](https://github.com/tonygward/audio-plugins/issues/175)
**Board:** [Songsmith Development](https://github.com/users/tonygward/projects/2)
**Status:** Implementation specification; not yet verified as a live board reader. Infrastructure implementation remains separately assigned.
**Model:** Sonnet; escalate to Opus only for unresolved API ordering or isolation architecture, document why and return to Sonnet.
**Policy:** [Current execution and delivery policy](../../docs/songsmith-execution-policy.md).

## Objective and existing behaviour
Let the owner choose the next task by dragging cards into order on the board. The existing worker reads Markdown via tools/matrix/backlog.mjs; do not claim this feature is operational before implementing and testing the board integration.

## Required behaviour
1. Read the specific board view's visible manual top-to-bottom order. Todo supplies overnight work; Doing requests immediate execution after any active task. Backlog/Done are excluded. Select the topmost eligible card; ordering never comes from prompt priority, task ID, issue number or repository.
2. Verify API ordering with real A/B drag-and-drop tests. Generic GraphQL pagination order is not proof. If manual view rank cannot be read reliably, stop board selection and report the limitation. No fallback ordering; propose a numeric Queue Order field only for an explicit owner decision.
3. Resolve each card to exactly one canonical merged prompt, stable task ID, tracking issue and actual implementation repository. Never refuse a card because of issue repository, prompt location or prefix (AUTO-, WEB-, EQ-, etc.). Ignore implementation PR cards as separate jobs. Scope execution to the actual task, not the issue's location.
4. Check dependencies, merged prompt, existing claims/PRs, status and execution permissions. Infrastructure and website work use the appropriate isolated execution path with scoped credentials. The functional worker must not gain orchestration, secret or release privileges. If a prerequisite fails, record the real reason and consider the next eligible card; never cite location as the reason.
5. Enforce batchLimit: 1, maximumPrompts: 1 and one active job globally. Stop new pickup at weekly Claude usage >=60%, including scoped meters, or unknown/stale usage. A global usage gate stops the invocation; do not walk down the queue to bypass it.
6. Atomically persist the claim. On actual pickup comment on the issue and record Matrix Job, prompt revision and repository. Publish meaningful progress, PR, CI, failure and release-blocker updates on that same issue.
7. Automatically approve and merge only after exact-head required tests and hosted Windows/macOS CI pass through the trusted merge workflow. No routine manual approval for authorised beta work. Retain the independent credential and integrity boundaries.
8. Trusted automation increments the beta build/patch counter, publishes the two suite installers, verifies private R2, and deploys matching metadata/changelog and downloads to beta.songsmith.uk. Major/minor changes are deferred.
9. Keep merged work open in Doing/release-pending until beta delivery is verified. Then post the final issue comment with PR, SHA, build/version, release, deployment and authenticated installer verification evidence; close the issue as completed and finally set the card Done. A failed final comment blocks Done. Never recode merged work while waiting for publication.
10. Poll the personal project with bounded retries, stale-snapshot detection and durable recovery. Reconcile all existing Doing cards against merged/open PRs and releases before enabling; do not rely on stale prose in older issues. Failed/refused work gets an issue comment and recoverable state without retry loops.

## Acceptance and tests
- Moving A above B selects A; reversing their visible order selects B, verified against the actual board view.
- Cards tracked in audio-plugins, songsmithstudio and songsmith-website are considered in board order. Prefix or location never excludes them; tests independently check actual execution permissions.
- At most one task is claimed; concurrent invocations/restarts/duplicate cards cannot create a second job.
- Blocked top card records its actual reason before considering the next eligible card. Missing ordering, API errors or stale/partial snapshots trigger no work.
- Verified weekly usage 59.9% permits pickup; 60.0% and higher, including scoped meters, and unknown usage block it.
- Issue comments cover genuine pickup, progress, failure, merge, deployment blockers and completion.
- Current-head green CI permits automatic approval/merge. Missing, failed or stale checks prevent it.
- A merged PR with absent, stale, inaccessible or mismatched beta installers cannot close its issue or mark the board Done.
- Successful release/R2/site deployment and authenticated Windows/macOS downloads are recorded; final issue comment precedes close and board Done.
- Publication failure preserves the merged implementation and release-pending state without duplicate coding.

## Dependencies, risks and delivery
AUTO-08 validates the runtime; AUTO-09 validates development/CI/merge. Validate scoped board read/write access, actual API ordering and recovery before rollout. Keep board updates and release credentials in trusted components. Implement on a branch with tests and PR; retain the product VERSION for this infrastructure specification change. Documentation is not activation.
