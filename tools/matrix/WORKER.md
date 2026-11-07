# Controlled development worker

AUTO-01 Phase C adds actual orchestration from the canonical queue to **draft PRs**.
This is executable worker code, not a command-plan preview. It is disabled in both
example configurations. No live Claude development, scheduler or account is activated
by merging it. Auto-merge and release publication remain unavailable.

## Execution

The owner-controlled configuration is separate from the checkout; start from
[`worker.example.json`](worker.example.json). `work` is an explicit entry point:

```powershell
node C:/Matrix/trusted/matrix.mjs work C:/Matrix/private/config.json daily
node C:/Matrix/trusted/matrix.mjs work C:/Matrix/private/config.json notify
node C:/Matrix/trusted/matrix.mjs work C:/Matrix/private/config.json timer
node C:/Matrix/trusted/matrix.mjs pause C:/Matrix/private/config.json
```

With either `liveExecution` or `worker.enabled` false, `work` exits without invoking
an agent. Ordinary dry-run entry points reject live configuration. Pause/status/resume
can still use that configuration; resume only removes the pause file and does not
start work. The worker has its own daily/event cursors, so a dry run cannot consume
its schedule. Missed immediate work is recovered by daily reconciliation. The same
exclusive lock protects dry-run and live operations and remains held across awaits.

For each selected prompt, the worker:

1. Verifies owner approval, clean source checkout and the expected HTTPS GitHub origin.
   Fetches canonical main with the configured absolute Git executable; rejects backlog
   metadata errors and stale main before claims. Uses the existing dependency/model/
   effort/conflict selection rules; only Sonnet low/medium work is eligible.
2. Checks fresh subscription usage, the 85% threshold, session allowance and disabled
   paid extra usage. An unknown result stops work. Claims and pins the prompt/hash/base
   to flushed state before making any worktree or provider side effect.
3. Creates an isolated named branch/worktree. Existing retained worktrees are never
   reused automatically. Tracked symlinks/submodules require manual review.
4. Sends pinned task text and AGENTS.md/CLAUDE.md to Claude over stdin. Claude gets
   `Read,Edit,Write,Glob,Grep` only, safe/restricted modes, `dontAsk`, explicit Sonnet,
   no custom settings, MCP or persisted agent session. No shell, browser, subagent,
   installer, GitHub or release tools are granted. Provider/GitHub environment secrets
   are not passed to the agent. CLI subscription credentials remain accessible through
   its normal login; file tools are restricted to the worktree.
5. Restricts changes to plugin/shared Source, Tests, Resources, docs and plugin READMEs.
   VERSION, parameters' identity policies, prompt tracking, workflow/automation code,
   CMake, executable scripts and hidden paths cannot be delivered by this worker.
   Tasks needing those changes become interrupted/manual work, rather than widening
   permissions. Protected files are never silently discarded or reset.
6. Runs fixed Release configure/build/test commands for each affected plugin; shared
   changes validate all three. Commands run via the trusted Windows build launcher
   under a distinct non-admin build account. No installer runs. Build output is
   retained in `build/matrix/build.*.log`. Warnings conservatively block the draft.
   Source fingerprints must match before and after build execution.
7. Allows at most one repair, after another fresh usage check, using the remaining
   per-prompt turn and estimated-dollar bounds. A failed build or stopped repair
   produces a draft PR with a Blocked section. UI/editor work also remains blocked
   pending retained before/after screenshots and visual review; this worker does
   not yet render/commit those automatically.
8. Revalidates changed paths and Git base, commits only the permitted explicit paths,
   pushes a feature branch without force, checks existing PRs for the exact branch/
   commit, and creates a **draft** via GitHub CLI with a body file. It records the PR
   and `awaiting-review`; this never means merged or done. Main advancing during
   execution blocks review until a human rebases/revalidates. No PR approval/merge,
   version increment, prompt archival or release happens here.

## Required isolated deployment and owner approval

Use a dedicated non-admin Windows VM/controller account with outbound network
restrictions. Do not activate on Tony's normal desktop account. Keep personal files,
LAN and release/R2 credentials inaccessible. A config boolean alone is insufficient:
the worker requires an owner-managed approval JSON outside all writable checkout,
worktree and state roots, matching configuration, machine/account, launcher hash and
an expiration no more than 24 hours away. Protect trusted code/config/approval with
owner-only write ACLs. They must not self-update from a fetched branch.

The approval contract is:

```json
{
  "authorised": true,
  "user": "Matrix",
  "host": "MATRIX-VM",
  "isolatedAccount": true,
  "networkRestricted": true,
  "separateBuildAccount": true,
  "noReleaseCredentials": true,
  "expiresAt": "<ISO timestamp within 24 hours>",
  "configHash": "<approvalDigest(config) from worker.mjs>",
  "buildScriptHash": "<SHA-256 of installed Invoke-MatrixBuild.ps1>"
}
```

