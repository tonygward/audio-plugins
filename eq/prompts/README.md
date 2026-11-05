# Roadmap prompts

There is one self-contained prompt per item in [`eq/ROADMAP.md`](../ROADMAP.md). Prompts still to run sit in this folder; finished ones move to [`done/`](done/) once their PR is merged into `main`. Each file starts with the same working rules: branch from `main`, don't break parameter IDs or saved sessions, keep the audio thread real-time safe, build and test, never run `install.ps1`, and finish with a **draft PR**. Each file can be handed to an agent as it is.

References use the stable **EQ-##** IDs (for example EQ-27 means prompt 27 in this folder). The ID is preserved in `done/`; the linked PR numbers are separate delivery records. See [suite traceability rules](../../shared/prompts/README.md#prompt-identity-and-traceability).

## Status

| Prompt | Status |
|---|---|
| 01–03 (in [`done/`](done/)) | The prompts that built the plugin before the roadmap: [01 three-band EQ](done/01-three-band-eq.md), [02 interactive EQ curve](done/02-interactive-eq-curve.md), [03 24-band EQ](done/03-24-band-eq.md). The spectrum-analyser prompt that came between 02 and 03 wasn't saved |
| 05–22 (in [`done/`](done/)) | Done, merged into `main` (21 and 22 shipped in v0.3.0) |
| 23–26 (in [`done/`](done/)) | Done (#28–#31), merged into `main` in #32 and shipped in v0.4.0 |
| 27–31 (in [`done/`](done/)) | Done overnight on 3–4 Nov 2026 (#37, #45, #40, #42, #49), merged into `main` in #52 and shipped in v0.5.0: [27 vertical output meter](done/27-vertical-output-meter.md), [28 piano notes toggle](done/28-piano-strip-toggle.md), [29 edge clicks add cuts](done/29-edge-click-cuts.md), [30 Abbey Road reverb template](done/30-abbey-road-reverb-template.md), [31 tom presets](done/31-tom-presets.md) |
| [32 instrument frequency guide](done/32-frequency-guide.md) | **Done** in #89: an optional overlay that names what you hear in each part of the graph, for ten instruments from Acoustic Guitar to Any Instrument. See [docs/frequency-guide.md](../docs/frequency-guide.md). Shipped in 1.3.0 |
| [EQ-38 Frequency Guide instrument icons](38-frequency-guide-instrument-icons.md) | **To do — early Priority 2 (Sonnet):** recognisable icons beside all ten instrument names in the Frequency Guide selector, without changing EQ processing or saved sessions. Follow-up to shipped EQ-32. |
| [EQ-39 Stereo meter and latched clipping warning](39-stereo-meter-clip-latch.md) | **To do — early Priority 2 (Sonnet):** mono bar on mono tracks, separate L/R bars on stereo tracks; red CLIP label above meter stays until sustained silence. Follow-up to shipped EQ-27. |
| [37 Draggable band panel and Dynamic visibility](done/37-draggable-band-panel.md) | **Done** in #87: drag the floating band controls by their background; the panel always stays inside the graph and moves just enough when Dynamic opens. Shipped in 1.3.0 |
| [36 Disabled Dynamic tooltip](done/36-dynamic-disabled-tooltip.md) | **Done** in #78: the greyed-out Dynamic button explains why and lists Bell, Low Shelf and High Shelf, including the Linear phase cases. Shipped in 1.2.0 |
| [35 Piano footer placement](done/35-piano-footer.md) | **Done** in #81: Piano button pinned to the far right of the bottom strip beside the piano-note strip, with [SUI-12](../../shared/prompts/done/SUI-12-processing-load-footer.md). Shipped in 1.2.0 |
| [34 Linear Phase tooltip](done/34-linear-phase-tooltip.md) | **Done** in #79: the processing-mode selector explains when to try Linear phase, why it adds delay and why Zero latency suits recording. Shipped in 1.2.0 (UI milestone) |

## Overnight runs merge into `main`

Run unattended with [`shared/prompts/overnight-prompt.md`](../../shared/prompts/overnight-prompt.md), each task goes **straight into `main`** through its own PR: the agent waits for CI, approves the PR and merges it itself once the build, tests and CI all pass, then starts the next task from the updated `main`. There's no integration branch any more. Nothing is pushed to `main` without a PR, and blocked tasks stay as draft PRs. (GitHub doesn't let an account formally approve its own PR, so the agent records its approval as a PR comment listing what it checked, then merges.)

## Run in waves, merging between waves

Every prompt branches from the latest `main`. Tasks in the same wave are mostly independent, so they can run in parallel. Review and merge a wave before starting the next, so later tasks build on the earlier work.

| Wave | Prompts | Notes |
|---|---|---|
| 1 | 05 output gain/bypass · 06 cut slopes · 07 band solo · 09 CI · 16 note labels | 05 and 06 both bump `stateVersion`, so expect a small merge conflict. |
| 2 | 08 presets · 10 AU/macOS · 11 stereo modes · 14 auto gain · 15 scale curve | 08 should come after 05/06 so presets include their parameters. 10 reuses 09's workflow. 14 sits before 05's output gain. |
| 3 | 12 sidechain analyser · 13 analyser controls | Both change `SpectrumAnalyser`. Run them one after the other, or expect conflicts. |
| 4 | 17 dynamic EQ | Large. Can use 12's sidechain bus. |
| 5 | 18 linear phase, **then** 19 oversampling | Both change latency reporting. Run them one at a time. |
| 6 | 20 starting-point templates | |
| 7 | 21 rename to Songsmith EQ, **then** 22 Windows installer | One at a time: 22 uses the names, paths and presets folder that 21 introduces. |
| 8 | 23 macOS installer · 24 plain wording and fonts · 26 starting-points review | Independent of each other. 23 is built and tested on the macOS CI job, since the dev machine is Windows. 24 and 26 both touch `README.md` and preset hints; expect a small conflict. |
| 9 | 25 higher-resolution, scalable UI | Needs 24's embedded font and final label sizes. |
| 10 | 27 vertical output meter · 29 edge clicks add cuts · 30 Abbey Road reverb | Independent of each other. |
| 11 | 28 piano notes toggle · 31 tom presets | 28 relayouts row 3 after 27 changed rows 2 and the graph width; 31 adds presets after 30's, so both avoid conflicts by waiting a wave. |
| [33 filter shape icons](done/33-filter-shape-icons.md) | **Done** in #80: a response picture beside each filter type in the band panel and the right-click menu. Shipped in 1.2.0 |

## Conflict hotspots

- `stateVersion` and the migration code in `PluginProcessor.cpp`: any task that adds parameters bumps it.
- The parameter layout in `createParameterLayout()` and the parameter-identity test.
- The top bar in `PluginEditor.cpp`: tasks 05, 08, 13, 14 and 15 all add controls there; 24 and 25 relayout it.
- `PresetManager.cpp` factory list and `StartingPointTests`: task 26 changes values and output gains; 30 and 31 append presets.
- `layoutContent()` in `PluginEditor.cpp`: 27 moves the meter out of row 2 and narrows the graph; 28 adds a button to row 3.

## Morning checklist

1. `gh pr list --draft`: read each PR's "decisions" and "Blocked" sections.
2. Check that the benchmark numbers haven't regressed.
3. Build the PR locally, run `install.ps1`, and test it in Studio One: save, reopen, automate, run several instances.
4. Merge, then start the next wave.
