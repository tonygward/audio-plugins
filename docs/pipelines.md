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
| **CI - Build and test** | `build.yml` | Pull requests, and pushes to `main` that change plugin, installer or workflow files | Builds and tests every plugin and both suite installers on Windows and macOS. On `main` it also produces the installers a release is made from. |
| **CI - Matrix orchestration tests** | `matrix-tests.yml` | Changes to Matrix's own code | Runs Matrix's unit tests on Windows, macOS and Linux. |
| **CI - Release policy tests** | `auto-release-policy-checks.yml` | Changes to the release scripts or workflows | Tests the version and release rules. |
| **Gate - Matrix auto-merge** | `matrix-auto-merge.yml` | After a CI run completes | Merges a Matrix pull request once its CI passed for the exact head commit and every rule holds. Matrix's own code uses the core lane (see the [Matrix README](../tools/matrix/README.md)). |
| **CD - Increment beta version after a successful build** | `auto-beta-version.yml` | After a successful CI build on `main` | Increases the third part of `VERSION` and moves the changelog notes into the new release. |
| **CD - Publish validated suite release** | `auto-publish-release.yml` | After the build for that version passes | Creates the GitHub release with exactly the two installers. |
| **CD - Distribute published beta release to R2** | `distribute-beta-r2.yml` | When a release is published | Copies the release to private storage for the beta website. The website deploy itself is in the website repository. |
| **Housekeeping - Archive merged prompt** | `matrix-archive-prompt.yml` | After a Matrix pull request merges | Moves the finished prompt into its `done/` folder. |

## The two paths

- **A plugin change:** CI - Build and test, then Gate - Matrix auto-merge, then (on `main`) CI - Build and test again, then CD - Increment beta version, then CD - Publish validated suite release, then CD - Distribute to R2. See [automatic release checkpoints](automatic-release-checkpoints.md).
- **A change to Matrix's own code:** CI - Matrix orchestration tests, then Gate - Matrix auto-merge. There is no release.

## Names other things depend on

Some code matches on these names, so they are not cosmetic:

- The merge gate (`scripts/matrix/automerge.mjs`) checks the run is `BUILD_RUN` (CI - Build and test) or `CORE_RUN` (CI - Matrix orchestration tests).
- The CD and gate workflows start from `workflow_run` triggers that list a CI workflow by name.
- The Matrix worker looks up its build by file (`build.yml`), and the release scripts check the file path, so those do not depend on the name.

A test in `test/matrix-automerge.test.mjs` fails if a workflow is not named by kind, if a trigger names a workflow that does not exist, or if the merge gate's names drift from the files. When you rename a workflow, change the trigger lists and those constants in the same pull request.
