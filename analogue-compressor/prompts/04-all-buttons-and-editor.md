# Songsmith Analog Compressor — 4. all buttons and editor

## Working rules

Work in [tonygward/audio-plugins](https://github.com/tonygward/audio-plugins). Read applicable repo instructions and the [Songsmith EQ prompt roadmap](https://github.com/tonygward/audio-plugins/blob/main/eq/prompts/roadmap/README.md). Branch from main after the preceding step merges. Use JUCE/CMake and JUCE_DIR. Keep plugin-specific code in `fet/`. Preserve Songsmith EQ and stable compressor plugin and parameter IDs. Keep the audio callback free of allocations, locks, logging and file I/O. Run available builds and tests, report exact commands and outstanding checks, and finish with a draft PR if available. Skip system-wide installation during development. Stop after this step.

## 1176LN references

- [Universal Audio 1176LN manual](https://media.uaudio.com/assetlibrary/1/1/1176ln_manual.pdf) — controls, timing, ratios and operating modes.
- [1176LN technical manual](https://medias.audiofanzine.com/files/urei-1176ln-manual-473204.pdf) — FET gain control, feedback detector and Class A output stage.
- [1176 revision guide](https://vintageking.com/pages/urei-universal-audio-1176ln-compressor-limiter) — identify the Rev D blackface reference.
- [UREI schematic and DIY archive](https://www.masonaudio.org/diy/comp1176) — check the revision before using any drawing or parts list. The archive includes Rev H material.

Use Rev D as the design reference. Record the manual section or drawing revision for each material design claim and distinguish published specs from modelling choices.

## Task and acceptance checks

Use the UA manual to derive All Buttons behaviour. Implement a dedicated curve and envelope model, recording what is sourced and what is tuned. Test stepped tones, drum transients and click-free switching. Build an original vector editor informed by the hardware control layout and Songsmith EQ’s embedded fonts and scalable drawing. Include the four ratios, All Buttons, gain reduction off, host bypass and a GR/output needle meter with independent display ballistics. Add readable values, double-click defaults and automation gestures. Verify 100%, 150% and 200% layouts, meter calibration and session recall.
