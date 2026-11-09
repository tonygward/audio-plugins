# Songsmith Development — GitHub Project

This is the **shared portfolio board** across `audio-plugins`, `songsmith-website` and `songsmithstudio`. GitHub Issues remain traceability/tracking; canonical Markdown prompts remain Matrix's work specifications.

## One-time setup

1. Install [GitHub CLI](https://cli.github.com/) and sign in: `gh auth login`.
2. Authorise GitHub Projects: `gh auth refresh -s project`.
3. Run `node tools/project/setup-board.mjs` from any checkout of this repository.

The script is safe to rerun: it reuses a project named **Songsmith Development**, adds any missing fields and links the named issue/PR cards.

Open the project URL printed by the script. Use the GitHub UI to create/select a **Board** view, group by **Stage**, and set up columns **Backlog → Ready → Running → Done**. You can add new issue cards from any linked repository at any time. The script does **not** automate Matrix pickup or deployment.

## Board fields

- **Stage:** Backlog, Ready, Running, Done
- **Priority:** High, Medium, Low
- **Product:** Suite, Suite UI, EQ, Analog Compressor, Maximiser, Beta Website, Matrix, Release, Other
- **Execution:** Run Now, Scheduled, Deferred, Separate, Completed
- **Matrix Job:** optional actual worker job ID, never fabricated
- **Release Version:** product/version target, not evidence of a published release

Only move a card to **Running** with verified worker/job activity. Mark **Done** when the scope is actually merged, recording the PR. Track release/deployment separately from implementation completion.

## Growing the board

Add new cards for each new issue, maintaining one canonical prompt per backlog ID. Extend the `seed` array in `setup-board.mjs` when bulk onboarding existing work. Do not duplicate issues, nor treat linked PRs as new backlog requirements.

## Scope

This setup is a manually invoked project-configuration utility. It requires personal project permissions and intentionally does not run inside unattended Matrix functional jobs.

## One ticket per feature

Track a requested change as **one GitHub Issue card**. Link its implementation PR(s) inside that issue; do not add those PRs as separate board cards. The setup script removes the previously seeded duplicate version-alignment PR #152 card while retaining the canonical VER-01 issue. A merged PR is not necessarily a released or deployed change.
