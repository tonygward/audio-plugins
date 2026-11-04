Work through every prompt directly in the prompts folder named when this run
was started, in order: a plugin's folder (eq/prompts/, analog-compressor/prompts/,
maximiser/prompts/; numeric order) or shared/prompts/ for work across every plugin
(S1, S2, ...). If no folder was named, use eq/prompts/. Only the numbered prompt
files are tasks: skip README.md, this file, and the done/ subfolder (those tasks
are already merged). Ignore that folder's README.md wave table; it describes
parallel waves, which don't apply here. Do read its notes on order and conflicts.

Overnight runs merge straight into main. Every change still goes through a PR,
and Tony has agreed that the AI approves and merges its own PRs. (GitHub doesn't
let an account formally approve its own PR, so "approve" means: check
everything below, then merge.) Never push commits directly to main.

For each prompt, follow it exactly, with these overrides:
- Branch from an up-to-date main (git checkout main && git pull) for every
  task, so each task builds on the ones merged before it.
- Open the PR against main as the prompt says. A draft is fine while you work.
- CI runs on every PR, including the macOS jobs. Before merging, wait for it
  with `gh pr checks --watch` and fix failures. For tasks that can only be
  tested on CI (the macOS installers), CI passing is the test.
- Approve and merge only when all of these are true: the build passes with no
  new warnings, every test executable the prompt names passes (old and new
  tests), CI is green, and you have reread the diff against the prompt and
  CLAUDE.md's design principles. Then mark the PR ready (gh pr ready), add a
  PR comment headed "Approved by the overnight run" that lists what you
  checked, and merge (gh pr merge --merge --delete-branch).
- In the same PR, before merging, move the task's prompt into the folder's
  done/ subfolder and update that folder's README status table, if it has one.

If a task is blocked, or its tests can't be made to pass within reason:
leave its PR as a draft with a "Blocked" section, don't merge it, check out
main again and carry on with the next prompt. Later tasks may depend on a
skipped one; adapt and note it in their PRs.

Don't stop until all of them have been attempted. Then:
1. On main, do a clean Release build of every plugin, run every test
   executable, and run EQTests --bench.
2. Open one issue (gh issue create) titled "Overnight run YYYY-MM-DD" with: a
   table of each task (merged / blocked, PR link, one-line summary), the final
   test counts, before/after benchmark numbers, every design decision made
   without a human, and a short "what to check by hand" list (DAW tests,
   screenshots to look at, anything only verified on CI).
