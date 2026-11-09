# Songsmith execution and delivery policy

Owner decisions: 9 November 2026. This is the current policy for issues, prompts, Matrix and release documentation.

## Selection and limits
- The Songsmith Development board supplies execution intent and ordering: Todo is the overnight queue in its visible manual top-to-bottom order; Doing requests immediate work, serialised behind an active job. Backlog and Done are excluded.
- Never reject or reorder a card because of its ID prefix, issue repository or canonical prompt location. Resolve its unique issue, prompt and actual implementation repository. Board order is the deciding ordering factor.
- Validate the specific board view's manual order through the API. If unavailable, fail closed and report it; never silently substitute priority, issue number, alphabetical ID or generic pagination order. A numeric Queue Order alternative requires the owner's separate decision.
- Run one task per invocation and one active task total: batchLimit = 1 and maximumPrompts = 1. No multi-task batches.
- Stop new agent work at or above 60% verified weekly Claude usage, including scoped weekly meters; unknown, stale or invalid usage also blocks pickup. Recheck before pickup. No paid-usage fallback.
- Dependencies, existing claims and scoped execution permissions still apply. Route infrastructure and website work to the proper isolated execution path; do not grant the functional agent orchestration or release credentials. Location is never the refusal reason; record the actual unmet prerequisite.
- AUTO-11 implements the board reader. Until its implementation and live order test are verified, do not describe the existing Markdown selector as board-driven. Documentation changes do not activate it.

## Pull requests and beta versions
- Automatically approve and merge policy-compliant PRs after current-head required tests and hosted Windows/macOS CI pass. Record the approval checks in a comment where GitHub prevents self-approval. No routine manual approval step for authorised beta work; never bypass a failed or missing check.
- Automatically increment the third component of the shared beta VERSION for each successful qualifying development build/merge on main (0.5.2 → 0.5.3 → 0.5.4). This is the current beta build counter using the existing patch-version pipeline. Keep CI run IDs and commit SHAs as provenance.
- Major and minor increments are deferred. Do not choose a new major/minor milestone or reset VERSION without a new owner decision.
- PR-only builds, retries and documentation/prompt-only changes do not create extra releases. Prevent recursive bumps and races. Keep historic published tags/assets immutable.
- Trusted workflows publish exactly one Windows EXE suite installer and one macOS PKG suite installer, verify private R2 objects and update/deploy beta.songsmith.uk. The coding agent has no publishing credentials.

## Issue updates and Done
1. On genuine pickup, comment on the tracking issue with task ID, Matrix job/run, prompt revision and implementation repository; record the claim. A Doing card alone is not pickup evidence.
2. Comment on material progress, PR/CI outcomes, failures, timeouts and release blockers. Include actionable reasons and next steps; avoid duplicate heartbeat comments.
3. After automatic approval and merge, comment with PR, merge SHA and CI evidence. Keep the issue open and card Doing/release-pending while delivery is unverified.
4. Verify the matching beta release, Windows/macOS installers, private R2 integrity, deployed website version/changelog and authenticated download routes. A green build or manifest change alone is insufficient.
5. Post the final completion comment with PR, commit, release/build version, deployment and installer/download verification evidence. Only after that comment succeeds may automation close the issue as completed and finally set the board card to Done.
6. Failure at any stage leaves the issue open with a failure/blocker comment. Preserve merged work awaiting publication; do not start a second implementation job. Unclaimed refused cards may return to Backlog with the actual reason.

A prompt archived under done/ records merged implementation and prevents repeat coding; it is not by itself delivery completion. Keep explicit merged, release-pending and deployed evidence in the prompt/index/issue. Dependencies requiring delivery must check deployment, not just the archive path.
