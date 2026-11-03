Work through every prompt directly in eq/prompts/roadmap/ in numeric order.
Skip the done/ subfolder: those tasks are already merged.
Ignore that folder's README.md; it describes parallel waves, which don't apply here.

Setup (once). Use a fresh integration branch named after today's date, so an
older run's branch is never reused:
  git checkout main && git pull
  git checkout -b roadmap/overnight-YYYY-MM-DD && git push -u origin roadmap/overnight-YYYY-MM-DD
Below, "the integration branch" means that branch.

For each prompt, follow it exactly, with these overrides:
- Branch from the integration branch (git pull it first), not from main.
- Where a prompt says "on main" or "merged into main" when checking that an
  earlier task is done, read it as "on the integration branch".
- Open the PR with --base <integration branch> instead of main.
- CI runs on every PR, including the macOS job. Before merging, wait for it
  with `gh pr checks --watch` and fix failures. For tasks that can only be
  tested on CI (the macOS installer), CI passing is the test.
- When the build passes with no new warnings, ALL EQTests pass (old and new)
  and CI is green, merge that PR into the integration branch
  (gh pr merge --merge --delete-branch). Then move on, so the next task builds
  on it.
- Never push to or merge into main.

If a task is blocked, or its tests can't be made to pass within reason:
leave its PR as a draft with a "Blocked" section, don't merge it, check out
the integration branch again and carry on with the next prompt. Later tasks may
depend on a skipped one; adapt and note it in their PRs.

Don't stop until all of them have been attempted. Then:
1. On the integration branch, do a clean Release build and run EQTests and
   EQTests --bench.
2. Open one draft PR from the integration branch into main, containing: a
   table of each task (merged / blocked, PR link, one-line summary), the final
   test count, before/after benchmark numbers (compare against main), every
   design decision made without a human, and a short "what to check by hand"
   list (DAW tests, screenshots to look at, anything only verified on CI).
3. Move each merged task's prompt into eq/prompts/roadmap/done/ and update the
   status table in eq/prompts/roadmap/README.md, in that same PR.
