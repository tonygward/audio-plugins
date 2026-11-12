# ACOMP-15 — Move Oversampling to far left of processing controls

**Issue ID:** ACOMP-15. **Product:** Songsmith Analog Compressor. **Status:** backlog, not claimed. **Default agent:** Sonnet.

## User story
As a musician, I want the existing Oversampling control to sit at the **far-left of the Analog Compressor processing/control area below the shared title bar**, so technical processing options have a predictable, unobtrusive location instead of being tucked beneath Ignore Below.

## Existing / desired
SUI-18 moved Oversampling out of the title bar and located it **below Ignore Below**, as documented in `shared/docs/title-bar.md`. Move the **same existing working control**, not a duplicate, to the far-left edge of the first suitable processing/control row **under** the universal title bar. The title bar must remain exactly **Songsmith Analog Compressor | Preset | Undo | Redo | Bypass**. Preserve sound-related controls and natural reading order.

A separate story governs reducing persistent explanatory text and using tooltips. This story is **placement only**: retain the current visible label/value text and existing tooltip behaviour until that independent story runs; do not introduce or duplicate tooltip/text work here.

## Implementation
- Inspect `analog-compressor/Source/PluginEditor.cpp/.h`, `FETParams.h` and `shared/docs/title-bar.md`; find the real Oversampling component and its layout constraint.
- Choose far-left position within the main processing row below shared header, visually aligned with existing controls, without colliding with Input/Output, ratio buttons, Ignore Below, meter, controls, hints or footer. Support minimum editor width and 100–200% scaling/HiDPI.
- Preserve current options, order/default, parameter ID/automation/recall, latency reporting, bypass behaviour and DSP.
- No second Oversampling UI remains in its previous below-Ignore-Below location.
- Coordinate with existing SUI-18 title bar work; do not put Oversampling back in title bar.

## Acceptance / tests
1. Oversampling physically occupies the far-left available position in the processing control area immediately below the title bar.
2. Shared header still has only name, Preset, Undo, Redo, Bypass; no duplicate Oversampling.
3. Unchanged Oversampling choices correctly drive DSP, saved sessions, presets, host automation, CPU/latency reporting; no audio regression.
4. Automated layout tests and screenshot evidence at minimum/default/max window dimensions and high-DPI scaling; native Windows/macOS CI.
5. Follow `docs/songsmith-execution-policy.md`: board-driven pickup, start/progress/failure comments, exact-head checks and trusted merge, appropriate beta build increment, suite installers/R2/live beta download verification, final evidence comment before issue close and board Done.

**Risk:** avoid unintended changes to the header and unrelated tooltip work. No Matrix execution follows from issue creation alone.
