# Songsmith Audio Analyzer

A standalone app for checking a **finished song**. Open a WAV, AIFF, FLAC or MP3 file and it shows how loud the file is and how it was recorded, with short, optional advice in plain English. It needs no recording software, and it is not a plugin.

![Songsmith Audio Analyzer showing the True Peak chart, the loudness, the waveform and the advice for a finished file](docs/images/audio-analyzer-results.png)

![A loud file whose left channel goes above 0 dBTP: the True Peak chart turns red above the 0 line, and the right channel stays below it](docs/images/audio-analyzer-results-over.png)

![The waiting screen: drop a file or choose one](docs/images/audio-analyzer-waiting.png)

![A file that could not be analysed, and what to try](docs/images/audio-analyzer-error.png)

*Real screenshots at 100 %, rendered by the test executable (`AudioAnalyzerTests --screenshots <folder>`).*

## What it shows

The results are a small dashboard, and the **True Peak chart** is the main picture:

- **True Peak chart** (right): one tall bar for each channel (Left and Right, or one bar for Mono) showing the **highest true peak in the whole file** on a scale from -48 to +6 dBTP, with 0 dBTP marked. The bar turns red above 0, and the number under it says exactly how far (for example +0.9). The small ticks beside each bar are the highest **stored sample**, which is a different reading and can be a little lower. It is a result for the whole file, not a live meter, and it says so.
- **Integrated Loudness** (top left): the large number. It is the average loudness of the whole file. There is no pass or fail and no loudness a song has to reach.
- **Waveform** (left): how big the sound is from the start to the end of the song. Taller means a bigger sound at that moment, and red marks a moment that goes past the top. It is **not** a loudness chart and does not show true peak. It holds the same number of slices (800) however long the file is, so an hours-long file uses no more memory.
- **Facts** (below): sample rate, bit depth and encoding, duration and channels.

Hover over any of them for a plain explanation. Each one also has a name and a description for screen readers that include the real numbers.

