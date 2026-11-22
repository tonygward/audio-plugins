# Songsmith Compressor engine

The engine of the planned **Songsmith Compressor** (COMP-01). It is a clean, conventional compressor and is separate from the 1176-inspired Songsmith Analog Compressor, which it does not touch. Code: `Source/CompressorEngine.h` and `.cpp`. Tests: `Tests/CompressorTests.cpp` (`CompressorTests`).

This page describes the engine. The plugin around it (parameters, editor, presets) is described in the [README](../README.md); Analog Warmth is not built yet.

## What is published and what is a Songsmith design choice

| Behaviour | Source | Test |
|---|---|---|
| Static curve: no change below the threshold, slope of 1/ratio above it | Textbook compressor transfer curve (for example Giannoulis, Massberg and Reiss, "Digital Dynamic Range Compressor Design", JAES 2012) | `Static curve` |
| Quadratic soft knee centred on the threshold, width in dB | The same paper | `Soft knee` |
| Feed-forward layout, gain reduction smoothed in dB | Standard practice; the paper's smooth branching detector | `Settled gain reduction`, `Attack`, `Release` |
| Detector reads the **sample peak of the louder channel** and one gain is applied to both channels (stereo link) | Songsmith design choice | `Stereo link` |
| Attack and release times are **time constants**: the reduction reaches 63 % of a step after the attack time and falls to 37 % after the release time | Songsmith design choice (the usual meaning of the controls) | `Attack`, `Release` |
| Ratio 1:1 does nothing and is bit-identical | Songsmith design choice | `1:1 produces no gain reduction` |
| Auto make-up restores the level of a steady signal at **-18 dBFS** and is never negative | Songsmith design choice; the reference level is typical programme level, not full scale, so gentle low-threshold settings stay level-matched | `Make-up gain` |
| Mix blends the untouched input with the compressed signal; there is no latency, so they are aligned | Songsmith design choice | `Mix` |

## Analog Warmth

An optional stage after the compression and before the Mix blend, switched by one button. **Off is exactly the clean compressor** (the signal is bit-identical to an engine that has no warmth stage), and Off is the default. It is a Songsmith design, not a model of any hardware or any other company's processing, and it does not change the threshold, ratio or timing.

| Behaviour | Status | Test |
|---|---|---|
| A smooth `tanh` curve with a drive of 0.8 and a small bias (0.08). The bias adds even harmonics as well as odd ones; the curve is scaled so its gain for small signals is exactly 1 | Songsmith design choice | `Analog Warmth on` |
| Small signals keep their level: within 0.05 dB below -24 dBFS and 0.25 dB at -18 dBFS. Peaks are rounded: a full-scale peak comes out under 3 dB lower, about 2 dB for a steady full-scale sine. Harmonics at -12 dBFS stay below -25 dB relative to the tone | Songsmith design choice | `Analog Warmth on` |
| Aliasing is reduced with first-order antiderivative anti-aliasing (the published ADAA technique) instead of oversampling, so there is **no latency**. It is applied only to the non-linear part of the curve, because averaging the straight part would also act as a low-pass filter and dull the highs | Published technique; the split is a Songsmith choice | `does not alias` (at least 6 dB better than no anti-aliasing and under -40 dB), `does not dull or brighten` (within 0.1 dB from 50 Hz to 15 kHz at 44.1, 48 and 96 kHz) |
| A very gentle high-pass at about 5 Hz removes the DC the bias would make | Songsmith design choice | `leaves no DC offset` |
| Switching it glides over 20 ms, with no click, and returns to the exact clean signal once off | Songsmith design choice | `Switching Analog Warmth` |
| The gain reduction is the same on or off (the detector listens to the input, not the warmed signal) | Songsmith design choice | `does not change the compressor` |

Limits to know: the loudest peaks are rounded off, so a very hot signal comes out a little lower than it went in; this is what saturation does, and the level is matched only for quiet and normal levels. First-order anti-aliasing reduces aliasing but does not remove it. The effect is gentle by design and is not meant as a distortion effect. Nothing here has been checked by ear on real recordings.

## Analog Warmth models (songsmithstudio#14)

With Analog Warmth on, a model (Black, Blue, Anniversary, Lindell) decides what it does. The models are the Songsmith Analog Compressor's, and the code is shared (`shared/Source/SongsmithAnalogModels.h`, moved out of the Analog Compressor without change; its own tests still pass), so a fix to a model reaches both plugins.

