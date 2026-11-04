# Songsmith Maximiser — 03 true peak and auto release

## Working rules

Work in [tonygward/audio-plugins](https://github.com/tonygward/audio-plugins). Follow applicable repo instructions and the [EQ roadmap workflow](https://github.com/tonygward/audio-plugins/blob/main/eq/prompts/roadmap/README.md). Use JUCE_DIR and CMake. Start from main after the preceding step merges. Keep the audio callback free of allocations, locks and file I/O. Preserve other plugins and stable automation IDs. Run relevant builds and tests, report exact commands and blocked checks, open a draft PR if available, and stop after this step. Skip system-wide installs during development.

## L1 reference specs

- [Original Waves L1 Plug-In Manual](https://www.bhphotovideo.com/lit_files/90350.pdf) — classic Threshold, Out Ceiling, Release, Attenuation and IDR workflow.
- [Waves L1 UltraMaximizer User Guide](https://assets.wavescdn.com/pdf/plugins/l1-ultramaximizer.pdf) — later reference for True Peak and Automatic Release Control.
- The documented limiter uses brickwall look-ahead. Below Threshold, the constant gain in dB is Out Ceiling minus Threshold. Above Threshold, limiting adds attenuation. The manual release range is 0.01–1000 ms. The legacy manual suggests 1 ms for many sources and 3–7 ms as a mastering starting point.
- Treat the published behaviour as the target. Identify the chosen look-ahead duration, gain smoothing, adaptive release and dither implementation as Songsmith design choices. State which tests verify each behaviour.

## Task and acceptance checks

Preserve manual sample-peak mode. Implement an optional true-peak mode with an oversampled detector and an independent reconstruction meter for verification. Account for filtering, peak alignment and any extra latency. Test phase-shifted high-frequency tones, intersample peaks and transient bursts. State measurement tolerances and report overshoot. Add optional content-dependent release using transient and sustained-energy estimates. Treat Waves ARC as a behavioural reference and document your own recovery law. Compare pumping, distortion, recovery and CPU against the manual mode.
