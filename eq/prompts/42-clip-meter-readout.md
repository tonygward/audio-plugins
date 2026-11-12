# EQ-42 — Accurate clipping indication and peak readout placement

**Issue ID:** EQ-42  
**Product:** Songsmith Clean EQ  
**Workstream:** Functional UX / metering correctness  
**Default agent:** Claude Sonnet; Opus only if metering architecture needs complex debugging.  
**Related:** EQ-39 (#117 implementation), `eq/docs/output-meter.md`; EQ-41 (#203) unrelated layout change.

## User story
As a musician applying EQ to previously mastered and limited audio, I want the peak level and clipping indicator to convey accurate, independent information, so I know whether the output actually exceeds digital full scale without false-looking warnings or confusing red numbers.

## Current behaviour (verified in source)
- `eq/Source/OutputMeter.h` displays the maximum held sample peak in dBFS, currently **underneath** the meter in the frequency-axis row.
- `getPeakText()` prints positive values with a `+` prefix and negative values with `−` (JUCE numeric formatting). Thus `+0.6` genuinely means +0.6 dBFS, not 0.6 dB below 0.
- The text is currently red whenever `clipped || getHoldDb() >= 0`; therefore a **latched historic CLIP** can make a currently negative held peak appear red.
- `CLIP` latches when any main-output sample reaches **or exceeds** 0 dBFS and clears after sustained silence (currently ~0.75 s below −80 dBFS), per EQ-39; the meter is *sample-peak*, not true-peak.
- Masters with sample peaks exactly at 0 dBFS may legitimately trigger the current `>= 0` threshold; prior limiting does not inherently mean an EQ plugin cannot exceed 0.

## Desired behaviour
1. Move the numeric held-peak dBFS readout from *below* the meter to **directly below CLIP and above the meter bars/channel labels**. Reflow layout rather than overlay the value onto bars. Maintain existing one/two-bar mono/stereo presentation, peak scale, tooltip, and readability at minimum size/HiDPI.
2. Correct semantic colour: **negative held peak (e.g. −0.6 dBFS) always uses normal non-red text**, including while the separate CLIP latch remains lit. **Positive held peaks (e.g. +0.2 or +0.6 dBFS) are red**; 0.0 dBFS uses a deliberate, documented boundary rule aligned with the clip indicator. No fabricated inversion of sign.
3. Audit the clip threshold and audio-thread source of peaks to establish *why* previously limited masters trigger CLIP. Distinguish (a) exactly 0 dBFS samples from a limiter, (b) overs after EQ gain/boost, (c) past transient retained by latch, and (d) display/rounding edge cases. Prefer a clearly documented 0 dBFS sample-peak policy; do not suppress genuine overs. If tolerance is needed for floating-point rounding, make it tightly scoped, deterministic and tested, rather than hiding +0.2 dBFS.
4. Keep warning and held-peak data separate; do not change peak measurement to a 'distance from top' metric. Tooltip should explain **negative means below 0 dBFS, positive means over 0**, the sampling point (post-EQ/post-output-gain), the latch, and sample peak versus true peak. Remove misleading advice that clipping is fine simply because distortion is inaudible.
5. Preserve real-time safety, no audio-thread allocations, audio DSP/gain, parameter IDs, presets, automation, sessions, peak hold length, and existing latency.

## Acceptance criteria / tests
- UI visually shows **CLIP → numeric peak → channel labels/bar(s)**, with no duplicate number below the bars.
- Test −6.0, −0.6, just below 0, exactly 0, +0.2, +0.6 dBFS and silence, including rounding to one decimal; displayed sign and colour represent measured held-peak value (not stale CLIP state).
- With CLIP latched and the held peak decayed below 0, CLIP may remain red per established behaviour while negative number is non-red.
- Create deterministic sample input fixtures: a brickwall-limited master with max sample at −1 dBFS, exact 0 dBFS, post-EQ gain to +0.2 dBFS, and brief historical over followed by sustained subzero audio. Assert actual detection, latch clearance and peak readout independently for mono/stereo.
- Confirm no false CLIP caused by metre graphics/rounding; document exactly 0 dBFS policy and avoid claiming true-peak precision.
- Visual test default/minimum/maximum editor sizes, DPI 100/125/150/200%, Windows/macOS, no collisions with EQ graph or scale.
- Run EQ unit/UI tests, native CI, and verify in a DAW against limited material if available; provide screenshot evidence.
- Update `eq/docs/output-meter.md`, relevant tests/changelog, and add issue progress/evidence comments.

## Risks and dependency
The existing EQ-39 latch behaviour is intentionally sticky; do not equate 'red CLIP stays on' with a current overshoot. Accurate user-facing semantics are more important than hiding indications of true clipping.

## Build and delivery
Follow `docs/songsmith-execution-policy.md`. One Matrix job at a time; usage gating and board order apply. Require current-head CI, trusted merge, appropriate build-number increment from current baseline, suite installers, R2 release and authenticated beta website download verification. Comment on claim/start, progress, failures, completion; close and mark board Done **only after verified deployment**.
