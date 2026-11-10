**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/160

# 07 · Low Cut for the compressor detector

**Recommended model:** Sonnet.  
**Estimated effort / usage:** Medium (relative, not a token estimate).  
**Model escalation:** Opus only if the detector DSP architecture proves nontrivial. Never switch to Opus silently: record why, scope its use to the difficult design/validation step, then return to Sonnet.  

Follow `CLAUDE.md`. Build after 06 to avoid overlapping processor and state changes.

Add an optional **Low Cut** in the compressor's level detector. It should reduce how much bass triggers compression while leaving the input audio's bass intact. Label it clearly and use a tooltip such as “Stops deep bass from making the compressor turn the whole sound down.” Provide Off and a practical frequency adjustment. Select and document the range and slope using kick, bass and vocal listening tests. Start Off in fresh and existing sessions. Describe the feature as a Songsmith addition to the 1176-inspired design.

Give the setting stable parameter IDs, migrate old states, smooth changes and keep stereo linking, bypass and latency consistent. Verify a bass-heavy signal produces less gain reduction with Low Cut enabled, and verify the filter is confined to the detector with a no-gain-reduction response test. Cover automation, recall and supported oversampling modes. Include listening notes and a tooltip and layout check.

**Current execution and delivery:** Follow [the owner policy](../../docs/songsmith-execution-policy.md). Board order decides selection regardless of issue/prompt location or prefix, with one task and a 60% weekly usage stop threshold. Use the actual implementation repository's scoped execution path. Automatically approve/merge after current-head required checks; verify beta deployment and downloads, post the final issue comment, then close and finally mark board Done. Major/minor increments are deferred; beta build/patch increments are automatic.


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the prompt is deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This specification update does not activate Matrix or demonstrate operational board integration.
