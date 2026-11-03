# Roadmap prompts

There is one self-contained prompt per item in [`../../ROADMAP.md`](../../ROADMAP.md). Prompts still to run sit in this folder; finished ones move to [`done/`](done/) once their PR is merged into `main`. Each file starts with the same working rules: branch from `main`, don't break parameter IDs or saved sessions, keep the audio thread real-time safe, build and test, never run `install.ps1`, and finish with a **draft PR**. Each file can be handed to an agent as it is.

## Status

| Prompt | Status |
|---|---|
| 05–20 (in [`done/`](done/)) | Done, merged into `main` |
| [21 rename to Songsmith EQ](21-rename-songsmith.md) | Done in PR (roadmap/21-rename-songsmith), not yet merged |
| [22 Windows installer](22-windows-installer.md) | To do, after 21 |

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

## Conflict hotspots

- `stateVersion` and the migration code in `PluginProcessor.cpp`: any task that adds parameters bumps it.
- The parameter layout in `createParameterLayout()` and the parameter-identity test.
- The top bar in `PluginEditor.cpp`: tasks 05, 08, 13, 14 and 15 all add controls there.

## Morning checklist

1. `gh pr list --draft`: read each PR's "decisions" and "Blocked" sections.
2. Check that the benchmark numbers haven't regressed.
3. Build the PR locally, run `install.ps1`, and test it in Studio One: save, reopen, automate, run several instances.
4. Merge, then start the next wave.
