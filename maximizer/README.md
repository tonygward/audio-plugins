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
