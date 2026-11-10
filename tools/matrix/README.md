**Current owner policy (9 November 2026):** [Board order, one task, 60% usage, automatic beta build numbers and verified delivery](../../docs/songsmith-execution-policy.md). Historical setup examples below do not override it. AUTO-11 must implement the board reader; never infer live board selection from documentation. A merged prompt archive does not close its issue: beta deployment/download verification and the final issue comment precede issue close and board Done.

# Matrix development orchestrator

Implementation for [AUTO-01](../../shared/prompts/AUTO-01-matrix-autonomous-development.md).
**Sprint 0/Phase B and a disabled controlled development worker are implemented.**
See the [worker deployment guide](WORKER.md) for actual worktree → Claude → build →
draft-PR execution and its independent activation gates. No worker is activated by
this repository's example configuration. No PR merge, release or website update runs.
An optional read-only Claude `/usage` command can populate the allowance report.
Phase C includes the controlled worker; Phase D remains validation helpers only.
AUTO-02 is excluded by its deferred policy. No suite release/version is assigned.

## Run without installing anything

Requires Node.js 24 or newer, Git, and PowerShell 7 on Windows. No npm packages,
database server, open terminal or inbound HTTP listener are required.

Copy `config.example.json` to an owner-controlled location and set absolute paths.
Use a separate state directory for experiments. The repository is read through
immutable Git objects; local edits are never included. `daily` fetches main;
`local` inspects tracked HEAD without fetching and labels the report non-canonical.

```powershell
node tools/matrix/matrix.mjs local C:/Matrix/config.json
node tools/matrix/matrix.mjs daily C:/Matrix/config.json
node tools/matrix/matrix.mjs status C:/Matrix/config.json
node tools/matrix/matrix.mjs usage C:/Matrix/config.json
node tools/matrix/matrix.mjs pause C:/Matrix/config.json
node tools/matrix/matrix.mjs resume C:/Matrix/config.json
node --test tools/matrix/tests/*.test.mjs
```

`local` writes diagnostic reports/logs but does not commit queue state or acknowledge
notifications. `daily` persists availability and history but never claims development
work. A candidate is eligible for planning; it is **not permission to execute**.
Every report includes authoritative revision, execution mode/default, dependencies,
priority, effort, model, skip reasons, validation errors and usage blocker.
Dry-run entry points require all live/merge/publication flags false. Only the explicit
`work` entry point permits live execution with separate deployment approval; merge
and publication remain false. `enabled: false` or a state-directory `PAUSED` file prevents
even fetching. Pause remains effective after reboot. Resume only restores dry runs.

## Metadata and normalisation

Existing Markdown prompts work without edits and default to `scheduled`. New metadata
supports the following restricted YAML subset (scalars and inline JSON arrays):

```yaml
---
id: EQ-41
status: ready
execution: build-now
priority: high
model: sonnet
effort: low
dependencies: ["EQ-32"]
conflicts: []
---
```

Allowed status values: `ready`, `planned`, `blocked`, `in-progress`, `deferred`, `done`.
`done` must be in the canonical merged archive. Allowed execution values are exactly
`scheduled`, `build-now`, `deferred`. Empty/unknown/duplicate fields, nested YAML,
aliases/tags, inconsistent IDs and invalid explicit values block the batch visibly.
Priority accepts a non-negative number or `high`, `medium`, `low` (1, 50, 90).
Effort is `low`, `medium`, `high`; model must recommend Sonnet for candidate selection.
Missing model/effort never gets guessed. Opus design work requires owner review.

IDs come from scope-qualified filenames, or the numeric filename plus plugin scope
(`EQ`, `ACOMP`, `MAX`). The historical `done/S7-...` alias is reported as an archive
warning and cannot satisfy dependencies; canonical SUI-07 supplies that identity.
Duplicate active IDs or malformed active metadata block **all** selection.
Done/future/archive folders, explicit deferred prose/status (including AUTO-02),
blocked/claimed work and durable non-pending states are excluded. Metadata cannot
override a deferred prose policy; promotion requires deliberately editing both.

Labelled dependency links and `Build after 06` / `Run this after task 06` prose are
normalised. Explicit `after ID` in a linked local README row adds a dependency;
blocked/deferred/claimed README rows exclude work. The shared README Delivery
priority table supplies roadmap priority. Other free-form prose is **not a complete
dependency language**; review it and add explicit metadata before live activation.
README-embedded tasks without canonical prompt files are not invented as jobs.
Missing/circular/unmerged dependencies skip work. Only tracked `done/` files on
fetched main supply completion evidence; an agent report cannot complete a prompt.

