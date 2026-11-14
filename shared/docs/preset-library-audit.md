# Preset library audit (SUI-09)

Prompt: `shared/prompts/SUI-09-preset-library.md`. This is the audit, what changed, and what is still open. Written without a compiler, so **nothing here has been built or run**: the hosted build and tests are the first check. Not verified in a DAW.

## Evidence gaps (read first)

- **Joe Gilder tutorials were not available.** The sources the prompt names (`tonygward/music-recording/techniques/joe-gilder.md` and the transcripts) are outside this repository and could not be opened. No preset was re-checked against a video, and no timestamp, setting or lesson was added or changed. The EQ's existing source table, with timestamps, is `eq/docs/starting-points.md`; it is carried over as recorded, **not re-verified**.
- **No auditioning.** Tony's mixes were not available, so no preset was auditioned or level-matched by ear. Existing level-matching tests are unchanged (pink noise for EQ, a -18 dBFS RMS sine for the compressor).
- **Not Joe Gilder-based, so not yet in line with the prompt's rule.** These EQ presets come from Tony's own mixes and notes, as `eq/docs/starting-points.md` says: Acoustic Guitar (Full Mix), Acoustic Guitar (Pickup), Reverb Send Clean-up, Mid Tom, High Tom, Floor Tom and Toms Bus. The prompt says to leave unsupported presets out of the curated set. **They are still listed.** Hiding them is a product decision for Tony (the menu is a view, so it would not affect saved sessions).
- **No tutorial source recorded** for the Analog Compressor starting points (Lead vocal, Acoustic guitar, Bass, Drum room) or the Maximiser starting points (Mix bus, Demo loudness, Final export). Their documented sources are the 1176 manual and the L1 manual, with conservative Songsmith choices. Same decision applies.

## Audit

| Plugin | Factory presets | User presets | Gaps found |
|---|---|---|---|
| EQ | 27 compiled in, 24 listed by use ("Drums - Kick"), read-only | `.sspreset` (and legacy `.twpreset`) in `Songsmith Studio/Songsmith EQ/Presets`; Save, Save As, delete, legacy folder migration, session recall | Save As replaced a same-name preset silently. One title named a technique. |
| Analog Compressor | 4 compiled in | `.sspreset` in `Songsmith Studio/Songsmith Analog Compressor/Presets`; save, load, open folder | Silent replace. A user preset could take a starting point's name. No size limit when loading. |
| Maximiser | 3 compiled in | **None.** | No way to save, reload or replace a custom preset. |

## What changed

- **EQ:** Save As asks before replacing (`PresetManager::userPresetExists`, `PresetBar::saveAsDialog`). The preset at index 21 is now **Reverb send clean-up** (menu label **Effects - Reverb Send Clean-up**), not "Abbey Road reverb"; its index, bands and gain are unchanged. Tests updated, with a new duplicate-name test.
- **Analog Compressor:** the editor asks before replacing; starting-point names are refused (any case); files over 1 MB or missing are refused on load. New tests for duplicates, factory protection and oversize files.
- **Preset names:** every other existing name already describes a use or job, so none were changed.

## Not done

- **Maximiser custom presets.** It needs a save/load class, a menu and editor changes, plus tests. It was left out because it could not be built or checked here, and a broken editor would block the other changes. Needs a follow-up prompt.
- **Cross-platform and upgrade runs.** The new tests use temporary folders and need a macOS and a Windows run on CI.
- **Screenshots.** No editor layout changed (the only new UI is a confirmation box), so no before/after screenshots were taken.

## What a preset recalls

- **EQ:** every band (used or not, type, slope, frequency, gain, Q, on/off, dynamic settings) and Output Gain. Never Bypass, editor size or analyser settings.
- **Analog Compressor:** Input, Output, Attack, Release, Ratio, All Buttons and Gain Reduction Off. Never Bypass, Oversampling or editor size. (Presets now also store 2:1 and Ignore Below; see [presets.md](../../analog-compressor/docs/presets.md).)
- **Maximiser starting points:** Threshold, Output Ceiling, Release, Peak Mode, Auto Release and Dither (always off). Never Bypass or Link.
- No parameter ID, state version or user folder location changed, so host sessions and automation are unaffected.
