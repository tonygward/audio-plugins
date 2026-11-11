# ACOMP-13 — Joe Gilder compression preset research and allocation

**Status:** Backlog only. **Issue:** [ACOMP-13 #233](https://github.com/tonygward/audio-plugins/issues/233).  
**Product decision (11 Nov 2026):** [COMP-01 #235 — Songsmith Compressor](https://github.com/tonygward/audio-plugins/issues/235) owns **conventional low-ratio, slow-attack gentle compression**. Do not extend the existing 1176-inspired Analog Compressor to imitate the butter technique. This research/story must be scoped independently from COMP-01's DSP and preset implementation; do not queue it in parallel as duplicate code work.

## Objective
Research and document Joe Gilder's source-backed compression settings; produce a concise, curated instrument/use-case preset specification for **Songsmith Compressor**. His butter-style acoustic preset belongs in the new plugin and must retain the exact user-facing name **Acoustic Guitar**. The original **Songsmith Analog Compressor** keeps its 1176/FET character and existing compatible factory presets. No existing preset sound, session or program index changes should be made under this research story without an explicit separate compatibility review.

## Original source evidence
- [2018 How to Mix Acoustic Guitar — Butter Compression](https://www.youtube.com/watch?v=d7vHaydnTuQ&t=190s) and transcript in `tonygward/music-recording/transcripts/Joe Gilder • Home Studio Corner/2018-02-19 - How to Mix Acoustic Guitar - Butter Compression.md`. Very low threshold, ratio ~1.2–1.5:1, attack ~30 ms, release ~50 ms, light nearly continual GR; dense-mix use, not compulsory for sparse acoustics.
- [Bass](https://www.youtube.com/watch?v=CwyOBwOAsoQ&t=547s): slow attack for pick/transient, fast release, ~4:1.
- [Vocal compression](https://www.youtube.com/watch?v=soC_h30NsNI&t=121s): ~3–4:1, attack/release ~10–20 ms and context-specific heavy reduction.
- [Vocal finishing butter stage](https://www.youtube.com/watch?v=3Z_7G3vsN8E&t=594s): low ratio (~1.2:1), low threshold, attack/release ~30 ms, example ~5 dB GR.
- [Upright bass experiment](https://www.youtube.com/watch?v=LJ4eNuSO4-o&t=645s): trial of butter-style sustain, not universal.
- `tonygward/music-recording/techniques/joe-gilder.md` and associated archived transcripts are primary reference notes. Independently verify disputed numbers/timestamps; label interpretation as Songsmith recommendation, not Gilder quotation.

## Preset candidates for the new plugin
**Acoustic Guitar** (butter-style smoothing), **Lead Vocal Control**, **Bass Guitar**, **Electric Guitar Sustain**, **Backing Vocals**, **Drum Bus Glue**. Add only useful and distinct entries justified by source; optional **Vocal Finishing** if genuinely different from lead control. No person's name, tutorial label, or compressor model name in factory preset titles. Cite the original inspiration in preset help/docs.

## Technical boundaries and acceptance
- Existing Analog Compressor: fixed-threshold FET behaviour; 4:1 existing acoustic preset, ~430 µs attack, ~84 ms release; 2:1 capability still cannot do true ~30 ms attack or 1.3:1. Do **not** relabel an incompatible preset as Joe's butter compression or modify legacy model time constants. Keep current Analog Compressor preset unchanged unless a later user request specifically authorises it.
- Produce source traceability table with URLs, timestamps, exact source values, song/instrument/use case and whether each value was directly stated, approximated, or independently selected for Songsmith.
- Give COMP-01 a ready-to-implement preset list with threshold/ratio/timing and target GR where explicitly supported, plus qualitative listening cues; choose source-dependent threshold by test signal, not imaginary universal fixed levels. Validate actual behaviour with test fixtures and representative audio where available; do not claim audible review without it.
- Record duplicates with existing presets and remove/revise proposed entries to keep curated library small.
- Standard Matrix lifecycle if scheduled: claim/issue progress comments, review/verification evidence, complete only on its task-specific research deliverables. No plugin release required for docs-only work.
