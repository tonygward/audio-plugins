# VER-01 — Standardise Songsmith suite version to 0.5.0

- Issue: https://github.com/tonygward/songsmithstudio/issues/2
- Status: implementation merged in [PR #152](https://github.com/tonygward/audio-plugins/pull/152); delivery tracked on the issue, not inferred from this archive.
- Execution: archived implementation; do not re-run the 0.5.0 reset. Later beta build numbers advance automatically.
- Model: Claude Sonnet; escalate to Opus for release/version migration architecture if necessary
- Repositories: `tonygward/audio-plugins`, `tonygward/songsmith-website`; coordinate with SUI-15 and WEB-03
- Scope: historical 0.5.0 baseline implementation; current version policy below supersedes the original fixed-build requirement.

## Objective and musician value
Give the full plugin suite one intelligible product version, **0.5.0** as the historical reset baseline, consistently in binaries, installers, UI, changelog, website and release metadata. Retain independent build traceability.

## Current behaviour
Existing suite releases and version references are inconsistent with the owner's intended 0.5.0 baseline. The current `audio-plugins/CLAUDE.md` describes a root `VERSION` and deliberate version checkpoints; published higher version tags must not be rewritten or deleted. Verify actual `main`, tags, workflows, installer manifests and live beta metadata before changing code.

## Desired behaviour and implementation
1. Inventory actual version sources, existing semver tags, plugin identifiers, CMake/JUCE metadata, Windows and macOS suite installer versions, `VERSION`, website release JSON and generated packaging.
2. Establish one authoritative suite version of `0.5.0` for the **next intended product baseline**. Do not silently overwrite historic releases or downgrade/update a live website manifest to 0.5.0 without matching, successfully produced installer artifacts. Explicitly identify and resolve any version monotonicity, updater, DAW compatibility and distribution-policy conflict caused by existing versions higher than 0.5.0; stop and report a blocked release rather than bypassing checks.
3. Keep CI run numbers and commit SHAs as provenance. Trusted automation increments the beta third-component build/patch number on qualifying successful main builds. Major/minor increments are deferred; do not reset subsequent releases to 0.5.0.
4. Use unchanged manufacturer/plugin/bundle identifiers and parameter IDs. Preserve existing DAW-session loading and user presets.
5. Coordinate SUI-15 so the footer derives from the authoritative product version rather than a hardcoded duplicate. Align website labels only when corresponding real downloads exist.
6. Add clear documentation, migration notes and readable user-facing changelog, without altering unrelated DSP.

## Acceptance criteria
- All plugins and Windows/macOS installer metadata agree on 0.5.0 for the validated target build.
- Qualifying new main development builds advance the beta build/patch counter exactly once; retries and PR-only/docs-only builds do not double-increment.
- Version consistency assertions cover CMake/JUCE, installer names and metadata, release manifest and website references.
- Historic tags/assets remain immutable; no older or nonexistent release is promoted.
- Existing plugin identities and DAW saved sessions remain compatible; any unresolvable downgrade constraint is reported as **blocked**, not hidden.
- For publication, the full Windows/macOS hosted CI, exactly two suite installers, signed-off release evidence, private R2 and beta portal downloads are independently verified. No live/deployed claim before that evidence.

## Tests, dependencies and risks
- Add automated parsing/packaging tests for version consistency and release manifest guards; run existing tests and Windows/macOS CI.
- Dependencies: release scripts and `VERSION`, website dispatch/monotonicity policy, SUI-15, release tag conventions.
- High risk: existing public 1.x tags and beta release monotonicity checks may reject a 0.5.0 release. Do not retag, overwrite artifacts, disable security or circumvent provenance checks to force it.
- Delivery: separate implementation PR(s), explicit test results, version compatibility assessment, verified release checkpoint only after distribution checks.

## Current delivery policy
Follow [automatic beta builds and issue completion](../../../docs/songsmith-execution-policy.md). Automatic approval/merge requires exact-head green checks. Keep the tracking issue open until matching beta website deployment and both installer downloads are verified; post the final completion comment, then close the issue and finally set board Done. Automatic third-component build increments apply; major/minor increments are deferred.


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the prompt is deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This archived implementation remains historical and must not be requeued; reconcile delivery evidence only.
