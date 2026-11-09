# Automated Songsmith release checkpoints (AUTO-01 Phase D hand-off)

## How it works

A successful source merge to `main` starts the existing full Windows/macOS
`Build` workflow when the changed paths match its build filters. Once **all**
platform/plugin/suite jobs succeed, the new `auto-publish-release.yml`
workflow examines the exact Build run.

**A new canonical version in root `VERSION` and a nonempty, unique matching
`## X.Y.Z` section in root `CHANGELOG.md` constitute the release checkpoint.**
There is no new release for routine merges while VERSION is unchanged.

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
dispatches to the website. The website checks again and creates one
`release.json` + `changelog.md` PR. **Tony still merges the website PR
manually** on GitHub Free: no automatic website deployment before that merge.

## Required one-time GitHub configuration

In `tonygward/audio-plugins` Actions repository secrets, create
`RELEASE_PUBLISH_TOKEN`: a **fine-grained GitHub PAT** with resource owner
`tonygward`, access to **only** `audio-plugins`, repository permission
**Contents: Read and write** (Metadata read is automatically included), and
a short documented expiration (e.g. 90 days). No other repo scope and no
classic PAT. Never paste its value into code, issues or chat.

In Actions repository variables, set `AUTO_RELEASE_ENABLED=true` **after**
the initial controlled test and token verification. Existing distribution
switches `BETA_DISTRIBUTION_ENABLED` and
`BETA_WEBSITE_RELEASE_ENABLED` govern the following two stages separately.

**Required on each release checkpoint:** update root VERSION to a new
semver X.Y.Z, add matching dated/reviewed CHANGELOG section, and merge
the version bump together with code changes via PR. A docs-only VERSION
change lacking full Build trigger will **not** publish; use a source-affecting
change on that checkpoint, or deliberately run full Build with a new
source merge. Never force/reuse existing tags or publish partial artifacts.

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

This does **not** automatically operate Matrix, implement prompts, merge PRs
or choose release scope. AUTO-01's live controlled worker and automatic
merging are separate phases. It **does** close the missing link between an
approved merge/version checkpoint and GitHub Release publishing.

## Automatic beta version bump

When `AUTO_BETA_VERSION_ENABLED=true`, every successful first-attempt `Build` push on `main` whose current `VERSION` is already tagged is followed by a `[beta-version]` commit that raises the patch number and adds a changelog entry. That commit builds again, and the publish workflow then releases it. Commits that only change documentation, prompts or screenshots (the paths `build.yml` ignores) landing after a successful build no longer make that build "stale": the bump is applied on top of them and a release is still published. A later *source* commit still supersedes the build, and its own build takes over. Pause with `AUTO_BETA_VERSION_ENABLED=false`.

### Release notes are for musicians

Notes on the website and the release page come from the `CHANGELOG.md` section for that version. Write them in plain words about what a musician sees or hears, not commit hashes or pull request titles. When a PR changes something users will notice, add bullets under a `## Unreleased` heading at the top of `CHANGELOG.md`; the automatic bump moves that text into the new version's section. If there is no such text, the notes say: "Behind-the-scenes improvements and fixes. There is nothing you need to do."

## Matrix pull requests merge and archive themselves

With `MATRIX_AUTOMERGE_ENABLED=true`, two workflows close the loop after the Matrix worker opens a draft pull request:

- **Matrix auto-merge** (`matrix-auto-merge.yml`) runs after the hosted `Build` (Windows and macOS) succeeds for a `matrix/*` pull request from this repository. It checks, from `main`'s own scripts, that the build was for the exact head commit, that every changed file is inside the worker's permitted source, test, documentation, README or `CHANGELOG.md` scope, and that the worker recorded a clean local build and test run. The one block it accepts is the worker's "UI changes need screenshots and visual review" note, because the owner chose hands-off merging of UI work. It then comments with what it checked, marks the draft ready and squash-merges it. The usual beta bump and publish pipeline follows.
- **Matrix archive merged prompt** (`matrix-archive-prompt.yml`) runs on `main` after such a merge and moves the prompt into its `done/` folder with `status: done`. A prompt is therefore only "done" once its work is in `main`, and prompts that depend on it become available.

Logic and tests: `scripts/matrix/automerge.mjs`, `test/matrix-automerge.test.mjs`. Set `MATRIX_AUTOMERGE_ENABLED=false` to stop both. Not covered: a person did not see the interface before the merge, and nothing was checked in a DAW; check the beta build.

## Version 0.5.0 reset (SUI-15)

The suite version was deliberately lowered from 1.3.x to **0.5.0** because the product is not finished. Nothing else about versioning changed:
each later source merge still bumps the patch number automatically (0.5.1, 0.5.2 and so on, `AUTO_BETA_VERSION_ENABLED=true`). A bigger
step (for example 0.6.0 or 1.0.0) is made only when a ticket asks for it, by changing `VERSION` and adding the matching `## X.Y.Z` changelog
section. Per-build identity stays in the CI run number and commit SHA.

Installers: the Windows installer replaces any installed version, so 0.5.0 installs over 1.3.x. The macOS component packages are built with
`BundleIsVersionChecked` off for the same reason; without it Installer keeps a newer 1.x bundle. Existing 1.x releases and tags stay as published.
