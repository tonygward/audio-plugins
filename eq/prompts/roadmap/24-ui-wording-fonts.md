You are an experienced C++ audio developer and UI designer working on **Songsmith EQ** (by Songsmith Studio, formerly TW Pro EQ), a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/Source/EQLookAndFeel.h`, `eq/Source/PluginEditor.h/.cpp`, `eq/Source/BandPanel.h/.cpp`, `eq/Source/EQDisplay.h/.cpp`, `eq/Source/PresetBar.cpp`, `eq/Source/OutputMeter.h`, `eq/Source/PluginProcessor.cpp` (`createParameterLayout()`), `eq/CMakeLists.txt` and the feature list in `README.md`.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter **IDs**, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`. Changing a parameter's **display name** is allowed (hosts store parameters by ID), but list every display-name change in the PR.
- Don't change DSP, parameter ranges, `stateVersion` or the saved-state layout.
- Keep every existing keyboard and mouse gesture working.

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings, on Windows here and on the macOS CI job.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests in the existing `juce::UnitTest` style.
- Take before and after screenshots of the standalone app (`build/SongsmithEQ_artefacts/Release/Standalone/Songsmith EQ.exe`) at the default size and at the minimum size, with a dynamic band, a Mid band and the band panel open. Commit them under `eq/docs/screenshots/24/` and show them in the PR.
- **Do not run `install.ps1`.**

**Finish**
- Tick the checkbox for this task in `eq/ROADMAP.md` and update `README.md` so it uses the new wording.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, the full old → new wording table, every choice you made differently from this prompt (and why), test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 24: Plain wording and better fonts

**Branch:** `roadmap/24-ui-wording-fonts`

## Goal

Someone who has never used a pro EQ should be able to read every control. Today the UI is full of shorthand (`SC`, `AG`, `OS 2x`, `THRESH`, single-letter node badges) and uses 8.5–11 px text in a font that only exists on Windows. Fix both.

## 1. No abbreviations

**Rule:** every word on screen is spelled out. The only things that stay short are units (`Hz`, `kHz`, `dB`, `dB/oct`, `dBFS`, `ms`, `%`), note names (`A4`, `C#5`), key names in tooltips (`Ctrl`, `Alt`, `Cmd`) and `Q`, which is the name of the control, not an abbreviation. Give `Q` a tooltip that says "Q: how narrow the band is. Higher is narrower."

The user's own example: **`SC` must read `Side Chain`**. Use the two-word spelling "Side Chain" everywhere it appears (buttons, menus, tooltips, parameter display names, README), so the plugin is consistent with itself.

Known cases (search for more, including tooltips, popup menus, undo descriptions and text drawn directly in `paint()`):

| Where | Now | Change to |
|---|---|---|
| `BandPanel.h` dynamic button | `Dyn` | `Dynamic` |
| `BandPanel.h` external detector button | `SC` | `Side Chain` |
| `BandPanel.cpp` knob labels | `FREQ`, `GAIN`, `THRESH`, `RANGE`, `ATTACK`, `RELEASE` | `Frequency`, `Gain`, `Threshold`, `Range`, `Attack`, `Release` (title case, not capitals) |
| `BandPanel.cpp` title, power button | `BAND 3`, `ON` / `OFF` | `Band 3`, `On` / `Off` |
| `PluginEditor.h/.cpp` auto-gain button | `AG`, `AG +1.2` | `Auto Gain`, `Auto Gain +1.2 dB` |
| `PluginEditor.cpp` oversampling box | `OS off`, `OS 2x`, `OS 4x` | `Oversampling: off`, `Oversampling: 2×`, `Oversampling: 4×` |
| `PluginEditor.cpp` output slider | `Out +0.0 dB` (and the odd `trimCharactersAtStart ("Ou t")` parser) | `Output +0.0 dB`, with a parser that accepts both `Output` and the bare number |
| `PluginEditor.cpp` analyser menu | `2048-point FFT` | `Resolution: 2048 points` (or Low/Medium/High with the point count in brackets) |
| `PluginEditor.cpp` side-chain box | `Sidechain: spectrum`, `Sidechain: + overlap` | `Side Chain: spectrum`, `Side Chain: spectrum and overlap` |
| `EQDisplay.cpp` dynamic marker | `D` next to the node | the word `Dynamic` in a small pill beside the node, or drop it from the node and show it in the hover readout. No single letters |
| `EQDisplay.cpp` stereo badge | `S`/`L`/`R`/`M` letters | `Left`, `Right`, `Mid`, `Side` in a small pill, or in the hover readout only. No single letters |
| `EQDisplay.cpp` axis labels | `1k`, `10k` | `1 kHz`, `10 kHz` (keep `100`, `1 kHz`, `10 kHz`-style labels readable at the minimum width; drop labels before abbreviating them) |
| `EQDisplay.cpp` solo banner | `SOLO  Band 3` | `Solo: Band 3` |
| `PluginProcessor.cpp` parameter display names | `Band 1 Freq`, `Band 1 On`, `Band 1 Dyn Sidechain` | `Band 1 Frequency`, `Band 1 Enabled`, `Band 1 Dynamic Side Chain` |

If a spelled-out label doesn't fit, change the layout (wider control, two rows, or move it into the band panel), don't shorten the word. If something genuinely can't fit at the minimum editor size, raise the minimum size and say so in the PR.

Add a test that walks the editor's component tree (buttons, labels, combo-box items, slider text) and fails if it finds any of the banned tokens: `SC`, `AG`, `OS`, `Dyn`, `FREQ`, `THRESH`, `Out `, `Sidechain`, or a standalone single capital letter other than `Q`. This keeps abbreviations from creeping back.

## 2. Better fonts

Today `EQColours::font()` hard-codes **Segoe UI**, which doesn't exist on macOS, so Mac users get a fallback font. Sizes go down to 8.5 px.

- **Embed one typeface** in the plugin with `juce_add_binary_data` so Windows and macOS look the same. Use **Inter** (SIL Open Font License 1.1): Regular, Medium and SemiBold are enough. Check the font files and licence into `eq/Resources/Fonts/` with the `OFL.txt` licence next to them, and add a "Fonts" credit to `README.md`. If you find a clear reason to pick a different open-licence UI font, say why in the PR.
- Use tabular (fixed-width) figures for numbers that change while you drag (frequency, gain, Q, meter readouts), so they don't jitter. Inter supports this; if JUCE can't switch the feature on, pick the variant or approach that gives steady digits.
- Load the typeface once (a shared `juce::Typeface::Ptr`) and serve it from `EQLookAndFeel` (`getTypefaceForFont`, label, button, combo-box and popup-menu fonts). Keep `EQColours::font (height, bold)` as the single entry point so the rest of the code doesn't change much.
- **Minimum sizes** at the default editor size: 12 px for any text, 13 px for control labels and values, 14–15 px for the band title and top-bar controls. The graph's grid labels can be 11 px if they're lighter weight, but no smaller. Replace every `font (8.5f…)`, `font (9.0f…)`, `font (10.0f…)` call.
- Use weight, not capitals, for hierarchy: SemiBold for titles, Medium for labels, Regular for values and hints.
- Check contrast: label and value text must meet WCAG AA (4.5:1) against its background. Note any colour you change.
- Text must stay crisp at Windows display scaling of 100%, 150% and 200% and on a Retina Mac (CI screenshot is enough for the Mac).

## Out of scope

- Changing the editor's default size, adding a UI scale setting or making the layout proportional. That's task 25, which runs after this one.
- New features, colour-scheme redesigns, or icon artwork.
- Changing preset names.
