---
id: ACOMP-18
priority: medium
effort: medium
dependencies: []
conflicts: []
---

# ACOMP-18: Release in the control-voltage domain; engine runs while bypassed

Issue: [#280](https://github.com/tonygward/audio-plugins/issues/280). Implemented by hand from the DSP review (no Matrix run).

## Objective
Make Songsmith Analog Compressor's release behave like the 1176's gain cell, and keep its engine running while bypassed as the other two plugins do.

## Scope
- **Release law:** the gain cell is a divider, R5 over the FET (UG p. 8, Figs. 3-4): gain = 1 / (1 + u), with u = R5 × FET conductance, roughly proportional to the timing capacitor's voltage, which discharges exponentially (UG p. 10, Fig. 5). Release now decays u = 10^(GR/20) − 1 exponentially instead of GR in dB. Shallow release is unchanged; deep gain reduction recovers at a nearly steady dB rate (programme-dependent release). No new constants. Solved in the existing zero-delay loop, in double precision with expm1 / log1p.
- **Bypass:** remove the early return that skipped the engine while fully bypassed; the output stays the delayed dry signal, bit-exact.

## Acceptance criteria
- Release time of u matches the knob at every knob, ratio, level and rate (within 0.1 %).
- From about 22 dB of GR, more than 60 % of the GR remains after one release time; from about 1 dB, about 1/e remains.
- Calibration (1 dB GR at 20:1), starting-point level matching, click-free switching and every other existing test pass.
- Bypassed output is bit-exact, and gain reduction is tracked while bypassed.

## Tests
New: "ACOMP-18: release follows the control voltage…" and "ACOMP-18: the engine keeps running while bypassed…". Release timing is measured on u. The oversampling-aliasing test is restated for the re-measured figures (docs/colour.md).

## Build and release

Songsmith Analog Compressor only. Users will hear a different release on heavy compression, so the changelog has a note.
