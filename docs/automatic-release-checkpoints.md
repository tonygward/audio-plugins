# Automated Songsmith release checkpoints

How a merged change becomes a version, a release and a beta website update. The rules themselves are in the [execution policy](songsmith-execution-policy.md); which workflows are CI and which are CD is in [pipelines](pipelines.md).

## How it works

**Two builds per release.**

1. **The pull request build** (`CI - Build and test`, `build.yml`) builds and tests only the plugins the pull request changed. A change to shared code, the installers or the workflows builds everything.
2. **The packaging build** is one full build of everything (all three plugins on Windows and macOS, plus both suite installers). It runs on the version bump commit, and the release is made from it.

A plain merge to `main` builds nothing. The version bump follows the merge directly (see below), and the bump commit is the packaging build. Once **all**
platform/plugin/suite jobs of that build succeed, `auto-publish-release.yml`
examines the exact Build run. `scripts/release/plan.mjs` makes the call on each push to `main`:
a bump commit builds; a source merge builds nothing, because its bump always follows (the planner still builds when it cannot tell whether `VERSION` is released).
When `AUTO_BETA_VERSION_ENABLED` is off, every source push builds in full, as before.

**A new canonical version in root `VERSION` and a nonempty, unique matching
`## X.Y.Z` section in root `CHANGELOG.md` constitute the release checkpoint.**
Each source merge to `main` advances the patch number, as described [below](#automatic-beta-version-bump). Major/minor increments wait for an owner decision. Pull request builds, retries and docs/prompt-only changes do not create releases.

Only if `AUTO_RELEASE_ENABLED=true`, the Build run is successful from a first
attempt on `main`, the run matches current main HEAD, and all Windows/macOS
suite and plugin jobs passed, the workflow downloads the two tested suite
installers from that exact CI run. It refuses missing/extra/empty assets and
any existing `vX.Y.Z` tag.

A scoped `RELEASE_PUBLISH_TOKEN` creates a **draft GitHub Release with both
assets attached**, then publishes it. The **published** event automatically
triggers `distribute-beta-r2.yml` (already enabled by
`BETA_DISTRIBUTION_ENABLED=true`); the default workflow `GITHUB_TOKEN`
cannot reliably trigger that downstream workflow because GitHub suppresses
many workflow events caused by it.

The R2 distributor independently verifies complete GitHub release, full main
CI, source asset bytes, immutable private R2 objects and SHA-256, then
dispatches to the website. The website checks again and publishes
`release.json` + `changelog.md` straight to its `main` branch (the commit
is titled "Publish verified beta X.Y.Z"), and Cloudflare Pages deploys it.
**No manual merge is needed: the whole path is hands off.**

## Required one-time GitHub configuration

In `tonygward/audio-plugins` Actions repository secrets, create
`RELEASE_PUBLISH_TOKEN`: a **fine-grained GitHub PAT** with resource owner
`tonygward`, access to **only** `audio-plugins`, repository permission
**Contents: Read and write** and **Pull requests: Read and write** (Metadata
read is automatically included), and a short documented expiration (e.g. 90
days). Pull requests access is there because `matrix-auto-merge.yml` uses the
same token to comment on, mark ready and merge Matrix pull requests, so the
merge starts the Build and release workflows. No other repo scope and no
classic PAT. Never paste its value into code, issues or chat.

In Actions repository variables, set `AUTO_RELEASE_ENABLED=true` **after**
the initial controlled test and token verification. Existing distribution
switches `BETA_DISTRIBUTION_ENABLED` and
`BETA_WEBSITE_RELEASE_ENABLED` govern the following two stages separately.

## Failure and recovery

If a Build job fails/skips, the version isn't new, the run is stale, an
artifact is missing or tag exists: fail or skip without publishing.

If GitHub Release creation fails after draft creation, it remains **draft**.
An operator must inspect and repair or discard it; automation never
overwrites existing tags or releases. Never republish a mutated asset under
an existing version.

For emergency pause set `AUTO_RELEASE_ENABLED=false` and, if necessary,
`BETA_DISTRIBUTION_ENABLED=false`. Cancel already running jobs separately.
All credentials should be rotated before expiry or after any suspected leak.

## Automatic beta version bump

When `AUTO_BETA_VERSION_ENABLED=true`, every source merge to `main` is followed straight away (`auto-beta-version.yml`, on the push itself) by a `[beta-version]` commit that raises the patch number and moves the changelog notes into the new version. It bumps only if a merged pull request for that commit had a **successful pull request Build at exactly the head that merged**, and only if `main` has not moved on to a later source commit. That commit then runs the packaging build, and the release is published from it.

**A merge while the previous version is still unreleased** (its packaging build is running, or it failed) is bumped too. The bump folds that version's changelog section into the new one, so every note ships in the release that actually contains the change, and that version is never published: its build is superseded, and publishing treats a build whose `VERSION` `main` has moved past as a quiet no-op. Version numbers can therefore skip (0.5.15 to 0.5.17), which is expected. Before 13 November 2026 such a merge was not bumped: it shipped inside the unreleased version while its notes stayed under Unreleased (this is how ACOMP-14 and ACOMP-15 went out in 0.5.15 with the wrong notes).

The trade-off: the version number is committed before the packaging build proves the combined result. If that build fails, the version is used up with no release; the next source merge bumps past it and carries its notes forward.

### Release notes are for musicians

Notes on the website and the release page come from the `CHANGELOG.md` section for that version. Write them in plain words about what a musician sees or hears, not commit hashes or pull request titles. When a PR changes something users will notice, add bullets under a `## Unreleased` heading at the top of `CHANGELOG.md`; the automatic bump moves that text into the new version's section. If there is no such text, the notes say: "Behind-the-scenes improvements and fixes. There is nothing you need to do."

## Matrix pull requests merge and archive themselves

With `MATRIX_AUTOMERGE_ENABLED=true`, two workflows close the loop after the Matrix worker opens a draft pull request:

- **Matrix auto-merge** (`matrix-auto-merge.yml`) runs after **CI - Build and test** (Windows and macOS) succeeds for a `matrix/*` pull request from this repository. Pull requests on a `matrix/core/<name>` branch use the core lane instead, after **CI - Matrix orchestration tests** (see the [Matrix README](../tools/matrix/README.md#matrix-core-lane-auto-merge-for-matrixs-own-code)). It checks, from `main`'s own scripts, that the build was for the exact head commit, that every changed file is inside the worker's permitted source, test, documentation, README or `CHANGELOG.md` scope, and that the independently required tests and native CI have passed. The Linux worker may delegate native validation to hosted CI; never fabricate a local build. The one block it accepts is the worker's "UI changes need screenshots and visual review" note, because the owner chose hands-off merging of UI work. It then comments with what it checked, marks the draft ready and squash-merges it. The usual beta bump and publish pipeline follows.
- **Matrix archive merged prompt** (`matrix-archive-prompt.yml`) runs on `main` after such a merge and moves the prompt into its `done/` folder (the folder is the record; prompts carry no status or execution, which the board owns). This archive records merged implementation and prevents repeat coding; it is not delivery.

Logic and tests: `scripts/matrix/automerge.mjs`, `test/matrix-automerge.test.mjs`. Set `MATRIX_AUTOMERGE_ENABLED=false` to stop both. Not covered: a person did not see the interface before the merge, and nothing was checked in a DAW; check the beta build.

## Historical version 0.5.0 reset (VER-01)

The suite version was deliberately lowered from 1.3.x to **0.5.0** because the product is not finished. Nothing else about versioning changed:
each later source merge still bumps the patch number automatically (0.5.1, 0.5.2 and so on, `AUTO_BETA_VERSION_ENABLED=true`). Major and minor increments are deferred until an explicit new owner decision. Per-build identity stays in the CI run number and commit SHA.

Installers: the Windows installer replaces any installed version, so 0.5.0 installs over 1.3.x. The macOS component packages are built with
`BundleIsVersionChecked` off for the same reason; without it Installer keeps a newer 1.x bundle. The earlier 1.x GitHub releases and tags were removed as redundant (beta software), which also frees old tags so a version number is never blocked by one.

## Finishing the task

After the release reaches the beta website, Matrix posts the final comment with the download link, closes the issue and moves the card to Done. See [Finishing a task](songsmith-execution-policy.md#8-finishing-a-task).
