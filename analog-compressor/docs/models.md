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
| 0 | `revD-blackface` | Blackface (Rev D reference) | Clean and straightforward. A good place to start for taming vocal, bass or guitar peaks while keeping the sound fairly clean. |
| 1 | `revA-bluestripe` | Blue Stripe (Rev A reference) | Faster and grittier, with more colour. Try it for an upfront vocal, or to make room microphones sound more aggressive. |
| 2 | `ae1176-anniversary` | Anniversary Edition (1176AE reference) | Gentler and a little cleaner. It adds a 2:1 ratio, and a Slow Attack button that lets more of the first hit through. |
| 3 | `lindell-inspired` | Lindell (Lindell-inspired, reference unconfirmed) | Smooth and relaxed, with a slower attack that lets the first hit through. Try it on acoustic guitar to even out strong strums. It also has 2:1. |

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
- **2:1** appears on the Anniversary Edition and Lindell models only (ACOMP-20 added it to Lindell; the global removal in ACOMP-19 still applies to Blackface and Blue Stripe). **Slow Attack** appears only on the Anniversary Edition. 2:1 uses the existing `ratioTwoToOne` parameter; Slow Attack is the new `slowAttack`. Slow Attack sits under the Release column, the nearest free room to Attack, an exception to the control-proximity rule.
- Switching model by hand from the Anniversary Edition or Lindell to a model without 2:1: 2:1 becomes 4:1 (and Slow Attack goes off when leaving the Anniversary Edition), so no ratio is shown falsely and nothing is silently reinterpreted; everything else keeps its position. Presets and loaded sessions are never remapped, so a session saved with 2:1 on Rev D still sounds as saved (it shows "Saved 2:1").
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

## Faceplate colours (ACOMP-20)

The model shows mainly through the faceplate colour, drawn under the same brushed grain. Colour is an extra signal: the model name is always on screen too.

| Model | Faceplate |
|---|---|
| Blackface | unchanged dark plate |
| Blue Stripe | a clear, bright blue with one broad light-blue stripe |
| Anniversary Edition | a deeper navy blue with white pinstripes at the top and bottom, so it reads as blue and white and differs from Blue Stripe |
| Lindell | a lighter neutral grey with fine vertical lines |

These are original designs, not copies of any faceplate. The model is chosen from four buttons (Black, Blue, Anniversary, Lindell; songsmithstudio#12) in the configuration row under the title bar, outside the textured panel (ACOMP-23; it keeps its full click size, at least 26 design units tall). The display names were shortened: Blackface (was "Blackface · Original Songsmith"), Anniversary Edition (was "1176AE · Anniversary Edition") and Lindell (was "Lindell-inspired"); the saved IDs and indexes are unchanged. The Lindell reference is still unconfirmed, as noted above. Screenshots: `docs/images/acomp-20-*.png`. Tests: `Models` in `ModelTests.h`.

## Model buttons and their tooltips (songsmithstudio#12)

The tooltip on each button is the "Use" text in the table above. The ticket suggested other wording; it was **not** used where the code does not support it: Lindell has the same colour as Blackface (x1), a slower attack (x1.6) and the softest knee (x1.5), so "colourful, extra analogue attitude" would be wrong (Blue Stripe is the colourful one, x2); the Anniversary Edition is the *cleaner* model (colour x0.6, softer knee), not especially "versatile"; and Blackface is described as clean, not "punchy". The earlier unverified claim that Lindell "keeps the pick attack" was dropped for the measured fact that its attack is slower. Listening checks are still a person's job (see "Still to do by a person").
