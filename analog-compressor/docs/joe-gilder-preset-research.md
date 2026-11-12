# Joe Gilder compression research and preset allocation (ACOMP-13)

Prompt: `analog-compressor/prompts/13-joe-gilder-grounded-presets.md`. This is a **research and specification document only**. No plugin code, preset, parameter, program index or session behaviour was changed.

## Evidence gaps (read first)

- **The primary sources could not be opened.** `tonygward/music-recording/techniques/joe-gilder.md` and the archived transcripts are outside this repository. The videos were not watched. Every "source value" below is copied from the task specification, **not re-checked against the video or transcript**. Where the specification gives an approximate value (`~`), it is recorded as approximate.
- **No timestamp was independently verified.** The timestamps are the `&t=` offsets in the links the specification gives. Someone with access to the transcripts should check them before COMP-01 cites them in preset help.
- **Nothing was auditioned and nothing was run.** No guitar, vocal, bass or drum audio was used, and no build or test was run. The gain reduction figures below are **static transfer-curve arithmetic** (hard knee, steady tone), not measurements of the plugin. Do not describe any preset as listening-verified until it has been tested on real audio.
- **Interpretation is Songsmith's.** Where a row says "Songsmith selection", the value is our design choice, not something Joe Gilder said.

## Decision recorded

Conventional, low-ratio, slow-attack compression belongs to **Songsmith Compressor** ([COMP-01](../../compressor/prompts/01-songsmith-compressor.md)). The Analog Compressor keeps its 1176-style fixed-threshold FET behaviour and its four existing starting points unchanged.

Why the Analog Compressor cannot host these: its ratios are 4, 8, 12 and 20 (plus the 2:1 capability in [two-to-one-ratio.md](two-to-one-ratio.md)), its slowest attack is about 800 µs, and its threshold is fixed. A 1.2–1.5:1 ratio with a 30 ms attack is outside all of that. Relabelling an existing preset as gentle smoothing would be inaccurate, and changing the model time constants would alter existing sessions.

## Source traceability

"Basis": **Stated** = the specification gives it as the source's value; **Approximate** = the specification says "about"; **Selected** = chosen by Songsmith, not taken from the source. All source values are unverified against the originals (see above).

