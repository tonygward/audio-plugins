**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/160

# 07 · Low Cut for the compressor detector

**Recommended model:** Sonnet.  
**Estimated effort / usage:** Medium (relative, not a token estimate).  
**Model escalation:** Opus only if the detector DSP architecture proves nontrivial. Never switch to Opus silently: record why, scope its use to the difficult design/validation step, then return to Sonnet.  

Follow `CLAUDE.md`. Build after 06 to avoid overlapping processor and state changes.

Add an optional **Low Cut** in the compressor's level detector. It should reduce how much bass triggers compression while leaving the input audio's bass intact. Label it clearly and use a tooltip such as “Stops deep bass from making the compressor turn the whole sound down.” Provide Off and a practical frequency adjustment. Select and document the range and slope using kick, bass and vocal listening tests. Start Off in fresh and existing sessions. Describe the feature as a Songsmith addition to the 1176-inspired design.

Give the setting stable parameter IDs, migrate old states, smooth changes and keep stereo linking, bypass and latency consistent. Verify a bass-heavy signal produces less gain reduction with Low Cut enabled, and verify the filter is confined to the detector with a no-gain-reduction response test. Cover automation, recall and supported oversampling modes. Include listening notes and a tooltip and layout check.

**Current execution and delivery:** Follow [the owner policy](../../docs/songsmith-execution-policy.md). Board order decides selection regardless of issue/prompt location or prefix, with one task and a 60% weekly usage stop threshold. Use the actual implementation repository's scoped execution path. Automatically approve/merge after current-head required checks; verify beta deployment and downloads, post the final issue comment, then close and finally mark board Done. Major/minor increments are deferred; beta build/patch increments are automatic.
