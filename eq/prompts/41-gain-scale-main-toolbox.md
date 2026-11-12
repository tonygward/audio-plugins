---
id: EQ-41
model: sonnet
effort: low
dependencies: []
conflicts: []
---

# EQ-41 — Label the Gain Scale control and explain it plainly

**Issue:** [EQ-41 #203](https://github.com/tonygward/audio-plugins/issues/203)  
**Related:** [SUI-18](../../shared/prompts/done/SUI-18-consistent-plugin-title-bars.md) (standardise title bars, #202, shipped in 0.5.10)

## Already delivered: do not redo

This prompt originally moved the Scale control out of the title bar. **SUI-18 (#223, 0.5.10) already did that:** `scaleSlider` now sits in the second toolbox row, beside Auto Gain and Output (`layoutContent()` in `eq/Source/PluginEditor.cpp`), with the band count next to it when there is room. Do not move it again, add a second control or touch the parameter.

## Objective and user value

The control scales every band's gain, but on screen it reads only **Scale 100%**, which is easy to mistake for interface size. Name it for what it does and give it a plain-language tooltip.

## Existing behaviour

- Value text: `"Scale " + percent + "%"` (`scaleSlider.textFromValueFunction`), parsed back by `valueFromTextFunction`.
- Host parameter name is already **Gain Scale** (`EQ::gainScaleId`, `PluginProcessor.cpp`), so the screen and the host disagree.
- Tooltip: "Scale every band's gain together: 0 % = flat, 100 % = as set, negative inverts (cuts and notches are unaffected). Double-click: 100 %".

## Desired behaviour

1. The value text reads **Gain Scale 100%** (and so on), matching the host parameter name. Typing a value still works with or without the words.
2. Tooltip, in sentence case and plain words, keeping the facts the current one states: "Turns every band's boosts and cuts up or down together. 100% uses the values you set, 0% flattens them, and negative values turn boosts into cuts. Low Cut, High Cut and Notch bands are not affected. Double-click to return to 100%."
3. Widen the slider's `barWidth` measurement to the new longest text (`"Gain Scale -100%"`, `"Gain Scale 200%"`) so nothing is truncated. If the row no longer fits at the minimum editor width, hide the band count first (it already hides when there is no room), never shorten the words.

## Scope

`eq/Source/PluginEditor.cpp` (value text, parser, tooltip, layout width), EQ tests, `eq/README` / docs that quote the old text, before-and-after screenshots. No DSP, parameter, preset or state change; no `stateVersion` bump.

## Acceptance criteria

- On screen the control reads **Gain Scale** with its percentage at 100 %, 150 % and 200 % interface sizes and at the minimum window width, with no truncation or overlap.
- Typing `50`, `50%` and `Gain Scale 50%` all set 50 %; double-click returns to 100 %.
- The tooltip text matches the wording above.
- `EQ::gainScaleId`, its range, automation and saved sessions are unchanged.

## Tests

- Extend the existing EQ editor tests: value text, text parsing, tooltip text, and the plain-words test (no abbreviations).
- Layout test: the slider's bounds fit its longest text at the minimum width, and it stays in the toolbox row, not the title bar.
- Before and after screenshots from `EQTests --screenshots`.

## Build and release

Plugin source change: the pull request builds EQ, and the merge takes the normal automatic beta version bump. Add a musician-facing line under `## Unreleased` in `CHANGELOG.md` (the control is now labelled Gain Scale; nothing else changes and there is nothing to do). Not yet verified in a DAW until checked.
