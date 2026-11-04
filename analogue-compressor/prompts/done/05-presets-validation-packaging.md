# Songsmith Analog Compressor — 5. presets validation packaging

## Working rules

Work in [tonygward/audio-plugins](https://github.com/tonygward/audio-plugins). Read applicable repo instructions and the [Songsmith EQ prompt roadmap](https://github.com/tonygward/audio-plugins/blob/main/eq/prompts/roadmap/README.md). Branch from main after the preceding step merges. Use JUCE/CMake and JUCE_DIR. Keep plugin-specific code in `fet/`. Preserve Songsmith EQ and stable compressor plugin and parameter IDs. Keep the audio callback free of allocations, locks, logging and file I/O. Run available builds and tests, report exact commands and outstanding checks, and finish with a draft PR if available. Skip system-wide installation during development. Stop after this step.

## 1176LN references

- [Universal Audio 1176LN manual](https://media.uaudio.com/assetlibrary/1/1/1176ln_manual.pdf) — controls, timing, ratios and operating modes.
- [1176LN technical manual](https://medias.audiofanzine.com/files/urei-1176ln-manual-473204.pdf) — FET gain control, feedback detector and Class A output stage.
- [1176 revision guide](https://vintageking.com/pages/urei-universal-audio-1176ln-compressor-limiter) — identify the Rev D blackface reference.
- [UREI schematic and DIY archive](https://www.masonaudio.org/diy/comp1176) — check the revision before using any drawing or parts list. The archive includes Rev H material.

Use Rev D as the design reference. Record the manual section or drawing revision for each material design claim and distinguish published specs from modelling choices.

## Task and acceptance checks

Build conservative lead vocal, acoustic guitar, bass and drum room starting points with listening cues. Use the UA manual to explain Input-driven compression, timing direction and independent Output matching. Reuse the EQ workflow and installer patterns. Build and test both plugins in CI, produce separately named Windows VST3/Standalone and universal macOS VST3/AU/Standalone artifacts, and run auval with the new AU identifiers. Test isolated install, upgrade and uninstall while preserving EQ and user presets. Provide Studio One and Logic checks for scan, mono/stereo insertion, automation, recall, bypass, offline render and multiple instances. Separate executed checks from pending checks. Leave publishing for review.
