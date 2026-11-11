---
id: MAX-09
status: backlog
execution: scheduled
priority: medium
model: sonnet
effort: low
dependencies: []
conflicts: [MAX-08, SUI-18]
---

# MAX-09 — Single Integrated LUFS readout beside the main dial

**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/229  
**Related:** [MAX-07 LUFS meter](https://github.com/tonygward/audio-plugins/issues/161), [MAX-08 meter styling](https://github.com/tonygward/audio-plugins/issues/224), [SUI-18 title bar](https://github.com/tonygward/audio-plugins/issues/202).  
**Policy:** [Songsmith execution policy](../../docs/songsmith-execution-policy.md).  
**Execution:** Backlog only; no pickup or work triggered by creating this prompt.

## Objective and value
Reduce unnecessary information in Songsmith Maximiser: show **only Integrated LUFS**, move that value **right of the main dial/control area (never below it)**, and add an understandable tooltip describing the value and a qualified reference target. Keep UI uncluttered and approachable.

## Existing behaviour
As currently implemented in `maximiser/Source/PluginEditor.cpp` and `PluginEditor.h`, a dedicated bottom loudness band displays Integrated, Short Term and Momentary, an explanatory caption and Reset. The underlying LUFS engine lives in `maximiser/Source/Loudness.h`. Assess the latest code and recent concurrent UI changes before altering layout.

## Desired behaviour and implementation guidance
1. The visible UI contains a single **Integrated LUFS** value and a straightforward reset affordance. Remove Short Term and Momentary labels/readouts and the bottom loudness band; reclaim the vertical space, without altering unrelated diagnostics/footer or existing UI controls.
2. Place the Integrated LUFS readout to the **right of the principal dial/primary level control**, with enough separation and alignment to feel intentional. Since the existing Maximiser editor uses threshold and ceiling faders, inspect the actual rendered layout and identify the current primary control; document the chosen placement rather than making an unverified assumption. Avoid overlap with Input/Gain Reduction/Output meters, title bar or bypass. Preserve usability at min/default/max dimensions and HiDPI.
3. Retain Integrated LUFS gating/measurement logic, displayed units, sensible placeholder while no valid programme audio has accumulated, full-program accumulation and existing reset/transport semantics. A click-to-reset value and/or small labelled Reset button are acceptable, but make the behaviour discoverable and accessible. No changes to audio-thread processing or parameter state.
4. Use a tooltip on the label/value: “Integrated LUFS is the average perceived loudness of everything played since the last reset. Play the whole song through for a representative reading. Around −14 LUFS is a useful reference for some streaming masters, not a rule: different music can sound best louder or quieter. Avoid clipping and judge by ear.” Explain streaming normalisation as guidance rather than a mandated target. Ensure tooltip can be shown via normal hover and suitable keyboard/focus behaviour where practical.
5. Retain short-term/momentary internal DSP computations only if dependencies warrant it; remove redundant UI update calls and avoid needless computation without changing public behaviour or introducing regression. Do not remove the correct integrated path, BS.1770 gating or metering test coverage.
6. Coordinate with MAX-08 styling, MAX-07 measurement requirements and SUI-18 title bar. Avoid broad UI redesign.

## Acceptance criteria and tests
- Only Integrated LUFS is visible, with clear `LUFS` units. No Short Term or Momentary UI fields.
- Reading is right of the primary dial/control, **not below it**, at minimum, normal and largest supported sizing, with no truncation or conflict with peak/GR meters.
- Tooltip explicitly covers average perceived loudness, full-track playback, how reset affects measurement, and qualified ~−14 LUFS reference without implying it is a universal standard.
- Existing integrated reading accuracy/gating, no-signal placeholder, reset, transport restart and cross-rate tests remain valid.
- Existing metering, gain reduction, limiting audio, latency, preset loading and parameter IDs remain unchanged.
- Update automated UI/layout and integrated measurement tests. Provide visual before/after screenshots and DAW sanity check. Native Windows/macOS exact-head CI must pass.
- Finish through standard trusted Matrix and release: merged PR, appropriate beta version and native EXE/PKG suite installers, verified R2 and authenticated beta downloads; final evidence comment before issue closure and board Done. Do not mark complete on CI alone.

## Dependencies and risks
Main risk is the intended position of the “dial” against the current fader-based editor. Resolve from the actual current UI, preserve the user's explicit right-side placement, and avoid moving the readout back below. Coordinate edits with MAX-08 and SUI-18 to minimise merge conflicts. Sonnet default; one global Matrix task, verified usage under 60% before pickup.
