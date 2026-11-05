# SUI-13 — Consistent global Bypass position and active red

**Status:** Done in #82 — **Priority 1, early UI consistency**  
**Model:** Sonnet  
**Scope:** EQ, Analog Compressor, Maximiser, and every future Songsmith plugin with a global Bypass control.

## Goal
Always place **global Bypass at the top-right** of the plugin interface, as already done in Songsmith EQ and Maximiser. Its active/pressed colour must be the **existing Analog Compressor bypass red, #C9563C** (JUCE ARGB `0xffc9563c`), across all plugins.

## Verified source baseline
- EQ: `eq/Source/PluginEditor.cpp` already anchors Bypass on the right of its top header row; active colour currently orange `0xffff9f43`.
- Maximiser: `maximiser/Source/PluginEditor.cpp` already positions Bypass top-right; colour currently inherits the plugin accent.
- Analog Compressor: `analog-compressor/Source/PluginEditor.cpp` currently puts Bypass at `(736, 300, 144, 36)`, lower-right. `analog-compressor/Source/FETLookAndFeel.h` defines its bypass red as `0xffc9563c`.

## Requirements
1. **Standard location:** top-right for every plugin, with consistent inset, alignment and similarly sized click targets, while respecting each editor's dimensions and scaling. EQ and Maximiser are reference placements; move Analog Compressor's global Bypass to the top-right.
2. **Standard colour:** create/reuse a single shared `Songsmith::UI::Colours::bypass` token set to `#C9563C`; apply it to every global Bypass button's toggled-on colour. Remove duplicate magic values and per-plugin divergent bypass colours. Retain shared subtle bevel, focus/hover and accessible contrast.
3. **Standard meaning:** red/pressed = Bypass **engaged**, processing bypassed; neutral/raised = Bypass **not engaged**, normal processing. Verify that the actual host parameter and audio behaviour match, without changing any parameter IDs or DSP.
4. Maintain the label **Bypass**, keyboard accessibility, tooltips and minimum click target. Do not conflate global Bypass with Band On/Off, Gain Reduction Off, or other local toggles.
5. Make it a standing rule for future plugins. Document in `CLAUDE.md` and `docs/style-guide.md`.
6. Coordinate editor layout changes with **ACOMP-08** and any shared header changes. Do not relocate unrelated controls merely for cosmetic symmetry. Preserve responsive resizing and no clipping.

## Acceptance tests
- Bypass visible in roughly the same **top-right** region for EQ, Analog Compressor and Maximiser at minimum/default/large interface sizes and 1×/2× DPI.
- Toggling Bypass ON visibly uses the same **#C9563C** red for all three; OFF visibly returns to a neutral button. Tests verify the token and colour applications.
- No altered DSP, bypass automation, session recall or parameter IDs.
- Capture real screenshots of each editor in both Bypass states in the PR; keep useful README screenshots.
- Build and run tests for all three plugins; DAW checks documented accurately.

## Delivery
Follow `CLAUDE.md`: branch from latest `main`, PR-only merge, no system-wide installs, do not bump `VERSION` for an individual prompt. Record PR/release checkpoint in the backlog; move this prompt to `done/` only after merge.
