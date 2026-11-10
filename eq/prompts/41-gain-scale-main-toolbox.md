---
id: EQ-41
status: planned
execution: scheduled
model: sonnet
effort: small
dependencies: []
conflicts: []
---

# EQ-41 — Move Gain Scale from title bar to main EQ toolbox

**Issue:** [EQ-41 #203](https://github.com/tonygward/audio-plugins/issues/203)  
**Related:** [SUI-18 #202](https://github.com/tonygward/audio-plugins/issues/202) (standardise title bars) and [SUI-17 #201](https://github.com/tonygward/audio-plugins/issues/201) (plugin title tooltip)  
**Execution:** Backlog specification only; no Matrix pickup or execution from creating this file.

## Objective and user value

Retain the useful full-curve EQ gain adjustment but place it in the **EQ main toolbox** rather than the global title bar. The title bar will be reserved for plugin name, preset, Undo, Redo, and Bypass, consistent with SUI-18.

## Existing behaviour

`eq/Source/PluginEditor.cpp` sets up a `scaleSlider` in the first/title row, with `EQ::gainScaleId` APVTS attachment and shown range roughly -100% to 200%. It scales every EQ band's gain (0% flat, 100% as set; negative values invert gain), **not interface zoom**. It is an actual DSP control whose parameter values, states and automation must remain stable.

## Desired behaviour and implementation

1. Inspect the current EQ editor layout, plugin parameter definitions, engine gain-scale handling, graph/toolbox layout, presets, undo support, and tests. Coordinate the single source edit with SUI-18; do not independently build a second scale control.
2. Remove `scaleSlider` from title-row layout and relocate it **visibly into the main EQ toolbox near other curve controls** (not hidden inside an unrelated settings popover). Keep the title row simplified and aligned with SUI-18.
3. Label **Gain Scale** (or **Scale EQ Gain**) and display percent, distinguishing it clearly from interface size. Plain-English tooltip: “Increase or reduce all EQ boosts and cuts together. 100% uses the values you set, 0% flattens them, and negative values turn boosts into cuts.” Retain double-click reset to 100% and editable value where appropriate.
4. Reuse the exact existing `EQ::gainScaleId` attachment, parameter identity, range, normalisation and preset/session state; **do not replace or reset** the parameter. Existing sessions and automation must sound exactly as before.
5. Preserve keyboard interaction, accessible labels, Undo/Redo and host automation. Ensure placements do not clash with frequency guide, selected-band controls, phase mode, oversampling, processing diagnostics or graph; respect 980px minimum editor and all supported scales/high DPI.
6. Update editor docs, screenshot tests and any tooltip/QA references. Coordinate with SUI-17 so the plugin title tooltip remains unobstructed.

## Acceptance tests

- Scale absent from title bar and present as an obvious, operable control in main EQ toolbox.
- Default 100%, neutral 0%, negative inversion and upper positive range preserve current DSP behaviour; controlled gain-curve tests at key values.
- Factory/custom preset state, historical saved sessions, normalised host automation, existing `EQ::gainScaleId` ID/parameter mapping all preserved; no migration necessary.
- Pointer, text, keyboard, Undo/Redo and double-click reset work.
- No overlap/clipping at minimum/maximum supported sizes; retain before/after screenshots and host check notes.
- Required current-head Windows/macOS CI passes; trusted merge and qualifying beta version bump, Windows EXE/macOS PKG installers, private R2 and live beta website/download verification; final issue evidence comment precedes closure and board Done.

## Dependencies, risks and versioning

SUI-18 owns the **shared header layout**; EQ-41 owns relocation of the existing Scale control. If Matrix executes SUI-18 first, keep gain-scale relocation to this story rather than omitting the control. If EQ-41 is first, SUI-18 must reuse it, not duplicate it. Primary risk: accidental change to saved EQ audio and automation, or cramming the toolbox at minimum window size. Sonnet default, single active task, verified weekly Claude usage below 60%, issue comments for delivery trace, third-component beta VERSION increment under current policy.
