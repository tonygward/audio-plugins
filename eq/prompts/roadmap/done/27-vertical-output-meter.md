You are an experienced C++ audio developer and UI engineer working on **Songsmith EQ** (by Songsmith Studio, formerly TW Pro EQ), a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/Source/OutputMeter.h`, `eq/Source/PluginEditor.h/.cpp` (especially `layoutContent()` and the timer that calls `outputMeter.update`), `eq/Source/EQDisplay.h/.cpp` (`getPlotArea()`, the dB scale labels), `eq/Source/EQLookAndFeel.h` and `eq/Source/EditorSize.h`.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter IDs, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`.
- Older sessions must open at their saved editor size. No `stateVersion` bump should be needed; if one is, add a migration test.
- Don't change DSP.

**Real-time safety**
- Nothing in this task may touch `processBlock` or `Engine::process`. The meter keeps reading the existing lock-free `processor.outputMeter`.

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings, on Windows here and on the macOS CI job.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests in the existing `juce::UnitTest` style.
- Commit before and after screenshots of the standalone app at the default size, the minimum size and the 200% UI size under `eq/docs/screenshots/27/`, and show them in the PR.
- **Do not run `install.ps1`.**

**Finish**
- Tick the checkbox for this task in `eq/ROADMAP.md` and update `README.md` if it describes the meter.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, every choice you made differently from this prompt (and why), test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 27: Vertical output meter on the right

**Branch:** `roadmap/27-vertical-output-meter`

## Why

The output meter is a small horizontal bar at the right end of control row 2 (`outputMeter.setBounds (row.removeFromRight (52))` in `layoutContent()`). It's hard to read at a glance. Tony wants it where most EQs and DAW channels put it: a **tall vertical meter on the right-hand side**, next to the graph.

## Requirements

- Move the output meter out of row 2 into a vertical strip on the right of the graph, running the full height of the plot area (top of the plot to the bottom of the frequency axis, not the control rows).
- Fills bottom to top. Keep the existing ballistics (fast attack, about 24 dB/s fall-off), the 1.5 s peak-hold line, the clip indicator (now at the top, clicking it resets it as today) and the tooltip.
- Draw a dB scale next to it (for example +6, 0, −6, −12, −24, −48 dBFS) in the existing font and colours, and show the held peak as a number above or below the bar. Keep the −60 to +6 dB range unless a different one reads better; say which you chose.
- Colours: keep the current meter colours, with a clear change at 0 dBFS. Must work in the existing theme at every UI size (75%–200%) and stay sharp at 100%, 150% and 200% display scaling.
- Width: about 28–40 px at 100% UI size, scaled with the UI size like everything else. The graph gets narrower by that much; check the minimum window width (760 px) still fits every control in rows 1–3 and the band panel.
- Row 2 gets the freed space back. Keep Auto Gain and Output where they are, right-aligned.
- If the sidechain analyser or a stereo output makes a two-channel (L/R) meter easy with the data the processor already publishes, show L and R side by side; otherwise keep one mono-summed peak bar and say so. Don't add audio-thread work for this.

## Tests

- A unit test for the meter's level-to-height mapping (−60 dB at the bottom, +6 dB at the top, 0 dB where the scale says) and for the clip indicator latching and resetting.
- The editor lays out without overlaps at the minimum size, the default size and the largest size (extend the existing layout test if there is one; otherwise add a simple one that checks component bounds don't intersect).

## Out of scope

- An input meter, loudness (LUFS) metering, or gain-reduction meters for dynamic bands.
