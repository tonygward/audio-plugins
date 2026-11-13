# Pultec-style EQ: reference notes and engine

Reference and source notes for the planned Pultec-style EQ (PEQ-01), written before any final tuning, as the prompt asks. Code: `Source/PassiveEngine.h` and `.cpp`. Tests: `Tests/PassiveTests.cpp` (`PassiveTests`).

This is slice 1 of PEQ-01: the engine only. There is no plugin, editor, preset or installer entry yet, so nothing here is audible to musicians. The product name is still open; "Pultec" is the hardware's name and is used here only to say what the design is inspired by.

## Published panel values (EQP-1A)

These are the values on the hardware's panel, from manufacturer and retailer pages. They are the only hardware facts this design relies on.

- Low shelf boost: 20, 30, 60 or 100 Hz, 0 to 13.5 dB.
- Low shelf attenuation: the same four frequencies, 0 to 17.5 dB.
- High peak boost: 3, 4, 5, 8, 10, 12 or 16 kHz, 0 to 18 dB.
- High shelf attenuation: 5, 10 or 20 kHz, 0 to 16 dB.
- The hardware's bandwidth control is a dial with no published frequency or Q value.
- Universal Audio states the original unit adds about 1.13 dB of level in the signal path. This design does **not** model that: with every control at zero the signal passes through unchanged.

Sources: [Pulse Techniques EQP-1A](https://www.pulsetechniques.com/products/tube-equalizers/eqp-1a/), [MusicTech: Vintage Pultec EQP-1A](https://musictech.com/2014/05/pultec-eqp-1a/), [Universal Audio help](https://help.uaudio.com/hc/en-us/articles/7183176266260), [Vintage King preset charts](https://vintageking.com/blog/presets-pultec-eqp-1a-program-equalizer/). One retailer listing gives different switch counts; the manufacturer figures were used.

## What is published and what is a Songsmith design choice

| Behaviour | Status | Test |
|---|---|---|
| Switch positions and gain ranges above | Published | `The switch positions and gain ranges are the published panel values` |
| Filter maths (shelves and peak) | Published: the Audio EQ Cookbook by Robert Bristow-Johnson | `Low boost`, `Low cut`, `High boost`, `High cut` |
| Low boost and low cut at the same frequency can be used together | Published behaviour of the hardware, widely documented | `Boost and cut together` |
| The low cut's corner sits **3 times** above the boost corner, which makes a net bump at the bottom with a dip above it | Songsmith design choice, not measured from hardware | `Boost and cut together` (checks the shape, not a measured match) |
| High boost bandwidth maps to Q 0.35 (broad) to 3 (narrow) | Songsmith design choice | `High boost` (narrow is narrower than broad) |
| Shelves use Q 0.5 (gentle) | Songsmith design choice | `Low boost`, `High cut` |
| Everything at zero is bit-identical to the input | Songsmith design choice | `Everything at zero` |
| No modelled colour (no tube or transformer saturation) | Songsmith design choice; colour is a later slice if wanted | none |

## Behaviour notes

- **Minimum phase, no latency.** Four biquads in series per channel, in double precision (a 20 Hz shelf at 48 kHz is inaccurate in single precision).
- **Smoothing.** The four gains glide over 20 ms; coefficients are refreshed every 16 samples on a grid that does not depend on the host's block size (`Block size does not change the result`).
- **Stepped frequency switches are not smoothed.** Changing a frequency switch while audio plays can click. A later slice should crossfade; this is a known limitation.
- **Sample rates.** Settings sound the same at 44.1, 48 and 96 kHz below 10 kHz (`The same settings sound the same`). At 44.1 kHz the 16 kHz boost and the 20 kHz cut are limited to 0.45 of the sample rate.
- **Real-time safety.** `process()` does not allocate, lock or log. Non-finite input is treated as silence.

## Preset source notes (before tuning)

Joe Gilder's broad tone-shaping tutorials are the named starting point for presets. They have **not** been reviewed yet, so there are no preset values or attributions here. Slice 5 writes the source notes first and keeps educator names in these notes and in optional credits, not in tooltips or preset titles.

## Not yet done

- Plugin target, stable identity, parameters, state, editor, response curve.
- Presets, installer and CI wiring.
- Level-matched listening notes and a real host check. The tests use synthetic sine signals, so no listening verification is claimed.

## Slices

| Slice | What | State |
|---|---|---|
| 1 | Reference notes and engine ([this page](reference.md)) | In review |
| 2 | Plugin shell: target, identity, parameters, state, CI and installer entries (a person reviews the workflow and installer changes) | To do |
| 3 | Editor with response curve | To do |
| 4 | Frequency switch crossfade, optional colour | To do |
| 5 | Source notes, presets, host and listening checks, release | To do |
