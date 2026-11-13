# Roadmap prompts

There is one self-contained prompt per item in [`eq/ROADMAP.md`](../ROADMAP.md). Prompts still to run sit in this folder; finished ones move to [`done/`](done/) once their PR is merged into `main`. Each file starts with the same working rules: branch from `main`, don't break parameter IDs or saved sessions, keep the audio thread real-time safe, build and test, never run `install.ps1`, and finish with a PR (see the [execution policy](../../docs/songsmith-execution-policy.md)). Each file can be handed to an agent as it is.

References use the stable **EQ-##** IDs (for example EQ-27 means prompt 27 in this folder). The ID is preserved in `done/`; the linked PR numbers are separate delivery records. See [suite traceability rules](../../shared/prompts/README.md#prompt-identity-and-traceability).

## Status

| Prompt | Status |
|---|---|
| 01–03 (in [`done/`](done/)) | The prompts that built the plugin before the roadmap: [01 three-band EQ](done/01-three-band-eq.md), [02 interactive EQ curve](done/02-interactive-eq-curve.md), [03 24-band EQ](done/03-24-band-eq.md). The spectrum-analyser prompt that came between 02 and 03 wasn't saved |
| 05–22 (in [`done/`](done/)) | Done, merged into `main` (21 and 22 shipped in v0.3.0) |
| 23–26 (in [`done/`](done/)) | Done (#28–#31), merged into `main` in #32 and shipped in v0.4.0 |
| 27–31 (in [`done/`](done/)) | Done overnight on 3–4 Nov 2026 (#37, #45, #40, #42, #49), merged into `main` in #52 and shipped in v0.5.0: [27 vertical output meter](done/27-vertical-output-meter.md), [28 piano notes toggle](done/28-piano-strip-toggle.md), [29 edge clicks add cuts](done/29-edge-click-cuts.md), [30 Abbey Road reverb template](done/30-abbey-road-reverb-template.md), [31 tom presets](done/31-tom-presets.md) |
| [32 instrument frequency guide](done/32-frequency-guide.md) | **Done** in #89: an optional overlay that names what you hear in each part of the graph, for ten instruments from Acoustic Guitar to Any Instrument. See [docs/frequency-guide.md](../docs/frequency-guide.md). Shipped in 1.3.0 |
| 38–40 (in [`done/`](done/)) | Merged into `main` on 7 Nov 2026 and shipped in 0.5.1: [EQ-38 Frequency Guide instrument icons](done/38-frequency-guide-instrument-icons.md) (#116), [EQ-39 stereo meter and latched clipping warning](done/39-stereo-meter-clip-latch.md) (#117), [EQ-40 organise Starting Points](done/40-organise-starting-points.md) (#118). Not yet verified in a DAW. |
| [37 Draggable band panel and Dynamic visibility](done/37-draggable-band-panel.md) | **Done** in #87: drag the floating band controls by their background; the panel always stays inside the graph and moves just enough when Dynamic opens. Shipped in 1.3.0 |
| [36 Disabled Dynamic tooltip](done/36-dynamic-disabled-tooltip.md) | **Done** in #78: the greyed-out Dynamic button explains why and lists Bell, Low Shelf and High Shelf, including the Linear phase cases. Shipped in 1.2.0 |
| [35 Piano footer placement](done/35-piano-footer.md) | **Done** in #81: Piano button pinned to the far right of the bottom strip beside the piano-note strip, with [SUI-12](../../shared/prompts/done/SUI-12-processing-load-footer.md). Shipped in 1.2.0 |
| [33 filter shape icons](done/33-filter-shape-icons.md) | **Done** in #80: a response picture beside each filter type in the band panel and the right-click menu. Shipped in 1.2.0 |
| [34 Linear Phase tooltip](done/34-linear-phase-tooltip.md) | **Done** in #79: the processing-mode selector explains when to try Linear phase, why it adds delay and why Zero latency suits recording. Shipped in 1.2.0 (UI milestone) |
| [41 Gain Scale label](41-gain-scale-main-toolbox.md) | To do ([#203](https://github.com/tonygward/audio-plugins/issues/203)). The move into the toolbox already shipped with SUI-18 in 0.5.10; what is left is the label and tooltip |
| [42 Clipping indication and peak readout](42-clip-meter-readout.md) | To do ([#232](https://github.com/tonygward/audio-plugins/issues/232)) |

Versions 1.x above are from before the suite was renumbered to 0.5.0 ([VER-01](../../shared/prompts/done/VER-01-standardise-suite-version-0.5.0.md)).

## How these prompts run

Matrix picks prompts up from the project board and each one goes straight into `main` through its own pull request, merged automatically once CI passes. See the [execution policy](../../docs/songsmith-execution-policy.md).

## History: how prompts 05–31 ran in waves

This is how the early roadmap was run, before Matrix. Today Matrix runs one task at a time in board order ([execution policy](../../docs/songsmith-execution-policy.md)); the table is kept as a record and for its conflict notes.

Every prompt branched from the latest `main`. Tasks in the same wave were mostly independent, so they ran in parallel, and each wave was merged before the next.

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

## Conflict hotspots

- `stateVersion` and the migration code in `PluginProcessor.cpp`: any task that adds parameters bumps it.
- The parameter layout in `createParameterLayout()` and the parameter-identity test.
- The top bar in `PluginEditor.cpp`: tasks 05, 08, 13, 14 and 15 all add controls there; 24 and 25 relayout it.
- `PresetManager.cpp` factory list and `StartingPointTests`: task 26 changes values and output gains; 30 and 31 append presets.
- `layoutContent()` in `PluginEditor.cpp`: 27 moves the meter out of row 2 and narrows the graph; 28 adds a button to row 3.

## Morning checklist

1. Read the comments on any card that moved to Blocked, and the "decisions" section of each new PR.
2. Check that the benchmark numbers haven't regressed.
3. Install the latest beta with the suite installer and test in Studio One: save, reopen, automate, run several instances.
