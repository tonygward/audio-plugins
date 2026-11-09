# VER-01 — Standardise Songsmith suite version to 0.5.0

- Issue: https://github.com/tonygward/songsmithstudio/issues/2
- Status: ready for separately coordinated immediate execution; **not** evidence of Matrix pickup
- Execution: RUN NOW requested; cross-repository release/version work must use an explicitly authorised, reviewed path, not the functional overnight worker
- Model: Claude Sonnet; escalate to Opus for release/version migration architecture if necessary
- Repositories: `tonygward/audio-plugins`, `tonygward/songsmith-website`; coordinate with SUI-15 and WEB-03
- Scope: implementation prompt, not a completed change

## Objective and musician value
Give the full plugin suite one intelligible product version, **0.5.0**, consistently in binaries, installers, UI, changelog, website and release metadata. Retain independent build traceability.

## Current behaviour
Existing suite releases and version references are inconsistent with the owner's intended 0.5.0 baseline. The current `audio-plugins/CLAUDE.md` describes a root `VERSION` and deliberate version checkpoints; published higher version tags must not be rewritten or deleted. Verify actual `main`, tags, workflows, installer manifests and live beta metadata before changing code.

## Desired behaviour and implementation
1. Inventory actual version sources, existing semver tags, plugin identifiers, CMake/JUCE metadata, Windows and macOS suite installer versions, `VERSION`, website release JSON and generated packaging.
2. Establish one authoritative suite version of `0.5.0` for the **next intended product baseline**. Do not silently overwrite historic releases or downgrade/update a live website manifest to 0.5.0 without matching, successfully produced installer artifacts. Explicitly identify and resolve any version monotonicity, updater, DAW compatibility and distribution-policy conflict caused by existing versions higher than 0.5.0; stop and report a blocked release rather than bypassing checks.
3. Keep unique CI run numbers, commit SHAs or build IDs separate from the public semver. Routine non-release builds must not increment public semver; intentional release checkpoints follow repository policy.
4. Use unchanged manufacturer/plugin/bundle identifiers and parameter IDs. Preserve existing DAW-session loading and user presets.
5. Coordinate SUI-15 so the footer derives from the authoritative product version rather than a hardcoded duplicate. Align website labels only when corresponding real downloads exist.
6. Add clear documentation, migration notes and readable user-facing changelog, without altering unrelated DSP.

## Acceptance criteria
- All plugins and Windows/macOS installer metadata agree on 0.5.0 for the validated target build.
- Repeated builds from identical release state keep the same public version but have traceable build provenance.
- Version consistency assertions cover CMake/JUCE, installer names and metadata, release manifest and website references.
- Historic tags/assets remain immutable; no older or nonexistent release is promoted.
- Existing plugin identities and DAW saved sessions remain compatible; any unresolvable downgrade constraint is reported as **blocked**, not hidden.
- For publication, the full Windows/macOS hosted CI, exactly two suite installers, signed-off release evidence, private R2 and beta portal downloads are independently verified. No live/deployed claim before that evidence.

## Tests, dependencies and risks
- Add automated parsing/packaging tests for version consistency and release manifest guards; run existing tests and Windows/macOS CI.
- Dependencies: release scripts and `VERSION`, website dispatch/monotonicity policy, SUI-15, release tag conventions.
- High risk: existing public 1.x tags and beta release monotonicity checks may reject a 0.5.0 release. Do not retag, overwrite artifacts, disable security or circumvent provenance checks to force it.
- Delivery: separate implementation PR(s), explicit test results, version compatibility assessment, verified release checkpoint only after distribution checks.
