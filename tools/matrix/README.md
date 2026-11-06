# Matrix development orchestrator

Implementation for [AUTO-01](../../shared/prompts/AUTO-01-matrix-autonomous-development.md).
**Sprint 0 and Phase B are implemented as a dry-run queue and report generator.**
No AI agent, build, PR merge, release or website update runs from this deployment.
Phase C/D command plans and validation contracts are tested helpers, not operational
integrations. AUTO-02 is excluded by its deferred policy. No suite release/version is assigned.

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
node tools/matrix/matrix.mjs pause C:/Matrix/config.json
node tools/matrix/matrix.mjs resume C:/Matrix/config.json
node --test tools/matrix/tests/*.test.mjs
```

`local` writes diagnostic reports/logs but does not commit queue state or acknowledge
notifications. `daily` persists availability and history but never claims development
work. A candidate is eligible for planning; it is **not permission to execute**.
Every report includes authoritative revision, execution mode/default, dependencies,
priority, effort, model, skip reasons, validation errors and usage blocker.
All three live/merge/publication flags must be explicitly false; true or missing
values are rejected. `enabled: false` or a state-directory `PAUSED` file prevents
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

## Usage feasibility and activation blockers

Checked 6 November 2026 against official documentation:

- [Status line data](https://code.claude.com/docs/en/statusline) documents
  `rate_limits.seven_day.used_percentage` and `resets_at`, but says windows may be
  absent and appear only **after the first API response**. That does not establish
  a supported fresh pre-start query. No production telemetry adapter is installed.
- [Usage/cost documentation](https://code.claude.com/docs/en/costs) distinguishes
  subscription limits from locally estimated API costs and describes cached usage
  after request failure. Estimated dollars are not a weekly percentage.
- [CLI reference](https://code.claude.com/docs/en/cli-reference) documents print mode,
  model/turn controls and authentication status. Their presence does not verify
  Matrix's account, subscription-only billing, security isolation or automation terms.

The tested gate rejects unknown/source mismatch, malformed/future/stale (>60 seconds)
observations, expired/contradictory reset times and usage **>=85%**. The actual worker
always supplies unknown and fails closed; a JSON file claiming an authoritative
source cannot activate it. No private endpoints, session scraping, paid API fallback,
manual allowance override or Codex fallback is implemented. Before Phase C, verify
a supported fresh **pre-start and between-prompt** usage retrieval method, permission
to automate this account, and disabled extra-credit/API billing with Anthropic.
An independently approved fixed API budget would be a different policy/change.

Also outstanding: verify Matrix hardware/resources, VM/network isolation, Claude
CLI version/authentication, sandbox enforcement, JUCE/compiler toolchain, GitHub App
least-privilege development permissions, required checks/branch protections and
owner authorisation. The current dev host has Node 26.13.0, PowerShell 7, Git, CMake,
GitHub CLI and a Claude executable; Python is absent. Sandbox denied hardware CIM
inspection, so OS/CPU/RAM and Matrix provisioning are **not verified**. Claude was
not invoked. Tests do not consume subscription usage.

## Remaining phases and release boundary

`claudePlan`, `worktreePlan` and `buildPlan` provide fixed, validated command plans;
they do not launch processes. `mergeGate` checks exact head, approval, protections,
warnings/tests and independently green Windows/macOS required checks. It does not
query GitHub or merge. Implement authenticated evidence retrieval, isolated Claude
execution with environment/network restrictions, process-tree cancellation, finite
turn/time/token limits, per-prompt usage rechecks, local builds, draft PR creation,
bounded repair and protected merges only after the above prerequisites are verified.
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
