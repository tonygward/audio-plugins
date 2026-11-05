# EQ-40 — Organise Starting Points by instrument and remove duplicate Factory menu

**Status:** To do. **Priority:** 2 — early EQ usability, alongside EQ-38 and EQ-39. **Model:** Sonnet. **Branch:** `roadmap/40-organise-starting-points`.
**Related shipped work:** EQ-20, EQ-26 and [EQ-31 tom presets](done/31-tom-presets.md). **Coordinate with:** shared preset rule SUI-09.

## Request
Make Songsmith EQ presets easy for a musician to find. The picker must be grouped by instrument, using clear titles such as **Drums - Kick**, **Drums - Snare**, **Drums - Mid Tom**, **Drums - High Tom**, **Drums - Floor Tom** and **Drums - Toms Bus**. Remove **Five-zone start** from the visible choices. Eliminate the duplicated **Factory** group: usable factory-supplied instrument presets belong under **Starting Points**, not a competing Factory section.

## What the code currently does (confirmed on main)
`eq/Source/PresetManager.cpp` stores both legacy Factory presets and Starting Points in the same indexed `factoryPresets()` vector, distinguished by `startingPoint`. Legacy entries include Flat, Vocal clean-up, Low-end tidy, Air, Telephone, Kick punch, Bright acoustic, De-mud. Starting Points include Lead vocal, Backing vocals, Spoken word, Acoustic guitar, Electric guitar, Bass guitar, Drum bus, Snare, Reverb return, Five-zone start, Acoustic guitar in a full mix, Acoustic guitar pickup, Abbey Road reverb, Rack tom, Floor tom and Toms bus. Existing **Rack tom** is one shared starting point derived from both mid and high tom settings from *Between You and Me* (their documented SSL EQ settings coincide). The absence of separately labelled Mid Tom and High Tom options is a discoverability issue, not proof that audio settings are missing.

## Desired menu
- One prominent **Starting Points** menu, grouped and sorted by instrument. Example user-facing labels (adjust only to keep naming consistent):
  - **Drums - Kick**; **Drums - Snare**; **Drums - Mid Tom**; **Drums - High Tom**; **Drums - Floor Tom**; **Drums - Toms Bus**; **Drums - Drum Bus**
  - **Guitars - Acoustic Guitar**; **Guitars - Acoustic Guitar (Full Mix)**; **Guitars - Acoustic Guitar (Pickup)**; **Guitars - Electric Guitar**; **Guitars - Electric Guitar Bus**; **Guitars - Bass Guitar**
  - **Vocals - Lead Vocal**; **Vocals - Backing Vocals**; **Vocals - Spoken Word**
  - **Effects - Reverb Return**; **Effects - Abbey Road Reverb**.
- Follow the user's requested **Drums - Kick** naming (not obscure technical prefixes). Use a grouped submenu or a consistently prefixed alphabetical list according to what the existing JUCE UI supports and what tests show is more usable. Name by *instrument and purpose* (no cute/model-based labels).
- **Add separately selectable Mid Tom and High Tom** entries, taking the existing Rack Tom settings as the initial baseline. Investigate the documented per-track sources in the music-recording repo and `eq/prompts/done/31-tom-presets.md` for real distinctions. If genuinely identical, keep the same EQ curve under two descriptive labels rather than inventing different frequency values. Also preserve legacy Rack Tom recall.
- Reconcile **Kick punch** and any other useful legacy Factory settings with Starting Points. Move unique, usable entries into Starting Points with sensible instrument-purpose names; remove redundancies from the visible menu instead of presenting both names for the same job.
- **Five-zone start** must not be visible as a selectable preset. The already-shipped Frequency Guide offers a more appropriate educational view.
- Remove the **Factory category from the visible picker** in favour of **Starting Points**. Leave **User/Custom Presets** separate and clearly labelled, with saving/editing/deleting user presets working as before.
- Audit all existing defaults against one another by actual band settings and intended use, not just their names. Avoid overaggressive consolidation: `Bright acoustic` may genuinely differ from `Acoustic guitar`; retain a useful distinct variation under the right instrument name if so. Document keep/merge/hide decisions in the PR.

## Critical backwards compatibility
- **DO NOT delete/reorder indexed preset records** in `factoryPresets()`. Existing sessions/host programs can refer to a factory index (`presetFactory` / host program number), and some renamed entries have `formerName`. Remove or hide choices from the menu while retaining their legacy index, content and recall paths. Test old host program indices and old session files explicitly.
- Store menu display labels/group separately from stable preset identity/index if necessary. Preserve unique user saved presets and their references; no destructive migration.
- No DSP changes, audio-thread operations or parameter ID/plugin code changes. No saved-state bump unless demonstrably unavoidable and migrated.

## Acceptance tests
1. No visible Factory section; Starting Points are grouped logically by instrument and clearly named. User Presets unaffected.
2. Drums - Kick, Snare, Mid Tom, High Tom, Floor Tom, Toms Bus and Drum Bus all appear and load. Mid/High Tom sources/behaviour documented without fictional differences.
3. Five-zone start is absent from the menu but an older project referencing its factory index still loads correctly.
4. Every former Factory preset is inventoried: either retained as a distinct Starting Point, consolidated to an existing visible starting point, or hidden as redundant/utility while remaining loadable in old projects. Include a before/after list and rationale.
5. Old sessions and host program-number selection still reproduce the correct EQ curve at each original index. New display order cannot disturb saved references.
6. Tests cover names, group assignments, menu ordering, pick/load, duplicate handling, old indices, preset session round trips and custom preset management.
7. Validate in Windows/macOS builds, with screenshots of the menu and tests in the PR. Clearly state DAW verification status; keep screenshot evidence.

## Delivery instructions
Read `CLAUDE.md`, `docs/style-guide.md`, `eq/Source/PresetManager.h/.cpp`, `eq/Source/PresetBar.cpp`, `eq/Tests/EQTests.cpp`, `eq/docs/starting-points.md`, and `eq/prompts/done/31-tom-presets.md`. Check source tracks before inventing Mid/High Tom differences. Make a focused, backwards-compatible UI/preset-list PR from current main. Test and merge under repo CI rules. Move EQ-40 into `done/` and update docs/changelog only after implementation is merged. No version bump until an agreed release milestone.
