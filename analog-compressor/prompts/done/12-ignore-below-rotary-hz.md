---
id: ACOMP-12
status: done
execution: scheduled
model: sonnet
effort: medium
dependencies: ["ACOMP-07"]
conflicts: []
---

# ACOMP-12 — Ignore Below rotary Hz control

**Issue:** https://github.com/tonygward/audio-plugins/issues/200
**Policy:** https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md
**Status:** Ready for implementation; the board decides when it starts.

## Objective and user value
Replace the Analog Compressor's detector Low Cut dropdown with a rotary frequency control like Input, Output, Attack and Release, and rename the visible label to **Ignore Below**. Make the controls approachable without changing the audible signal path.

## Existing and desired behaviour
ACOMP-07 implemented a detector-only filter. The existing parameter ID `detectorLowCut` is a six-choice parameter: Off, 60, 80, 100, 150, 200 Hz; Off default. Existing dropdown sits near Gain Reduction Off. Desired: compact rotary knob, readable **Ignore Below** caption, and visible `Off` or numeric `Hz` readout. Provide natural frequency adjustment including all existing values. Prefer continuous/logarithmic Hz behaviour where backward-compatible, but don't silently corrupt older sessions or host automation.

## Implementation guidance
1. Inspect `analog-compressor/Source/FETParams.h`, `FETEngine.h`, plugin processor/editor, JUCE parameter attachment and `docs/detector-low-cut.md`. Reuse the suite's established knob look, focus, keyboard and accessibility design.
2. Preserve all previously saved parameter meanings. Check saved discrete choices and host-normalised automation before changing parameter type, ID, range or normalisation. If introducing a continuous frequency parameter, devise explicit compatibility and migration with deterministic old/new precedence, test host project recall, and document any unavoidable limitations. A stepped rotary retaining six original values is a compatibility-safe fallback only when a continuous approach cannot be implemented safely; record this trade-off.
3. Display **Ignore Below** (exact label) and `Off` or `60 Hz`, `80 Hz`, etc. Tooltip: “Ignore bass below this frequency when deciding how much to compress. The bass you hear stays unchanged.”
4. Keep the detector filter at its current slope and location, with no alteration to bass in the output audio. Smooth parameter changes safely, retain stereo linking, bypass, oversampling and latency.
5. Fit the knob cleanly into the current 960 design-unit editor, away from Gain Reduction Off and footer. Test minimum and maximum sizes and high DPI. Preserve existing control names, tooltips and accessibility.
6. Update UI screenshots, concise documentation, changelog and relevant presets without unnecessary DSP redesign.

## Acceptance and tests
- No dropdown remains; Ignore Below is a circular rotary Hz control matching the main plugin knobs.
- Off is obvious, default and remains recallable. All old Off/60/80/100/150/200 projects and automated controls retain their intended meaning.
- If continuous, cutoff responds predictably and click-free across the documented Hz range; if stepped, every existing value remains accessible and test verifies labels.
- Bass passes unchanged with the filter affecting only gain-reduction detection; regression tests cover low-cut DSP, ratio and All Buttons, automation and bypass.
- Screen layouts and screenshots verified; JUCE Windows/macOS native CI passes at exact PR head.
- Trusted merge, version increment for qualifying plugin change, Windows EXE/macOS PKG installers, private R2, beta website manifest and authenticated downloads are verified before final evidence comment and issue Done.

## Dependencies, risks and delivery
ACOMP-07 implemented the detector filter; don't duplicate it. Main risk is breaking existing JUCE parameter states by changing a discrete choice into a continuous parameter. Model Sonnet. One active Matrix task, below 60% verified weekly usage; board selection controls execution. Do not start work because this prompt file exists.
