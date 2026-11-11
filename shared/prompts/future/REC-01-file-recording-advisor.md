**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/173

# REC-01 — Standalone file Recording and Streaming Advisor

**Model:** Sonnet; Opus for detailed measurement design if necessary.

## Goal

Extend the proposed standalone Songsmith Loudness Analyser with musician-friendly whole-file advice. Drag in a finished track and understand its technical format and loudness without needing to be a recording engineer.

Follow `CLAUDE.md` and reuse the Maximiser's validated measurement building blocks where appropriate. The initial feature is **analysis only**. Mastering and export remain separate later options.

## Example user experience

For a genuine 44.1 kHz, 16-bit WAV:

> “Your recording is CD quality and fine for mixing or streaming. There's no need to record it again. Next time, 24-bit recording gives you more flexibility with recording levels.”

For a genuine 48 kHz, 24-bit recording:

> “These are good recording settings. You can focus on the performance and the mix.”

Do not imply 44.1 kHz is inferior for music, 16-bit is unusable, or changing an existing 16-bit file to 24-bit restores missing captured detail.

## What to measure and explain

- **Actual file metadata:** format, sample rate, actual stored sample depth where known, channel count and duration. Distinguish original recording bit depth from DAW internal 32-bit float processing and exported format. If metadata is unavailable, say so.
- **Whole-track loudness:** BS.1770 integrated LUFS; optional loudness range, independently validated maximum true peak, and peak readings. Handle silence, very short inputs and edge cases.
- **Clipping:** explain detected ceiling violations or suspicious peaks cautiously; do not claim every flat-topped waveform proves the original capture clipped.
- **Streaming:** explain normalisation without presenting -14 LUFS as a mandatory Spotify acceptance threshold or inventing an Apple/YouTube pass criterion. For any platform-specific claim, cite a dated authoritative source and make the estimate conditional on playback settings.
- **Next steps:** distinguish “No change necessary”, “Try this if you're hearing a problem” and “For your next recording”. Avoid fear-driven quality scores and alarm badges.

## Acceptance

1. Analyze user-selected WAV/AIFF/FLAC files offline; claim only file formats actually supported in the build.
2. Report reproducible metadata, LUFS and maximum true peak with tests against known reference files/standards. Never read full files in a plugin's real-time audio callback.
3. No automatic remastering, resampling or overwriting originals. Advice must be optional and never sound like an instruction to re-record usable material.
4. Test 44.1/16, 44.1/24, 48/24, 32-bit float, clipping, silence, short material, very loud or dynamic mixes and malformed/unreadable files.
5. Show screenshots in the PR and keep useful app screenshots in the README/documentation. Document what is a real measurement, what is an estimate, and what the tool cannot know.

**Principle:** “What matters, why it matters, and whether I need to do anything.”

**Current execution and delivery:** Follow [the owner policy](../../../docs/songsmith-execution-policy.md). Board order decides selection regardless of issue/prompt location or prefix, with one task and a 60% weekly usage stop threshold. Use the actual implementation repository's scoped execution path. Automatically approve/merge after current-head required checks; verify beta deployment and downloads, post the final issue comment, then close and finally mark board Done. Major/minor increments are deferred; beta build/patch increments are automatic.


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the prompt is deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This specification update does not activate Matrix or demonstrate operational board integration.
