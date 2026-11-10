# Detector Low Cut (ACOMP-07)

**Low Cut** is a **Songsmith addition** to the 1176-inspired design. The hardware has no such control ([references.md](references.md)); everything below is a Songsmith design choice, not published 1176 behaviour.

## What it does

The compressor decides how much to turn the sound down by listening to the level. Deep bass carries a lot of level, so a loud kick or bass note can make the whole sound duck. Low Cut removes the deepest bass from **what the compressor listens to** only. The audio you hear is never filtered: the bass stays as it was, and it is simply no longer the reason for the gain reduction.

Tooltip (sentence case, plain words): "Low Cut: stops deep bass from making the compressor turn the whole sound down. Only the part of the sound the compressor listens to is filtered; the bass you hear stays as it is. Off is the usual setting."

## Design choices

| Choice | Value | Why |
|---|---|---|
| Parameter ID | `detectorLowCut` (choice, version hint 3) | New ID; no existing ID changes. |
| Settings | Off, 60, 80, 100, 150, 200 Hz | Six steps cover kick fundamentals (about 50 to 100 Hz), bass guitar (low E is 41 Hz; its body sits at 80 to 200 Hz) and vocals (fundamentals above about 80 Hz, so 150 and 200 Hz start to touch a deep male voice). Steps stay on a combo box in the 150 px place ACOMP-08 kept free. |
| Slope | 12 dB/oct Butterworth (second-order high-pass) | Steep enough to calm kick and bass; gentle enough that vocal body is not ignored. Zero latency (an IIR filter), so latency is unchanged. |
| Default | Off, for fresh instances, sessions saved at state version 1 and 2, and factory starting points | Nothing sounds different until the musician chooses. |
| Presets | Saved in user presets; missing from older preset files means Off; factory starting points set Off | Matches how grOff is handled. |
| Position | Right of Gain Reduction Off, under the Gain Reduction meter | Both control what the compressor reacts to (principle 4). |

The slope and the six frequencies are reasoned from instrument ranges, not measured with a panel of listeners. The listening notes below are what to check by ear; they have **not** been done yet.

## Signal flow

The engine detects after gain reduction (a feedback design, [engine.md](engine.md)). A copy of the input to the gain cell is filtered, and only that copy is used to find the level. The audio path multiplies the unfiltered samples by the gain. Stereo linking is unchanged: each channel is filtered, then the loudest channel sets one gain for both.

- **Smoothing:** the cutoff glides (in octaves) and the Off/On blend fades over about 20 ms. Nothing is allocated, locked or logged in the audio path.
- **Oversampling:** the engine runs at the oversampled rate, and the filter is designed for that rate, so the cutoff in Hz is the same at Off, 2x and 4x. Coefficients and states use double precision because the cutoff is a tiny fraction of the sample rate at 4x.
- **Off and settled:** the filter is skipped entirely, so the default sound is bit-exact as before.
- **Bypass and latency:** untouched. Bypass still passes the (delay-matched) input; the latency reported for each oversampling mode is the same with Low Cut on or off.

## Tests (AnalogCompressorTests, `EngineTests.cpp` "Detector Low Cut", `AnalogCompressorTests.cpp`)

| Claim | Test |
|---|---|
| Bass-heavy signal gets less gain reduction with Low Cut on | 50 Hz sine: at least 3 dB less at 100 Hz, almost none at 200 Hz, never more as the cutoff rises, at 44.1 and 48 kHz (engine) and at Off, 2x and 4x (processor) |
| Mid-range is not affected | 1 kHz sine: same gain reduction at every setting (within 0.15 dB engine, 0.3 dB processor) |
| Filter is confined to the detector | With no gain reduction (signal below every knee), output is identical sample for sample with Low Cut Off and at 200 Hz, for 30 Hz to 8 kHz, at Off, 2x and 4x |
| Stereo linking | Bass on one channel and a 1 kHz tone on the other: with Low Cut the tone leads, and the reduction equals the tone alone |
| Smoothing | Changing the setting while playing moves gain reduction by under 0.3 dB a sample |
| Automation | Changing the setting every block at Off, 2x and 4x stays finite and never louder than the input |
| Latency, bypass | Same latency on or off at every oversampling setting; bypass stays bit-exact with Low Cut on |
| Recall and migration | Round trip; fresh instance Off; version 1 and 2 sessions load it Off even if the current value differs; parameter ID, range, default and version hint pinned |
| Editor | Position (right of Gain Reduction Off, inside the reserved place, 150 by 32 design units), label, tooltip wording, items, and the box follows the parameter |

## Listening notes (to do by ear; not yet done)

1. **Kick and bass, mixed loop:** with a gentle ratio and a hot Input, compare Off and 100 Hz. Expect the vocal or guitar on top to duck less when the kick hits. If the bass feels louder in the mix, that is the compressor leaving it alone, not a level change; match Output before judging.
2. **Bass alone:** at 150 and 200 Hz the notes should pass through with less squash and more thump. Too much and the bass feels uncontrolled; step back to 80 Hz.
3. **Lead vocal:** 60 to 100 Hz should sound the same as Off. At 150 and 200 Hz a deep voice may feel less controlled.
4. **Automation:** sweep between Off and 100 Hz during playback; listen for clicks or zipper noise (there should be none).

Not yet verified in a DAW: see [daw-checks.md](daw-checks.md), check 11.
