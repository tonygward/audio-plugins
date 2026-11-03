You are an experienced C++ audio developer and UI engineer working on **Songsmith EQ** (by Songsmith Studio, formerly TW Pro EQ), a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- This task runs **after task 24** (plain wording and embedded fonts). If `eq/Resources/Fonts/` doesn't exist on the branch you started from, stop and open a draft PR with a "Blocked: task 24 not merged" section.
- Before changing anything, read `eq/ROADMAP.md`, `eq/Source/EQLookAndFeel.h`, `eq/Source/PluginEditor.h/.cpp` (constructor sizing, `resized()`, `paint()`), `eq/Source/BandPanel.h/.cpp`, `eq/Source/EQDisplay.h/.cpp`, `eq/Source/SpectrumAnalyser.h/.cpp`, `eq/Source/OutputMeter.h`, and in `eq/Source/PluginProcessor.h/.cpp` the `editorWidth` / `editorHeight` atomics and the `UI` child of the saved state.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter IDs, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`.
- Older sessions must open at their saved editor size (clamped to the new limits). New UI settings go in the existing `UI` child of the state with defaults, so no `stateVersion` bump is needed. If one turns out to be needed, bump it and add a migration test.
- Don't change DSP.

**Real-time safety**
- Nothing in this task may touch `processBlock` or `Engine::process`.
- The editor must not get slower: measure `paint()` time for the graph at the default size and at the largest size before and after, and put the numbers in the PR.

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings, on Windows here and on the macOS CI job.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests in the existing `juce::UnitTest` style.
- Commit screenshots of the standalone app at every UI size option and at the minimum and maximum window sizes under `eq/docs/screenshots/25/`, and show them in the PR.
- **Do not run `install.ps1`.**

**Finish**
- Tick the checkbox for this task in `eq/ROADMAP.md` and update `README.md`.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, every choice you made differently from this prompt (and why), the paint timings, test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 25: Higher-resolution, scalable UI

**Branch:** `roadmap/25-ui-resolution`

## Goal

The editor opens small (980 × 580) and everything in it is laid out in fixed pixels: control sizes, stroke widths, node radii and fonts. On a 1440p or 4K screen it looks tiny, and making the window bigger only gives the graph more room while the text and controls stay the same size. Make it look sharp and readable at any size and on any screen.

## What to do

1. **Bigger default and limits.**
   - Default size for new instances: about **1280 × 760**. Pick the exact number so the band panel, top bar and graph have comfortable room with task 24's fonts.
   - Maximum: at least **3840 × 2160**, so the editor can fill a 4K screen. Minimum: whatever task 24 left it at, or larger if the layout needs it.
   - Keep a sensible aspect-ratio range so the graph never gets squashed (for example 1.3 to 2.4). Don't lock it to one ratio.
   - Older sessions keep their saved size, clamped to the new limits.

2. **A UI size setting.** Add **UI size: 75%, 100%, 125%, 150%, 175%, 200%** to the `...` menu (and to the standalone app's menu if it has one). It scales everything together: text, controls, node size, stroke widths and spacing.
   - Implement it once, centrally: lay the editor out in a design-unit coordinate space and apply the scale with a single transform on a root content component (or `AudioProcessorEditor::setScaleFactor` if it behaves in VST3 and AU hosts; test both and say which you chose and why).
   - Changing the size resizes the window to match, keeping the same content layout.
   - Save it in the `UI` child of the state **and** as a per-machine default (a small `juce::PropertiesFile` under the Songsmith Studio app-data folder), so a new instance opens at the size the user last chose. The session value wins over the machine default.
   - Combine it correctly with the host's own display scaling (VST3 content-scale support on Windows, Retina backing scale on macOS). At Windows 150% with UI size 100%, the editor must not be scaled twice or blurred.

3. **Proportional layout.** Within any size, resizing the window should grow the graph first, but the top bar, band panel and fonts should step up in size on large windows rather than staying at their minimum. Replace hard-coded pixel numbers in `resized()` and `paint()` with values derived from one layout scale, kept in one place (for example a `Layout` struct in `EQLookAndFeel.h`).

4. **Sharp drawing at high DPI.**
   - Stroke widths, node radii, hit-test radii and the analyser's line thickness scale with the UI size and stay at least 1 physical pixel.
   - The analyser path and the response curve must have enough points for the physical width: compute them per physical pixel (or per two), not a fixed count, so curves don't look faceted on a 4K screen. Watch the paint time.
   - Align grid lines and meter edges to physical pixels so they don't look blurry.
   - Don't cache anything as a bitmap at 1× and scale it up. If you add a cached image (for the grid, say), create it at the physical resolution and rebuild it when the scale changes.

5. **Mouse feel.** Drag sensitivity (pixels per dB, pixels per octave) and click targets must feel the same at every UI size. Click targets on nodes are at least 24 × 24 design units.

## Tests

- Layout test: for each UI size option at the default window size and at the minimum window size, every child component is inside the editor bounds, no two controls in the top bar overlap, and no label text is truncated (compare the text width in the embedded font with its bounds).
- State test: UI size and window size round-trip through `getStateInformation` / `setStateInformation`; an old state without the new property opens at 100%; an out-of-range saved size is clamped.
- Hit-test test: clicking a node's centre selects it at 75% and at 200%.

## Out of scope

- Changing wording or fonts (task 24 did that).
- OpenGL rendering. Only consider it if the paint timings show the software renderer can't keep up at 4K, and then propose it in the PR rather than adding it.
- New features or a visual redesign.
