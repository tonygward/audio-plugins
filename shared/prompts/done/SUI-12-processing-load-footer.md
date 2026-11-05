# SUI-12 — Left-aligned Processing Load diagnostic toggle

**Status:** Done in #81  
**Model:** Sonnet  
**Scope:** Every current and future Songsmith plugin (EQ, Analog Compressor, Maximiser, and plugins using the shared diagnostics bar).  
**Reference:** User screenshot 5 November 2026 (EQ bottom status strip).  
**Dependency:** SUI-11 shipped diagnostic instrumentation. This is a presentation/layout follow-up, not a new DSP feature.

## Goal
Replace the current left-hand Latency value + far-right Processing Load switch/readout with a **single far-left Processing Load toggle** and **one diagnostic text readout immediately to its right**.

## Required behaviour
1. Place the bevelled **Processing Load** toggle at the **far left** of the bottom status strip in **all plugins**, with consistent padding, baseline, sizing and spacing.
2. Clicking/tapping it toggles diagnostic monitoring on/off. When **on**, show immediately to the right: `CPU 0%, Latency 0.0ms (0 samples)` (illustrative values only). Render live, truthful values using the existing processor load estimate and host-reported latency; do not hardcode zeros.
3. When **off**, hide the right-hand diagnostic text completely (not an inert label or stale number). Preserve the existing off-by-default behaviour and per-machine setting; don't put it in presets/session state.
4. The order and wording are **CPU {load}%, Latency {milliseconds}ms ({samples} samples)**. Use **one decimal** for milliseconds, e.g. `0.0ms`; format CPU intelligently so small non-zero loads aren't rounded misleadingly to zero (e.g. `0.35%`), while an actual zero may read `0%`. Handle a single sample grammatically if appropriate.
5. When processing is stopped or there is no valid measurement, show `CPU Unavailable` rather than a fabricated zero; still display accurate latency. Keep units and value updates robust at all supported sample rates and buffer sizes.
6. Remove the old always-visible left-hand Latency label/value and the old right-aligned Processing Load metric. Do not duplicate the diagnostics elsewhere.
7. Keep the shared UI look (Inter type roles, bevel, accessible tooltip and keyboard focus), crisp at Retina and scaled sizes, and adapt for each editor minimum width without clipping. The right end of the strip remains available for **plugin-specific controls**, especially EQ's Piano button (EQ-35).
8. In the shared wording policy, **allow the specific acronym `CPU` for this diagnostics readout**, despite the existing general acronym ban. Prefer a narrow exception to globally relaxing wording validation.
9. Preserve SUI-11's real-time safety, load-measurement lifecycle, CPU overhead limits, atomics and host latency reporting. No changes to audio processing algorithms or parameter IDs.

## Acceptance tests
- Toggle off: left button visible, **no** CPU/Latency text.
- Toggle on: values immediately follow button; load updates while processing; latency matches the host's samples converted using the current sample rate.
- Stop processing: no misleading `CPU 0%`; handle unavailable data as above.
- Check EQ, Analog Compressor and Maximiser, different buffer/sample-rate settings, resizes and DPI scaling; check new plugin reuse.
- Existing diagnostics tests still pass, with added unit/UI/layout/wording tests.
- Include before/after **real screenshots** of all three plugin footers in the PR and update representative README/style-guide screenshots.

## Delivery rules
Branch from latest `main`; never commit directly to `main`. Build/test all three plugins; no system-wide installation. Open a PR with test evidence, known DAW validation gaps, and screenshots. Move this prompt to `shared/prompts/done/` and update prompt table/change log **only when merged**. Do not independently bump `VERSION`; align with agreed suite release checkpoint.
