You are an experienced C++ DSP engineer working on **TW Pro EQ**, a JUCE VST3/Standalone parametric EQ for Windows. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You are running **unattended overnight**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/Source/EQParams.h`, `eq/Source/EQEngine.h`, `eq/Source/PluginProcessor.h/.cpp` and `eq/Tests/EQTests.cpp`.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove existing parameter IDs, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`.
- New per-band settings go to all 24 bands in the fixed bank. Host parameters must never appear or disappear at runtime.
- If the saved-state layout changes, bump `stateVersion` by one from its value on `main` and migrate older states. Add a test that an older session loads and sounds identical.

**Real-time safety**
- No allocation, locks, logging or file I/O in `processBlock` or `Engine::process`.
- Parameter changes must stay click-free, matching the existing smoothing and cross-fade strategy.
- No NaN, Inf or denormals at any supported sample rate (44.1–192 kHz).

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests for the new behaviour using the existing `juce::UnitTest` style.
- If you touch DSP, run `.\build\EQTests_artefacts\Release\EQTests.exe --bench` on `main` and on your branch, and report both sets of numbers.
- **Do not run `install.ps1`.** It triggers a UAC prompt and will hang with nobody there to answer it.

**Finish**
- Tick the matching checkbox in `eq/ROADMAP.md` and update `README.md` if anything user-facing changed.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover: what changed, design decisions you made alone, test and benchmark results, known limitations, and the line "Not yet verified in a DAW".
- If you get truly stuck, commit what works and open the draft PR anyway with a section titled "Blocked" explaining why. Don't leave work uncommitted.

---

# Task 16: Piano-roll frequency labels and note snapping

**Branch:** `roadmap/16-note-labels`

## Goal
Help users think musically: show the note names for frequencies and optionally snap to them.

## Requirements
- **Note readout:** tooltips and the band panel show the nearest note and cents next to the frequency, e.g. `440 Hz · A4 +0¢`, with A4 = 440 Hz.
- **Optional piano strip:** a thin keyboard strip along the bottom of the graph aligned to the log-frequency axis. Toggle it in the display settings and save it with the UI state. Highlight the key nearest the selected band.
- **Snapping:** holding Ctrl while dragging snaps frequency to the nearest semitone. Include it in the existing undo grouping for drags. Don't conflict with Shift (fine adjust) or the existing modifiers. Check `EQDisplay.cpp` for what's already used and pick a free modifier if Ctrl is taken.
- **Numeric entry:** accept note names in the band panel's frequency field (e.g. `A2`, `C#5`, `Eb3`) as well as Hz/kHz.

## Tests
- Frequency to note+cents and note to frequency round-trip across the full 20 Hz–20 kHz range.
- Note-name parsing handles sharps and flats, octaves -1 to 9, and rejects garbage.
- The snapped frequency equals the exact semitone frequency.
