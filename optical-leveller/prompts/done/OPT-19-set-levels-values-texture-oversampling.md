---
id: OPT-19
priority: medium
effort: high
dependencies: []
conflicts: []
---

# OPT-19: Optical Leveller: Set Levels target gain reduction, editable Gain and Peak Reduction, lighter texture and Oversampling

Issue: [songsmithstudio#19](https://github.com/tonygward/songsmithstudio/issues/19). Implemented in [audio-plugins#358](https://github.com/tonygward/audio-plugins/pull/358), released in 0.5.74. This prompt was written after the work, from the issue, so that the task has one in `done/`; an archived prompt means merged, not delivered.

## Objective

Bring the Songsmith Optical Leveller into line with the Analog Compressor's Set Levels experience, let the musician type exact Gain and Peak Reduction values, lighten the grey faceplate, and add real Oversampling, with a shared listening ear in the Set Levels panel.

## Current and desired behaviour

- No Set Levels. Desired: a **Set Levels** button that listens to a typical part, takes an editable **Target Gain Reduction** in dB (`-3.0 dB` means 3 dB of reduction), and suggests the Peak Reduction that gives it. Apply sets the real parameter and closes the panel at once; Cancel and Escape change nothing; Undo restores the old value. The target is an average over the sound played, and the panel says so.
- Gain and Peak Reduction show their value as text only. Desired: the number is a field you can click and type into, in step with the knob, automation, presets and Undo.
- The faceplate grey is too dark. Desired: noticeably lighter, with legible labels.
- No Oversampling. Desired: Off (default), 2x, 4x, doing real processing, reporting latency to the host, with older sessions loading as Off.
- Shared ear: lit with "Listening…" only while sound is arriving, "Waiting for audio…" before, hidden otherwise; the same component in the Analog Compressor and (later) the Maximiser.

## Implementation notes

- Shared: `shared/Source/SongsmithSetLevels.h` (dialog and ear), `shared/Source/SongsmithLevelCapture.h` (listening buffer).
- Optical Leveller: `OptCalibration.h` replays the capture through the real engine and bisects Peak Reduction for the target.
- Oversampling uses `juce::dsp::Oversampling` half-band polyphase IIR stages; latency is reported from a message-thread timer, never the audio thread; Bypass uses the dry signal delayed by the same latency. The parameter is `oversampling` (version 2) and `stateVersion` is 2.

## Acceptance criteria and tests

See the issue. Covered by `OpticalLevellerTests` (typed values, Oversampling latency and dry alignment, Set Levels accuracy, silence, too short, too quiet, too loud, target parsing, ear states, Apply, Cancel, Undo, a change while listening) and `AnalogCompressorTests` (the ear in its panel).

## Dependencies and risks

- Oversampling has a modest audible effect here, because the engine is a smooth gain control with no saturation.
- Not yet verified in a DAW. No listening comparison of the lighter faceplate or of Oversampling.
- The Maximiser has no Set Levels yet (songsmithstudio#18), so it does not show the ear.

## Build and release

Optical Leveller and Analog Compressor change; users will notice; changelog notes were added under Unreleased and are in 0.5.74.
