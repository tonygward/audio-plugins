**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/169

# OPT-01 · Analog · Optical Leveller

**Recommended model:** Opus for DSP design/validation; Sonnet for implementation.  
**Estimated effort / usage:** High (relative, not a token estimate).  
**Model escalation:** Use Opus only for modelling and recovery design, then Sonnet for implementation. Never switch to Opus silently: record why, scope its use to the difficult design/validation step, then return to Sonnet.  

Follow `CLAUDE.md` and the [suite backlog](../README.md). This is a standalone plugin for Songsmith Plugins.

Build an LA-2A-inspired optical leveller with a simple gain and compression interface. Research the optical element’s programme-dependent recovery and validate it with short and long bursts. Document the reference unit and the behaviours you model. Start preset research with Joe Gilder’s vocal and bass levelling tutorials.

## Deliver and verify

Use the shared UI and follow the [preset requirements](../done/SUI-09-preset-library.md) and [editor requirements](../done/SUI-10-resize-and-plain-language.md), including saving and reloading custom presets. Give the plugin stable identities. Ship through the same VST3, Audio Unit and standalone build and suite installer workflow as the existing plugins.

Write the reference and preset source notes before tuning the final settings. Verify DSP behaviour with automated checks, test saved state and automation, and record a real host check and level-matched listening notes before calling the plugin release-ready.

**Current execution and delivery:** Follow [the owner policy](../../../docs/songsmith-execution-policy.md). Board order decides selection regardless of issue/prompt location or prefix, with one task and a 60% weekly usage stop threshold. Use the actual implementation repository's scoped execution path. Automatically approve/merge after current-head required checks; verify beta deployment and downloads, post the final issue comment, then close and finally mark board Done. Major/minor increments are deferred; beta build/patch increments are automatic.


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready; Doing means genuinely claimed with a start comment, never an implicit run-now command. Needs attention is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Needs attention. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This specification update does not activate Matrix or demonstrate operational board integration.
