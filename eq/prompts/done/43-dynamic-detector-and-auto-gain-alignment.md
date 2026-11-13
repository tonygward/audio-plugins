---
id: EQ-43
priority: medium
effort: low
dependencies: []
conflicts: []
---

# EQ-43: Steadier dynamic detector and latency-aligned Auto Gain

Issue: [#277](https://github.com/tonygward/audio-plugins/issues/277). Implemented by hand from the DSP review (no Matrix run).

## Objective
Make two Songsmith EQ measurements steadier and more accurate: the level detector of dynamic bands at low frequencies, and Auto Gain in linear-phase and oversampled modes. Share one K-weighting filter between Songsmith EQ and Songsmith Maximiser.

## Scope
- **Dynamic detector:** smooth the mean square with two cascaded one-poles (one period of the band frequency each, 0.5–20 ms) instead of one (two periods, 1–20 ms). The ripple at twice the band frequency falls from about ±0.35 dB to ±0.03 dB at 50 Hz, with the same response time from 50 Hz to 1 kHz.
- **Auto Gain:** when the path has latency (linear phase up to N/2 samples, or oversampling), measure the delayed input, so input and output loudness cover the same audio.
- **Shared K-weighting:** move the BS.1770-4 coefficients to `shared/Source/SongsmithKWeighting.h`; both plugins used identical formulas, so no output changes.

## Acceptance criteria
- A 50, 100 or 400 Hz dynamic bell with a 1 ms attack, 6 dB over its threshold, varies by under 0.15 dB peak to peak and still sits at half its range.
- Flat EQ, linear phase at the longest delay, noise with level jumps every 250 ms: the Auto Gain correction stays within 0.15 dB of 0.
- All existing EQ and Maximiser tests pass; Maximiser loudness readings are unchanged.

## Tests
New "EQ-43" tests in Dynamic EQ and Linear phase. Both fail on the old code (0.22 dB ripple at 50 Hz; 0.31 dB Auto Gain error).

## Build and release

Songsmith EQ (and Songsmith Maximiser, which only switches to the shared K-weighting header). Users may notice steadier low dynamic bands, so the changelog has a note.
