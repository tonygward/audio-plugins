You are an experienced C++ audio developer working on **Songsmith Maximiser**, a JUCE VST3/AU/Standalone plugin in `C:/dev/audio-plugins/maximiser`. JUCE is at `C:/dev/JUCE`. Follow the repo's `CLAUDE.md` (design principles and working rules) throughout.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

Run this after task 06 (true peak by default) if that hasn't merged yet. Both touch the editor and the README.

# Task 07: Loudness meter (LUFS)

**Branch:** `maximiser/07-loudness-meter`

## Why

The maximiser is usually the last plugin before export, and the question people ask there is "how loud is this?", in LUFS, the unit streaming services use. Today you have to open a separate meter to find out.

The meter only hears audio while it plays through the plugin in real time. It isn't an offline measurement of the whole file the way a DAW's export analysis is. That's still useful: play the song through and the Integrated reading is the song's loudness. The editor and README say this plainly, so nobody mistakes it for a file analysis.

## Read first

- `maximiser/Source/PluginProcessor.h/.cpp` (the meters the audio thread publishes, `pushPeak`, `readAndReset`), `Meters.h`, `EditorLogic.h` (`MeterBallistics`, `EditorLayout`), `PluginEditor.h/.cpp`, `TruePeak.h`.
- `maximiser/README.md` (the editor and design-choice sections), `docs/style-guide.md`, `shared/` (the UI library).
- The standards: **ITU-R BS.1770-4** (K-weighting, gating, channel weights) and **EBU R 128** with **EBU Tech 3341** (Momentary, Short Term and Integrated loudness, and the compliance test signals). The L1 had no loudness meter, so this is a Songsmith addition: say so in the README.

## Requirements

### Measuring (audio thread)

- Measure the plugin's **output**: after the limiter and dither, which is what will be exported. When the plugin is bypassed, measure what passes through.
- K-weighting as in BS.1770-4: the two-stage filter (high shelf, then high pass). The standard gives coefficients for 48 kHz only, so derive them for the session's sample rate from the analogue prototype in `prepareToPlay`, and test at 44.1, 48, 96 and 192 kHz.
- Mono and stereo, with BS.1770 channel weights (1.0 for left and right). If the plugin supports other layouts, say what you did in the PR.
- Real-time safe: the audio thread only filters and adds up the mean square per 100 ms step. It publishes each finished step to the UI through a lock-free single-producer, single-consumer FIFO allocated in `prepareToPlay`. No allocation, locks or logging in `processBlock`.
- The meter must not change the audio. The existing "identical with and without an editor open" test still passes.

### Readings (UI thread, from the 100 ms steps)

- **Momentary** (400 ms window), **Short Term** (3 s window) and **Integrated** (everything since the last reset, with the −70 LUFS absolute gate and the relative gate 10 LU below, as in BS.1770-4). Use a histogram or running sums so Integrated costs the same after an hour as after a minute.
- Show `−∞`, or a dash, until there is enough audio for a reading, never a made-up number. Keep to the plugin's finite dB floor (`Max::gainToDb`).
- **Reset**: a button, plus clicking the readout. Also reset automatically when the host starts playback from a stop, so each play-through measures one pass (read the play head in `processBlock` and flag it to the UI without locking). Make that automatic reset a design choice listed in the README, and don't save it as a parameter.
- The meter isn't a parameter and isn't automatable. If the readings would be useful to keep, they don't belong in the session: don't add anything to the state, and don't bump `stateVersion`.

### Editor

- Add a loudness section: **Integrated** as the main number, and **Short Term** and **Momentary** smaller, all in LUFS. A short-term bar or history strip is welcome if it fits. Change the layout (and `EditorLayout`'s fixed aspect ratio if needed) rather than cramming. It must work from 75 % to 300 % and stay crisp.
- Use the shared UI library, the type scale and the accent colour. No new fonts, no system fonts.
- Plain words: "Integrated", "Short Term", "Momentary", "Reset". `LUFS` is an allowed unit. Tooltips explain each reading in one sentence, for example Integrated: "The loudness of everything played since the last reset. Play the whole song through for its loudness." Add one line on the section itself: "Measures what plays through the plugin, as it plays." Don't show Loudness Range: "LU" isn't on the allowed list, and it can come later.
- The plain-words test passes with the new labels and tooltips.

## Tests

- **EBU Tech 3341 compliance:** generate the test signals in code (no audio files in the repo) and meet the standard's tolerances (±0.1 LU), at least for: 1 kHz sine at −23 dBFS on both channels reads −23.0 LUFS (Momentary, Short Term and Integrated); the same at −33 dBFS reads −33.0; the gated sequences (cases 3-5 in Tech 3341) give the right Integrated value; the Momentary and Short Term windows rise and fall over 400 ms and 3 s.
- K-weighting response matches BS.1770-4's at 48 kHz, and the readings agree within 0.1 LU at 44.1, 96 and 192 kHz.
- Silence: no reading, no NaN or infinity anywhere.
- Integrated after a reset only counts audio after the reset. Starting playback from a stop resets it, but carrying on playing doesn't.
- Mono matches stereo with the same signal in both channels, minus the expected 3 dB.
- Timing: block sizes of 1, 64, 441 and 4096 samples, and blocks that don't line up with the 100 ms steps, all give the same readings.
- A real-time check in the style of the existing tests: no allocation in `processBlock` with the meter running.

## Finish

- Build in Release, run `MaximiserTests.exe`, with no new warnings. Note the extra processing cost (`--bench` if it exists, otherwise measure it) in the README.
- Before and after screenshots of the editor at 100 % and 200 % (`--screenshot`, `--scale 2`), committed and linked by SHA in the PR, then deleted before merging (see `CLAUDE.md`).
- README: what the meter shows, that it only hears real-time playback, the automatic reset, the sources (BS.1770-4, R 128, Tech 3341) and which test checks each.
- Open the PR with what changed, test output, known limitations, and "Not yet verified in a DAW". Ask in `docs/host-checks.md` for a check against a reference meter (for example the DAW's own loudness meter) on a real mix. Move this prompt to `done/` and update `maximiser/prompts/README.md` in the same PR.
