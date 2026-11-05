**Recommended model:** Sonnet.  
**Estimated effort / usage:** Low (relative, not a token estimate).  
**Model escalation:** None expected; escalate only for verified DSP or compatibility risk. Never switch to Opus silently: record why, scope its use to the difficult design/validation step, then return to Sonnet.  

You are an experienced C++ audio developer working on **Songsmith Analog Compressor**, a JUCE VST3/AU/Standalone plugin in `C:/dev/audio-plugins/analog-compressor`. JUCE is at `C:/dev/JUCE`. Follow the repo's `CLAUDE.md` (design principles and working rules) throughout.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

# Task 06: Oversampling off by default

**Branch:** `compressor/06-oversampling-off-by-default`

## Why

A new instance of the compressor starts with **Oversampling: 4x**. Step 3 chose that without a human (`analog-compressor/docs/colour.md`, "Oversampling (evaluated)"): 4x cuts compression aliasing by 8-38 dB, at about 2.4 % of a core per stereo instance and 6 samples of latency.

Tony wants it **off by default**. A compressor gets used on many tracks at once, and on tracks being recorded. Zero latency and low processing power matter more there than aliasing, which is hard to hear at normal settings. People who want the cleaner top end can turn it on. Only keep 4x as the default if you find a concrete problem with Off (for example a test that shows clearly audible aliasing at the factory presets' settings). If you do, don't change the default: open the PR with the evidence instead.

## Read first

- `analog-compressor/Source/FETParams.h` (`oversamplingDefault`), `PluginProcessor.h/.cpp` (parameter layout, `stateVersion`, `setStateInformation`, latency reporting).
- `analog-compressor/docs/colour.md` "Oversampling (evaluated)", `analog-compressor/README.md`.
- The tests in `analog-compressor/Tests/`, especially the parameter table in `AnalogCompressorTests.cpp` (`{ "oversampling", 0.0f, 2.0f, 2.0f, 2 }`).

## Requirements

- Set `oversamplingDefault` to Off (index 0). Don't rename the parameter ID, its choices or their order.
- **Existing sessions must sound the same.** A session saved with state version 2 already stores its oversampling setting, so it keeps 4x. Check that this is true, rather than assuming it: if `setStateInformation` ever falls back to the default for a stored parameter, fix that.
- State version 1 sessions (from before oversampling existed) load with the default. That now means Off, which matches how version 1 sounded. Update the comment in `PluginProcessor.h` to say so.
- Adding no parameters, so no `stateVersion` bump. If you decide you need one, add the migration and a test.
- A fresh instance reports **0 samples** of latency to the host.
- The editor's oversampling menu shows Off on a fresh instance. Its tooltip explains, in one plain sentence, when to turn it up ("Runs the compressor at a higher rate inside the plugin, for a cleaner top end when compressing hard. Uses more processing power and adds a little delay.", or better).
- Factory presets don't touch oversampling. Keep it that way.
- Update `docs/colour.md` (the default and why, keeping the measurements) and `analog-compressor/README.md`.

## Tests

- Default value: a fresh processor has oversampling Off and reports 0 samples of latency.
- A version 2 state saved with 4x loads as 4x and reports 4x latency after `prepareToPlay`.
- A version 1 state (no oversampling property) loads as Off.
- Update the parameter table test for the new default. Every other test keeps passing.

## Finish

- Build in Release, run `AnalogCompressorTests.exe`, with no new warnings.
- Before and after screenshots of the editor (`--screenshot`, `--scale 2`), committed and linked by SHA in the PR, then deleted before merging (see `CLAUDE.md`).
- Open the PR with what changed, test output, and "Not yet verified in a DAW". Move this prompt to `done/` and update `analog-compressor/prompts/README.md` in the same PR.
