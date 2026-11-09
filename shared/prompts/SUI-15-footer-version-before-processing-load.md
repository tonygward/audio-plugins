# SUI-15 — Show version before Processing Load

- Issue: https://github.com/tonygward/songsmithstudio/issues/3
- Status: ready, pending VER-01 authoritative version decision
- Execution: owner requested RUN NOW; only a real Matrix record can prove pickup
- Model: Claude Sonnet
- Implementation repository: `tonygward/audio-plugins`
- Dependency: VER-01

## Objective and user value
Make the Songsmith version visible and easy to find in every plugin without cluttering the interface.

## Current and desired behaviour
The shared bottom bar shows the Processing Load control but not a clearly consistent suite version immediately before it. Display compact text such as `v0.5.0` **to its left** on EQ, Analog Compressor and Maximiser, drawing the value from the one authoritative version source established in VER-01.

## Implementation guidance
1. Inspect shared JUCE footer layout and the actual version-definition/build generation code before editing.
2. Add a reusable, read-only suite-version label; derive the displayed value at build time from root version metadata instead of copying a literal into editors.
3. Arrange **version → Processing Load button → existing load diagnostics** at the bottom-left. Preserve spacing, click targets, on/off states, keyboard/accessibility and scaling, including small supported editors.
4. Keep version unambiguous across all available plugins; no per-plugin version drift or release-policy changes.
5. Do not touch DSP, parameter IDs, preset state or latency calculations.
6. Update appropriate shared UI documentation and include real before/after editor screenshots.

## Acceptance criteria
- Every shipped plugin editor displays the same correct suite version before Processing Load.
- Version text updates automatically after the intentional release-version change without modifying UI source.
- No overlap or clipping at supported DPI and interface scales; Processing Load still works exactly as before.
- No changes to audio or session compatibility.
- Existing UI tests and full relevant builds pass.

## Automated tests, dependencies and risks
- Extend editor layout tests for ordering, bounds at minimum/maximum supported scaling, and version-source matching.
- Run EQTests, AnalogCompressorTests, MaximiserTests and hosted platform CI required by repository policy; attach screenshots.
- Depends on VER-01; if the release version is disputed, build label integration with the current authoritative source but do not present 0.5.0 as published.
- Delivery: implementation PR, test evidence, changelog entry, and release/publish status verified separately; issue creation alone is not a queued or completed Matrix job.
