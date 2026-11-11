# ACOMP-13 — Joe Gilder-grounded acoustic and instrument compression presets

**Product:** Songsmith Analog Compressor. **Status:** backlog only; board controls scheduling. **Agent:** Sonnet, escalate architecture decisions to Opus if necessary.  
**Issue:** ACOMP-13. **Canonical sources:** Joe Gilder original tutorials and user-maintained transcripts, not general web compressor charts.

## Objective / user value
Make the **existing Acoustic Guitar** factory preset genuinely reflect Joe Gilder's "butter compression" technique, while keeping the exact user-facing preset name **Acoustic Guitar**. Review his *other* documented compression techniques and add a small curated set of instrument/use-case presets with clear listening descriptions. Preserve existing saved sessions/host program indices/automation.

## Source evidence (read before implementation)
- [2018 Butter Compression tutorial](https://www.youtube.com/watch?v=d7vHaydnTuQ&t=190s), transcript in `tonygward/music-recording/transcripts/Joe Gilder • Home Studio Corner/2018-02-19 - How to Mix Acoustic Guitar - Butter Compression.md`.
- `tonygward/music-recording/techniques/joe-gilder.md`: acoustic: start threshold minimum, ratio 1:1, attack ~30 ms, release ~50 ms; gradually increase ratio to ~1.2–1.5:1 for gentle ongoing gain reduction. Joe also says acoustics often require **no compression** in sparse mixes; his butter setting is for making acoustic audible in dense mixes.
- [Joe Gilder bass advice](https://www.youtube.com/watch?v=CwyOBwOAsoQ&t=547s): slow attack to retain pick click, fast release, roughly 4:1.
- [Joe Gilder vocal mistakes](https://www.youtube.com/watch?v=soC_h30NsNI&t=121s): 10–20 ms attack/release, 3–4:1, potentially high GR (~12 dB) as dictated by the voice.
- [Joe Gilder favourite compression uses](https://www.prosoundweb.com/in-the-studio-favorite-ways-to-use-compression/): bass levelling, vocal intelligibility, electric guitar sustain, drum/snare punch, drum bus glue, occasionally reverb return.
- [2025 Vocal chain](https://www.youtube.com/watch?v=3Z_7G3vsN8E&t=594s) `tonygward/music-recording` transcript: optional extra butter stage at end of vocal chain, ~30 ms attack/release, very low threshold, ~1.2:1, ~5 dB GR on the demonstrated vocal.
- [2017 Bass mixing](https://www.youtube.com/watch?v=LJ4eNuSO4-o&t=645s): butter approach trialled for upright bass sustain (an experiment, **not** a universal bass setting).
- [2026 vocal compression](https://www.youtube.com/watch?v=G3jF6oVw9BE&t=187s): same low-threshold low-ratio stage for vocals.
- [2018 butter tutorial](https://www.youtube.com/watch?v=d7vHaydnTuQ): verify precise transcript before selecting any value. Avoid conflating other engineers' acoustic settings with Joe's.

## Existing behaviour verified
`analog-compressor/docs/presets.md`: four starting points **Lead vocal**, **Acoustic guitar**, **Bass**, **Drum room**. Existing Acoustic guitar: Input +2 dB, Output +3.3 dB, attack knob 2 (~430 **microseconds**), release 6 (~84 ms), ratio **4:1**. The core 1176-like compressor has a fixed threshold and fast ~20–800 µs attack range. 2:1 has been added (v0.5.8 changelog), but it is **still not Joe's 1.2–1.5:1, 30 ms attack, low threshold**. DO NOT claim it reproduces Joe precisely, and DO NOT silently reinterpret fixed ratio and attack ranges.

## Required change
1. Keep the exact factory preset name **Acoustic Guitar** (capitalisation per suite style). In help/tooltip, say 'Gentle, almost continuous control that helps acoustic guitar sit in a busy mix. Often unnecessary for a sparse acoustic recording.' Attribution and 'butter compression' terminology belong in documented source notes, not the preset title.
2. **Feasibility gate before implementation:** identify a clean, backward-compatible way to achieve approximate 1.2–1.5:1, ~30 ms attack, ~50 ms release, and very low effective threshold with the plugin's DSP. A 1176 input/output/ratio tweak cannot simulate an arbitrary longer attack. Prefer a well-isolated optional gentle mode/parallel or separate detector when technically warranted, with its actual measured transfer/dynamics documented; do not introduce hidden misleading equivalent controls. If this requires substantial DSP architecture, split into a **small dependency child issue** for that capability, retain ACOMP-13 as blocked by it, and do not ship a falsely 'Joe grounded' acoustic preset. Preserve the sound of legacy 1176 modes. Never copy proprietary GilderComp DSP; use independent general compressor implementation principles.
3. Review and add only well-grounded, acoustically distinct factory presets, with use-case names and documented quantitative support:
   - **Lead Vocal Control**: use Joe's vocal 3–4:1 10–20 ms recommendations only if the mode supports the times, otherwise note compatible adaptation honestly.
   - **Bass Guitar**: slow attack / faster release / ~4:1 to retain thump and pick articulation.
   - **Electric Guitar Sustain**: intended to make sustained guitars ring longer; specific parameters must be derived and auditioned, not fabricated as Joe's numbers.
   - **Backing Vocals**: very gentle continuous levelling, if soft compression mode exists; cite butter technique.
   - **Drum Bus Glue**: conservative bus approach, supported by Joe's subgroup methodology; avoid hard invented values.
   Optionally evaluate **Snare Punch** or **Reverb Return** if sources and distinct practical utility justify them; no preset proliferation.
4. Keep the current **Drum Room** preset unless a clear sourced improvement exists; preserve current **Bass**, **Lead vocal**, **Acoustic guitar** session recall and prior factory indices while updating their current menu treatment as applicable. For old sessions, preserve sound of stored parameter values; distinguish preset definition changes from recalled session state. Coordinate with SUI-09 preset library audit and canonical factory-preset naming rules in `CLAUDE.md`.
5. Include source references/timecodes, source vs Songsmith interpretation, user guidance including suggested gain reduction (only where source-supported), and automatic level matching (no baked-in ungrounded output boost). Plain-English one-line tooltips. Avoid brand name 'Joe Gilder', 'Butter', or vintage device names in preset titles.
6. For each new preset, measure actual compression with calibrated signals and demonstrate signal-dependent input, transfer curve/attack/release, transient retention and no clipping; use guitar/vocal/bass/drum listening/DAW checks where audio fixtures are available. Unit tests for old state and program index preservation, preset save/load, automation/mode recall, stereo linking, no clicks, no CPU or latency regression.

## Acceptance / release
- Existing **Acoustic Guitar** is legitimately a gentle, nearly continuous compression option *or* the requirement remains blocked with a clearly documented DSP prerequisite; never pretend fast 4:1 1176 is butter compression.
- No duplicate names or redundant presets; use cases distinct and concise.
- Source-backed descriptions and documented adaptation where settings aren't directly attainable.
- Pass native Windows/macOS tests and exact-head CI; follow standard version increment, trusted merge, installers, R2 and beta website checks. Comment on claim, implementation/failure, final evidence; issue close and board Done only after verified delivery.