What a model does here:

1. **Voicing.** The same table as the Analog Compressor's moves the compressor's own controls while Analog Warmth is on: Threshold plus an offset (Black 0, Blue -1.5 dB, Anniversary +2.0 dB, Lindell -0.5 dB), Knee times a factor (1, 0.7, 1.3, 1.5), Attack times a factor (1, 0.7, 1, 1.6) and Release times a factor (1, 0.85, 1.15, 1.3). Black's are 1 and 0, so **with Black the compression is bit-identical to the clean compressor** (tested). Auto Makeup follows the settings it is really running with, so the level stays matched. These are Songsmith voicing choices, as in the Analog Compressor (analog-compressor/docs/models.md).
2. **Colour.** The compressed sound goes through the Analog Compressor's own stages, in its order: the FET gain cell (a second harmonic that grows with the gain reduction, scaled by the model: Black 1, Blue 2.0, Anniversary 0.6, Lindell 1), then the make-up level, then the Class A output amplifier and its transformer.
3. **Mapping from the Analog Compressor.** The Analog Compressor has an Input drive; this plugin has none. The colour therefore depends on what it depends on in the Analog Compressor with Input at 0 dB: the level of the sound (0 dBFS is +22 dBm in that calibration) and how hard the compressor is working. Make-up gain takes the place of the Output trim. Quiet and moderate levels stay clean, which is why this is a gentle warmth.
4. **Click free.** Switching Analog Warmth on or off fades the colour over 20 ms; changing model scales the colour over 20 ms and moves the compression through its normal attack and release; going between the original curve and a model starts from the clean sound and fades in. Tested: no sample-to-sample step beyond the signal's own across repeated switching.
5. **Off is off:** Analog Warmth off is bit-identical to the clean compressor at every model (tested), and the model chosen is remembered.
6. **Older sessions.** The original Analog Warmth (the smooth curve described above) is kept as "Classic": a session saved before the models has no model in it, so it opens with Classic, bit-identical to before (tested), and its dropdown reads Classic with nothing selected. Choosing any model (even Black), or the host automating the model, replaces it. A new instance uses the models. Presets leave the choice alone.

**Measured, not claimed** (`WarmthModelTests`): the output equals the shared FET cell and output stage applied to the gain-reduced sound, sample for sample; the second harmonic is ordered Blue > Black > Anniversary; a short burst is caught in the order Blue, Black, Lindell (attack 0.7, 1, 1.6); at -18 dBFS every model keeps the loudness within half a decibel of the clean compressor. **Not done:** level-matched listening on real material, and DAW checks. The dropdown list cannot carry a separate tooltip for each entry in this version of JUCE, so the description is on the dropdown itself and follows the model shown.

## Ranges

Threshold -60 to 0 dB. Ratio 1:1 to 20:1, continuous. Attack 0.1 to 100 ms. Release 10 to 2000 ms. Knee 0 (hard) to 24 dB. Make-up 0 to 24 dB. Mix 0 to 100 %. Values outside a range, and NaN, are clamped inside the engine (`Extreme settings`).

## Real-time behaviour

`process()` does not allocate, lock or log. Make-up gain and mix are smoothed over 20 ms, so changing them while running does not click (`Parameter changes`). Non-finite input samples are treated as silence (`non-finite input`). The reduction snaps to exactly zero below 1e-9 dB and `juce::ScopedNoDenormals` is set, so there is no denormal tail. Block size does not change the output (`Block size`).

## Latency

None. There is no look-ahead and no oversampling.

## Not yet done

- Real audio listening checks (guitar, vocal, bass). The tests use synthetic signals only, so no listening verification is claimed.

## Known limitation: sine waves read slightly louder

The detector follows the peak of every wave, one sample at a time, and the attack and release smoothing is applied to the resulting gain reduction. For noise or full music this behaves as expected. For a pure low-frequency sine the reduction rises and falls a little within each wave, so a sine can come out up to about 0.8 dB louder than a steady signal at the same peak level with the Acoustic Guitar preset. Auto Makeup is exact for a steady level (`Make-up gain` test) and close for music. A peak-hold or envelope stage in the detector would remove this, at the cost of lengthening the effective release; that is a later decision, not made here.
