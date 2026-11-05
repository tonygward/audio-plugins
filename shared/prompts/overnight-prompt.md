Work through every prompt directly in the prompts folder named when this run
was started, in order: a plugin's folder (eq/prompts/, analog-compressor/prompts/,
maximiser/prompts/; numeric order) or shared/prompts/ for work across every plugin
(SUI-01, SUI-02, ...). If no folder was named, use eq/prompts/. Only the numbered prompt
files are tasks: skip README.md, this file, and the done/ subfolder (those tasks
are already merged). Ignore that folder's README.md wave table; it describes
parallel waves, which don't apply here. Do read its notes on order and conflicts.

Overnight runs merge straight into main and release. Every change still goes through a PR,
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
  Keep the original scope-qualified ID (EQ-32, ACOMP-06, MAX-07, SUI-11, OPT-01, CLN-01, ADV-01),
  record its PR number and canonical relative link, and update any related roadmap
  references. If a release checkpoint is delivered, also cross-link the prompt IDs
  in CHANGELOG.md. Check that all new Markdown links resolve; never infer completion
  from a draft PR. See shared/prompts/README.md#prompt-identity-and-traceability.
- Align versions to the agreed delivery checkpoints in `shared/prompts/README.md`, following `CLAUDE.md`. Do not automatically bump VERSION for every completed implementation prompt. Intermediate PRs can merge without a version change. At a planned release checkpoint, after its agreed scope passes validation, bump the shared VERSION once and update CHANGELOG.md. If no release checkpoint is agreed, leave VERSION unchanged and record that a release decision is pending. Docs-only changes never bump VERSION.

Releasing is automatic too. Tony has agreed that every merged PR that bumps
the version is released straight away, without asking:
1. Wait for main's Build run of the merge commit (gh run list --branch main
   --event push --commit <merge sha>, then gh run watch). If it fails, fix it
   with a new PR as the next task; don't release a red build.
2. Download its installer and zip artifacts (gh run download <run id>), and
   check that every file name carries the new version.
3. Publish: gh release create vX.Y.Z --target <merge sha> --title
   "Songsmith Plugins vX.Y.Z" with those files and plain-language notes in the
   style of the previous releases (gh release view): what's new for each
   plugin, downloads, the unsigned-installer warnings, uninstalling, and
   known limitations ("Not yet verified in a recording application" when
   that's true).
Follow the Releasing section of TECHNICAL.md. There is one version for every
plugin and installer (the VERSION file), so there is one tag, vX.Y.Z, and one
release per version.

If a task is blocked, or its tests can't be made to pass within reason:
leave its PR as a draft with a "Blocked" section, don't merge it, check out
main again and carry on with the next prompt. Later tasks may depend on a
skipped one; adapt and note it in their PRs.

Don't stop until all of them have been attempted. Then:
1. On main, do a clean Release build of every plugin, run every test
   executable, and run EQTests --bench.
2. Open one issue (gh issue create) titled "Overnight run YYYY-MM-DD" with: a
   table of each task (merged / blocked, PR link, one-line summary, release
   link if one was published), the final
   test counts, before/after benchmark numbers, every design decision made
   without a human, and a short "what to check by hand" list (DAW tests,
   screenshots to look at, anything only verified on CI).
