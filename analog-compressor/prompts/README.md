# Songsmith Analog Compressor prompts

There is one self-contained prompt per build step. They were run in order by the overnight run of 2026-11-03: each step branched from the integration branch `compressor/overnight-2026-11-03`, got a draft PR, and was merged once CI was green. Finished prompts are in [`done/`](done/). The code is in [`analog-compressor/`](../). (The prompts still say `fet/` and `FETTests`: the folder and test executable were renamed afterwards.)

| Prompt | Status | PR |
|---|---|---|
| [01 plugin shell](done/01-plugin-shell.md) | Done | #36 |
| [02 feedback compressor](done/02-feedback-compressor.md) | Done | #41 |
| [03 analogue colour](done/03-analogue-colour.md) | Done | #43 |
| [04 All Buttons and editor](done/04-all-buttons-and-editor.md) | Done | #47 |
| [05 presets, validation and packaging](done/05-presets-validation-packaging.md) | Done | #50 |
| [06 oversampling off by default](06-oversampling-off-by-default.md) | To do · **Sonnet** | |
| [07 detector Low Cut](07-detector-low-cut.md) | To do · **Sonnet** | |
| [08 1176-inspired editor](08-1176-editor.md) | To do · **Sonnet** | |
| 09 Guided compressor calibration | To do · **Sonnet** | |

Still to do by hand before a release: the Studio One and Logic checks in [`analog-compressor/docs/daw-checks.md`](../docs/daw-checks.md), and the listening checklists in `analog-compressor/docs`.

## 09 · Guided compressor calibration

**Model: Sonnet.** Follow `CLAUDE.md`. Build after the detector Low Cut and editor updates.

Add an optional **Set Levels** action. Ask the user to play a representative loud section, then estimate an Input setting that gives a few dB of gain reduction on louder peaks (start with 2–4 dB) and an Output setting that approximately matches the dry and compressed loudness. Account for the current ratio, attack, release and Low Cut. Show the proposed settings and measured reduction, let the user apply or cancel, and make the applied settings undoable and manually adjustable. Keep Input and Output fixed during normal playback. Handle silence, short captures, clipping risk, and targets that cannot be reached. Keep measurement real-time safe and avoid writing over active host automation. Test on transients and sustained sources, and verify the output match using a level-matched bypass comparison.

Add explicit tooltips for the Attack and Release knobs. Attack explains how quickly compression starts, Release explains how quickly it lets go, and both say turning clockwise makes it faster. Verify that the knob direction agrees with the DSP.
