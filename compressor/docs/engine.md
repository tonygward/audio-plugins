# Songsmith Compressor engine

The engine of the planned **Songsmith Compressor** (COMP-01). It is a clean, conventional compressor and is separate from the 1176-inspired Songsmith Analog Compressor, which it does not touch. Code: `Source/CompressorEngine.h` and `.cpp`. Tests: `Tests/CompressorTests.cpp` (`CompressorTests`).

This is slice 1 of COMP-01: the engine only. There is no plugin, editor, preset or Analog Warmth yet, so nothing here is audible to musicians.

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

## Ranges

Threshold -60 to 0 dB. Ratio 1:1 to 20:1, continuous. Attack 0.1 to 100 ms. Release 10 to 2000 ms. Knee 0 (hard) to 24 dB. Make-up 0 to 24 dB. Mix 0 to 100 %. Values outside a range, and NaN, are clamped inside the engine (`Extreme settings`).

## Real-time behaviour

`process()` does not allocate, lock or log. Make-up gain and mix are smoothed over 20 ms, so changing them while running does not click (`Parameter changes`). Non-finite input samples are treated as silence (`non-finite input`). The reduction snaps to exactly zero below 1e-9 dB and `juce::ScopedNoDenormals` is set, so there is no denormal tail. Block size does not change the output (`Block size`).

## Latency

None. There is no look-ahead and no oversampling.

## Not yet done

- The plugin target, parameters, state and stable identity.
- Metering in the editor, presets, Analog Warmth, installer and CI wiring.
- Real audio listening checks (guitar, vocal, bass). The tests use synthetic signals only, so no listening verification is claimed.