`build-now` precedes scheduled work; then priority and stable ID break ties. A
notification selects only `build-now`; daily scans recover all missed immediate
requests. Batches contain at most six low/medium prompts. Plugin-local work uses
at most one prompt per plugin; shared work occupies the batch alone. Explicit
conflicts are symmetric. This conservative footprint can yield fewer than three.

## Windows setup (owner action; not performed by this PR)

1. Provision a dedicated non-admin Matrix account or isolated VM. Keep personal
   files, LAN, GitHub release tokens and R2 credentials inaccessible. Verify
   supported Node/Git/PowerShell versions, disk space and outbound network policy.
2. Copy reviewed `*.mjs`, this configuration and `Notify-Matrix.ps1` into
   `C:/Matrix/trusted`. Keep the state directory separate from the checkout.
   Owner-only write ACLs protect trusted code/configuration. Matrix needs read
   access there, read/fetch access to the repo and write access to state/logs.
   Deploying a new revision is a deliberate owner operation, never a self-update.
3. Under the dedicated account, verify `daily` succeeds non-interactively. Public
   HTTPS fetch needs no credentials; private repositories need an independently
   verified read-only credential provider. Never embed a PAT in the URL/config/logs.
4. Review and run `Register-MatrixTask.ps1 -Config C:/Matrix/config.json
   -TrustedScript C:/Matrix/trusted/matrix.mjs -WhatIf`, then without `-WhatIf`.
   Registration creates a **disabled** S4U task with limited privileges. Verify
   the account's batch-logon rights, ACLs and network access before enabling it
   in Task Scheduler. Registration may require an administrator to grant rights;
   execution must remain non-admin. No tasks are registered during tests.
5. Enable only the dry-run task after explicit owner approval. It triggers at
   startup and every five minutes without an open terminal, with one Task Scheduler
   instance, missed-start recovery and two bounded five-minute restart attempts.
   Each tick performs at most one scan. London daily time is evaluated explicitly
   using Node ICU; the host timezone is irrelevant. A missed daily scan is caught
   after the next configured time, including spring/fall DST. Git subprocesses
   have 60-second timeouts; the task has a 20-minute overall limit.
6. Inspect `latest-report.json`, daily `logs/*.jsonl` and Task Scheduler last-run
   status. A nonzero exit means fetch/validation/storage failure; no work was
   started. A malformed prompt appears in the successful diagnostic report.

Uninstall with `Register-MatrixTask.ps1 -Remove` (same task name), then stop/remove
the separately provisioned runner service. Preserve state/history/logs. No plugin
installer or system-wide plugin installation is involved.

## GitHub notification setup

The workflow is off unless repository variable `MATRIX_NOTIFY_ENABLED=true` is set.
Create the `matrix-notification` environment with owner approval and restrict its
deployment branch to main. Provision a runner labelled `matrix-notify` in a runner
group restricted to **this workflow on main**. Do not register an unrestricted
runner on a public repository. If your GitHub plan cannot enforce workflow-level
runner isolation, leave this feature disabled and rely on daily reconciliation.
Environment approval alone is not sufficient to isolate a runner from other jobs.

The notification runner uses a **different account** with write access only to
`C:/Matrix/state/inbox` and read/execute access to the fixed trusted notifier.
It cannot read the orchestrator's config/credentials or write its code/state JSON.
The workflow has no checkout, no GitHub permissions, no secrets and no PR trigger.
Only a validated hexadecimal revision becomes an inbox filename. The worker does
not trust its contents/revision for selection; it always fetches current main.
The job contacts GitHub outbound; there is no home-network inbound listener.
Five-minute polling bounds the normal notification delay. Busy/offline workers
leave signals durable. Duplicate signals are harmless. Daily correctness does not
depend on runner availability or enabling this optional acceleration.

## State, failures and maintenance

`state.json` contains schema, task IDs, pinned hashes/revisions, attempts, events,
last daily date and the latest 1,000 history summaries. JSON replacement is atomic
after a flushed temporary file. An exclusive `worker.lock` protects the transaction
across processes. A crashed worker deliberately leaves the lock: **stop the task,
verify no Matrix process remains, back up state, then remove that exact lock**.
Do not steal locks by age or PID alone. Reboot recovery needs this review if the
machine lost power during a transaction; no agent automatically resumes.
Recovered claimed work becomes `interrupted`; inspect any retained worktree/PR
before deliberately resetting a claim. Two attempts are the hard default for the
tested claim primitive; exhaustion enters `dead-letter`. These primitives are
tested without invoking an agent and are not yet wired to a live worker.

