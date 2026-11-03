# Songsmith Maximiser

A look-ahead brickwall maximiser built from the Waves L1 manuals' published behaviour. It is a sibling of `eq/` (JUCE + CMake).

## Build and test

```
cmake -S maximizer -B maximizer/build -A x64 -DJUCE_DIR=C:/dev/JUCE
cmake --build maximizer/build --config Release --parallel 8
maximizer/build/MaximizerTests_artefacts/Release/MaximizerTests.exe
```

## Identity and parameters (step 01)

- VST3, Standalone, plus AU on Apple. AU: `aufx Mxlm Tnyw`. Bundle ID: `com.songsmithstudio.songsmithmaximiser`.
- Stable IDs (version 1): `threshold` (-30..0 dB, default 0), `ceiling` (-30..0 dB, default 0), `release` (0.01..1000 ms, log control, default 1 ms), `bypass` (host bypass, 20 ms cross-fade).
- **Finite dB floor (design choice):** the manual shows silence and full attenuation as minus infinity. Here every level/gain conversion stops at -120 dB, so meters, readouts and state never hold -inf or NaN (`Max::gainToDb`, `Max::dbToGain`; tested in "Finite dB floor").
- State is versioned (`version` = 1). Missing parameters restore to their defaults; out-of-range values are clamped; unknown IDs are ignored.
- Step 01 processing is transparent. The limiter arrives in step 02.

## Limiter (step 02)

`Source/BrickwallLimiter.h` holds the look-ahead sample-peak limiter. It can be tested without the plug-in.

- **Gain law (from the manual):** below Threshold the gain is a constant Out Ceiling − Threshold (dB). Above it, the limiter adds attenuation (≤ 0 dB) so no sample exceeds the ceiling. Tested in "T = -6, C = -1" and "Threshold / ceiling interaction".
- **Stereo:** one shared gain, driven by the louder channel, so the stereo image never shifts. Tested in "Stereo image".
- **Look-ahead (design choice):** 1.5 ms, rounded up to whole samples. That is 67 samples at 44.1 kHz, 72 at 48 kHz and 144 at 96 kHz. It is also the reported latency. Bypass delays the dry signal by the same amount, so switching bypass never shifts the audio in time. Tested in "Look-ahead and reported latency" and "Bypass is time-aligned".
- **Gain scheduler (design choice):** the needed gain is `r = min(1, ceiling / peak)`. It goes through an instant-attack release envelope, then a sliding minimum over L + 1 samples, then a box-car mean over the same L + 1 samples. Each term of that mean is a minimum over a window that contains sample n − L, so the gain at the moment the delayed sample n − L is output is ≤ r[n − L]. The ceiling therefore holds at the exact sample it protects, with no clipper. A final `min(g, r[n−L])` removes float rounding in the running sum. The attack is a linear ramp over the look-ahead. Tested in "Impulses at every block offset", "Automation never breaks the ceiling" and "Variable block sizes".
- **Release law (design choice):** the gain recovers exponentially towards unity, with time constant = Release. After one Release time, 63 % of the gain reduction has recovered. The result then passes through the same 1.5 ms box-car, so even 0.01 ms recovers over at least about 1.5 ms. Tested in "Release law".
- **Parameter smoothing:** Threshold and Out Ceiling glide over 20 ms. Each sample is protected by the ceiling that was current when it was detected.
- **Reset:** `AudioProcessor::reset()` clears the delay line and the envelope. Tested in "Reset clears…".
