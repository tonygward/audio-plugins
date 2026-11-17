# Switchable models (ACOMP-10)

One plugin, one **Model** selector (lower right of the window). Each model changes the sound and the faceplate texture. Source: `Source/FETModels.h` (the numbers), `Source/FETEngine.cpp` (timing and gain reduction), `Source/PluginProcessor.cpp` (colour). Tests: `Tests/ModelTests.h`.

## Evidence gaps (read first)

- **The reference pages could not be opened in this run** (UA collection tips, the 1176LN and 2-1176 manuals, the Plugin Alliance 7X-500 page). No published specification below was re-checked. Before release, someone should compare the table in "What is published" with those four sources.
- **Tony's exact Lindell reference is unconfirmed.** The task names the 7X-500 as a possibility. The model is therefore called **Lindell-inspired** and its voicing is a Songsmith design choice, not a copy of any circuit. If the reference is not the 7X-500, change the voicing note and tooltip, not the model ID.
- **Nothing was auditioned.** No level-matched listening on real material was done, so the use-case tooltips are the task's suggested copy, unverified. In particular, the claim that Lindell-inspired keeps the pick attack on strummed and picked guitar is **unchecked**.
- **Not yet verified in a DAW.** The measurements below are engine-level and plugin-level unit tests, not host tests.

## Models

| Index | Stable ID (in preset files) | Name on screen | Use (shown beside the selector and as a tooltip) |
|---|---|---|---|
| 0 | `revD-blackface` | Rev D · Blackface (Original Songsmith) | Start here to tame vocal, bass or guitar peaks while keeping the sound fairly clean. |
| 1 | `revA-bluestripe` | Rev A · Blue Stripe | Try this for a gritty, upfront vocal or to make room mics sound more aggressive. |
| 2 | `ae1176-anniversary` | 1176AE · Anniversary Edition | Try 2:1 for gentler control, or Slow Attack to let more of the first hit through. |
| 3 | `lindell-inspired` | Lindell-inspired | Try this on acoustic guitar to even out strong strums while keeping the pick attack. |

Index 0 is the original sound and the default: older sessions and presets have no model and load it, bit-exact (tested). Never reorder indexes or change IDs. Rev E is a comparison reference only and has no model. Host choice lists show the name only; the use line is in the window and the tooltip.

## What is published, what is measured, what is Songsmith's choice

| Difference | Basis |
|---|---|
| Rev A, Rev D, 1176AE exist as revisions with different character; the 1176AE adds 2:1 and a slow attack | Published (UA collection notes, per the task). **Not re-checked here.** |
| Threshold offset, knee, attack and release scale, colour scale (table below) | **Songsmith voicing choices.** Directions follow the task's descriptions (grittier, gentler, lets the attack through); sizes are ours. |
| Measured effects: more or less gain reduction, faster or slower attack and release, more or less second harmonic | **Measured** in `ModelTests.h` (engine step response, static curve, second harmonic of the FET stage). Full-plugin harmonic sweeps per model are not done. |

| Model | Threshold | Knee | Attack | Release | Colour |
|---|---|---|---|---|---|
| Rev D | 0 | ×1 | ×1 | ×1 | ×1 |
| Rev A | −1.5 dB | ×0.7 | ×0.7 | ×0.85 | ×2.0 |
| 1176AE | +1.0 dB | ×1.3 | ×1 | ×1.15 | ×0.6 |
| Lindell-inspired | −0.5 dB | ×1.5 | ×1.6 | ×1.3 | ×1 |

Slow Attack (1176AE only) makes the attack ×4 longer, a Songsmith choice. The colour factor is smoothed over 20 ms, so switching during playback does not click.

## Controls and switching

- The shared controls (Input, Output, Attack, Release, ratios, All Buttons, Gain Reduction Off, Ignore Below, Oversampling, Bypass) are the same on every model.
- **2:1** and **Slow Attack** appear only on the 1176AE. 2:1 uses the existing `ratioTwoToOne` parameter; Slow Attack is the new `slowAttack`. Slow Attack sits under the Release column, the nearest free room to Attack, an exception to the control-proximity rule.
- Switching model by hand from the 1176AE: 2:1 becomes 4:1 and Slow Attack goes off; everything else keeps its position. Presets and loaded sessions are never remapped, so a session saved with 2:1 on Rev D still sounds as saved (it shows "Saved 2:1").
- New parameters: `model` (choice, version hint 6) and `slowAttack` (bool, version hint 6). `stateVersion` is 6. No ID was renamed or removed.
- Presets store the model by its stable ID (`model="…"`). A file with no model, or an unknown one, loads Rev D Blackface. Factory presets all state a model (currently Rev D). Set Levels measures the selected model.

## Acoustic Guitar Smoothing: pending

Not added. The prompt requires the tutorial link, timestamp and settings to be verified first. [joe-gilder-preset-research.md](joe-gilder-preset-research.md) records the Butter Compression link (`https://www.youtube.com/watch?v=d7vHaydnTuQ&t=190s`, 3:10) but says its values were **copied from the task, never checked against the video**, and they describe a conventional compressor (about 1.3:1, 30 ms attack, 50 ms release) that this FET design cannot reproduce directly. It also needs Input and Output set against a real acoustic guitar recording and an audition on Tony's strummed and picked parts.

Once verified: add `{ "Acoustic Guitar Smoothing", …, model 3 }` to `Presets::factory()` (the placeholder comment is there), record source and adaptations in `presets.md`, update the preset count tests. The mechanism it relies on is already tested: loading a preset switches model, and a custom preset keeps it.

## Still to do by a person

1. Confirm the Lindell reference; check the published facts against the four sources.
2. Level-matched listening on real material for each model; adjust the use lines if they do not hold.
3. Plugin-level harmonic and timing measurements per model; DAW checks (switching during playback, session recall, bypass, latency).
4. Before and after screenshots of each faceplate (`--screenshots`).
