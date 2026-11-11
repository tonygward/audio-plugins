---
id: SUI-16
priority: 1
model: sonnet
effort: low
dependencies: ["SUI-15"]
conflicts: []
---

# SUI-16 — Version label is cut off; give it room to be fully visible

- Issue: https://github.com/tonygward/songsmithstudio/issues/4
- Implementation repository: `tonygward/audio-plugins` (shared footer, so all three plugins change together)

## Objective and user value
SUI-15 added the product version to the footer, but a musician cannot read it. Make the whole version readable so they can tell which build they are running and report it.

## Actual vs desired behaviour
**Original bug (seen in a DAW, Songsmith EQ at 100 % scale):** the bottom-left of the footer shows `v0....`, cut off with dots, before the Processing Load button. See [the screenshot](../../../docs/screenshots/sui-16-version-label-cut-off.png) (a real capture from a DAW).

**Desired:** the full text, for example `v0.5.2`, is visible without dots. It also stays fully visible when the version grows (`v0.5.10`, `v1.12.3`) and at every interface size from 75 % to 200 %. It stays directly before Processing Load, unobtrusive, in the same dim text.

## Likely cause (verify first)
`shared/Source/SongsmithDiagnostics.h`: `Bar::getVersionWidth()` sizes the label to the measured text plus 4 pixels, but a `juce::Label` also keeps its own border padding around the text, so the text area is narrower than the text and JUCE truncates it. Check this, then fix it at the source: measure the text including the label's border (or set a zero border), and add a small safety margin. Do not hardcode a pixel width for one version string.

## Work
1. Reproduce: render the footer and measure the label's text area against the text width.
2. Fix the sizing in the shared footer so all three plugins (EQ, Analog Compressor, Maximiser) get it.
3. Make sure the footer layout still works when the version label is wider: Processing Load, the CPU and latency readout and the EQ's Piano button must not overlap or be cut off at minimum and large sizes.
4. Add a plain-language bullet under `## Unreleased` in `CHANGELOG.md` (for example: the version number at the bottom left of each plugin is now fully visible; nothing you need to do).

## Acceptance criteria
- The full version text is visible with no truncation in EQ, Analog Compressor and Maximiser.
- A test fails if the label's text area is narrower than its text, and it covers long versions (for example `v10.20.30`), all three footers and the smallest and largest interface sizes.
- No overlap with Processing Load, the readout or the Piano button at any size.
- No DSP, parameter, preset or session changes. The version still comes from the one root `VERSION` (never hardcoded).
- Windows and macOS builds and all tests pass.

## Tests and evidence
Layout and width test as above. Before-and-after screenshots of the footer for each plugin committed to the PR. Not verified in a DAW unless a person does it.

## Release expectation
A patch release follows automatically from the merge. No manual version change.

## Risks
Changing footer widths could squeeze the readout at the smallest size. Check it explicitly.

## Status
Distinct restored truncation bug, not a duplicate of SUI-15. Implementation merged in [PR #177](https://github.com/tonygward/audio-plugins/pull/177). This archive prevents repeat coding. Beta publication and authenticated installer verification must be recorded on the tracking issue before completion; do not infer delivery from merge.

## Current delivery policy
Follow [automatic beta builds and issue completion](../../../docs/songsmith-execution-policy.md). Automatic approval/merge requires exact-head green checks. Keep the tracking issue open until matching beta website deployment and both installer downloads are verified; post the final completion comment, then close the issue and finally set board Done. Automatic third-component build increments apply; major/minor increments are deferred.
