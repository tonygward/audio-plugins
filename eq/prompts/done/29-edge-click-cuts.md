You are an experienced C++ audio developer and UI engineer working on **Songsmith EQ** (by Songsmith Studio, formerly TW Pro EQ), a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/Source/EQDisplay.h/.cpp` (`mouseDoubleClick`, `xToFreq`, `getPlotArea`, `hitTestNode`, and any other place a band gets added, such as a click on empty space or a menu), `eq/Source/EQEditController.h/.cpp` (`addBand` and its undo grouping), `eq/Source/EQParams.h` (`FilterType`, slope defaults, frequency range) and the band-editing tests in `eq/Tests/EQTests.cpp`.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter IDs, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`.
- No saved-state change is expected. If one is needed, bump `stateVersion` and add a migration test.

**Real-time safety**
- Nothing in this task may touch `processBlock` or `Engine::process`. Adding a band must stay click-free, as it is today.

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests in the existing `juce::UnitTest` style.
- **Do not run `install.ps1`.**

**Finish**
- Tick the checkbox for this task in `eq/ROADMAP.md` and update `README.md` where it explains adding bands.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, the edge zones you chose (and why), test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 29: Clicking at the far left adds a Low Cut, at the far right a High Cut

**Branch:** `roadmap/29-edge-click-cuts`

## Why

Double-clicking empty space in the graph always adds a **Bell** (`processor.edits.addBand (..., EQ::FilterType::Bell)` in `EQDisplay::mouseDoubleClick`). Nobody puts a bell at 25 Hz or 18 kHz: when Tony clicks near the left edge he wants a **Low Cut**, and near the right edge a **High Cut**. Pro-Q works the same way.

## Requirements

- When a band is added from the graph:
  - in the **left edge zone**, it's a **Low Cut**;
  - in the **right edge zone**, it's a **High Cut**;
  - anywhere else, a Bell, as today.
- Choose the zones by frequency, not pixels, so they behave the same at every window and UI size. Suggested starting points: below about **40 Hz** for Low Cut, above about **12 kHz** for High Cut. Try them at the default display range and adjust if they feel wrong, and put the final numbers as named constants in one place.
- For a cut, ignore the click's vertical position: gain stays at 0 dB and the node sits on the 0 dB line. Use the plugin's default cut slope and Q (check `EQParams.h`; don't invent new defaults).
- The frequency is where you clicked, clamped to the parameter range.
- If a Low Cut already exists below the click, still add the new one (people stack cuts), but don't silently change existing bands.
- Holding a modifier while double-clicking forces a Bell anywhere, for the rare time someone wants one at the edge. Pick a modifier that's free in `EQDisplay.cpp` (Shift is fine adjust, Alt is solo, Ctrl may be note snap: check) and mention it in the tooltip or hint text.
- Undo removes the new band in one step, like adding a Bell does today.
- If there's any other way to add a band from the graph (single click on empty space, a context menu "Add band here"), apply the same rule there.

## Tests

- Adding a band at 25 Hz gives a Low Cut with 0 dB gain; at 16 kHz a High Cut; at 1 kHz a Bell with the clicked gain.
- The zone boundaries: just inside and just outside each one.
- The modifier forces a Bell at 25 Hz.
- Undo after an edge add restores the previous state.

## Out of scope

- Changing the type of existing bands, or auto-adding cuts when a preset loads.