| # | Source (URL, offset) | Instrument or use | Value in the specification | Basis | Notes |
|---|---|---|---|---|---|
| 1 | [How to Mix Acoustic Guitar – Butter Compression, 2018](https://www.youtube.com/watch?v=d7vHaydnTuQ&t=190s), 3:10 | Acoustic guitar in a dense mix | Threshold very low | Stated | Exact level depends on the source; not a fixed number |
| 2 | same | same | Ratio about 1.2–1.5:1 | Approximate | COMP-01 suggests 1.3:1 as the working value |
| 3 | same | same | Attack about 30 ms | Approximate | |
| 4 | same | same | Release about 50 ms | Approximate | |
| 5 | same | same | Light, nearly continual gain reduction | Stated | Qualitative; no dB figure given |
| 6 | same | same | Dense-mix use; not compulsory for sparse acoustic parts | Stated | Belongs in the preset help |
| 7 | [Bass](https://www.youtube.com/watch?v=CwyOBwOAsoQ&t=547s), 9:07 | Bass | Slow attack so the pick or transient passes | Stated (qualitative) | No millisecond value given in the specification |
| 8 | same | Bass | Fast release | Stated (qualitative) | No millisecond value given |
| 9 | same | Bass | About 4:1 | Approximate | |
| 10 | [Vocal compression](https://www.youtube.com/watch?v=soC_h30NsNI&t=121s), 2:01 | Lead vocal | About 3–4:1 | Approximate | |
| 11 | same | Lead vocal | Attack and release about 10–20 ms | Approximate | |
| 12 | same | Lead vocal | Heavy reduction depends on context | Stated | Not a default; see the preset notes |
| 13 | [Vocal finishing butter stage](https://www.youtube.com/watch?v=3Z_7G3vsN8E&t=594s), 9:54 | Vocal, finishing stage | About 1.2:1, low threshold | Approximate | |
| 14 | same | Vocal, finishing stage | Attack and release about 30 ms | Approximate | |
| 15 | same | Vocal, finishing stage | Example of about 5 dB gain reduction | Approximate, example only | The only explicit gain reduction target in the sources. Not a promise |
| 16 | [Upright bass experiment](https://www.youtube.com/watch?v=LJ4eNuSO4-o&t=645s), 10:45 | Upright bass | Trial of butter-style sustain | Stated, not universal | No settings. Not used for a preset |
| 17 | none | Electric guitar, backing vocals, drum bus | none | **Selected** | No Gilder source is cited for these |

## Proposed preset list for Songsmith Compressor

Names describe the use. They contain no person's name, tutorial title, hardware name or model name. The words "butter" and "Gilder" belong in the help text and these docs only.

**Threshold rule.** No preset gets an invented universal threshold. COMP-01 should set each stored threshold on the shared test fixture (a steady -18 dBFS RMS signal, the same one the existing starting-point tests use) from the "level over threshold" column, then re-check on real material before release. The user is expected to lower or raise Threshold for their own signal.

Static arithmetic used below: gain reduction = (level over threshold) × (1 − 1/ratio), hard knee, steady signal. Real programme material, knee and release will differ.

| Preset | Threshold | Ratio | Attack | Release | Target gain reduction | Basis |
|---|---|---|---|---|---|---|
| **Acoustic Guitar** | Very low: about 13 dB under the fixture level | 1.3:1 | 30 ms | 50 ms | Light and nearly continual, about 3 dB (static 13 dB × 0.23 ≈ 3 dB) | Rows 1–6. The 3 dB is a **Songsmith selection**; the source gives no figure |
| **Lead Vocal Control** | About 6 dB under the level of loud lines | 3.5:1 | 15 ms | 15 ms | About 4 dB on loud lines (6 dB × 0.71) | Rows 10–12, mid-points of the stated ranges. The dB target is a Songsmith selection |
| **Vocal Finishing** (optional) | Very low: about 30 dB under the fixture level | 1.2:1 | 30 ms | 30 ms | About 5 dB (30 dB × 0.17) | Rows 13–15. The 5 dB comes from the source's example |
| **Bass Guitar** | About 8 dB under the loud notes | 4:1 | 30 ms | 50 ms | About 6 dB on loud notes (8 dB × 0.75) | Rows 7–9 give the ratio and the slow-attack, fast-release shape. The millisecond values and dB target are Songsmith selections |
| **Electric Guitar Sustain** | Selected by test | 2:1 | 20 ms | 150 ms | 2–4 dB | **No source. Songsmith selection only** |
| **Drum Bus Glue** | Selected by test | 2:1 | 30 ms | 100 ms | 1–3 dB | **No source. Songsmith selection only** |

Listening cues for the preset help (suggestions, not measurements; each answers what to listen for and whether to do anything):

- **Acoustic Guitar:** the strum should sit more steadily with the pick attack still clear. If it sounds flat or squashed, raise the threshold. Doing nothing is fine in a sparse arrangement.
- **Lead Vocal Control:** loud lines should stay in place without the consonants going dull. If they do, lengthen the attack slightly.
- **Vocal Finishing:** use it after a firmer vocal compressor to even out what is left. You should barely hear it working.
- **Bass Guitar:** notes should come out even with the pick still audible. If low notes distort or wobble, lengthen the release.

## Curation: duplicates and removals

The library should stay small. Recommended outcome from six candidates plus one optional:

| Candidate | Overlaps with | Decision |
|---|---|---|
| Acoustic Guitar | Analog Compressor "Acoustic guitar" (4:1, about 430 µs, about 84 ms) | **Keep, in Songsmith Compressor only.** Different technique and ratio, so it is not a duplicate in sound. It shares a name with the Analog Compressor starting point only by spelling ("Acoustic guitar"); a name must not repeat **within** one plugin, and these are in different plugins. COMP-01 should check how the host lists both. Leave the Analog Compressor one alone |
| Lead Vocal Control | Analog Compressor "Lead vocal" (4:1, 234 µs, about 140 ms) | **Keep.** Slower, clean timing; the FET version stays where it is |
| Bass Guitar | Analog Compressor "Bass" (8:1, 234 µs, about 390 ms) | **Keep.** Gentler ratio and a fast release |
| Vocal Finishing | Lead Vocal Control | **Keep as the only optional entry.** It is distinct (1.2:1 versus 3.5:1, a finishing stage rather than a control stage). Drop it if COMP-01 wants fewer than six |
| Electric Guitar Sustain | Acoustic Guitar | **Recommend dropping.** No source, and the settings sit between Acoustic Guitar and Drum Bus Glue. Add it only if listening tests show it is useful |
| Backing Vocals | Lead Vocal Control, Vocal Finishing | **Recommend dropping.** No source, and not distinct enough from the two vocal entries. Users can start from Lead Vocal Control |
| Drum Bus Glue | Analog Compressor "Drum room" (8:1, about 430 µs, 50 ms) | **Keep, flagged as an unsourced Songsmith selection.** Different purpose (bus glue versus room punch) |
| Upright bass experiment | Bass Guitar | **Not a preset.** The source calls it an experiment |

Result: four sourced presets (Acoustic Guitar, Lead Vocal Control, Bass Guitar, optional Vocal Finishing) and one unsourced (Drum Bus Glue), with two candidates dropped.

## Analog Compressor: no change

- The four starting points in [presets.md](presets.md) (Lead vocal, Acoustic guitar, Bass, Drum room) stay exactly as they are, with their program indexes.
- No model time constant, parameter ID or state version changes.
- If the owner later wants renamed or re-voiced FET presets, that needs a separate compatibility review.

## Hand-off to COMP-01 and what is still needed

1. Someone with access to `music-recording` should confirm rows 1–16 (values and timestamps) and update the Basis column.
2. COMP-01 should implement the table above, then add tests: each preset loads, round-trips through save and load, and stays within 1 dB of the input level on the -18 dBFS RMS fixture (matching the existing convention).
3. Gain reduction on the fixture and on real guitar, vocal and bass audio should be measured and recorded here. Until then, the targets above are design intentions.
4. Preset help may credit "inspired by Joe Gilder's published compression tutorials" in provenance or an optional credit. It must not imply endorsement and must not appear in the preset titles or ordinary tooltips (SUI-14).
5. Not verified in a DAW.
