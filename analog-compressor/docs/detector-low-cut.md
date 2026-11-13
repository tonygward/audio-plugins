# Detector Low Cut (ACOMP-07)

**Low Cut** is a **Songsmith addition** to the 1176-inspired design. The hardware has no such control ([references.md](references.md)); everything below is a Songsmith design choice, not published 1176 behaviour.

## What it does

The compressor decides how much to turn the sound down by listening to the level. Deep bass carries a lot of level, so a loud kick or bass note can make the whole sound duck. Low Cut removes the deepest bass from **what the compressor listens to** only. The audio you hear is never filtered: the bass stays as it was, and it is simply no longer the reason for the gain reduction.

## ACOMP-12: the Ignore Below knob (superseded by the ACOMP-16 slider above; kept as history)

The dropdown named Low Cut became a round knob named **Ignore Below**, in the same look as Input, Output, Attack and Release. It sits right of Gain Reduction Off (design units x 760, y 302, 176 x 50), clear of the Gain Reduction readout above and the hint line below. The name and the readout (`Off`, `60 Hz`, `80 Hz`, `100 Hz`, `150 Hz`, `200 Hz`) are to the right of the knob. Double-click returns it to Off; arrow keys step through the values.

Tooltip: "Ignore bass below this frequency when deciding how much to compress. The bass you hear stays unchanged."

**Trade-off, stepped rather than continuous:** the knob is *stepped* with the six original values. `detectorLowCut` stays a six-item choice, so its ID, its host-normalised automation values (index / 5) and every saved session and preset keep their meaning exactly, with no migration and no old/new precedence rules. A continuous Hz parameter would need a new parameter ID, a state-version migration and a rule for which value wins when a session holds both; that risk was not worth it for this change. Only the host's displayed parameter name changed, from "Low Cut" to "Ignore Below" (the ID is unchanged). The filter, its slope, location and smoothing are untouched. A continuous range could follow later as its own task.

Not yet verified in a DAW; screenshots are not refreshed in this change because the run could not build the plugin.

## ACOMP-16: now a continuous slider, 20 to 500 Hz

The knob is replaced by a horizontal slider (no rotary remains): **Ignore Below** on the left, an editable readout on the right (`127 Hz`, or `Off`), the track and handle in the accent colour beneath, and `20 Hz` and `500 Hz` under the ends. It occupies the same place (x 760, y 302, 176 x 50 design units). Dragging, the arrow keys (one whole Hz; Shift, Page Up and Page Down ten; Home is Off; End is 500 Hz), double-click (Off) and typing into the readout all work. Typed text accepts `127`, `127 Hz`, `0.3 kHz`, `off`; below 20 Hz becomes 20 Hz, above 500 becomes 500, and text that is not a frequency changes nothing. Whole Hz only: `127.4` becomes 127.

Tooltip: "Ignore Below: the compressor ignores bass below this frequency when deciding how much to compress. The bass you hear stays unchanged. Drag, use the arrow keys, or click the number to type a frequency from 20 to 500 Hz. Double-click for Off."

### Parameters and compatibility

| Parameter | Type | Role |
|---|---|---|
| `ignoreBelowHz` (version hint 5, new) | float, plain value 0 to 500, whole Hz | What the slider shows and what the audio uses. `0` is Off; otherwise 20 to 500 Hz. Default Off. Display name "Ignore Below". |
| `detectorLowCut` (version hint 3, **unchanged**) | six-item choice, 0 to 5 | Kept exactly as before so saved sessions, presets and host automation keep their meaning. Not shown in the window. Host display name is now "Ignore Below Steps" (the ID is unchanged). |

- **Off is its own state.** The host-normalised range of `ignoreBelowHz` spends its first 4 % on Off (the slider's left end), then runs logarithmically from 20 to 500 Hz (the last 96 %), so 100 Hz sits halfway along the 20 to 500 Hz part. 20 Hz as the lowest cutoff is kept: nothing in the DSP needed a lower one (the filter is second order, so 20 Hz is already far below any note the detector needs to ignore).
- **State version 5.** Sessions saved at version 1 to 4 have no `ignoreBelowHz`. On load it is set from their saved `detectorLowCut` through the exact mapping Off, 60, 80, 100, 150, 200 Hz (a missing value is Off). Other saved values are not touched. Version 5 sessions load their own `ignoreBelowHz`; the saved `detectorLowCut` is kept but does not override it.
- **Presets.** Preset files now carry `ignoreBelowHz` and, for older versions of the plugin, a `detectorLowCut` attribute (the exact legacy step, otherwise Off). A preset file with only `detectorLowCut` loads as the same frequency. Factory starting points leave Ignore Below Off.
- **Which one wins (new versus old automation): the one that changed most recently.** The audio thread watches both. If `detectorLowCut` changes (old automation, an old preset) its frequency is used and the slider follows within about 0.1 s. If `ignoreBelowHz` changes (the slider, new automation, a preset) it takes over. If both change in the same block, `ignoreBelowHz` wins. After a session loads, the loaded values are the starting point, not a change. **Limitation:** a host that writes both parameters continuously (for example a stale lane for each) will have them fight, as with any two automated parameters; write automation for the new one. Hosts and projects that only automate the old parameter keep working with the six frequencies they always had.
- **Filter and smoothing are unchanged**: the same 12 dB/oct high-pass on the detector only, a 20 ms glide in octaves, double precision, designed at the oversampled rate. A continuous cutoff uses the same glide, so sweeping it every block is click-free (tested). The cutoff is limited to 0.45 times the running sample rate inside the engine, which no value up to 500 Hz reaches.

### Tests added

| Claim | Test |
|---|---|
| Frequency response across the range at 44.1, 48 and 96 kHz | `EngineTests.cpp`, "Continuous cutoff (engine)" and "(detector response)": bass gain reduction never rises with the cutoff, 127 Hz takes at least 3 dB off, 499 Hz leaves under 0.5 dB, 4 kHz untouched, the same Hz gives about the same result at 44.1 and 96 kHz |
| Processor, mono and stereo, every oversampling mode | "Continuous cutoff (processor)": 127 Hz cuts bass gain reduction by at least 3 dB; 2 kHz unaffected |
| Audible path unfiltered | Output is sample for sample identical at Off, 73 and 499 Hz when no gain reduction applies |
| Click-free automation | Cutoff jumped every block over 20 to 500 Hz and Off: finite, bounded, no sample-to-sample step above 0.06 |
| Latency, bypass | Unchanged with a cutoff set; bypass bit-exact |
| Legacy recall | `AnalogCompressorTests.cpp`: every saved six-step value at state versions 3 and 4 loads the exact Hz and leaves input and ratio alone; whole-Hz round trip; every whole Hz 20 to 500 survives the normalised round trip; text parsing |
| Precedence | Old automation is heard, then the slider wins, then old automation wins again; a loaded session ignores a stale legacy value |
| Presets | Preset files without `ignoreBelowHz` load the legacy frequency; a saved preset recalls 127 Hz |
| Editor | Slider geometry (name left, readout right, track below, `20 Hz` and `500 Hz` under the ends, inside its place), typing, Off, keyboard, tooltip wording, logarithmic track |

**Not yet verified in a DAW**, and the change was written without a local build: it needs a native CI run, a real-host check of old-session recall and old-parameter automation (daw-checks.md, check 11), and refreshed screenshots at the default and minimum window sizes and on a high-DPI display.

## Design choices (ACOMP-07; the six steps below are now the legacy mapping)

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
