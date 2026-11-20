---
id: ANA-03
priority: medium
effort: high
dependencies: [ANA-02]
conflicts: []
---

**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/336

# ANA-03: Visualise True Peak, integrated loudness and whole-song dynamics

Turn the Audio Analyzer's results from mostly text into a small, accurate dashboard. The chosen priority is True Peak as the main graphic.

## What was done

- **True Peak chart (principal):** a tall bar per channel (Left and Right, or Mono; one bar, "All", for more than two channels) for the highest true peak in the whole file, on a -48 to +6 dBTP scale with 0 dBTP marked and the suite's meter colours, red above 0. The number under each bar is exact. The highest stored sample is a separate tick beside the bar, never merged with the true peak. Labelled "Highest peak in the file" and "not a live meter".
- **Integrated Loudness:** the large number, in the same plain tile as before. No second giant meter, no pass or fail, no required value.
- **Waveform overview:** the largest sample in each of 800 slices of the file, measured in the same pass on the worker thread. The same memory for any length of file. Labelled as size, not loudness and not true peak.
- **Facts:** sample rate, bit depth and encoding, duration and channels, compact, below the charts.
- Per-channel sample and true peaks and the waveform slices were added to the shared analysis result (`FileAnalysis::Result`). The window is a little taller (design size 760 by 800) and still scales as a whole from 75 % to 200 %.
- Tooltips and screen reader descriptions for every chart, with the real numbers.

## Tests

Measured value to drawn position for the scale, both channels, mono, six channels, over 0 dBTP, silence; the waveform mapped to the file (silent half, loud half), capped for a long file, tiny files; layout (inside the window, no overlap, True Peak chart the tallest), text fit and plain words at 75 %, 100 % and 200 %; and real screenshots (waiting, results, a loud file over 0 dBTP, an error).

## Limits to keep honest

- The waveform is a peak envelope. A loudness history through the song is not shown.
- Not checked on a real Mac, on a high-density display by eye, or with a screen reader.
