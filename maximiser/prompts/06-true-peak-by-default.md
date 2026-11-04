You are an experienced C++ audio developer working on **Songsmith Maximiser**, a JUCE VST3/AU/Standalone plugin in `C:/dev/audio-plugins/maximiser`. JUCE is at `C:/dev/JUCE`. Follow the repo's `CLAUDE.md` (design principles and working rules) throughout.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

# Task 06: True peak by default

**Branch:** `maximiser/06-true-peak-by-default`

## Why

A new instance starts in **Sample peak** mode. A maximiser is usually the last plugin before export, and streaming services and broadcast specs measure **true peak** (dBTP). In sample peak mode, peaks between samples get through the ceiling (the tests measure more than 1 dB over in some cases). That is the safe choice for the job the plugin is for, so Tony wants **True peak** to be the default. The extra processing power is the right trade-off for a plugin that normally runs once, on the master.

## Read first

- `maximiser/Source/MaxParams.h` (`peakModeId`), `PluginProcessor.h/.cpp` (parameter layout, `stateVersion`, `restoreFromValueTree`, latency), `BrickwallLimiter.h`, `StartingPoints.h`.
- `maximiser/README.md`, `maximiser/docs/`.
- `maximiser/Tests/MaximiserTests.cpp`, especially "Version-1 sessions load as sample peak, manual release".

## Requirements

- Make **True peak** (index 1) the default of `peak_mode`. Don't rename the ID, the choices or their order. Update the comment in `MaxParams.h`.
- **Existing sessions must sound the same.** Version 2 and 3 sessions store `peak_mode`, so they keep what they saved: check that this is true. Version 1 sessions don't store it, and today they rely on the default being sample peak. Make `restoreFromValueTree` set **Sample peak** explicitly when a state has no `peak_mode` (a migration for older states, not a default), and update the comment there.
- Adding no parameters, so no `stateVersion` bump. If you decide you need one, add the migration and a test.
- Latency: check whether it changes between modes. As far as `BrickwallLimiter::getLatencySamples()` shows, it doesn't. If it does, the default latency must be reported to the host correctly.
- The starting points set `peakMode` themselves: leave them as they are ("Mix bus" stays sample peak on purpose).
- The editor shows True peak on a fresh instance. Check that its tooltip still reads well now that it is the default, and update the README and docs where they mention the default.

## Tests

- A fresh processor is in true peak mode, and a 0 dBFS intersample-peak test signal stays under the ceiling, measured with the true-peak meter.
- Change the version 1 test so it expects **sample peak** because of the migration, not because of the default (same assertion, new message).
- A version 3 state saved with sample peak still loads as sample peak.
- Update every test that assumed the old default. Every other test keeps passing.

## Finish

- Build in Release, run `MaximiserTests.exe`, with no new warnings.
- Before and after screenshots of the editor (`--screenshot`, `--scale 2`), committed and linked by SHA in the PR, then deleted before merging (see `CLAUDE.md`).
- Open the PR with what changed, test output, and "Not yet verified in a DAW". Move this prompt to `done/` and update `maximiser/prompts/README.md` in the same PR.