Back up the state directory while stopped; restore only verified schema-1 state.
Never replace a corrupt state file with empty state to make an error disappear:
that could erase ownership. GitHub main and PRs remain the durable authority.
Rotate daily logs and acknowledged inbox files under owner-controlled retention
(for example 90 days) while the task is stopped; retain audit backups. The current
deployment does not silently delete logs. Monitor growing disk usage. A failed
fetch leaves signals and the daily date untouched for the next bounded tick.

## Usage check and remaining activation blockers

The usage adapter uses the documented structured
[`SDKUsageReport`](https://code.claude.com/docs/en/agent-sdk/typescript#sdkusagereport)
returned by Claude Code's built-in `/usage` command. It requires no model inference
and does not scrape credentials, undocumented APIs or session files. The status-line
limitation recorded in the original foundation is superseded by this interface.

Enable **only the read-only check** in your owner-controlled configuration:

```json
"claudeUsage": {
  "enabled": true,
  "executable": "C:/Users/Matrix/.local/bin/claude.exe"
}
```

Use the actual absolute native executable path under Matrix's dedicated account;
PATH lookup and `.cmd`/`.bat`/PowerShell wrappers are rejected. The example config
keeps the check disabled until configured. Old configs without `claudeUsage` still
work and report unknown usage without invoking Claude. Daily, notification and
local dry-run reports call the adapter when enabled. `usage` queries just allowance
without fetching the backlog. Paused, disabled and idle timer workers do not query.

The adapter verifies Claude version and first-party subscription authentication,
then invokes a fixed `/usage` command with safe mode, no tools, no custom settings,
no MCP servers, no session persistence and a one-turn limit. It runs in a newly
created empty temporary directory, with no repository context. Safe mode disables
customisations but does not override administrator-managed policy. Each invocation
is bounded: version 5 seconds, authentication 15 seconds, usage 30 seconds, and
output 1 MiB. The temporary directory is removed afterwards. No raw event stream,
stderr, identity, payment details or credentials are written to Matrix logs.

Only a successful zero-turn/zero-cost response with one structured report is accepted.
The adapter reads the server's `weekly_all` meter, not session-dollar estimates or
local activity percentages. It requires valid percentage/reset data, checks all
reported scoped weekly meters conservatively, and blocks at **>=60%**. An exhausted
session also blocks. Paid extra usage must explicitly report `is_enabled: false`;
enabled or unknown status blocks eligibility. This checker never changes account
settings. Disable extra usage yourself before any future unattended development.
Alternate API/auth/provider environment overrides are rejected, never silently used.

[Claude documents cached fallback after usage retrieval failure](https://code.claude.com/docs/en/costs#when-the-usage-request-fails).
The parser rejects last-known/cached/stale/rate-limit/retry notices, cache flags,
missing/contradictory data, unsupported meters, errors, diagnostics and timeouts.
It makes a new command request each time and never reuses stored reports for
eligibility. The request start timestamp bounds the observation to 60 seconds;
it is **not a server-provided fetch timestamp**. The experimental structured schema
does not currently provide an independent cache-age field, so freshness detection
also depends on the CLI's documented fallback notices. Revalidate those notices
and the schema when upgrading. Supported CLI versions are verified 2.1.319 and later
2.1.x patches; other version families fail closed until reviewed. Real testing on
2.1.319 returned weekly usage with zero turns/cost; sandbox-denied network access
returned null limits and was correctly rejected. Simulated cache/failure paths are
unit-tested; a real provider-side cached fallback has not been induced/verified.

The dry-run may now show a real percentage. This does **not** enable development:
the example configurations keep live execution disabled. The controlled worker
calls this same fresh check before every prompt and repair; the standalone result
and `latest-report.json` are diagnostics, not reusable execution authorisations.
No paid API fallback, manual allowance override or AUTO-02 implementation is added.

Also outstanding: verify Matrix hardware/resources, VM/network isolation, Claude
CLI version/authentication, sandbox enforcement, JUCE/compiler toolchain, GitHub App
least-privilege development permissions, required checks/branch protections and
owner authorisation. The current dev host has Node 26.13.0, PowerShell 7, Git, CMake,
GitHub CLI and a Claude executable; Python is absent. Sandbox denied hardware CIM
inspection, so OS/CPU/RAM and Matrix provisioning are **not verified**. Claude was
invoked only for version/authentication and `/usage` verification; these returned
zero model turns and reported cost. Automated tests mock the CLI and need no login.

## Remaining phases and release boundary

The [controlled worker](WORKER.md) now executes the worktree/build plans through
bounded processes and creates draft PRs. `mergeGate` checks exact head, approval, protections,
warnings/tests and independently green Windows/macOS required checks. It does not
query GitHub or merge. Protected automatic merges and authenticated CI evidence
retrieval remain separate work. Actual Claude/Windows-account/build/PR integration
must pass a supervised pilot under the deployment prerequisites before activation.
No dangerous permission-bypass flag is generated. Update prompt traceability after
verified merge, never on an agent's self-reported success.

`releaseManifest` validates version/commit-specific approval and successful main
Windows/macOS/universal installer evidence, enforces exactly two versioned files,
and computes SHA-256, sizes and immutable version-specific R2 keys. `websiteDispatch`
requires matching publication evidence **and both verified private immutable copies**;
its deterministic event ID supports receiver deduplication. These are tested contract
helpers, not a publisher. No credentials or network publication are implemented.

Phase D remains blocked on approved release scope, protected owner-reviewed trusted
CI/environment, independent artifact provenance/checksum verification adapters,
private R2 and GitHub publication credentials, immutable-object enforcement, website
repository identity/dispatch credentials and an idempotent receiver that independently
verifies R2 and commits **release.json plus matching changelog atomically**. The
previous website version must survive failures. Matrix never needs these release
credentials. Review sensitive workflow changes independently; a PR cannot approve
its own policy. Existing release workflows and root VERSION remain unchanged.

See [architecture decision](../../docs/adr/AUTO-01-matrix-orchestration.md) for trust
boundaries and the staged delivery decision. This is infrastructure; not yet
verified in a DAW.

## Board pickup (AUTO-11)

Board order decides what the Linux worker builds. It is **off** unless `boardPickup.enabled` is `true` in the worker config, and
`dryRun: true` makes the tick report its choice without claiming anything. Policy: [execution policy](../../docs/songsmith-execution-policy.md).

Each run is two steps in one process, run by `linux-worker.mjs`:

1. **Board tick** (`board-run.mjs`). Reads the *Songsmith Development* board in its manual order and the open and merged pull requests in
   each repository, then resumes unfinished work: a lost pickup comment, or a merged task still waiting for beta delivery. It checks the Doing
   cards against pull requests and worker state, applies the 60% weekly usage stop, and picks **one** task. **Todo** is the overnight queue, in board order: one task per London day, on the first run after `dailyTime`.
   **Doing** is a run-now request: a Doing card with no claim is picked up ahead of Todo. A card marked Separate on the board is never picked up. A deferred card is left alone and blocks nothing: set the board **Execution** field to Deferred (Run Now or Scheduled releases it); with any other value the prompt's own deferred status decides.
   The choice goes to `board-handoff.json` in the state folder.
2. **Worker** (`linux-worker.mjs`). Under its own lock it re-checks the hand-off against a fresh copy of `main`, claims through its normal path,
   moves the card to Doing, posts the starting comment (task, Matrix job, prompt revision, repository, limits), then builds. If either step fails it writes
   no code and the task needs attention. With no fresh, valid hand-off it does nothing.
   There is no fallback to the Markdown selector.

Delivery (`delivery.mjs`) runs in the next tick after a merge: verify the beta release, post the final comment, close the issue, then set the card
to Done. Each stage is saved. A failed or missing check leaves the issue open and the card in Doing. Evidence comes from the existing release
pipeline (`verifiers.mjs`): the release, the private-storage distribution run, the website `release.json` and changelog, and the Cloudflare Pages
check. `downloadEvidence: "worker-checked"` accepts the website Worker's own size and hash check on every download. Matrix does not call the
download route, and the final comment says so.

Config (all required when enabled; `batchLimit` and `worker.maximumPrompts` must both be 1):

```json
"boardPickup": {
  "enabled": true, "dryRun": true,
  "projectId": "<project node id>", "statusFieldId": "<Status field id>", "doingOptionId": "<Doing option id>", "doneOptionId": "<Done option id>",
  "repositories": ["owner/audio-plugins", "owner/songsmithstudio", "owner/songsmith-website"],
  "executionRepositories": ["owner/audio-plugins"], "promptRepository": "owner/audio-plugins",
  "downloadEvidence": "worker-checked", "tokenFile": "/absolute/path/to/board-token"
}
```

The token needs to read the project and both repositories, and to comment on and close issues. It must not be able to publish releases.
`tokenFile` is optional and defaults to the worker's token. A separate board token is recommended.

**Known gaps:** a claimed task whose worker died stays claimed; the tick reports it and picks nothing until the owner clears it (see issue #175).
The board has no **Needs attention** column yet and RUN NOW is not implemented, so interrupted work is reported but not moved automatically.
