# Songsmith execution and delivery policy

**This is the one place the development and delivery rules are written down.** Other documents link here and do not restate them.
Owner decisions: 10 November 2026, revised 13 November 2026 (board columns are the only hold; Sonnet by default; no download check; optional token budget; every finished task keeps a prompt).

The process has nine parts. Each has one owning document; this page holds the rules for parts 1 to 3 and 8, and points to the rest.

| # | Part | Owning document |
|---|---|---|
| 1 | Backlog: issues and prompts | [Backlog](#1-backlog-issues-and-prompts) below, with the [issue–prompt contract](backlog-issue-prompt-contract.md) for the prompt template |
| 2 | Selection: what runs next | [Selection](#2-selection-what-runs-next) below |
| 3 | Bounded execution: limits, Blocked and resume | [Execution](#3-bounded-execution) below; mechanics in the [Matrix README](../tools/matrix/README.md) |
| 4 | Continuous integration: the two builds | [Pipelines](pipelines.md) |
| 5 | Merge gate | [Pipelines](pipelines.md) |
| 6 | Versions and release notes | [Release checkpoints](automatic-release-checkpoints.md) |
| 7 | Release and distribution | [Release checkpoints](automatic-release-checkpoints.md) and the [R2 runbook](beta-release-r2-runbook.md) |
| 8 | Finishing a task: final comment, close, Done | [Finishing](#8-finishing-a-task) below |
| 9 | Archive and traceability | [Prompt README](../shared/prompts/README.md#prompt-identity-and-traceability) |

## 1. Backlog: issues and prompts

- Every task has a GitHub issue whose title starts with a stable ID (`EQ-41`, `ACOMP-16`, `SUI-19`). Search for an existing issue or prompt before creating one.
- **A prompt file is optional at the start.** A task is ready to run once its card is in **Todo** or **Doing**. If there is no prompt file, Matrix builds one from the issue when it has an objective, scope or implementation notes, acceptance criteria and tests, and saves it in the implementation pull request. If the issue lacks that detail, Matrix comments once on what is missing and moves the card to **Blocked**.
- **A prompt must exist by Done.** Every finished task has a canonical prompt in a `done/` folder, whether a person wrote it or Matrix generated it. Delivery does not set Done until it is there.
- Prompts live in `<plugin>/prompts/` (`eq`, `analog-compressor`, `maximiser`, `compressor`) or `shared/prompts/` for work across the suite. There is no other holding folder: a prompt waits because its card is in Backlog, not because of where its file is.
- A prompt never carries a status or an execution mode. The board owns both, and Matrix ignores any such line.
- **Model: Sonnet is the default.** Matrix always runs Sonnet. A prompt needs no `model:` line; `model: opus` is only advice for a person running the prompt by hand, and it never holds a card back.

## 2. Selection: what runs next

- **The board column is the only hold.** Cards in **Backlog** or **Blocked** wait. Cards in **Todo** or **Doing** run. Nothing else holds a card back: not wording in a prompt or issue ("deferred", "not queued"), not the prompt's folder, not its ID prefix, not a missing `model:` line. The board's old Execution field is no longer read.
- **Doing** is a run-now request: an unclaimed Doing card is picked up at the worker's next run (every few minutes), ahead of Todo, in board order.
- **Todo** is the overnight queue in board order. Matrix takes **one Todo card per London day**, on the first run after `dailyTime` (02:00). A Doing pickup does not use that slot.
- Board order decides. Matrix reads the view's actual order and fails closed if it cannot (no fallback by priority, ID or repository).
- One task per run and one active job at a time (`batchLimit` = `maximumPrompts` = 1).
- No new work at or above **60% verified weekly Claude usage**, including scoped meters. An unknown, stale or invalid reading blocks pickup. There is no paid fallback.
- Unmet dependencies (`dependencies:` in the prompt) skip a card with a reason; it does not block the cards behind it.
- A card whose work has already merged is never coded again: it goes to delivery.
- **Current limit:** Matrix only builds in `audio-plugins` (`executionRepositories`). Website tasks are listed but wait for a person. Moving plugin code under a `src` folder is a later refactor.

## 3. Bounded execution

- Matrix claims one task, moves the card to Doing and posts a starting comment (task, Matrix job, prompt revision, repository, limits, and why it was picked up) before any code is written. No code without a recorded claim and starting comment.
- **Limits** (configured, all enforced):
  - per attempt: Claude turns, a dollar cost limit and a time limit;
  - per run: **120 minutes** elapsed from the claim (CI waiting counts) and **3 repairs**;
  - per run, optional: a **token budget** (`worker.maximumTokens`), an extra safety net on top of the others. It counts input, output and cache-write tokens and is checked before each attempt.
  - Counters are saved, so a restart cannot reset them.
- At a limit or blocker, Matrix comments once with the reason, what is done, what is left and how to resume, and moves the card to **Blocked**. Nothing re-queues a Blocked card automatically.
- **Resume** by moving the card back to Todo or Doing. Committed work carries on from its branch and pull request; nothing is duplicated (pull request, merge, version bump or release).

## 4–7. Build, merge, version and release

- **Two builds per release:** the pull request build (changed plugins only) and one packaging build of everything on the version bump commit. A plain merge to `main` builds nothing. See [pipelines](pipelines.md).
- Matrix pull requests merge automatically once the exact head commit passed the required CI. Docs-only pull requests get a quick CI run that builds nothing and never trigger a release.
- Matrix's own code merges automatically through the **core lane** (branch `matrix/core/<name>`, description carrying "Matrix core change: owner-approved lane") after the hosted Matrix orchestration tests pass. Workflows, the merge gate (`scripts/matrix`), PowerShell and shell scripts, the Dockerfile, the egress allow-list and plugin source stay outside that lane and need a person. Merging does not rebuild the container.
- Every source merge to `main` bumps the third part of `VERSION` and moves the `## Unreleased` changelog notes into the new version. Major and minor increments wait for an owner decision. See [release checkpoints](automatic-release-checkpoints.md).
- Each release has exactly two public files: one Windows EXE and one macOS PKG suite installer.
- Trusted workflows hold all merge, publishing, storage and website credentials. The coding agent holds none of them.

## 8. Finishing a task

**Fully hands off.** After the merge, Matrix finishes the task itself:

1. **Verify delivery:** a GitHub release containing the merge, with exactly the two installers; private storage matching the release; the beta website showing that version and its changelog, with the deployment confirmed; and the task's canonical prompt archived in `done/`. Downloads are not tested.
2. **Comment** on the issue that the task is done, with the pull request, merge commit, version, release, installers and the **download link** (the beta website).
3. **Close** the issue.
4. **Move** the card to **Done**.

Each stage is saved. While a check is missing the issue stays open and the card stays in Doing, with one comment saying what is missing. A failed final comment blocks closing and Done; a failed close or Done is retried on its own without a second comment.

## Not proof of anything

A prompt in `done/` shows the implementation merged, not that it reached testers. A Doing card is a request, not proof that work started. Documentation describing an integration is not evidence that it runs. Issue comments are the audit trail.