| Reading | What it tells you |
|---|---|
| **Integrated Loudness** (LUFS) | How loud the whole file is on average, measured the way broadcasters and streaming services measure it (ITU-R BS.1770-4, with the standard's gates). |
| **True Peak** (dBTP) | The highest level the sound reaches once the stored samples are turned back into a smooth wave. It is measured on a 4× oversampled copy (2× from 96 kHz), so it can be a little above the highest stored sample. The highest stored sample is shown beneath it, clearly labelled as a sample peak. |
| **Sample Rate** | How many times a second the sound is stored. |
| **Bit Depth and Encoding** | How finely each sample is stored, and how the file is stored (uncompressed, lossless, floating point or compressed). This is how the file is kept, not how the sound was first recorded. An MP3 has no bit depth, so it says **Not applicable**, and shows that it is lossy with its average bitrate. |
| **Duration** | How long the file plays. |
| **Channels** | Mono, stereo or the number of channels. |

If a reading cannot be made, the box says **Not available** and why (for example, "Too short or too quiet to measure"). Nothing is invented.

Under the readings, **plain-English guidance** says what matters, why, and whether you need to do anything. It separates measured facts from listening suggestions and from what the tool cannot know. There is no score, no pass or fail and no platform target, because services and listeners normalise loudness in their own ways. It is the same guidance as the rest of Songsmith (REC-01, [shared/docs/file-recording-advisor.md](../shared/docs/file-recording-advisor.md)).

## Using it

1. Open the app. Drop a WAV, AIFF, FLAC or MP3 file on the window, or click **Choose File...** (shortcut: Ctrl+O, or Cmd+O on a Mac).
2. A progress bar shows while the file is read. A long file does not freeze the window, and **Cancel** (or Escape) stops it.
3. Read the results. Click **Analyse Another File**, or drop another file anywhere on the window.

If a file cannot be read (a different kind of file, a damaged file, a folder), the window says so in plain words and tells you what to try. Your file is only read: it is never changed, copied or sent anywhere.

The window scales as a whole from 75 % to 200 % and keeps its shape.

## MP3 files

An MP3 is a compressed (lossy) file, so a few things are different:

- **The readings are for the decoded sound.** The app turns the MP3 back into sound, the way a player does, and measures that. Loudness and true peak are those of what a listener hears, so they can differ a little from the WAV the MP3 was made from, and compression can push the loudest peaks slightly higher, even above 0 dBTP.
- **No bit depth.** It is not applicable to an MP3, so none is shown. The app shows **MP3, lossy** and the average bitrate (kilobits per second), worked out from the size and length of the file. For a variable bitrate file it is the average over the whole file.
- **Exact length.** The app counts the file's frames itself, so the length and average bitrate are exact for constant and variable bitrate files, with or without the headers some encoders add. This covers MP3 (MPEG Layer III) files at every sample rate music programs use.
- **Encoder silence.** An encoder adds a very short stretch of silence at the start and end (usually under a twentieth of a second). The app's decoder plays it, as most players do, and it counts towards the length. It has almost no effect on the loudness reading, and it is not trimmed.
- **Not knowable from an MP3:** the loudness and true peak of the original before it was compressed, and the settings it was recorded at.

The MP3 decoder is JUCE's own, built in, so there is nothing extra to install and no paid or system codec is used.

## Install

It comes in **Songsmith Plugins**, the one installer for each system, as its own tick box (selected by default):

- **Windows:** installs `Songsmith Audio Analyzer.exe` in `Program Files\Songsmith Studio\Songsmith Audio Analyzer` and adds a Start menu shortcut. Untick it on a later run to remove it. The exe, the shortcut and the taskbar button all show the Songsmith logo. If Windows still shows a plain icon after an update, it is showing a saved picture; sign out and in again, or restart, to refresh it.
- **Mac:** installs `Songsmith Audio Analyzer.app` in Applications, with the Songsmith logo as its icon. Finder can open WAV, AIFF, FLAC and MP3 files with it.

Like the plugins, the installers are not code-signed, so Windows SmartScreen or macOS Gatekeeper may warn the first time. See the [installation guide](../TECHNICAL.md).

## The icon

The icon is the Songsmith logo mark, cropped square from the official logo on the website (`Resources/songsmith-icon.png`, 1024 by 1024), not redrawn. The build embeds it in the exe at several sizes and makes the Mac icon from it. The installer tests check that the installed exe carries it and that the Start menu shortcut takes it from the app.

<img src="Resources/songsmith-icon.png" alt="The Songsmith logo mark: a gold and violet S with a gold curve and three dots across it" width="128">

## Build and test

```powershell
cmake -S audio-analyzer -B audio-analyzer/build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE
cmake --build audio-analyzer/build --config Release
.\audio-analyzer\build\AudioAnalyzerTests_artefacts\Release\AudioAnalyzerTests.exe
```

`AudioAnalyzerTests --screenshots <folder> [--scale 2]` renders the three screens (waiting, results, an error).

The tests write real reference files to a temporary folder (WAV 16-, 24- and 32-bit float, AIFF, FLAC, mono and stereo, and MP3 files built by hand from silent frames, plus the four small real MP3 reference files in `Tests/fixtures`) and read them back through the same code the app uses:

- Known loudness: a 997 Hz sine reads the same number of LUFS as its level in dBFS (EBU Tech 3341), mono reads 3 dB lower, and a gated signal reads as the standard says.
- True peak: a wave that rises between samples reads about 3 dB above its highest stored sample, and a sample peak is never shown as a true peak.
- Missing measurements (silence, too short, too quiet), every error (missing file, wrong kind of file, folder, damaged file, empty file), repeated and cancelled analyses, a three-minute file read in the background with the window responsive, dropping several files.
- MP3 (ANA-02): valid MP3 files of silent frames at 32, 44.1 and 48 kHz, mono and stereo, at 64 to 320 kilobits per second, with a changing bitrate and with no header, twelve minutes long, cut short, empty and not an MP3 at all. They check the format, sample rate, channels, exact length, average bitrate, "Not applicable" for bit depth, silence reported as silence, and the words used.
- The charts (ANA-03): each bar of the True Peak chart is its own channel's measured peak, at the height the scale says (for example -6 dBTP and -12 dBTP on the left and right, 0 dBTP at its mark, a level above 0 drawn above the line, levels far above or below sitting on the edges), mono and six channel files, silence, the highest stored sample kept apart from the true peak; and the waveform of a file that is silent for five seconds then loud (the first half empty, the second at its true level), a ten minute file holding the same number of slices, a tiny file, and what each chart says it is. Also that the charts show only with results, do not overlap anything, and that the True Peak chart is the tallest.
- Plain words and text that fits at the smallest, default and largest window, in every screen; fonts from the embedded Inter typeface; keyboard use and screen reader names.
- The shared REC-01 analysis and advice tests (`shared/Tests/SongsmithFileAdvisorTests.h`) run here too.

## What has not been checked

- The charts are checked against measured values and drawn positions, and by looking at real screenshots (above, rendered by the test executable), but not on a real Mac, on a high-density display by eye, or with a screen reader.
- The waveform shows the largest sample in each slice (a peak envelope), not loudness over time. A loudness history (momentary or short-term loudness through the song) is not shown.

- The app has not been tried on a real Mac by a person, and the macOS package is built and tested only on the hosted macOS runner. Opening a file from Finder (or by dropping it on the app's icon) is set up in the app's property list and checked there, but has not been tried by hand.
- **MP3 loudness and true peak are checked against four real MP3 files of a known signal** (`Tests/fixtures`, a 997 Hz sine at -20 dBFS encoded with LAME at 128, 192 and 320 kilobits per second and as variable bitrate). Each reads what ffmpeg's EBU R128 meter reads for the decoded file, within 0.15 dB (the lower bitrates decode a little under their source: -20.5 LUFS at 128). That is a pure tone, not music, so it checks the decoder and the measurement, not how every kind of music encodes. The app does not remove the encoder's start and end silence.
- Reference files are generated; no commercially mastered files were used. True peak is checked on constructed signals with a known answer, not against another vendor's meter.
- Screen reader use is covered by the accessibility names and roles the tests check, not by a session with a screen reader.
