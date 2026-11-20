---
id: ANA-02
priority: medium
effort: high
dependencies: []
conflicts: []
---

**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/335

# ANA-02: Add MP3 support to the standalone Audio Analyzer

Let people analyse exported MP3 files for integrated loudness and true peak, as they can WAV, AIFF and FLAC files.

## What was found

- JUCE's own MP3 decoder is already built into the app (`registerBasicFormats`), under the same licence as the rest of JUCE, so no extra codec, paid library or system codec is needed.
- Only the file-type lists stopped MP3 files: the supported-extension check, the file chooser, the macOS document types, the wording, and the installer text.
- JUCE guesses an MP3's length from the file size and its first frame unless an Xing or Info header is present. For a variable bitrate file without one, the guess is wrong and a good file looks damaged. The app now counts the frames itself.

## What was done

- MP3 added to the supported types, the chooser, drop and open-with (macOS `DOCUMENT_EXTENSIONS`), the wording and both installers' text.
- The decoded sound is measured with the same loudness and true peak code as every other file.
- `scanMp3Frames` (Layer III, every sample rate music programs use) gives exact length and average bitrate; leading Xing, Info or VBRI frames are not counted.
- Results mark the file as lossy, show "Not applicable" for bit depth, show the average bitrate (kilobits per second), and the advice never claims a bit depth or CD quality.
- Tests: hand-built silent MP3 files (constant and changing bitrate, 32, 44.1 and 48 kHz, mono and stereo, twelve minutes, cut short, empty, not an MP3) and the regression tests for the other formats.

## Limits to keep honest

- Not trimmed: the encoder's start and end silence (usually under 0.05 s). It has almost no effect on loudness.
- The reference MP3s in the tests are a pure tone (a 997 Hz sine at -20 dBFS), checked against ffmpeg's EBU R128 meter. Music encoded at other settings is not in the tests.
- Peaks decoded from an MP3 can exceed 0 dBFS. That is reported as measured.
