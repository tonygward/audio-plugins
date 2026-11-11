---
id: SUI-09
priority: 2
model: sonnet
effort: medium
dependencies: ["SUI-14"]
conflicts: []
---

# SUI-09 · Curated factory presets and custom preset saving

**Recommended model:** Sonnet.  
**Estimated effort / usage:** Medium (relative, not a token estimate).  
**Model escalation:** Opus only if shared preset-state migration requires a new architecture. Never switch to Opus silently: record why, scope its use to the difficult design/validation step, then return to Sonnet.  

Follow `CLAUDE.md`. Audit EQ, Analog Compressor and Maximiser before changing them. Reuse the EQ preset implementation where its format and behaviour fit. Apply this acceptance standard to every future plugin.

- Name every factory preset for its practical use case in clear, concise language, such as **Acoustic Guitar Smoothing** or **Lead Vocal Control**. Keep hardware and model names, tutorial titles, and playful names out of the preset title. Record model selection in preset data and tutorial attribution in the preset notes. Users choose their own custom preset names.
- Give each plugin a short, curated set of factory starting points with plain names describing the source or job. Audit existing presets before replacing them. Ground each factory preset in an actual Joe Gilder tutorial. Use `tonygward/music-recording/techniques/joe-gilder.md`, `eq/docs/starting-points.md` and the original tutorials. Record the source link, timestamp where available, lesson, settings and any Songsmith adaptation. Use Tony's mixes to audition the settings where available. Flag evidence gaps and leave unsupported presets out of the curated release set.
- Provide clear controls to save a named custom preset, reload it in another instance or after restart, and overwrite deliberately. Keep factory presets read-only. Store user presets in per-plugin writable locations and preserve existing preset formats and locations or provide tested migration. Keep file access outside audio processing.
- Document which settings a preset recalls. Preserve host session recall and established automation IDs. Verify save, reload, duplicate names, invalid files, factory preset protection and upgrades. Test on both platforms and audition factory presets on relevant material with level matching where practical.


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the prompt is deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This archived implementation remains historical and must not be requeued; reconcile delivery evidence only.
