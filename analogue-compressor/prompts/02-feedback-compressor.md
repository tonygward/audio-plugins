# Songsmith Analog Compressor — 2. feedback compressor

## Working rules

Work in [tonygward/audio-plugins](https://github.com/tonygward/audio-plugins). Read applicable repo instructions and the [Songsmith EQ prompt roadmap](https://github.com/tonygward/audio-plugins/blob/main/eq/prompts/roadmap/README.md). Branch from main after the preceding step merges. Use JUCE/CMake and JUCE_DIR. Keep plugin-specific code in `fet/`. Preserve Songsmith EQ and stable compressor plugin and parameter IDs. Keep the audio callback free of allocations, locks, logging and file I/O. Run available builds and tests, report exact commands and outstanding checks, and finish with a draft PR if available. Skip system-wide installation during development. Stop after this step.

## 1176LN references

- [Universal Audio 1176LN manual](https://media.uaudio.com/assetlibrary/1/1/1176ln_manual.pdf) — controls, timing, ratios and operating modes.
- [1176LN technical manual](https://medias.audiofanzine.com/files/urei-1176ln-manual-473204.pdf) — FET gain control, feedback detector and Class A output stage.
- [1176 revision guide](https://vintageking.com/pages/urei-universal-audio-1176ln-compressor-limiter) — identify the Rev D blackface reference.
- [UREI schematic and DIY archive](https://www.masonaudio.org/diy/comp1176) — check the revision before using any drawing or parts list. The archive includes Rev H material.

Use Rev D as the design reference. Record the manual section or drawing revision for each material design claim and distinguish published specs from modelling choices.

## Task and acceptance checks

Implement a separately testable Rev D-inspired feedback engine. Detect after gain reduction and before Output trim. Input drives compression. Document loop evaluation, stereo linking, knob mapping and ratio-dependent knee and threshold assumptions. Use 4:1, 8:1, 12:1 and 20:1, with clockwise faster timing. Target nominal 20–800 µs attack and 50 ms–1.1 s release using an explicitly defined step-response measurement at multiple input levels. Smooth audible changes and expose GR metering. Verify transfer curves, timing at 44.1/48/96 kHz, stereo tracking, output trim independence, silence, hot input and variable blocks. Report any sub-sample timing limitations and provide a level-matched vocal and drum listening checklist.
