# Songsmith execution and delivery policy

Owner decisions: 10 November 2026; Doing amended the same day. Applies to issues, prompts, Matrix and release documentation. Supersedes older merge-only completion instructions.

## Board and selection
- Backlog: uncommitted/prioritised work. Todo: the overnight queue, in the board view's visible manual order; Matrix takes **one Todo task per London day**, on the first run after `dailyTime` (02:00), and a Todo pickup uses up that day's slot. Doing pickups never use it. Retain the existing Todo name; Ready is its meaning, not a second queue.
- Doing: work to run now, and work Matrix is running. A Doing card with no claim is a run-now request: Matrix picks it up at the worker's next run (every few minutes), ahead of Todo, in board order, once no job is active and every gate (usage, dependencies, scoped path) passes. Pickup creates the durable job and the starting issue comment. A card whose board Execution is **Separate** is scheduled outside the worker and is never picked up. A **deferred** card is never picked up. Defer it with the board's **Execution** field set to Deferred (Run Now or Scheduled releases it, even if the prompt says deferred); with any other value the prompt decides (`status: deferred` or `execution: deferred`). A deferred card stays where it is and does not block other work. A Doing card with an open or merged pull request resumes from that evidence and is never recoded.
- Needs attention: limit reached, blocked, failed checks after bounded repairs, or delivery stalled. Issue stays open. No automatic requeue on each poll; an explicit move back to Todo or Doing resumes saved work.
- Done: all task-specific acceptance and delivery verified, final evidence comment posted, issue closed, then card marked Done.
- Board order decides selection regardless of prefix, issue repository or prompt location. Resolve the unique issue, merged canonical prompt and actual implementation repository. Parent epics/historical batch trackers are not separate execution jobs.
- Verify actual view order through API and real A/B reordering; fail closed on unavailable rank, partial/stale snapshots or API errors. No priority/ID/pagination fallback. A numeric queue-order alternative requires owner decision.
- AUTO-11 owns board configuration, selection and atomic claims; AUTO-12 owns the bounded task loop; AUTO-10 owns verified delivery and finalisation. Documentation is not evidence these integrations run.
- Before rollout, reconcile all existing Doing cards with job/PR/release evidence; an unclaimed Doing card is a run-now request and stays, a deferred one is left alone, interrupted/blocked work goes to Needs attention with reason. Preserve board ordering and never recode merged work.

## Execution and limits
- Matrix schedules/claims one task; standard Claude agentic loop owns implementation, tests, review, merge orchestration and delivery verification through trusted workflows. Reuse the existing worker, not another orchestrator.
- One task per invocation and one active job globally; batchLimit = maximumPrompts = 1.
- Start only below 60% verified weekly Claude usage, including scoped meters; unknown/stale/invalid readings block pickup. Recheck before further Claude invocations; no paid fallback.
- Configurable initial defaults: 120 minutes elapsed/run and three repair iterations total across implementation/review/CI. Record a hard configured Claude turn/token budget at start. The first exhausted limit stops further agent work; CI/publication waiting counts in elapsed time. Persist counters across crash recovery within the run.
- At a limit or blocker, checkpoint job, prompt revision, branch, SHA, PR, checks, elapsed/repair/usage counters and delivery stage. Comment with reason/limit, completed work, remaining work and precise resume action; move to Needs attention and leave open.
- Resume reconciles existing work and starts from its saved stage. Never duplicate a PR, merge, version bump or release. A crash/stale lease is not permission for duplicate work.
- Use scoped infrastructure/website execution paths and credentials. Board placement never grants privileges; location/prefix alone never rejects a card. Infrastructure work remains separately scheduled; overnight work prioritises functional changes.

## Pull requests and beta versions
- Automatically approve/merge authorised work only after exact-head required checks and relevant native Windows/macOS CI pass. Record approval checks where GitHub prevents self-approval. No routine manual gate; no bypass of failed/missing/stale checks. Changes to Matrix's own code (controller, tests, evidence, container configuration and this policy) merge automatically after the hosted Matrix orchestration tests pass on the exact head commit, through the owner-approved core lane (branch `matrix/core/<name>`, description carrying "Matrix core change: owner-approved lane"; owner decision, 10 November 2026). Workflows, the merge gate (`scripts/matrix`), PowerShell and shell scripts, the Dockerfile, the egress allow-list and plugin source stay outside that lane and need a person. The worker's own edit scope is unchanged. Merging does not rebuild the container: the image keeps its baked code until it is rebuilt.
- Trusted workflows hold merge/publishing credentials. Claude drives the process and verifies outcomes without holding release/R2/website credentials.
- Automatically increment the shared beta VERSION third component per qualifying successful development main build/merge. Major/minor changes are deferred. No resets, duplicate bumps from retries, or releases for prompt/documentation-only changes. Published assets stay immutable.
- Plugin delivery requires one Windows EXE and one macOS PKG suite installer, matching private R2 integrity, beta website version/changelog and authenticated downloads.
- Website-only tasks verify their deployed site changes and existing downloads; do not manufacture a plugin release. Infrastructure-only tasks verify deployed controller/workflow behaviour and run evidence; no unrelated plugin release or version bump. Discovery/parent milestones use their explicit acceptance, not fabricated installers.

## Comments, recovery and Done
1. Claim durably, move to Doing and post starting comment with task ID, Matrix job/run, prompt revision, repository and limits before development. Reconcile partial claim/status/comment failures; no coding without a recorded claim and successful starting comment.
2. Comment on meaningful progress, PR/CI outcomes, merge, blockers and limits. Avoid heartbeat spam.
3. While actively monitoring delivery within limits, stay Doing with explicit release-pending stage. On delivery failure/stall or exhausted limit, Needs attention. Preserve merged work.
4. Verify all task-specific acceptance and delivery, including relevant exact commit/build/deployment identity.
5. Post final evidence comment, close issue as completed, then mark board Done. A failed final comment blocks closure/Done. If close succeeds but board update fails, retry only finalisation idempotently.
6. If a comment/status API fails, persist pending reconciliation and expose controller failure; never report Done or silently restart the task.

Archived done/ prompts establish merged implementation, not delivered completion. AUTO-03 heartbeat remains a separate stale-aware controller liveness signal even when Claude is idle or usage-gated; issue comments are the task audit trail.
