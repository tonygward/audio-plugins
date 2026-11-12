# Pipelines: which are CI and which are CD

Every GitHub Actions workflow in this repository starts its name with what it is, so the Actions tab sorts and reads at a glance.

| Prefix | Meaning |
|---|---|
| **CI** | Continuous integration: builds and tests a change. Nothing is published. |
| **Gate** | Decides whether a change that passed CI may merge. The hand-off from CI to CD. |
| **CD** | Continuous delivery: versions, publishes and distributes what is on `main`. |
| **Housekeeping** | Keeps the repository tidy. Neither CI nor CD. |

| Workflow (as shown in Actions) | File | Runs when | What it does |
|---|---|---|---|
| **CI - Build and test** | `build.yml` | Pull requests, and the version bump commit on `main` | On a pull request: builds and tests only the plugins the change touches (a docs-only change builds nothing but still gets a quick run, so Matrix and the merge gate always have a result). On the bump commit: the one packaging build of all three plugins and both suite installers on Windows and macOS, which the release is made from. A plain merge builds nothing. |
| **CI - Matrix orchestration tests** | `matrix-tests.yml` | Changes to Matrix's own code | Runs Matrix's unit tests on Windows, macOS and Linux. |
| **CI - Release policy tests** | `auto-release-policy-checks.yml` | Changes to the release scripts or workflows | Tests the version and release rules. |
| **Gate - Matrix auto-merge** | `matrix-auto-merge.yml` | After a CI run completes | Merges a Matrix pull request once its CI passed for the exact head commit and every rule holds. Matrix's own code uses the core lane (see the [Matrix README](../tools/matrix/README.md)). |
| **CD - Increment beta version after a source merge** | `auto-beta-version.yml` | Straight after a source merge to `main` | Checks the pull request build passed at the merged head (waiting up to 30 minutes if it is still running), then increases the third part of `VERSION` and moves the changelog notes into the new release. |
| **CD - Publish validated suite release** | `auto-publish-release.yml` | After a build on `main` completes | Creates the GitHub release with exactly the two installers when the packaging build passed. A build that deliberately built nothing is a quiet no-op. |
| **CD - Distribute published beta release to R2** | `distribute-beta-r2.yml` | When a release is published | Copies the release to private storage for the beta website. The website deploy itself is in the website repository. |
| **Housekeeping - Archive merged prompt** | `matrix-archive-prompt.yml` | After a Matrix pull request merges | Moves the finished prompt into its `done/` folder. |

## The two paths

- **A plugin change** has **two builds**: CI - Build and test on the pull request (changed plugins only), then Gate - Matrix auto-merge, then CD - Increment beta version straight after the merge, then CI - Build and test once more on the bump commit (the packaging build of everything), then CD - Publish validated suite release, then CD - Distribute to R2. See [automatic release checkpoints](automatic-release-checkpoints.md).
- **A change to Matrix's own code:** CI - Matrix orchestration tests, then Gate - Matrix auto-merge. There is no release.

## Names other things depend on

Some code matches on these names, so they are not cosmetic:

- The merge gate (`scripts/matrix/automerge.mjs`) checks the run is `BUILD_RUN` (CI - Build and test) or `CORE_RUN` (CI - Matrix orchestration tests).
- The CD and gate workflows start from `workflow_run` triggers that list a CI workflow by name.
- The Matrix worker looks up its build by file (`build.yml`), and the release scripts check the file path, so those do not depend on the name.

A test in `test/matrix-automerge.test.mjs` fails if a workflow is not named by kind, if a trigger names a workflow that does not exist, or if the merge gate's names drift from the files. When you rename a workflow, change the trigger lists and those constants in the same pull request.
