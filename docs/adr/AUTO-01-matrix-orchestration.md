# AUTO-01: trusted dry-run worker before agent activation

Status: **superseded** by [AUTO-05, the Linux agent container](AUTO-05-linux-agent-container.md). Kept for history: the Windows Task Scheduler, DPAPI and build-broker design below is retired. Current behaviour is in the [Matrix README](../../tools/matrix/README.md) and the rules are in the [execution policy](../songsmith-execution-policy.md).
Canonical prompt: [AUTO-01](../../shared/prompts/AUTO-01-matrix-autonomous-development.md).

Phase C follow-up: the controlled worker (its Windows deployment notes have since been retired; see the [Matrix README](../../tools/matrix/README.md)) now executes
real worktree, restricted-file-tool Claude, build, Git and draft-PR operations.
Claude cannot run commands; trusted orchestration owns Git and invokes builds under
a distinct Windows identity using a protected DPAPI credential. An expiring external
owner approval binds config/account/host/build-launcher hash and attests verified
ACL/network isolation. It is rechecked during execution; pause, approval loss and
timeouts terminate the process tree. Interrupted tasks retain all work and require
review before retry; no uncertainty automatically repeats PR creation. Default live
execution remains off. Phase D and protected automatic merging are still separate.

Use dependency-free Node modules and Windows Task Scheduler. Node is already on the
dev host; Python is not. A five-minute limited-account task handles notification
polling and a London-calendar daily reconciliation. No continuously resident agent
or inbound listener is needed. Git objects pinned to fetched main supply backlog
content. Reviewed installed code/configuration remain outside the mutable checkout.

One exclusive lock plus atomic JSON replacement is adequate for one worker and this
small backlog. GitHub retains canonical work and merge evidence. No lease expiry
steals a busy claim. Restart recovery preserves uncertain work for review; bounded
claim attempts end in a dead-letter state. Owner-managed backups/retention preserve
task history and daily structured diagnostics. A crash lock requires operator review;
this sacrifices unattended availability to prevent duplicate execution. The dry-run
remains useful without any provider or release credentials.

Use a separate notification-only runner/account and restricted runner group. GitHub
can only signal an inbox, never execute changed prompt content. The independent
worker fetches main. Notifications accelerate immediate work; daily scanning supplies
correctness. No runner is activated until workflow-level isolation and the protected
environment are verified. Accounts lacking that capability use daily fetch alone.

Prompt speed is not authority. Default scheduling supports legacy files. Explicit
deferred policy beats omitted/default execution (superseded on 10 November 2026: prompts
carry no execution; since 13 November only the board column, Backlog or Blocked, holds a card back); malformed explicit metadata blocks
selection. Conservative plugin footprints limit overlap; dependencies need merged
archives. Free-form requirements cannot be completely interpreted by regex and need
owner review/explicit metadata before live development. This implementation selects
Sonnet only; Opus recommendations are visible review blockers, never silently changed.

Follow-up decision: use the documented `/usage` structured report in a separate,
empty-directory CLI invocation to obtain weekly limits before model work. This
supersedes the original status-line-only feasibility blocker. Verify subscription
authentication and the tested CLI version family, reject cached/failure notices,
unknown limits and paid extra usage, and retain the 85% threshold. Cache detection
depends on the CLI's notices because the experimental schema lacks a server fetch
timestamp; version changes and real provider failure behaviour need revalidation.
The check is optional and read-only; live execution stays disabled. No manual or API
billing override is provided. See the [runbook and official sources](../../tools/matrix/README.md#board-pickup-auto-11).

Future agent execution belongs in an isolated VM/account without personal secrets,
LAN or release credentials. Only trusted orchestration may run allowlisted commands
and collect independent build/CI evidence. Worktree/build/Claude command plans and
head/checkpoint/asset validation contracts are testable now; process execution,
authenticated PR evidence/merge adapters and live usage retrieval remain blocked.
The existing overnight permission is narrower here: AUTO-01 explicitly requires
verified safety prerequisites and owner activation. Neither AGENTS.md nor CLAUDE.md
was changed and no merge permission is inferred for this deployment.

Release authority is separate from task success. Trusted owner-reviewed CI must
verify the exact main commit, both platforms, universal Mac and installer tests.
Exactly two installers can be published; hashes live in a manifest/notes, not extra
public assets. Only matching publication plus both verified immutable private R2
objects authorises a deterministic website event. The separate website independently
validates readiness and updates its manifest/changelog together. Credentials,
network adapters and protected environment approval remain outside Matrix. No live
release workflow is introduced before those external contracts are provisioned.
