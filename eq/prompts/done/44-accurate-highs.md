---
id: EQ-44
priority: medium
effort: medium
dependencies: []
conflicts: []
---

# EQ-44: Accurate Highs (matched bells and shelves near Nyquist)

Issue: [#285](https://github.com/tonygward/audio-plugins/issues/285). Implemented by hand from the DSP review (no Matrix run).

## Objective
Keep Songsmith EQ's high bells and shelves the shape the curve promises, without the delay oversampling adds. At 44.1 and 48 kHz the bilinear transform squeezes them towards Nyquist: a 16 kHz bell is off by up to 7–8 dB.

## Scope
- **Accurate Highs** (`hf_accuracy`, version hint 10): a toggle right of Oversampling. On for new instances; sessions saved before it load with it off, so they sound as they did.
- Bells: Vicanek's matched peaking EQ ("Matched Second Order Digital Filters", 2016, eqs. 12, 25–29, 42–45): impulse-invariant poles, numerator matched at DC and the centre (gain and a turning point).
- Shelves (not in the paper): the same scheme matched at DC, the corner frequency and Nyquist.
- The biquad is converted exactly to the SVF form, so processing, smoothing and the drawn curve are unchanged in kind. `makeCoeffs` keeps whichever of the matched and bilinear designs is closer to the analog shape, so the option is never worse. Cuts and notches are unchanged.
- Switching crossfades each band, as a type change does. Linear phase uses the same design (part of its change hash).
- The tom starting points' output gain is re-matched (−2.6 → −3.1 dB), because Accurate Highs keeps more of their 10 kHz band.

## Acceptance criteria
- Matched is never worse than bilinear, and at least halves the error wherever bilinear is more than 1 dB off (except shelves at 16 kHz, whose prototype poles sit at or above Nyquist and fall back to bilinear).
- Below 5 kHz, matched and bilinear agree within 0.1 dB for bands at 100 Hz and 1 kHz.
- Every type, rate, frequency, Q and gain gives a finite, stable filter; cuts, notches and 0 dB bands are bit-identical to before.
- The engine output matches the drawn curve within 0.1 dB (15 kHz bell at 44.1 kHz).
- New instance on; older session off; the toggle sits immediately right of Oversampling at 75, 100 and 200 %.

## Tests
New `Accurate Highs` test class (6 tests). Updated: parameter count, the old-session fixture test (expects hf_accuracy off), the linear-phase reference curve and the Gain Scale bit-identity test use the processor's setting.

## Build and release

Songsmith EQ only. Users will see a new toggle and hear more accurate high bands in new instances, so the changelog has a note and the README describes it.
