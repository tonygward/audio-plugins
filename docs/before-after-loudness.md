# Before and After loudness (songsmithstudio#7)

Every Songsmith processing plugin shows two readings in LUFS at the bottom of its window, right of the Processing Load switch: **Before** (the sound entering this plugin) and **After** (the sound leaving it, after every stage including make-up gain, output gain and Bypass). It is for comparing what the plugin does to the loudness, not for mastering to a target. It is not the standalone Plugin Analyzer (which compares two plugins through two separate instances).

Code: the meter and the readout are `shared/Source/SongsmithBeforeAfter.h`; the strip that hosts the readout is `Songsmith::Diagnostics::Bar` (`shared/Source/SongsmithDiagnostics.h`); the tests are `shared/Tests/SongsmithBeforeAfterTests.h`, built into every plugin's test executable.

## What is measured

- **Short-term loudness:** the K-weighted signal (the filter the Equaliser's Auto Gain and the Maximiser's loudness meter already use, `SongsmithKWeighting.h`), the mean power of the **last 3 seconds**, summed over the channels, in LUFS: `-0.691 + 10 log10 (power)`. This is the short-term loudness of ITU-R BS.1770 / EBU R 128, without gating. Before and After use exactly the same code, so the comparison is meaningful.
- **Reference check:** a 1 kHz sine at −23 dBFS reads −23.0 LUFS in stereo and −26.0 LUFS in mono, as the EBU test signals say; tested at 44.1, 48, 88.2, 96 and 192 kHz.
- **Mono:** a mono instance counts its one channel (so a mono signal reads 3 dB lower than the same signal on two channels).
- **Only this plugin:** the numbers describe the audio entering and leaving this plugin instance, not the track or the master.

## What the readout shows

| Reading | Meaning |
|---|---|
| **Measuring…** | fewer than 3 seconds have been heard since playback started, so the window is not full |
| **—** (an em dash) | no sound (below −70 LUFS) |
| **−18.0 LUFS** | the short-term loudness, to 0.1 LUFS |

- **Transport:** when the host stops, the last readings stay. When it plays again they start afresh. A host that reports no transport (a standalone app) counts as always playing.
- **Bypass:** nothing special: the output is the input, so Before and After agree (tested).
- **Latency:** After lags Before by the plugin's delay (a few milliseconds), which is nothing against a 3-second window.

## Real-time safety and cost

The audio thread uses fixed memory (two meters, each a fixed 30-step window and two filters per channel), no allocation, no lock, no logging and no file access, and publishes each reading in one lock-free atomic. The window is read by the footer's timer about four times a second. On the test machine the pair costs about 0.06 % of one core (12.7 ms for 20 seconds of stereo audio), and a test fails if it ever exceeds 5 % of real time.

## How a plugin uses it

One member and two lines:

```cpp
Songsmith::Loudness::BeforeAfterMeter beforeAfter;                       // in the processor
beforeAfter.prepare (sampleRate);                                        // in prepareToPlay
const Songsmith::Loudness::BeforeAfterMeter::Scope loudnessScope (       // top of processBlock
    beforeAfter, buffer, getTotalNumInputChannels(), getPlayHead());     //   Before on entry, After on every way out (early returns too)
diagnostics->setBeforeAfter (&proc.beforeAfter);                         // in the editor, after the footer strip is made
```

## Which plugins

Songsmith EQ, Analog Compressor, Compressor, Optical Leveller, Maximiser, Passive EQ, Bus Compressor and Analog Desk (the channel strip: Before is the input to the whole strip, After its final output, not per stage). The issue named seven of these; the Bus Compressor is included because it is the same kind of plugin and costs nothing extra. **Not included:** the Plugin Analyzer (a measuring tool that never changes the sound) and the Audio Analyzer (a standalone app with its own loudness display).

## Design choice: the footer

The readout is in the shared footer strip rather than in each plugin's controls, so it looks and behaves identically everywhere and needs no per-plugin layout. The trade-off is that it is small and out of the way, not next to the faders; the Maximiser's separate **Integrated LUFS** column is unchanged.
