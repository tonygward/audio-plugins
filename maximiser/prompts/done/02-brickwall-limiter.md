# Songsmith Maximiser — 02 brickwall limiter

## Working rules

Work in [tonygward/audio-plugins](https://github.com/tonygward/audio-plugins). Follow applicable repo instructions and the [EQ roadmap workflow](https://github.com/tonygward/audio-plugins/blob/main/eq/prompts/roadmap/README.md). Use JUCE_DIR and CMake. Start from main after the preceding step merges. Keep the audio callback free of allocations, locks and file I/O. Preserve other plugins and stable automation IDs. Run relevant builds and tests, report exact commands and blocked checks, open a draft PR if available, and stop after this step. Skip system-wide installs during development.

## L1 reference specs

- [Original Waves L1 Plug-In Manual](https://www.bhphotovideo.com/lit_files/90350.pdf) — classic Threshold, Out Ceiling, Release, Attenuation and IDR workflow.
- [Waves L1 UltraMaximizer User Guide](https://assets.wavescdn.com/pdf/plugins/l1-ultramaximizer.pdf) — later reference for True Peak and Automatic Release Control.
- The documented limiter uses brickwall look-ahead. Below Threshold, the constant gain in dB is Out Ceiling minus Threshold. Above Threshold, limiting adds attenuation. The manual release range is 0.01–1000 ms. The legacy manual suggests 1 ms for many sources and 3–7 ms as a mastering starting point.
- Treat the published behaviour as the target. Identify the chosen look-ahead duration, gain smoothing, adaptive release and dither implementation as Songsmith design choices. State which tests verify each behaviour.

## Task and acceptance checks

Implement an independently testable look-ahead sample-peak limiter. Let T and C be the threshold and ceiling in dBFS. Apply baseline gain C−T and additional nonpositive gain reduction from the detector. Use a shared stereo gain envelope driven by the higher absolute channel peak. Design the look-ahead and gain scheduler so the envelope respects the ceiling at the exact delayed sample it protects. Document chosen look-ahead and release law as implementation choices. Report latency and align bypass. Test impulses at every block offset, sustained tones, threshold/ceiling interaction, stereo image, variable block sizes, automation, reset and latency at 44.1/48/96 kHz. Verify T=−6 and C=−1 adds 5 dB below threshold and limits above it to −1 dBFS.
