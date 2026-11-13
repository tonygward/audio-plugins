# Matrix development orchestrator

Matrix is the autonomous developer for the Songsmith plugins ([AUTO-01](../../shared/prompts/AUTO-01-matrix-autonomous-development.md)). The rules live in
the [execution policy](../../docs/songsmith-execution-policy.md); this page explains how Matrix carries them out.

## How it runs today

- **Where:** a Linux worker in an isolated WSL container (`wslc`), started and managed by [`tools/matrix-linux/Start-MatrixAgent.ps1`](../matrix-linux/README.md).
  The worker code in this folder is baked into the container image, so a code change reaches the worker only after the image is rebuilt and the worker restarted.
- **Entry point:** `linux-worker.mjs`, run every five minutes by the container's `entrypoint.sh`. Each run is a board tick (`board-run.mjs`) followed by the worker.
- **Builds:** never in the container. Native Windows and macOS builds and tests run on GitHub-hosted runners (`build.yml`); the worker waits for them.
- **What decides what runs:** the project board column (Todo and Doing run; Backlog and Blocked wait), nothing in a prompt or issue. See [Board pickup](#board-pickup-auto-11).
- **Tests:** `node --test tools/matrix/tests/*.test.mjs test/*.test.mjs`. No npm packages are needed (Node.js 24 or newer).

Older Windows-account machinery (the scheduled worker task, build broker, supervised fixtures and their validation notes) has been retired; the Linux container
replaced it ([ADR AUTO-05](../../docs/adr/AUTO-05-linux-agent-container.md)). `matrix.mjs` is the original dry-run controller; it is no longer on the live path and is kept
for the repository snapshot reader and the selector tests.

## State and recovery

The worker keeps durable state in `/work/state/state.json` inside the container (a volume), with a log at `/work/state/logs/worker.jsonl`. Replacement is atomic and an
exclusive lock protects each transaction. Task states are `pending`, `claimed`, `interrupted`, `awaiting-review`, `withdrawn`, `dead-letter` and `complete`. A restart
turns claimed work into `interrupted` and recovers in-flight hosted builds; see [Blocked outcomes, resume and the run budget](#blocked-outcomes-resume-and-the-run-budget-auto-12).
GitHub (main, pull requests, the board and issue comments) remains the durable authority. Never replace a corrupt state file with an empty one, because that could erase ownership.

## Metadata and normalisation

A prompt carries no status and no execution mode: **the project board column decides** whether and when it runs. Any `status:` or `execution:` left in a file is ignored,
and the repository test fails if a prompt still has one. Metadata supports the following restricted YAML subset (scalars and inline JSON arrays):

```yaml
---
id: EQ-41
priority: high
model: sonnet
effort: low
dependencies: ["EQ-32"]
conflicts: []
---
```

Empty/unknown/duplicate fields, nested YAML,
aliases/tags, inconsistent IDs and invalid explicit values block the batch visibly.
Priority accepts a non-negative number or `high`, `medium`, `low` (1, 50, 90).
Effort is `low`, `medium`, `high`. **Sonnet is the default model** and Matrix always runs Sonnet; `model:` can be left out.
`model: opus` is recorded as advice for a person running the prompt by hand and never holds a card back.

IDs come from scope-qualified filenames, or the numeric filename plus plugin scope
(`EQ`, `ACOMP`, `MAX`). The historical `done/S7-...` alias is reported as an archive
warning and cannot satisfy dependencies; canonical SUI-07 supplies that identity.
Duplicate active IDs or malformed active metadata block **all** selection.
Archived prompts (`done/`, `archive/`) and durable non-pending states are excluded. Wording in a prompt such as "not queued" or "deferred" holds nothing: a card the owner puts in Todo or Doing runs. Backlog and Blocked are the only holds.

Labelled dependency links and `Build after 06` / `Run this after task 06` prose are
normalised. Explicit `after ID` in a linked local README row adds a dependency; other words in a README row (such as "blocked" or "deferred") hold nothing. Board order decides
priority (the shared README no longer has a Delivery priority table). Other free-form prose is **not a complete
dependency language**; review it and add explicit metadata before live activation.
README-embedded tasks without canonical prompt files are not invented as jobs.
Missing/circular/unmerged dependencies skip work. Only tracked `done/` files on
fetched main supply completion evidence; an agent report cannot complete a prompt.

The board decides what runs and in what order; the prompt scan supplies the prompt, its dependencies and its conflicts (conflicts are symmetric; a task
with a missing, circular or unmerged dependency is skipped with a reason). Priority only breaks ties in the legacy dry-run selector. This conservative footprint can yield fewer than three.

## Matrix core lane (auto-merge for Matrix's own code)

`matrix-auto-merge.yml` has a second, owner-approved lane. A pull request from a branch named `matrix/core/<name>`, whose description contains
`Matrix core change: owner-approved lane`, merges automatically when the hosted **Matrix orchestration tests** (Windows, macOS, Linux) succeed on its exact head
commit and every changed file is Matrix code: `tools/matrix/*.mjs|json|md`, `tools/matrix/tests` and `evidence`, `tools/matrix-linux/worker.config.json` and `README.md`,
or `docs/songsmith-execution-policy.md`. Workflows, the gate itself (`scripts/matrix`), PowerShell and shell scripts, the Dockerfile, the egress allow-list and
plugin source always need a person. The worker's own scope is unchanged, so it cannot edit this code. Merging does not rebuild the container; rebuild the image
(`Start-MatrixAgent.ps1 -Action Build`) and restart it to run the new code.

## Blocked outcomes, resume and the run budget (AUTO-12)

- **Blocked.** A task that stops is reported once on its issue (reason, job, attempts, branch and pull request, what to do next) and its card moves to **Blocked**
  (`blockedOptionId`). The comment is deduplicated by a marker and both steps are retried if they fail. Nothing re-queues a blocked task on a poll. Every stop is explicit:
  a turn limit, cost limit or per-attempt time limit names the limit in the reason (`limitReason`); the run time limit and an exhausted repair budget do the same; and a
  hosted Build that keeps failing, never starts or does not finish parks the task as interrupted with its pull request and commit saved, not as "awaiting review".
- **Resume is explicit, and never repeats work.** Move the card back to **Todo** or **Doing**; the budget and attempts start fresh.
  - No committed work: the task starts again from a clean worktree.
  - Committed work: the task carries on from the saved branch, worktree and pull request (`resumeWork`). The worker checks the saved paths are where Matrix keeps them and that the
    last pushed commit is still on the branch, recreates a lost worktree from the pushed branch, and asks the hosted Build about that commit. It runs the agent only when that build
    failed (the failure log is saved to a file, never pasted into the instructions); a green, pending or not-yet-started build goes straight on to waiting and the merge gate.
    The existing pull request is reused and its description rewritten, so an old Blocked section does not linger. Selection exempts a resumed task from the
    "open pull request already exists" rule.
  - A task whose pull request has merged is never resumed: delivery owns it. A task whose pull request was closed without merging is held (one comment, card back in Blocked).
- **Only the column holds a card.** Backlog and Blocked wait; Todo and Doing run. The board's old Execution field is not read, and wording in a prompt or
  issue (such as "deferred" or "not queued") never holds a card. Archived prompts stay withdrawn whatever the board says (`refreshTasks`).
- **Run budget.** `worker.maximumRunMinutes` (default 120) counts from the first claim, includes CI waiting and is saved in the state, so a restart cannot reset it.
  At the limit no further agent call starts and the run stops with the reason. `maximumRepairs` is 3 (up to 3 allowed). Per-attempt limits (turns, cost,
  minutes) still apply.
- **Token budget (optional).** `worker.maximumTokens` (10,000 to 100,000,000) caps the tokens one run may use, as an extra safety net. It counts the
  input, output and cache-write tokens Claude reports for each attempt (cache reads are re-reads and are left out), is saved with the task (`tokensUsed`)
  and is checked before each attempt, so the attempt that crosses it finishes and no further one starts. Leave it out to rely on the other limits.

- **Restart during CI is recovered (`recover.mjs`).** A restart marks a claimed task interrupted, but if its branch is pushed and its pull request is open, the tick looks the
  work up again by branch and exact head commit. Passing or still-running checks hand it back to the hosted Build and merge gate (`awaiting-review`); the existing merge hook then
  carries it through delivery. A failed Build, a head commit Matrix did not push, or no result within 90 minutes of recovery leaves it interrupted, so it is reported and parked in
  Blocked. Merged work is never parked: it goes to delivery. Recovery never starts the agent and never pushes.

## Why a card was picked up (AUTO-14)

The starting comment says why. The reason is captured at selection, before the card moves to Doing, written into the hand-off, persisted with the claim (`task.pickup`) and rendered from that copy, so a retry or restart
never changes it. Labels: `Scheduled (Todo)` (with the London schedule date; uses the day's slot), `Run now (Doing)` (does not), `Resumed (Todo|Doing)` (with the earlier job, reason and branch or pull request),
and `Unknown (provenance unavailable)` (with a `pickup-provenance-unknown` diagnostic). The initiator is always labelled Unknown for Doing and resumes: a Doing card never proves who moved it.

## CI-passed note on the issue (AUTO-13)

When the hosted Windows and macOS Build passes for the exact head commit of a task's pull request, the worker posts one progress note on the original tracking issue
(`ci-comment.mjs`): the task, the pull request, the CI - Build and test run link, the checked commit and what comes next. It says the work is **not delivered yet** and the card stays in Doing.

- **Once per task + pull request + head commit.** A hidden marker (`matrix-ci-passed:<task>:<pr>:<commit>:success`) is checked on the issue before posting, so reruns, repeated
  polls and restarts never repeat it. If the head changes and a new commit passes, that commit gets its own note.
- **Right issue only.** It goes to the tracking issue saved with the claim (in its own repository, even when the pull request is elsewhere), never the pull request and never a guess from a branch name.
- **Never blocks the work.** A failed post is saved on the task (`ciComments`, pending with the reason) and retried by the board tick (`recoverCiComments`); it is not reported as posted, and it does not stop the merge.
- **Same path after a restart.** In-flight recovery (`recover.mjs`) posts the note too when it finds the Build passed on the pushed commit.
- **Least access.** The trusted controller writes it with the board token; the Claude coding process has no comment access.
- **Not a final comment.** Merge, release, the final evidence comment, issue close and board Done stay with delivery.

## Issues without a prompt file (issue-as-prompt)

`issue-prompt.mjs`. A Doing card (or a Todo card on the day's slot) whose task has no prompt file, in an execution repository, with no merged or open pull request and no
existing task state, has its issue read. **Enough detail** means the body has headings for an objective, scope or implementation, acceptance criteria and tests
(a combined "Acceptance criteria and tests" heading counts for both; each section needs real text). Then the tick generates a canonical prompt
(`<plugin>/prompts/NN-slug.md` for EQ, ACOMP and MAX, otherwise `shared/prompts/ID-slug.md`; Sonnet (the default), medium; no status or execution, which the board owns) from the issue, hands it to the worker in the
hand-off (the worker checks it parses and matches the hand-off hash), and the worker **saves it in the same pull request**. **Not enough detail:** one comment lists what is
missing (marker includes a hash of the missing list and the issue text, so it is not repeated until the issue changes) and the card moves to Blocked. Cards outside Todo and Doing are left alone; wording in the issue never defers a card. A dry run assesses but writes nothing.

## Board pickup (AUTO-11)

Board order decides what the Linux worker builds. It is **off** unless `boardPickup.enabled` is `true` in the worker config, and
`dryRun: true` makes the tick report its choice without claiming anything. Policy: [execution policy](../../docs/songsmith-execution-policy.md).

Each run is two steps in one process, run by `linux-worker.mjs`:

1. **Board tick** (`board-run.mjs`). Reads the *Songsmith Development* board in its manual order and the open and merged pull requests in
   each repository, then resumes unfinished work: a lost pickup comment, or a merged task still waiting for beta delivery. It checks the Doing
   cards against pull requests and worker state, applies the 60% weekly usage stop, and picks **one** task. **Todo** is the overnight queue, in board order: one task per London day, on the first run after `dailyTime`.
   **Doing** is a run-now request: a Doing card with no claim is picked up ahead of Todo. Backlog and Blocked cards are never picked up; nothing else holds a card back.
   The choice goes to `board-handoff.json` in the state folder.
2. **Worker** (`linux-worker.mjs`). Under its own lock it re-checks the hand-off against a fresh copy of `main`, claims through its normal path,
   moves the card to Doing, posts the starting comment (task, Matrix job, prompt revision, repository, limits), then builds. If either step fails it writes
   no code and the task is blocked. With no fresh, valid hand-off it does nothing.
   There is no fallback to the Markdown selector.

Delivery (`delivery.mjs`) runs in the next tick after a merge: verify the beta release, post the final comment, close the issue, then set the card
to Done. Each stage is saved. A failed or missing check leaves the issue open and the card in Doing. Evidence comes from the existing release
pipeline (`verifiers.mjs`): the release, the private-storage distribution run, the website `release.json` and changelog, the Cloudflare Pages
check, and the task's prompt in its `done/` folder on `main`. Downloads are not tested: the final comment links to the beta website.

Config (all required when enabled; `batchLimit` and `worker.maximumPrompts` must both be 1):

```json
"boardPickup": {
  "enabled": true, "dryRun": true,
  "projectId": "<project node id>", "statusFieldId": "<Status field id>", "doingOptionId": "<Doing option id>", "doneOptionId": "<Done option id>",
  "repositories": ["owner/audio-plugins", "owner/songsmithstudio", "owner/songsmith-website"],
  "executionRepositories": ["owner/audio-plugins"], "promptRepository": "owner/audio-plugins",
  "tokenFile": "/absolute/path/to/board-token"
}
```

The token needs to read the project and both repositories, and to comment on and close issues. It must not be able to publish releases.
`tokenFile` is optional and defaults to the worker's token. A separate board token is recommended.

**Known gaps:** only `audio-plugins` is an execution repository, so website tasks wait for a person. A Doing card is the run-now request; there is no separate RUN NOW label.
