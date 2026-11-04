Work through every prompt in maximizer/prompts/ in numeric order (01, 02, ...).
Skip this file (overnight-prompt.md).
Put all plugin code in maximizer/ (CMakeLists.txt, Source/, Tests/), following
the layout of eq/. Call the test executable MaximizerTests.

Setup (once). Use a fresh integration branch named after today's date, so an
older run's branch is never reused:
  git fetch origin
  git checkout -b roadmap/maximizer-YYYY-MM-DD origin/main
  git push -u origin roadmap/maximizer-YYYY-MM-DD
(Don't check out main itself: it may be in use by another worktree.)
Below, "the integration branch" means that branch.

For each prompt, follow it exactly, with these overrides:
- Ignore "stop after this step". When a step is merged, go on to the next one.
- Branch from the integration branch (git pull it first), not from main.
- Read "after the preceding step merges" / "start from main" as "merged into
  the integration branch".
- Open the PR with --base <integration branch>, not main. Open it as a draft,
  then mark it ready (gh pr ready) once it passes.
- Only edit files inside maximizer/. The one exception is
  .github/workflows/build.yml, and only to add the maximizer's build and test
  jobs. Don't touch eq/, analogue-compressor/ or the root README.
- CI runs on every PR, including macOS. Wait for it with
  `gh pr checks --watch` and fix any failures.
- When the build has no new warnings, ALL MaximizerTests pass (old and new),
  EQTests still pass and CI is green, merge into the integration branch with
  `gh pr merge --merge --delete-branch`, as a standalone command. Then move
  on, so the next step builds on it.
- Never push to or merge into main.

If a step is blocked, or its tests can't be made to pass within reason: leave
its PR as a draft with a "Blocked" section, don't merge it, check out the
integration branch again and carry on. Later steps may depend on a skipped one;
adapt and note it in their PRs.

Don't stop until every prompt has been attempted. Then:
1. On the integration branch, do a clean Release build, run MaximizerTests and
   EQTests, and check that nothing in eq/ changed.
2. Open one draft PR from the integration branch into main, containing: a table
   of each step (merged / blocked, PR link, one-line summary), the final test
   count, every design decision made without a human (look-ahead time, release
   behaviour, dither choice, etc.), and a short "what to check by hand" list
   (Studio One smoke test, listening/loudness checks, true-peak readings,
   anything only verified on CI).
