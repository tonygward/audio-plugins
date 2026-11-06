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
