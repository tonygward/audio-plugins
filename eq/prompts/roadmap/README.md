# Roadmap prompts

There is one self-contained prompt per item in [`../../ROADMAP.md`](../../ROADMAP.md). Prompts still to run sit in this folder; finished ones move to [`done/`](done/) once their PR is merged into `main`. Each file starts with the same working rules: branch from `main`, don't break parameter IDs or saved sessions, keep the audio thread real-time safe, build and test, never run `install.ps1`, and finish with a **draft PR**. Each file can be handed to an agent as it is.

## Status

| Prompt | Status |
|---|---|
| 01–03 (in [`done/`](done/)) | The prompts that built the plugin before the roadmap: [01 three-band EQ](done/01-three-band-eq.md), [02 interactive EQ curve](done/02-interactive-eq-curve.md), [03 24-band EQ](done/03-24-band-eq.md). The spectrum-analyser prompt that came between 02 and 03 wasn't saved |
| 05–22 (in [`done/`](done/)) | Done, merged into `main` (21 and 22 shipped in v0.3.0) |
| 23–26 (in [`done/`](done/)) | Done (#28–#31), merged into `main` in #32 and shipped in v0.4.0 |
| 27–31 (in [`done/`](done/)) | Done overnight on 3–4 Nov 2026 (#37, #45, #40, #42, #49), merged into `main` in #52 and shipped in v0.5.0: [27 vertical output meter](done/27-vertical-output-meter.md), [28 piano notes toggle](done/28-piano-strip-toggle.md), [29 edge clicks add cuts](done/29-edge-click-cuts.md), [30 Abbey Road reverb template](done/30-abbey-road-reverb-template.md), [31 tom presets](done/31-tom-presets.md) |

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
