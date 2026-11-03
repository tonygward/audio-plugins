You are an experienced C++ audio developer and UI engineer working on **Songsmith EQ** (by Songsmith Studio, formerly TW Pro EQ), a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/prompts/roadmap/done/16-note-labels.md` (the task that added the piano strip), `eq/Source/PluginEditor.h/.cpp` (`layoutContent()`, row 3, and how `analyserSettingsButton` and the combo boxes are built), `eq/Source/EQDisplay.cpp` (`drawPianoStrip`, `showDisplayMenu`) and, in `eq/Source/PluginProcessor.h/.cpp`, `showPianoStrip` and how it's saved in the `UI` child of the state.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter IDs, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`.
- `showPianoStrip` is already saved with the UI state. Keep that; sessions saved with it on must still open with it on.
- Don't change DSP.

**Real-time safety**
- Nothing in this task may touch `processBlock` or `Engine::process`.

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests in the existing `juce::UnitTest` style.
- Commit before and after screenshots of the standalone app at the default and minimum sizes, with the strip on and off, under `eq/docs/screenshots/28/`, and show them in the PR.
- **Do not run `install.ps1`.**

**Finish**
- Tick the checkbox for this task in `eq/ROADMAP.md` and update `README.md` (it should say how to turn the piano strip on).
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, every choice you made differently from this prompt (and why), test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 28: A piano notes toggle in the bottom control bar

**Branch:** `roadmap/28-piano-strip-toggle`

## Why

Task 16 added a piano keyboard strip along the bottom of the graph, but the only way to turn it on is a right-click menu on the graph ("Show piano strip"). Tony didn't know it was there. He wants it as a visible item in the bottom control bar, row 3: *display range | analyser | settings | curve view | side chain ...... hint*.

## Requirements

- Add a **"Piano"** toggle button to row 3, after the side chain box, in the same style as the other toggle buttons (for example Auto Gain or Bypass). If "Piano" reads badly next to the other labels, "Notes" or "Piano Keys" are fine; follow task 24's rule of no abbreviations and say what you chose.
- It turns `showPianoStrip` on and off, saves with the UI state, and stays in sync with the right-click menu item, which stays as it is. Changing it from either place updates the other.
- Tooltip: what the strip shows (note names on the log-frequency axis, the key nearest the selected band highlighted) and a reminder that Ctrl-drag snaps to semitones (check the modifier task 16 actually used).
- Row 3 must still fit at the minimum window width (760 px) and at every UI size. The hint label already hides when there's no room; it should give way before the new button does.
- Turning the strip on or off doesn't move or resize the graph unexpectedly. If the strip takes height from the plot, the dB scale and nodes must still line up afterwards.
- Default stays **off** for new instances unless the README or task 16 says otherwise.

## Tests

- Toggling the button changes `showPianoStrip`, and the state round-trips through `getStateInformation` / `setStateInformation`.
- Row 3 has no overlapping bounds at the minimum size with the new button visible.

## Out of scope

- Changing how the strip is drawn, or adding a MIDI keyboard.
