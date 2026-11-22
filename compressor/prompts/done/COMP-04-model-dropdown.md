---
id: COMP-04
priority: medium
effort: medium
dependencies: []
conflicts: []
---

# COMP-04: one Model dropdown, and a toolbox that takes the model's colours

Issue: songsmithstudio#23. Implemented by hand with Claude Code (no Matrix run). Merged in #363 and shipped in 0.5.78.

## Objective
Replace the Analog Warmth button and the model dropdown beside it with one Model dropdown, and let the floating toolbox follow the chosen model's colours.

## Scope
- One dropdown with **Clean, Black, Blue, Anniversary, Lindell**, at the left of the panel's bottom row. Clean is the transparent compressor (Analog Warmth off); the other four are the Analog Compressor's models, run by the same shared code.
- Choosing one changes only the Analog Warmth and model parameters, in one Undo step. No other parameter moves.
- New shared table `shared/Source/SongsmithModelTheme.h`. The toolbox surface, outline, knob rings and lit buttons use it. The Analog Compressor draws its faceplate tint from the same table.
- Accents for Blue, Anniversary and Lindell are Songsmith choices (the Analog Compressor has one amber accent for all models), picked for readability.
- Parameter IDs `warmth` and `warmth_model` are unchanged. Older sessions: warmth off opens as Clean; on with a model opens as that model; on with no model keeps its original sound and the dropdown reads Classic.

## Acceptance criteria
- Five items, no Analog Warmth button anywhere, correct placement at the minimum, default and maximum widths.
- Other parameters untouched; Undo and Redo work; host automation updates the dropdown and colours.
- Each accent reaches 3:1 against its panel surface and the label colour 4.5:1.
- `CompressorTests` pass (61238 checks), `AnalogCompressorTests` pass (4554).

## Build and release

Songsmith Compressor, plus the shared theme used by the Analog Compressor. Released as 0.5.78 with exactly the two installers. Not yet verified in a DAW.
