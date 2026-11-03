Work through every prompt directly in eq/prompts/roadmap/ in numeric order.
Skip the done/ subfolder: those tasks are already merged.
Ignore that folder's README.md; it describes parallel waves, which don't apply here.

Setup (once):
  git checkout main && git pull
  git checkout -b roadmap/overnight && git push -u origin roadmap/overnight

For each prompt, follow it exactly, with these overrides:
- Branch from roadmap/overnight (git pull it first), not from main.
- Open the PR with --base roadmap/overnight instead of main.
- When the build passes with no new warnings and ALL EQTests pass (old and
  new), merge that PR into roadmap/overnight (gh pr merge --merge
  --delete-branch). Then move on, so the next task builds on it.
- Never push to or merge into main.

If a task is blocked, or its tests can't be made to pass within reason:
leave its PR as a draft with a "Blocked" section, don't merge it, check out
roadmap/overnight again and carry on with the next prompt. Later tasks may
depend on a skipped one; adapt and note it in their PRs.

Don't stop until all of them have been attempted. Then:
1. On roadmap/overnight, do a clean Release build and run EQTests and
   EQTests --bench.
2. Open one draft PR from roadmap/overnight into main, containing: a table of
   each task (merged / blocked, PR link, one-line summary), the final test
   count, before/after benchmark numbers (compare against main), every design
   decision made without a human, a