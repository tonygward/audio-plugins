---
id: MAX-08
priority: medium
model: sonnet
effort: medium
dependencies: []
conflicts: ["SUI-18"]
---

# MAX-08 — Maximiser textured faceplate and refined meters

**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/224  
**Reference:** Analog Compressor faceplate in `analog-compressor/Source/PluginEditor.cpp`.  
**Related:** SUI-18 #202 title-bar standardisation; MAX-07 #161 loudness-meter work.  

## Objective
Make the Maximiser look as polished as Analog Compressor by adopting its subtle brushed-metal texture and improving the housing/readability of Input, Gain Reduction and Output meters. This is visual polish only: no DSP, parameter, meter-ballistics or host-state changes.

## Existing behaviour
`maximiser/Source/PluginEditor.cpp` renders a plain background via `paintContent` and shared `drawPanel`. `maximiser/Source/Meters.h` contains vector vertical meters. `analog-compressor/Source/PluginEditor.cpp` has a deterministic brushed grain with a refined faceplate treatment. Inspect current implementation and latest SUI-18 changes before editing; preserve shared UI conventions.

## Desired behaviour and implementation guidance
1. Reuse or carefully adapt Analog Compressor's **design language**: subtle deterministic grain, restrained lighting and convincing panel depth, tailored to Maximiser's geometry. Do not blindly copy positions, screws or plugin-specific labels. Prefer cached/vector graphics scaled with the editor; any texture must remain static while meters repaint.
2. Keep background contrast subtle so threshold/ceiling faders, presets, bypass, title bar, captions and diagnostic area remain legible. Avoid noisy texture or noticeable rendering overhead.
3. Refine the three vertical meters together: premium dark recessed housing, crisp borders, precise scale/tick readability where space permits, polished but restrained active fill/highlights, prominent peak-hold and clipping cues. Gain Reduction must remain clearly visually distinct from Input/Output and retain its downward 0–24 dB indication.
4. Maintain the exact existing measurement ranges, conversions, ballistics, hold/reset, clip latch, timer update and meter source semantics. Do not invent stereo channels if current meters are not stereo. Preserve the LUFS work and current labelled metering semantics.
5. Preserve JUCE component bounds, tooltips and mouse reset behaviour; ensure legibility and control access at minimum/default sizes and high DPI. Coordinate with title-bar work (SUI-18), without changing its ownership or layout.
6. Avoid audio-thread graphics operations, repeated allocations/randomisation during repaint, new dependencies or expensive animation. Only edit shared look-and-feel if existing other plugins remain visually unchanged or the intentional effects are individually tested.

## Acceptance and validation
- Maximiser faceplate visibly echoes Analog Compressor's brushed-metal texture and quality while retaining its own layout.
- Input, Gain Reduction and Output meters are more visually polished and clearly readable during real operation.
- Correct numeric scaling and directions, zero-point, active bar, hold indicator, clip latch and click-to-reset, including silence, low signal, near 0 dBFS, clipping, and gain reduction.
- No changes to audio output, processing latency, parameter IDs, saved-session behaviour, presets or automation.
- Performance check shows no meaningful regression in idle/active UI redraw CPU; texture is not regenerated per meter tick.
- Before/after screenshots at default and minimum supported size, ideally a high-DPI capture, plus DAW visual and listening sanity-check notes.
- Automated meter/geometry tests where practical; required Windows/macOS CI succeeds for the exact PR head.
- Normal trusted Matrix completion: merged PR, correct beta build/version, suite installers, private R2, authenticated beta website verification, final evidence comment, issue closed, board Done. Do not claim success from CI alone.

## Risks and dependencies
Main risks: painting cost, loss of contrast, inadvertent meter response changes and overlapping UI edits with SUI-18. Scope remains a compact Maximiser-only visual improvement. Use Sonnet unless a genuine architecture/debugging problem warrants Opus. Preserve single-active-job and usage-below-60% rules.
