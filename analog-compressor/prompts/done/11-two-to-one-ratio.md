---
id: ACOMP-11
model: sonnet
effort: medium
dependencies: []
conflicts: []
---

# ACOMP-11 — Add 2:1 ratio to Songsmith Analog Compressor

**Tracking issue:** [ACOMP-11 #199](https://github.com/tonygward/audio-plugins/issues/199)
**Repository:** `tonygward/audio-plugins`
**Status:** Backlog only, not picked up or implemented. This prompt does not queue Matrix.
**Policy:** [Songsmith execution and delivery policy](../../../docs/songsmith-execution-policy.md).

## Objective and user value

Give musicians a gentle **2:1** option alongside 4:1, 8:1, 12:1 and 20:1, especially for acoustic guitar, vocals and bass. This is a **Songsmith extension** to the current 1176-inspired compressor, not a claim that the original 1176 model had a 2:1 button.

## Existing behaviour

`analog-compressor/Source/FETParams.h` currently contains four ratios in order [4:1, 8:1, 12:1, 20:1] with default ratio index 0. `FETEngine.h` has corresponding gain-computer ratio, knee and threshold data. The editor shows the hardware-style ratio buttons, plus a separate All Buttons mode. Hosts and existing presets may have saved ratio indexes, choice normalisation or automation. ACOMP-10 (planned model variants) already mentions 1176AE 2:1. Do not duplicate those model-specific plans or introduce an unnecessary dependency.

## Desired behaviour

Add a fifth selectable **2:1** ratio in the current compressor, with gentle, musically useful behaviour and an unambiguous tooltip. Keep the four historical choices and their exact saved-session and host-automation meanings, retain default 4:1, and preserve All Buttons. A saved project from any prior beta should load with the same ratio and sound as before.

## Implementation guidance

1. Inspect `FETParams.h`, `FETEngine.h`, parameter layout, state migration, editor, ratio button code, preset serialisation, docs and existing tests. Determine how JUCE stores and automates the existing `ratio` choice parameter.
2. **Compatibility is mandatory.** Never insert 2:1 at index 0 or renumber previous choices. Investigate how adding a fifth choice affects host-normalised automation even when existing indices remain unchanged. If that changes legacy automation values, implement an explicit versioned migration or alternative stable representation after reviewing the existing host parameter contract. Document what can and cannot be preserved; don't claim universal host-project migration without evidence. Avoid renaming/removing old parameter IDs.
3. Extend the DSP gain computer with genuinely measured 2:1 behaviour and an appropriately calibrated knee/threshold. Maintain sensible levels, fast real-time performance, existing sidechain Low Cut, oversampling, stereo link, bypass and latency.
4. Add a clearly labelled 2:1 UI choice to the existing ratio group, preserving 4/8/12/20 and All Buttons. Ensure readable spacing at minimum window size and all supported UI scales. Plain-English tooltip example: “Gentle compression that evens out louder parts without squashing the sound.”
5. Preserve existing presets and automation. Add a useful 2:1 setting or example only if it can be tested; avoid inventing a large new preset collection. Update `analog-compressor/README.md`, UI docs and changelog to distinguish Songsmith addition from the reference hardware.
6. Record compatibility design and trade-offs in the PR. Future ACOMP-10 model changes should reuse the 2:1 functionality where appropriate rather than creating a duplicate gain-computer path.

## Acceptance criteria and tests

- A user can select 2:1 in the UI and the plugin applies approximately 2:1 steady-state compression above its designed knee under controlled measured test conditions.
- Fresh and older sessions default/recall the existing 4:1 behaviour; saved sessions and factory/custom presets for 4/8/12/20 and All Buttons remain unchanged.
- Tests explicitly check both stored choice index and *normalised host automation* behaviour for each legacy value; any necessary migration is implemented and independently verified, not merely assumed safe.
- Switching ratios during playback is smooth and stable; Low Cut, All Buttons, meter, bypass and oversampling regression checks pass.
- UI remains readable, operable and accessible at supported sizes; screenshots and listening/DAW review notes are attached.
- Hosted Windows/macOS native CI and exact-head required checks pass, with no DSP performance regression of concern.
- The trusted pipeline automatically merges authorised passing changes and publishes a correctly versioned beta suite installer (Windows EXE/macOS PKG) to private R2 and the protected beta website; download integrity and deployment evidence recorded.
- The original issue receives a start comment, meaningful progress/CI comments when implemented, and a final completion comment before issue close and board Done. A merged PR alone is not completion.

## Dependencies, risks and delivery expectations

No dependency on future ACOMP-10 model variants; coordinate to avoid redundant implementation. Key risk: JUCE AudioParameterChoice index/normalisation changes causing old host automation or saved states to map to another ratio. Do not solve by silently changing IDs/defaults. Follow owner scope and 60% verified weekly Claude gate, single active task, Sonnet default, bounded run, trusted merge/release permissions. Qualifying plugin changes increment only the beta third VERSION component; major/minor deferred. Build and publish only after appropriate tests. Do not claim started merely because the issue/prompt exists.
