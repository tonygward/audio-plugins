---
id: ACOMP-17
model: sonnet
effort: low
dependencies: []
conflicts: []
---

# ACOMP-17 — Let the Analog Compressor window shrink to 75 %

**Issue:** [ACOMP-17 #255](https://github.com/tonygward/audio-plugins/issues/255)
**Related:** [SUI-10](../../shared/prompts/done/SUI-10-resize-and-plain-language.md) (drag resizing), [ACOMP-16](16-ignore-below-continuous-500hz.md) (Ignore Below slider; independent, but both touch the editor)

## Objective and user value

Every Songsmith editor scales as a whole from **75 %**, never smaller, to at least 200 %. Songsmith EQ and Songsmith Maximiser already shrink to 75 %, but Songsmith Analog Compressor stops at 100 % (960 × 428). On a small laptop screen it takes more room than the other plugins. Let it shrink to 75 % like the rest.

## Existing behaviour

- `analog-compressor/Source/FETParams.h`: `minEditorWidth = designWidth` (960) and `maxEditorWidth = designWidth * 2`. `constrainEditorWidth` clamps to that range. The comment says the smallest size is the design size "so text stays at least 12 px". That is out of date: the suite rule measures text sizes at 100 % (`CLAUDE.md`, `docs/style-guide.md`), and the other plugins already go to 75 %.
- `uiScales[] = { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f }`, used by `snapUiScale` to store the nearest size in `uiScale` (old sessions only have this value). The Interface size menu was removed in 0.5.10, so the list now serves only recall.
- `PluginEditor.cpp` sets the resize limits from `minEditorWidth` and `maxEditorWidth`, and restores `editorWidth` (or, for old sessions, `uiScale`).

## Desired behaviour

1. Smallest size **75 %**: `minEditorWidth = designWidth * 3 / 4` (720), height from `editorHeightFor` (321). Nothing smaller, whatever the host asks for. Largest stays 200 %.
2. Add `0.75f` to `uiScales` so a session saved at the smallest size recalls it. A saved `editorWidth` below 720 (impossible today, but defend against it) clamps to 720.
3. Update the `FETParams.h` comment: the 12 px and 11 px minimums are measured at 100 %, and 75 % is the suite's smallest size.
4. The editor is drawn in design units and scaled as a whole, so no relayout should be needed. If anything overlaps, clips or truncates at 75 %, fix the layout without shortening any word.

## Scope

`analog-compressor/Source/FETParams.h`, `analog-compressor/Source/PluginEditor.cpp` (only if recall needs it), Analog Compressor tests, `analog-compressor/docs/editor.md` (the Resizing section), `analog-compressor/README.md`, before-and-after screenshots. No parameter, preset, state-version or DSP change.

## Acceptance criteria

- The window can be dragged down to exactly 720 × 321 and no smaller, keeping the design proportions. The maximum is still 1920 × 856.
- At 75 % nothing overlaps, clips or truncates. Every button and knob can still be pressed and dragged.
- A session saved at 75 % reopens at 75 %. Old sessions with only `uiScale` reopen at their saved size, as before.
- The sound, parameters, presets and saved sessions are unchanged.

## Tests

- `AllButtonsEditorTests.cpp`: the resize-limit test uses the new minimum (720). A 700 px request clamps to 720, and 1920 stays the maximum. Include 75 % in the "keep the design proportions" sizes.
- The "large enough to press at the smallest size" check (`AnalogCompressorTests.cpp`) runs at 75 %. If 30 px is not met, agree a threshold and record why in the PR.
- Save and restore round-trip at 75 %, plus `snapUiScale (0.75f) == 0.75f`.
- Screenshots: `AnalogCompressorTests --screenshots <folder>` adds a 75 % render. Commit before-and-after images in the PR.

## Build and release

Plugin source change: the pull request builds Analog Compressor, and the merge takes the normal automatic beta version bump. Add a line under `## Unreleased` in `CHANGELOG.md`, such as: "Songsmith Analog Compressor's window can now be made smaller, down to three quarters of its usual size, like the other Songsmith plugins. Drag the bottom-right corner. Nothing else changes and there is nothing you need to do." Update `docs/style-guide.md` only if its wording no longer matches. Not yet verified in a DAW.
