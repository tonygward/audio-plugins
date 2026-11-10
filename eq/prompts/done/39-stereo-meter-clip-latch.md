# EQ-39 — Mono/stereo output meter and latched clipping warning

**Status:** To do. **Priority:** 2 — early EQ improvements (after v1.3.0; near EQ-38). **Recommended model:** Sonnet. **Estimated effort / usage:** Medium. **Escalation:** Opus only for meter transport/host-layout architecture review, returning to Sonnet for implementation.
**Branch:** `roadmap/39-stereo-meter-clip-latch`.
**Related:** [EQ-27 vertical output meter](27-vertical-output-meter.md) (shipped), [EQ-38 Frequency Guide icons](38-frequency-guide-instrument-icons.md).

## User story
The Songsmith EQ output meter must match the track's audio layout: **two visible bars (Left and Right) for stereo, one bar for mono**. A clear red **CLIP** warning appears **above the meter** when either active output channel reaches or exceeds digital full scale, then stays lit **for the rest of the current audio activity**, and disappears **only after audio has stopped**.

## Requirements

### Channel-aware output meter
- Use the plugin's currently negotiated **main output bus channel count** to select mono versus stereo. Track changes and host bus-layout changes promptly (including re-opening a session or moving the plugin to a mono/stereo track); do not assume one layout at construction or confuse the EQ band stereo modes, sidechain bus or analyser settings with actual main output channel count.
- Mono: one appropriately sized vertical level bar. Stereo: side-by-side **L** and **R** bars, each showing its own actual **post-EQ/post-output-gain** channel peak. The dB scale and peak readout must remain legible.
- Preserve existing ballistics, peak hold, scaling, colour thresholds and meter placement from EQ-27 wherever possible. Do not downmix stereo into a single peak reading.
- The meter must faithfully show levels for available output channels with no double counting; handle buffers with absent/disabled channels, suspended processing and bus changes safely.

### Sticky clip indication
- Place a noticeable but compact **red CLIP** warning directly **above** the meter, not elsewhere in the EQ editor. Show it when **either** active output channel peaks at or above 0 dBFS (sample peak |sample| >= 1.0); document that this is **sample-peak clipping**, not a true-peak/oversampling measurement. Evaluate both channels independently in stereo.
- Once triggered, **latch** it while audio continues. Do **not** clear on the next below-threshold block, on normal peak-hold expiry, or merely because the meter drops: a transient clip must remain visible until the audio stops.
- Define "audio stops" as **sustained near-silence on all active output channels** (e.g., below -80 dBFS for 750 ms), not a single quiet buffer or short musical pause. Verify the effective threshold/duration with tests; make a sensible adjustment if needed and document it.
- When sustained silence occurs, clear the red warning automatically. New playback begins clean, and a subsequent clip latches again. Persistent low-level noise can keep it lit, consistent with the rule "while there's any audio"; do not clear merely because transport is stopped if the track still outputs sound.
- When layout changes or plugin resets, handle previously latched state deterministically; avoid stale warnings from an old stereo/mono bus. Specify behaviour in PR.
- No flash/flicker; UI should update quickly enough for brief clips. Warning should be readable at 75–200% scaling and accessible using clear text rather than colour alone.

## Engineering constraints
- Read `CLAUDE.md`, `docs/style-guide.md`, `eq/ROADMAP.md`, `eq/Source/OutputMeter.h`, `eq/Source/PluginProcessor.cpp/.h`, `eq/Source/PluginEditor.cpp/.h`, and EQ-27 first.
- The audio thread must remain **real-time safe**: no UI operations, allocations, locks or string processing on the processing thread. If the existing meter telemetry only has one channel, extend it using a small lock-free/atomic snapshot or equivalent, without disturbing DSP. Use the existing editor timer to update the display.
- Keep parameter IDs, plug-in IDs, audio signal path and saved-state compatibility unchanged. No new host parameters or `stateVersion` change expected.
- Consider bypass, solo, linear-phase latency, offline rendering, and host mono/stereo configurations. Meter reports the audio actually output to the host, not dry input.

## Tests and acceptance
1. Mono input/output -> exactly one bar; stereo output -> exactly two distinctly labelled L/R bars. L-only and R-only test signals drive only their respective meters.
2. Test tone below 0 dBFS shows no CLIP; positive/negative full-scale sample on either stereo channel makes the red label appear, including very brief transient clips.
3. After a clip, quiet-but-audible signal keeps CLIP visible even if the current peak is safely below 0 dBFS. Brief near-silence or musical pauses do not clear it. Sustained near-silence on **both** active channels clears it; one active stereo channel prevents clearing.
4. Switching bus layouts and reopening sessions shows the right meter mode and no stale clip warning. Ensure robust behaviour with no signal and processing suspended.
5. Preserve meter ballistics, display scale and other controls at minimum/default window sizes and 75%, 100%, 200% scaling. Include screenshots of mono, stereo, and clipped stereo in the PR.
6. Run all EQ unit tests and a clean Release build, and verify no DSP audio difference and no material CPU regression. Record DAW verification separately (Studio One mono and stereo tracks plus Cakewalk if available); do not claim DAW verification unless done.

## Delivery
Use the repository's PR/CI workflow; create a branch, build and test, open a PR and merge only when checks pass. Keep the task to a UI/metering enhancement; avoid unrelated refactoring. Record the decisions, tests and screenshots. Mark EQ-39 done/move the prompt to `done/` **only when merged**. Do not bump suite version in this task; align with the next approved release checkpoint.


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the prompt is deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This archived implementation remains historical and must not be requeued; reconcile delivery evidence only.