These fields are the owner's attestation of a verified deployment, not automatic
discovery/proof of network/ACL isolation. Approval expires or config/launcher changes
require owner renewal. No approval is generated by this PR. First use must be a
supervised one-prompt pilot. The token/dollar bounds limit client work, not a provider
reservation or guarantee of a final bill; paid extra usage must remain disabled.

Provision a separate local **build account**, source/JUCE/toolchain access and its
batch-logon rights. Export its PSCredential with PowerShell `Export-Clixml` under the
controller account to the protected credential path; DPAPI binds it to that Windows
user/machine. Never commit it or put a password in config/arguments. The build account
must have access to worktree source/build files but **no access** to controller state,
private approval/credential files, Git common directory, Claude/GitHub credentials or
personal files. Restrict its network too. The launcher checks distinct Windows SIDs
and refuses elevated child execution. Explicitly test these ACL/network restrictions;
the script cannot provision or infer them. Neither account needs release credentials.

The controller uses its dedicated least-privilege Git/GitHub login; those credentials
must have branch push and draft-PR access only. Ordinary OS ACLs and the separate
build account, not environment filtering alone, protect credentials from untrusted
CMake/test code. Review trusted managed Claude settings too; safe mode does not
override administrator policy. The actual separate-account launcher is Windows-only;
cross-platform CI tests its orchestration with simulated external commands.

## Limits, logs and recovery

### Temporary build access

The approval-hashed build launcher grants one direct child of the configured
worktree root a temporary ACL lease. Source is read/execute only; only affected
plugin `build` directories are writable. Hidden paths, including the worktree
Git pointer, remain controller-only. The parent directory is never opened up.
The controller writes isolation targets into the build request before the lease;
the build child must actually be denied access to these paths before running CMake.
An isolation failure exits with code 74 and stops without an agent repair.

The launcher durably records original DACLs in `active-build-lease.json` beside
the protected build credential, then restores them in `finally`. Newly created
outputs are sealed to the controller rather than left readable by their creator.
OWNER RIGHTS rules prevent output ownership from implicitly granting WRITE_DAC.
The build child joins a kill-on-close Windows job before invoking CMake/tests.
An outstanding journal blocks all future worker invocations; recovery failure
also prevents repair, push and PR creation. Forced termination/power loss requires
operator recovery after confirming that all build processes have stopped.

See [build-access deployment evidence and rollback](BUILD-ACCESS.md). This change
does not install trusted code, renew approval, alter the scanner, or enable work.

Agent process timeout is 1–30 minutes; maximum turns 1–30 per prompt, shared across
at most two invocations; estimated-dollar cap is at most $20, also shared. Output is
bounded to 4 MiB. Output tokens are bounded by the configured CLI environment plus
the turn bound. The worker stops starting work after a four-hour batch deadline.
Process cancellation polls pause/approval expiry every 250 ms, kills the process tree
on Windows (process group on POSIX tests), and waits for exit before releasing its
lock. A synchronous usage query can delay noticing pause by its bounded 50-second
preflight; no development process is running during that query.

`logs/worker.jsonl`, state phase/branch/commit/PR fields and
`latest-worker-report.json` record progress. Raw agent output is not added to state
or PR text. Build diagnostics remain in the retained worktree. A process/policy/PR
uncertainty becomes `interrupted`; restarts do not rerun it, repush it, or make a
second PR. A power loss leaves the global lock for the existing operator recovery
procedure. Inspect retained worktrees and GitHub before any deliberate retry. Nothing
is deleted automatically; operator cleanup/retention is required.

The scheduled task installer accepts `-DevelopmentWorker` and a separate task name,
but still registers **disabled**. It runs `work ... timer`, retains one instance,
startup/catch-up triggers and has a 4h15 task cap. Do not enable both a dry-run task
and worker task on the same state until their shared locking has been tested.

## Verification and remaining work

Worker tests use real temporary repositories, real worktrees/commits/local-bare pushes
and a real fake-agent child process. They simulate compiler/Claude/GitHub responses;
no subscription development turn, real compiler build, public branch or external PR
is created by those tests. They test success, duplicate prevention, protected edits,
usage stops, bounded repairs, failed tests, pause, stale main, source tampering,
uncertain PR creation, approval and real process timeout/output/cancellation.

Still to validate before activation: actual restricted Claude file edits, separate
build-account credential/ACL/network behaviour, real JUCE/toolchain builds, genuine
GitHub draft creation with least-privilege credentials, reboot/scheduler behaviour,
UI screenshot capture, and a supervised pilot with usage below 85% and paid extra
usage disabled. Automatic merging and Phase D publication are separate unfinished
features. Not yet verified in a DAW.
