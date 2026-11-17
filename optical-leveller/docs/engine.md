# Optical Leveller engine

The reference unit is the **Teletronix LA-2A Leveling Amplifier**, a valve-and-transformer levelling amplifier whose gain is controlled by a light-sensitive cell lit by the sound. Songsmith Optical Leveller is an original design inspired by it. It models how the optical cell behaves in time, not the valves or the transformers. This page says which behaviours are published, which are Songsmith design choices, and which test checks each.

The code is `optical-leveller/Source/OpticalEngine.h`. The tests are in `optical-leveller/Tests/EngineTests.cpp`.

## Signal path

```
input -> detector (smoothed power of the loudest channel, in dB)
      -> gain computer (Peak Reduction sets the threshold; Compress or Limit sets ratio and knee)
      -> optical cell: a quick part and a slow part
      -> one gain for every channel, then Gain
```

No latency, no look-ahead, no oversampling. The engine does not allocate, lock or log while processing.

## Published behaviour, used as targets

Source: Universal Audio's Teletronix LA-2A specification ([help.uaudio.com](https://help.uaudio.com/hc/en-us/articles/206356233-Teletronix-LA-2A-Classic-Leveling-Amplifier)) and its article quoting the original manual ([uaudio.com webzine, June 2003](https://www.uaudio.com/webzine/2003/june/text/content4.html)).

| Behaviour | Published | Songsmith | Measured | Test |
|---|---|---|---|---|
| Attack | 10 ms, programme dependent | cell time constant 8.5 ms plus a 2 ms detector, so 63 % in about 10 ms | 9.7 ms at 44.1, 48 and 96 kHz | `Attack: about 10 ms, as published` (accepts 6 to 16 ms) |
| Release, first half | about 0.06 s to 50 % | quick part time constant 69 ms | 60.5 ms at 44.1, 48 and 96 kHz (after a 0.3 s signal) | `Release after a short signal` (accepts 40 to 90 ms) |
| Release, the rest | 0.5 to 5 s to complete release, depending on how much reduction came before | slow part, 0.5 s (fresh) to 5 s (memory full) | 80 % clear after 0.18 s for a 0.3 s signal and 6.0 s for a 20 s signal at -6 dBFS | `Release depends on the programme` |

The older manual quoted in the 2003 article gives a wider range for the slow tail (1 to 15 seconds). Songsmith uses the current specification's 0.5 to 5 seconds. Neither range is a measurement of a particular unit, so treat the long tail as a design target.

## Songsmith design choices

These are not published as exact numbers, so they are Songsmith's choices, written down here so each can be tested and changed on purpose.

- **Peak Reduction** sets the threshold from -4 dBFS (0) to -40 dBFS (100), measured as RMS. `thresholdDb()` in the engine. Test: `The static curve`.
- **Compress** is 3 to 1 with a 10 dB soft knee. **Limit** is 12 to 1 with a 4 dB soft knee. The original describes Compress as roughly 4 to 1 and Limit as firmer, and says the true ratio varies with the signal, so these are round design values, not copies. Tests: `The static curve`, `Compress and Limit`.
- **One detector for both channels** (the louder of the two), and one gain for both, so the stereo image holds. Test: `Stereo: one gain for both channels`.
- **Feed-forward**: the detector listens to the input, not the output. The original's sidechain listens after the gain element, which changes the feel at high settings; this is a simplification.
- **The cell's memory.** A slow average of the reduction (it fills over about 3 seconds and fades over about 8) sets how the reduction is shared between the two parts and how slowly the slow part lets go: after a short signal 90 % of the reduction is in the quick part, with a full memory 25 %. This is what makes a long, hard signal let go slowly. Tests: `The memory builds while the reduction is held and fades afterwards`, `Release depends on the programme`.
- **Level-matched starting points** at -18 dBFS. Test: `Every factory starting point is level-matched at -18 dBFS`.

## What is not modelled

Valve and transformer colour, noise, the Emphasis control of other models, the Stereo Link switch, and the unit's component tolerances. The engine is deliberately clean so the levelling can be judged by itself.

## Robustness checks

Block-size independence (bit-identical from 1 to 1000 samples per block), sample rates of 44.1, 48 and 96 kHz agreeing, silence staying silent, infinity and NaN input not poisoning the detector, huge input staying finite, and out-of-range settings being clamped.
