# Songsmith Compressor: the level graph, the floating panel and the meters

COMP-02. This page says what the window shows, where every number comes from, and which test checks it. It is original Songsmith design; the level-against-time idea is common to many compressors and nothing here copies another product's artwork.

## The level graph

A scrolling graph of the last 8 seconds, newest at the right.

- **Grey line: the sound going in.** The loudest sample at the input in each 10 ms, either channel (the same link as the compressor's detector).
- **Red line: the sound coming out.** The loudest sample at the output, after compression, Makeup Gain, Analog Warmth, Mix and Bypass.
- **Where red falls below grey, the compressor is turning the level down.** The gap is lightly shaded. Makeup Gain lifts the red line, so with Makeup Gain the red line can sit above the grey one; the graph does not hide that. The reduction alone is the Gain Reduction bar.
- **Dashed teal line:** the Threshold, as set.
- The numbers are sample peaks, not true peaks. The tooltip says so.

**Where the data comes from.** Nothing is drawn from the settings. In `processBlock` the audio thread measures, for every 10 ms of audio, the input peak, the output peak and the deepest gain reduction, and writes one small column into a fixed-size lock-free queue (`ColumnFifo`, one writer and one reader, no allocation, no locks). Every block is split at 10 ms boundaries, so the columns cover exactly the same time at any block size (tested at 44.1, 48 and 96 kHz and block sizes of 1 to 1024 samples: identical columns). The editor reads the queue 30 times a second. If the editor is closed, or slow, the newest columns are dropped; the audio never waits. The editor clears the queue when it opens, so it starts from now.

**Honest behaviour.**

| Case | What the graph does | Test |
|---|---|---|
| Output against input | Output equals input minus gain reduction, plus Makeup Gain only when set | `The columns are the measured signal` |
| Slower attack, longer release | The reduction arrives later, and recovers over the release time | `Attack and release show in the curves` |
| Silence | Reads very quiet, not a made-up level | same |
| Stereo | The louder channel sets the input level and drives the reduction | `Stereo is the linked maximum` |
| Mono | Works, and agrees with stereo | same |
| Bypass | Marked "Bypassed", the reduction is 0 and the red line lies on the grey one. Never a made-up reduction | same |
| Host stopped | No audio is processed, so no columns arrive and the picture holds | by design |
| Pause button | Holds the picture; sound and meters carry on | `The graph keeps the last 8 seconds` |

With Mix below 100% the graph still shows the compressor's own reduction and the real output level. The reduction is then partly blended away in the sound, and the Mix tooltip says it blends the compressed sound with the original.

## The floating panel

All the controls (Threshold, Ratio, Attack, Release, Knee, Makeup Gain, Auto Makeup, Mix, Analog Warmth) are in one panel on the graph. It has the same look as the Songsmith EQ's band panel (rounded, softly lit, a grip of dots at the top edge). Drag it by its background; it always stays whole inside the graph. Its place is saved with the session as two fractions of its free travel, so it lands in the same relative place at any window size. Older sessions open with it at the default place (bottom left). Every parameter keeps its COMP-01 range, automation and tooltip.

The drag, keep-inside and saved-position behaviour is a shared component, `shared/Source/SongsmithFloatingPanel.h`. The Songsmith EQ's band panel still has its own copy of that logic; moving it onto the shared component is a later clean-up, not done here.

## The meters

On the right: the **Gain Reduction** bar (0 to 24 dB, shown as a positive amount, so there is no confusing minus sign) with its number, and the **output meter**.

The output meter is **the Songsmith EQ's own meter**, moved into the shared library (`shared/Source/SongsmithOutputMeter.h`) and used by both plugins: one bar for mono, Left and Right bars for stereo, the CLIP warning, the signed held peak directly under it, red only at or above 0 dBFS, with the same boundary and the same latch as the EQ (EQ-39, EQ-42). The audio-thread side (a peak per channel and the latched warning that clears after sustained silence) is shared too. The Songsmith EQ's tests pass on it unchanged. The compressor gives it its own tooltip.

There is no separate input meter: the graph shows the input.

## Real-time safety and cost

The queue is a fixed array of plain data. The audio thread does no drawing and no allocation for the display. The graph redraws only when new columns arrive or the Threshold moves. Resizing the window does not touch the sound.

## Not verified

Not checked in a DAW, not timed for frame rate or CPU (the display adds a few comparisons per sample and one small write per 10 ms), and not checked by eye at Windows 125% and 150% scaling or on macOS.
