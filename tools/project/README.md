# Songsmith Development — GitHub Project

This is the **shared portfolio board** across `audio-plugins`, `songsmith-website` and `songsmithstudio`. GitHub Issues remain traceability/tracking; canonical Markdown prompts remain Matrix's work specifications.

## One-time setup

1. Install [GitHub CLI](https://cli.github.com/) and sign in: `gh auth login`.
2. Authorise GitHub Projects: `gh auth refresh -s project`.
3. Run `node tools/project/setup-board.mjs` from any checkout of this repository.

The script is safe to rerun: it reuses a project named **Songsmith Development**, adds any missing fields and links the named issue/PR cards.

Open the project URL printed by the script.

> **Out of date:** this script was the one-time seeding tool. It still creates the old **Stage** and **Execution** fields, which Matrix no longer reads. Use the board as described below; delete those two fields on the board.

## How the board is used

Matrix reads the built-in **Status** field. The [execution policy](../../docs/songsmith-execution-policy.md#2-selection-what-runs-next) owns the rules; in short:

| Column | Meaning |
|---|---|
| **Backlog** | Not ready. Matrix never picks it up. |
| **Todo** | The overnight queue, in board order: one card per day. |
| **Doing** | Run now (picked up at the next worker run, ahead of Todo), or being worked on. |
| **Blocked** | Stopped with a reason in the issue. Move it back to Todo or Doing to resume. |
| **Done** | Delivered: Matrix moves it here after the final comment and closing the issue. |

Other fields (**Priority**, **Product**, **Matrix Job**, **Release Version**) are for people; Matrix does not read them.

## Growing the board

Add new cards for each new issue, maintaining one canonical prompt per backlog ID. Extend the `seed` array in `setup-board.mjs` when bulk onboarding existing work. Do not duplicate issues, nor treat linked PRs as new backlog requirements.

## Scope

This setup is a manually invoked project-configuration utility. It requires personal project permissions and intentionally does not run inside unattended Matrix functional jobs.

## One ticket per feature

Track a requested change as **one GitHub Issue card**. Link its implementation PR(s) inside that issue; do not add those PRs as separate board cards. The setup script removes the previously seeded duplicate version-alignment PR #152 card while retaining the canonical VER-01 issue. A merged PR is not necessarily a released or deployed change.
