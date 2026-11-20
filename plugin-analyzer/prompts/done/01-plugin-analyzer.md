---
id: ANALYZER-01
priority: medium
effort: high
dependencies: []
conflicts: []
---

**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/331

# ANALYZER-01: In-DAW Plugin Analyzer for frequency-response measurement

## Objective and user value

Build **Songsmith Plugin Analyzer**, a VST3 utility plugin that measures the **frequency-response change another plugin produces inside a recording program**. The first use is measuring a hardware-inspired Passive EQ preset and comparing its real response with Songsmith Passive EQ, so tuning and regression tests do not need a paid measurement tool. It is separate from the standalone Audio Analyzer (REC-02, #313), which checks finished files.

## Current and desired behaviour

- Current: no in-host measurement of a plugin's frequency response.
- Desired: a Reference instance before the target plugin and a Result instance after it, paired by a session number. The Reference plays a known test program; the Result records, lines up and compares, then draws a logarithmic 20 Hz to 20 kHz curve in decibels against a 0 dB line.

## Scope

- One VST3 project, `plugin-analyzer/`, linking the shared UI library (fonts, look and feel, Output meter column, diagnostics strip, Bypass in the top right). No Audio Unit and no standalone app: it needs two copies inside a host.
- Reference and Result roles, eight sessions, channel choice (Left, Right, Both Combined), a Test Level, Start, Stop and Reset.
- A deterministic test program of two logarithmic sweeps, aligned by cross-correlation (so plugin and host delay is found, not assumed), compared as a transfer function with a bypass-identity calibration, smoothed to a sixth of an octave. No divide-by-zero, NaN or invented flat parts.
- Four named slots, Show and Compare With, a graph range choice, and Export CSV (frequency in hertz, decibels per measurement, notes and sample rate).
- Flags that never let it silently misstate: nothing arrived, a very quiet arrival, clipping, processing that changes between sweeps, sound that cannot be lined up, too little of the range measured. Pairing mistakes are named in plain words.
- Out of scope: hosting other plugins itself, harmonic distortion, phase, and accurate curves for level-dependent processing (these are shown at the test level and said so).

## Implementation guidance

Nothing in `processBlock` allocates, locks, logs or touches files: sessions are atomics, the capture is a pre-sized buffer, and analysis runs on a background thread. Parameter IDs, the plugin code and the package identifiers never change. Wording follows the plain-words rules (no abbreviations other than allowed units and `CSV`). State saves the settings, slot names and curves, and restores older or newer state by parameter ID.

## Acceptance criteria and tests

- Known filters (flat gain, high-pass, bell boost and cut, low shelf) read within 0.5 dB from 30 Hz to 18 kHz, at several block sizes, sample rates and delays, and with noise in the path.
- A Reference and a Result in the same session measure end to end; a different session never sees the sweep; two pairs at once do not mix.
- Silence, quiet arrival, clipping, a time-varying path and unrelated sound are flagged, and show no misleading curve.
- Every control has a tooltip, an accessible name and keyboard access; the editor passes the plain-words check at three window sizes.
- The plugin is in the CI build, both suite installers (one tick box), the version checks, the release checks and the Matrix scope. `test/suite-meters.test.mjs` passes (Output meter, no Gain Reduction for a measuring tool).

## Dependencies and risks

Sample alignment, host timing, nonlinear plugins, and pairing between instances. Hosts that load each plugin in its own process cannot pair instances, and the Result says so. **Not yet verified in a DAW.**

## Build and release expectations

New plugin; users will notice it, so a plain-language note goes under `## Unreleased` in `CHANGELOG.md`. Windows and macOS CI build, test and package it; the Windows installer test checks it installs as a VST3 only.
