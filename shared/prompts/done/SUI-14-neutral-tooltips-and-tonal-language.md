---
id: SUI-14
priority: 1
model: sonnet
effort: medium
dependencies: []
conflicts: []
---

# SUI-14 — Neutral tooltips and clear tonal language

**Status:** Planned; not implemented. **Priority:** Early UI/content polish (after current EQ-38–40 work, or alongside it if independent).
**Recommended model:** Sonnet. **Relative effort/usage:** Low–Medium. **Escalate to Opus:** Only if source attribution is entangled with generated UI content or needs an architecture decision; document and return to Sonnet.

## Objective
Make all Songsmith user-facing explanations sound like Songsmith, not like citations to a named teacher. Preserve the musical educational value and the traceable research sources.

## Work
1. Audit EQ Frequency Guide region tooltips, preset descriptions, processing hints, compressor and maximiser tooltips, help panels, and future reusable guidance strings. Remove explicit Joe Gilder references, tutorial citations, and other named educators **from user-facing controls/tooltips/help**, unless a dedicated credits/about screen deliberately attributes sources. Preserve research credits, tutorial links, timestamps and provenance in developer documentation, preset research notes and source metadata. Do not remove legal attribution where required.
2. Keep the current instrument-specific tone labels, including **Plastic** and **Cheap**, for now. These originated in documented listening guidance; do not silently replace them on subjective feedback alone. Rewrite their explanatory tooltip text in straightforward, acoustic-guitar-relevant terms, including what to listen for and that the band may need no adjustment. Avoid asserting that a frequency range *always* sounds plastic, cheap or bad, or that this vocabulary is exclusive to electronic music.
3. In tooltips, speak directly to the musician: *what it might sound like*, *when to listen for it in context*, and *whether a small adjustment may help*. Do not mention the named source, imply endorsement, or treat EQ recommendations as guaranteed diagnoses.
4. Where names are especially subjective, gather musician feedback and consider descriptive **secondary explanation** rather than wholesale relabelling. No new public naming conventions until reviewed.
5. Keep research provenance in `eq/Source/FrequencyGuide.h`, `eq/docs/frequency-guide.md`, `eq/docs/starting-points.md` and original `music-recording` notes. Preserve saved sessions, preset IDs, and all audio/DSP behaviour.

## Acceptance criteria
- Audit every visible tooltip/hint in the **three existing plugins**; no educator's name appears in the operational UI without a deliberate credits exception. Check Joe and Gilder, including case-insensitive matches; avoid blanket replacing strings in documentation.
- Frequency Guide descriptions (especially Plastic and Cheap) state a musical, source-specific audible quality and optional listening action; they do not claim certainty.
- Add focused UI text tests and manual hover checks (including disabled tooltips); screenshots where practical; validate at the minimum editor size.
- No audio changes, no preset-index migration, no unexpected release. PR explains any exceptions and shows provenance remains traceable.


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the prompt is deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This archived implementation remains historical and must not be requeued; reconcile delivery evidence only.
