**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/172

# ADV-02 — Reference-mix guided review

**Recommended model:** Opus for feasibility, user journey, architecture and evaluation design; Sonnet for isolated prototype and implementation after design approval.
**Relative effort/usage:** High. **Opus escalation:** Already scoped to initial system design; return to Sonnet for routine UI, tests and integrations.

## User story
A musician supplies a **reference mix** and their **own mix**. Songsmith helps them understand, in plain English, where their mix differs and what to *listen for or try*, organised by instrument/group when those sources are actually available. This extends ADV-01's optional point-of-use guidance; it is not a replacement for basic presets.

## Phased delivery proposal (validate before promising)
1. **Feasibility and consent:** Decide on local/offline analysis first; optional remote AI must require opt-in and clear privacy, cost and data-retention disclosure. Investigate supported DAW host constraints, sidechain/reference playback, offline file loading, and the appropriate home (separate standalone companion vs Channel Strip). No hidden audio uploads or subscription dependency.
2. **Reliable mix-to-mix comparison MVP:** Import two complete audio files, align comparable sections, gain-match for fair A/B, and report robust measurable differences: loudness, tonal balance/broad spectral shape, dynamics/crest factor, stereo width and peaks. Clearly distinguish measurements from opinions. Do not suggest blindly cloning a commercial master or equate louder with better.
3. **Track/group analysis:** Offer instrument- and bus-specific observations **only when the user explicitly supplies stems, isolated tracks, or reliable DAW routing**. A stereo master alone cannot reliably reveal an individual instrument's EQ/compressor choices; source separation, if explored, must label uncertainty and artefacts. Don't claim accurate per-track diagnosis from two stereo files.
4. **Interactive guided review:** Display differences as a navigable, optional tour: choose Vocals, Acoustic Guitar, Bass, Drums, Bus, Master as available; listen to time ranges and A/B snippets at matched level; explain *what differs, why it may matter, and an optional next step*. Allow skip, undo, user control and “leave it alone.” Suggested Songsmith EQ/compressor starting points may be offered with explicit preview/confirmation; never change a mix automatically.
5. **Validate with real projects:** Use permissioned reference material and user-owned mixes; test recordings with acoustic guitar, bass, drums and vocals, plus a full mix. Evaluate the accuracy/helpfulness with human listening, false positives, time alignment and different arrangements. Ensure DAW stability, recall, performance and accessibility.

## Non-goals / safeguards
- Not automatic mastering, not replacing critical listening or a human engineer, not a promise to recreate a copyrighted production.
- No fabricated instrument-specific advice when only stereo mixes exist.
- No model guesses presented as objective facts, no mandatory cloud connection and no surprise charges.

## Decision gate
Produce a short technical spike and UX prototype, with feasibility, privacy, computational cost, confidence limits and integration strategy **before** assigning an implementation phase, timeline or version. Coordinate with ADV-01, REC-01 and the planned Channel Strip.

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
