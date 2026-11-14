# COMP-02 — Real-time dynamics graph and EQ-consistent toolbox/meter

## Objective and user value
Give **Songsmith Compressor** (the new transparent compressor defined by COMP-01, not the 1176-style Songsmith Analog Compressor) an immediate, visual explanation of compression: show the live input level and the output after gain reduction, using the supplied FabFilter Pro-C 3 screenshot as **behavioural inspiration only**. Retain Songsmith's own branded visual language, not a copied FabFilter skin.

## Existing vs desired
COMP-01 specifies appropriate input/output and gain-reduction metering but does not define a full scrolling dynamics visualisation or precise editor layout. Desired: a prominent, efficient scrolling level-versus-time graph, Songsmith EQ-style draggable floating controls, and a right-edge level meter matching Songsmith EQ.

## Implementation guidance
1. **Graph**: horizontal axis represents recent time; vertical axis level in dBFS (labelled ticks). Draw the incoming detected audio envelope in muted grey and post-compression output envelope in clear red. When compression engages, the red line falls below grey and follows its recovery; both tracks stay time-aligned. The plot must reflect measured compressor DSP state, not invented/synthetic reduction; distinguish gain reduction from gain added by makeup gain, and offer informative tooltip/legend. A subdued filled gap between input and compressed envelope is optional. Stereo handling must be defined explicitly (e.g. linked maximum-of-channels envelope) with consistent smoothing. Provide smooth refresh, silence/idle behaviour and bypass display semantics; never show fake reductions while bypassed. Prefer a freeze/pause control if existing EQ UX has a reusable pattern.
2. **Floating toolbox**: reuse Songsmith EQ's existing draggable floating-panel component/look, positioning/resize conventions, title-bar interaction and state persistence where applicable. House the compressor's Threshold, Ratio, Attack, Release, Knee, Makeup/Auto, Mix and Analog Warmth settings in a compact, logically grouped control panel; preserve all COMP-01 ranges, automation and controls. Don't copy Pro-C 3's knob layout. Provide the established Songsmith plain-language tooltips. Keep it usable at minimum editor size and HiDPI.
3. **Right-side meter**: reuse Songsmith EQ's actual level-meter component or shared implementation, with matching size, scale, peak-hold/clip conventions and stereo indications. Display compressor input/output level clearly (plus dedicated gain-reduction reading where practical) without ambiguous positive/negative dB signs. Follow EQ-42 metering semantics if implemented; don't confuse true peak with sample peak. Numerics must be accurate and clearly labelled.
4. Maintain consistent Songsmith suite title bar (SUI-18), preset, undo/redo, bypass and footer processing-load readout. Keep CPU/GPU usage minimal; do no allocations, locks or UI paint work on audio callback. Pass low-rate metering snapshots to GUI with lock-free bounded data transfer and decimation; changing editor size must not alter sound. Support host GUI close/reopen safely.
5. Reference image: attached by product owner on 14 Nov 2026, showing FabFilter Pro-C 3's grey input, red compressed envelope, level-time graph, and right-hand vertical meter. It is a visual/interaction reference, not a request to copy FabFilter trade dress or assets.

## Acceptance criteria
- Live grey input and red compressed level curves scroll smoothly and track actual input/output samples/envelopes in sync; audible gain reduction produces visibly accurate separation.
- Attack/release changes affect the measured time-domain curves predictably; steady tones, transients, silence, bypass and makeup-gain cases remain honest and understandable.
- Floating panel matches EQ's look, drag constraints and persistence; each COMP-01 parameter remains independently adjustable and host-automatable.
- Right-edge meter visually matches EQ and accurately reflects real compressor levels and clip status, including stereo and retained peak indications.
- Layout works at minimum and maximum supported scales, HiDPI, dark UI; no clipping, overlaps or inaccessible controls.
- Measurably low overhead and real-time-safe DSP-to-UI data path; no dropouts when drawing is enabled.

## Tests
- Deterministic test samples for transients and sustained levels: assert measured graph snapshots and gain-reduction envelope relationship (including makeup gain and bypass).
- Graph time alignment/smoothing across 44.1, 48, 96 kHz and multiple host block sizes, stereo/mono.
- Meter sample/peak/clip correctness, regression against EQ's existing meter behaviour.
- Floating panel drag, persisted position, parameter automation, presets, editor lifecycle, and screenshot/layout tests at minimum/100%/maximum scale.
- Host/CI integration and audio realtime safety regression.

## Dependencies, risk, build/release
- Depends on COMP-01 core plugin/editor existing and being ready to accept UI changes; respect COMP-01 as separate parent and avoid concurrent conflicting edits.
- Reference existing EQ floating toolbox and meter before changing code; prefer reuse/extraction over duplicating rendering logic. Main risk is UI performance/complexity; make a bounded implementation and separate graph from reuse work if necessary.
- Keep COMP-01's clean DSP and Analog Warmth behaviour unchanged, including session/preset compatibility. Apply repository patch/build version policy, Windows/macOS VST3 builds and automated tests, PR/review/merge, release notes and private R2/beta site artifact verification. Only mark Done after merge and beta deployment are verified, with issue progress comments. Board status alone is not execution evidence.
- Scheduling: **Backlog**, not Run Now. Follow board ordering/Matrix claim and one-job-at-a-time policy.
