# ACOMP-16 — Continuous Ignore Below cutoff, up to 500 Hz

**Issue ID:** ACOMP-16. **Product:** Songsmith Analog Compressor. **Status:** backlog only; agent Sonnet (Opus only if difficult parameter migration).
**Predecessors:** ACOMP-07 detector filter, ACOMP-12 rotary UI (already implemented). Do **not** reopen or reimplement completed work.

## User story
As a musician, I want to dial **any cutoff frequency up to 500 Hz** using the existing **Ignore Below** rotary control, rather than jump between the currently fixed 60, 80, 100, 150 and 200 Hz options. I want precise, natural adjustment, especially for bass and kick-heavy audio.

## Existing and desired
The shipped ACOMP-12 rotated the earlier six-choice parameter `detectorLowCut` (Off, 60, 80, 100, 150, 200 Hz) but retained fixed discrete values to avoid breaking old parameter states and host automation.

Replace the **stepped frequency interaction** with a **continuous cutoff** of **20–500 Hz**, plus a clear **Off** position (proposed 20 Hz lower bound, verify with existing filter stability and justify if changed). Any whole-Hz value, such as 73, 127, 245 or 499 Hz, must be selectable precisely via value entry and automatable; smooth knob dragging should offer finer-than-old-step resolution. Label remains exactly **Ignore Below**, with live `Hz` value, keyboard entry/arrow control and double-click default to Off. Prefer logarithmic tuning to improve low-frequency precision.

## Parameter compatibility architecture (critical)
- Review `analog-compressor/Source/FETParams.h`, `FETEngine.h`, processor state/attachments and ACOMP-12 tests. Existing JUCE choice parameter `detectorLowCut` has six normalised values, and changing its type/range/meaning in place may silently corrupt older sessions and host automation.
- **Preserve the legacy parameter unchanged for saved-state and host compatibility**, add a new uniquely identified continuous parameter and explicit versioned migration/precedence rules as needed. Do not reuse old choice normalised values as a frequency nor create two competing active controls. Test old project recall for each six-value state and any stored automation.
- Automation of the old parameter should retain meaningful legacy behaviour, or provide an explicit documented compatibility pathway with clear limitations validated in real hosts. New continuous parameter supports stable fine adjustment.
- Any new state property/preset field must coexist with previously saved factory/custom presets; do not alter unrelated ratio, FET modelling, input/output or bypass.

## DSP and UX
- Change only the detector's high-pass cutoff coefficient smoothly, never EQ/cut the audible signal itself. Design smoothing without zipper noise or unstable filter updates; account for oversampling/sample-rate differences.
- Off bypasses the detector filter. Existing six cutoff values map exactly. Numeric text should not round input in a way that makes requested Hz unrecoverable. Control focus/accessible descriptions should expose true numerical frequency.
- Preserve 1176 compression character and meter/latency semantics. Coordinate layout with ACOMP-15 Oversampling relocation, but keep the tasks independent.

## Acceptance and tests
1. Values **Off**, 20, 60, 73, 80, 100, 127, 150, 200, 245, 499, **500 Hz** selectable; no discrete jumps from 60 to 80 etc. Numeric manual entry and automation work.
2. Frequency-response tests verify detector cutoff across range at 44.1, 48, 96 kHz; audible bass path unfiltered. Stereo/mono, oversampling, bypass and click-free automation tests.
3. Legacy session and preset fixtures for Off/60/80/100/150/200 restore exact effective cutoff and do not reset/shift other host automation values; document new-vs-old automation precedence.
4. UI shows Ignore Below, Off or correctly formatted Hz; tooltips accurately describe detector-only filtering. Layout, minimum size, HiDPI screenshot and keyboard accessibility tests.
5. Native CI, truthful release notes and source docs update (`analog-compressor/docs/detector-low-cut.md`), exact-head merge, qualifying beta VERSION bump, suite installers, private R2 and authenticated beta site verified before issue Done.

**Risk:** legacy JUCE parameter migration and conflicting host automation. Keep implementation separate from cosmetic relocation; do not pretend completion if compatibility is unverified.
