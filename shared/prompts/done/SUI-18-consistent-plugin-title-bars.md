---
id: SUI-18
status: done
execution: scheduled
model: sonnet
effort: medium
dependencies: []
conflicts: []
---

# SUI-18 — Consistent suite title bars, modelled on Songsmith EQ

**Tracking issue:** [SUI-18 #202](https://github.com/tonygward/audio-plugins/issues/202)  
**Related:** [SUI-17 plugin-title tooltip #201](https://github.com/tonygward/audio-plugins/issues/201), [ACOMP-12 Ignore Below #200](https://github.com/tonygward/audio-plugins/issues/200), [EQ-41 Gain Scale in toolbox #203](https://github.com/tonygward/audio-plugins/issues/203)  
**Execution:** Backlog only; the issue/prompt does not queue or start Matrix.  
**Policy:** [Songsmith execution and delivery](../../docs/songsmith-execution-policy.md).

## Objective and user value

Make the header predictable in every existing Songsmith plugin. The EQ title bar is the preferred reference for visual styling and function placement.

**Required logical order: plugin name → preset selector → Undo → Redo → Bypass (far right).** Remove the EQ Scale widget and Analog Compressor Interface size dropdown from the title bars. Apply the same simple header to Maximiser.

## Current source observations (verify against latest main before coding)

- `eq/Source/PluginEditor.cpp`: first row contains painted product name, `PresetBar`, Undo, Redo, **Scale** slider, band-count indicator and Bypass. The Scale widget is bound to `EQ::gainScaleId`, a real **gain curve processing** parameter, not interface zoom.
- `analog-compressor/Source/PluginEditor.cpp`: header has title, `presetBox`, `oversampling`, `sizeBox` (Interface size) and Bypass. Editor also has resize handling and a persistent UI scale. No current EQ-like Undo/Redo.
- `maximiser/Source/PluginEditor.cpp`: header has title, Peak Mode, Dither and Bypass; `startingPointBox` lives below the header. No current EQ-like Undo/Redo.
- SUI-17 requests a title-associated guidance tooltip; avoid two overlapping hit targets or conflicting title rendering.

## Implementation guidance

1. Define a small reusable header layout/style abstraction only where it saves duplication. Keep the same shared title typography, spacing, preset width, button sizes, Undo/Redo behaviour, Bypass red styling and hover/focus hit area across EQ, Analog Compressor and Maximiser. Preserve controls outside the header and consistent responsiveness.
2. **EQ:** remove the **Scale widget from the header** and rebalance spacing; hide/remove no audio parameters. Because `gainScaleId` changes sound and can be automated, preserve its parameter ID, stored value, legacy preset/host automation and sound. **EQ-41 owns relocating the existing control into the visible EQ main toolbox, NOT hidden in Settings.** If EQ-41 has not run, keep the original gain-scale parameter/attachment functioning and coordinate the one relocation with EQ-41; never remove its only accessible control without adding the toolbox replacement. Do not reset old values to 100%, reinterpret automation or delete the DSP feature. Review whether band-count status belongs off the header (yes under the strict five-element layout) and preserve it unobtrusively if valuable.
3. **Analog Compressor:** remove the `sizeBox` UI from the title bar, but keep drag-to-resize, resizable bounds, host DPI scaling, persisted size and all responsive layouts. Relocate `oversampling` outside the header, near processing controls or its existing logical setting location. Put preset box immediately after the product title. Provide real Undo and Redo on edits and preset loads. Prefer a shared transaction strategy but do not invent working buttons over a missing undo stack. Include keyboard shortcuts and enabled/disabled feedback.
4. **Maximiser:** move existing `startingPointBox` to the title bar after the product name; label it consistently with EQ/Compressor preset terminology. Place genuine Undo/Redo after it. Relocate Peak Mode and Dither to their relevant processing controls without loss of accessibility or automation. Leave Bypass at far right and preserve session state.
5. Implement undoable transactions for user parameter edits and preset recall where missing, with appropriate JUCE `UndoManager`/APVTS integration and safe host automation handling. Confirm existing EQ Undo/Redo semantics before modelling them. No effect on real-time processing or thread safety.
6. Coordinate the title's tooltip hit area with SUI-17. Tooltip must attach to the plugin title, not a whole header overlay. Neither implementation should introduce double painting, overlay clipping or inaccessible hover/focus targets.
7. Keep the title bar free of further feature-specific processing options. Confirm preset loading, saving (where available), keyboard navigation, button tooltips, session/preset compatibility and resize behaviour. Update documentation, screenshots and tests.

## Acceptance criteria and tests

- At supported UI scales, all three header layouts visually read **Name | Preset | Undo | Redo | Bypass**, with only those functions; Bypass aligned to the right, red and consistent.
- Undo/Redo genuinely reverses and reapplies meaningful user changes and preset loads. Test multiple sequential edits, button disabled state, shortcuts, host undo interaction and automation safety. No false promise that an undo button can revert host-owned automation or unrelated DAW actions.
- Previous EQ sessions with non-default `gainScaleId` preserve exact audio behaviour and automation after the visible Scale widget is removed; a working Gain Scale control is retained visibly in the main EQ toolbox as specified in EQ-41 (or hand off that relocation explicitly without leaving an inaccessible parameter).
- Analog Compressor no longer has the Interface size dropdown, while window dragging, stored size and resizing constraints work in existing DAWs.
- Maximiser presets are selectable from the header; moved Dither/Peak Mode stay functional and their saved values still recall.
- No title tooltip collision with SUI-17. Keyboard and accessibility traversal includes the title tooltip (when implemented), preset, undo, redo and bypass in logical order.
- All relevant unit, state-compatibility and JUCE editor-layout tests pass and native Windows/macOS CI validates the exact PR head. Include screenshots at minimum/typical/high DPI and host-testing notes.
- Trusted automatic merge, correct beta third-component VERSION increment, current Windows EXE/macOS PKG suite installer release, private R2 delivery and protected beta website downloads verified before the final issue comment, closure and board Done.

## Dependencies, risks, release

No hard dependency on unimplemented SUI-17, but coordinate design and tests with its title tooltip; SUI-17 and SUI-18 touch the same editors and should not execute concurrently. Main risk: meaningful Scale DSP currently bound to a top-row control and missing Undo/Redo transaction model in two editors. Keep historical automation/preset IDs and work within tested JUCE state APIs. Sonnet by default; Opus only for unusually complex undo/session design with reason recorded. One active task, below 60% verified weekly Claude usage, board-driven pickup. Infrastructure-only edits don't bump VERSION; a qualifying plugin UI build follows current beta-release policy.
