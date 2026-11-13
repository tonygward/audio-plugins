---
id: ACOMP-09
priority: medium
effort: medium
dependencies: []
conflicts: []
---

# ACOMP-09: Guided compressor calibration

Issue: [#264](https://github.com/tonygward/audio-plugins/issues/264). Follow [CLAUDE.md](../../CLAUDE.md); the rules for running it are in the [execution policy](../../docs/songsmith-execution-policy.md).

**Estimated effort / usage:** Medium. **Escalation:** Opus only for an unavoidable calibration-measurement architecture question; document why and return to Sonnet. The detector Low Cut (now Ignore Below) and editor updates it depended on have shipped.

Add an optional **Set Levels** action. Ask the user to play a representative loud section, then estimate an Input setting that gives a few dB of gain reduction on louder peaks (start with 2–4 dB) and an Output setting that approximately matches the dry and compressed loudness. Account for the current ratio, attack, release and Low Cut. Show the proposed settings and measured reduction, let the user apply or cancel, and make the applied settings undoable and manually adjustable. Keep Input and Output fixed during normal playback. Handle silence, short captures, clipping risk, and targets that cannot be reached. Keep measurement real-time safe and avoid writing over active host automation. Test on transients and sustained sources, and verify the output match using a level-matched bypass comparison.

Add explicit tooltips for the Attack and Release knobs. Attack explains how quickly compression starts, Release explains how quickly it lets go, and both say turning clockwise makes it faster. Verify that the knob direction agrees with the DSP.
