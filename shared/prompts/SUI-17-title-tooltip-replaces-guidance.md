---
id: SUI-17
model: sonnet
effort: medium
dependencies: []
conflicts: []
---

# SUI-17 — Replace Guidance button with tooltips on plugin titles

**Issue:** [#201](https://github.com/tonygward/audio-plugins/issues/201)  
**Related existing work:** [ADV-01 #171](https://github.com/tonygward/audio-plugins/issues/171) (implemented; do not recode); [SUI-18 #202](https://github.com/tonygward/audio-plugins/issues/202) (shared EQ-style title-bar layout and Undo/Redo).  
**Policy:** [Songsmith execution and delivery](../../docs/songsmith-execution-policy.md)  

## Objective and user value

Remove the visually intrusive **Guidance** button and its expanded card from **Songsmith EQ**, **Songsmith Analog Compressor** and **Songsmith Maximiser**. Move the useful high-level explanation (e.g. “What It Does”) into a concise tooltip triggered by hovering over the existing plugin name/title, with keyboard-focus and accessible equivalents.

## Existing behaviour

ADV-01 added an independent Guidance button for each plugin, backed by `shared/Source/SongsmithGuidance.h` and `shared/docs/guidance.md`. The card may show headings **Measured**, **What It Does**, **What To Listen For**, **Try This** and a Guidance preference toggle. It is placed beside EQ's frequency guide, Analog Compressor gain-reduction readout and Maximiser loudness band. Existing editor title is sometimes painted rather than a JUCE component (e.g. EQ's `titleBounds`). Do not mistake an arbitrary tooltip over the entire editor for an actual title hit target.

## Desired behaviour and implementation guidance

1. Inspect `eq/Source/PluginEditor.*`, `analog-compressor/Source/PluginEditor.*`, `maximiser/Source/PluginEditor.*`, the shared guidance component, tests and `shared/docs/guidance.md`. Determine whether each title is painted, a JUCE Label or another component. Add a specific title-associated hit target/tooltip without changing the appearance of the title. Account for scaled editors and focus/keyboard accessibility.
2. Remove the **Guidance** buttons and their floating cards completely in all three editors; reclaim freed layout space while keeping other widgets aligned. Do not replace them with a different global Guidance button.
3. Provide concise, static, plugin-specific title tooltip text that answers what the plugin does and optionally one listening suggestion. Suggested meaning, to be polished and validated:
   - **Songsmith EQ:** “Shape the tone by boosting or reducing selected frequencies. Make small changes while listening to the whole mix.”
   - **Songsmith Analog Compressor:** “Evens out louder and quieter parts and can add analogue-style character. Listen for lost punch if you compress too hard.”
   - **Songsmith Maximiser:** “Controls peaks and raises loudness. Compare at the same listening level so louder doesn't automatically seem better.”
   Do not claim spectral analysis diagnoses muddiness, promise a universal LUFS target or imply measured input properties when there is no valid meter evidence.
4. Keep tooltips readable and not overlong: no multi-section card squeezed verbatim into a tiny tooltip. If small “What To Listen For” or “Try This” context can fit use it; otherwise prioritise the plain-English description.
5. Review what to do with the existing **Measured** advice: do not replace live measurements with a static title tooltip. Keep genuine numerical metering in its original meter/control display, and remove unused guidance-only calculations. Do not add another overlay unless specifically justified by existing product behaviour. Preserve DSP and the audio thread's current computational load.
6. Carefully remove or deprecate the guidance-only `Guidance.xml` preference and UI plumbing without touching saved DAW session/preset state. No unexpected modal, standalone preference, stale tooltip or unused focus target after removal. If shared guidance code remains for other concrete uses, document those consumers.
7. **Maximiser preset title-bar requirement:** Move the existing Maximiser `startingPointBox` / preset selector from the lower editor into the title bar, immediately after **Songsmith Maximiser**, matching EQ's name → preset placement and preserving available starting points, selection, session state and accessibility. Keep the preset chooser visible after Guidance is removed; do not substitute a tooltip for presets. Coordinate the same work already specified by **SUI-18**, so it is implemented **once**, not duplicated or undone by either story. Ensure no overlap with the plugin-title tooltip hit target or the intended eventual **Name | Preset | Undo | Redo | Bypass** layout. SUI-18 owns the broader suite-wide header consistency/Undo/Redo scope.
8. Update `shared/docs/guidance.md`, plugin documentation, tooltip conventions, automated editor/shared tests and screenshots. Preserve ratio controls, frequency guide, loudness meter, bypass, Processing Load and individual parameter tooltips.

## Acceptance criteria and automated tests

- All three plugins have **no Guidance button/card**.
- The existing plugin title itself has a discoverable concise tooltip; hovering over adjacent blank space does not show the tooltip. Keyboard users/accessibility clients can obtain equivalent information.
- Title text and target stay correctly aligned on each supported UI scale and on Windows/macOS. No overlap/clipping or appearance regression.
- Tooltip copy is truthful, plain-English, appropriate to plugin purpose and independent of live meter validity. No fabricated measurement or automated sound claims.
- Maximiser's existing preset selector is in its title bar immediately after the title and works exactly as before; this is consistent with SUI-18 and is not implemented twice. Existing per-control tooltips and useful measurements still work; no presets, automation IDs, processing or audio output change.
- Unit and UI tests and current-head Windows/macOS CI pass. Retain before/after screenshots and DAW host check notes.
- Automatic trusted merge followed by qualifying beta version increment, EXE/PKG suite installers, private R2 distribution and verified live beta website/downloads. Post final issue evidence comment before closing and marking board Done.

## Dependencies, risks and delivery expectations

Functional suite UX refinement after ADV-01; no new independent guidance engine. Biggest risks are tooltip hit regions for painted/scaled titles, unintended removal of metering and backwards compatibility of UI-only preferences. Use Sonnet by default, scoped worktree, bounded task budget, one Matrix task and verified weekly usage below 60%. No plugin changes until the board actually requests execution.
