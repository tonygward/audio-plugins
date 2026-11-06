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
