You are an experienced C++ audio developer and UI writer working on the **Songsmith Studio** plugins, a set of JUCE VST3/AU/Standalone plugins in one repo: `C:/dev/audio-plugins`. Songsmith EQ is in `eq/`, Songsmith Analog Compressor in `fet/`, Songsmith Maximiser in `maximizer/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- `README.md`, `prompts/suite/README.md`, `docs/style-guide.md` and the `shared/` library added by task S1.
- `eq/prompts/roadmap/done/24-*.md`: the EQ task that set the "no abbreviations" rule, and the banned-token test it added in `eq/Tests/EQTests.cpp`.
- Every editor source file in `eq/Source`, `fet/Source` and `maximizer/Source`, every `createParameterLayout()`, the preset and starting-point lists (`eq/Source/PresetManager.cpp`, `fet/Source/FETPresets.cpp`, `maximizer/Source/StartingPoints.h`), and each plugin's `README.md`.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter **IDs**, and never change any plugin's `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, bundle ID or **product name**. Changing a parameter's **display name** is allowed (hosts store parameters by ID), but list every display-name change in the PR.
- Renaming a factory preset is allowed only if saved sessions and user presets still find it (check how each preset manager matches presets). If they wouldn't, keep the old name.
- Don't change DSP, parameter ranges, `stateVersion` or saved-state layouts.
- Each plugin must still configure and build on its own from its own folder.

**Real-time safety**
- Nothing in this task may touch `processBlock` or any engine's process code.

**Build and test (every plugin)**
- For each of `eq`, `fet` and `maximizer`: `cmake --build <plugin>/build --config Release`. If a `build/` folder is missing, first run `cmake -S <plugin> -B <plugin>/build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- Every build must succeed with no new warnings, here and on the macOS CI job.
- Run every test executable (`EQTests`, `FETTests`, `MaximizerTests`) and make sure all tests pass. Add tests in the existing `juce::UnitTest` style.
- Render before and after screenshots of every editor with each test executable's screenshot option, at the default and minimum sizes. Commit them under `docs/screenshots/S3/` and show them in the PR.
- **Do not run any `install.ps1`, installer or system-wide install.**

**Finish**
- Update `docs/style-guide.md` and every README so they use the new wording.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, the full old → new wording table for every plugin, every choice you made differently from this prompt (and why), test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task S3: Plain words in every plugin

**Branch:** `suite/S3-plain-words`

## Why

Task 24 made the EQ readable by someone who has never used a pro EQ: every word spelled out, title case instead of capitals, plain tooltips. The compressor and the maximiser were built afterwards and copied hardware shorthand back in (`GR`, `In`, `Out`, `Atten`, `Out Ceiling`). Tony's rule applies to **every** plugin: **no abbreviations. Clarity and simple language go a long way.**

## 1. The rule, written once

Add a "Wording" section to `docs/style-guide.md`. Start from task 24's rule and make it the rule for the whole suite:

- Every word on screen is spelled out: labels, buttons, menu items, combo-box items, tooltips, hints, undo descriptions, text drawn in `paint()`, parameter display names that hosts show, and preset names.
- The only short forms allowed: units (`Hz`, `kHz`, `dB`, `dB/oct`, `dBFS`, `dBTP`, `LUFS`, `ms`, `s`, `%`, `×`), note names (`A4`, `C#5`), key names in tooltips (`Ctrl`, `Alt`, `Cmd`, `Shift`), `Q` (the name of the EQ control, with its tooltip), and format names in installers and READMEs (`VST3`, `AU`).
- **Plain language:** say what a control does, not what the hardware called it. Use everyday words where they exist ("how fast it reacts" over "attack time constant"); when a technical term is the right name (Threshold, Ratio, Dither), keep it and explain it in the tooltip in one short sentence.
- Title case for control names (`Output Ceiling`), sentence case for tooltips and hints. No capitals for emphasis.
- One word for one thing across the suite. If the EQ says "Side Chain", the compressor doesn't say "Sidechain". If one plugin says "Output", none says "Out". Build a small glossary in the style guide (Input, Output, Gain Reduction, Bypass, Side Chain, Oversampling, Threshold, Ceiling, Release, Attack, Interface size, ...) and use it.
- If a spelled-out label doesn't fit, change the layout (wider control, two lines, a caption above), don't shorten the word.
- British spelling in UI text and READMEs, as the EQ already uses ("analyser", "colour"). Don't rename products or bundles to fix spelling (see below).

## 2. Fix every plugin

Search all three `Source/` folders for string literals, and check them against the rule. Known cases:

| Plugin | Now | Change to |
|---|---|---|
| Compressor | `GR -3.2 dB` under the needle | `Gain Reduction 3.2 dB` (or a caption "Gain Reduction" above the value) |
| Compressor | `GR` meter mode | `Gain Reduction` |
| Compressor | `All` ratio button (if present) | `All Buttons` |
| Maximiser | `In`, `Out` meters | `Input`, `Output` |
| Maximiser | `Atten` meter | `Gain Reduction` (match the compressor) |
| Maximiser | `Out Ceiling` | `Output Ceiling` (the README can say it matches the L1's "Out Ceiling") |
| Maximiser | `C−T` or similar shorthand for the linked gap, if shown | spell it out, for example "Ceiling minus Threshold" or "Loudness gain" |
| Maximiser | `TPDF` or bit-depth shorthand in the dither control | `Dither: 16-bit`, `Dither: 24-bit`, `Dither: off`, with "TPDF" only explained in the tooltip if at all |
| EQ | anything that crept back since task 24 | spelled out |

These are only the ones already spotted. Read every string; tooltips and parameter display names are where abbreviations hide. Read every tooltip aloud: if it needs audio-engineering knowledge to understand, rewrite it.

Don't rename products. "Songsmith Analog Compressor" vs British "analogue" and "Maximiser" vs the `maximizer/` folder are known inconsistencies; list them in the PR under "Questions for Tony" instead of changing them, because product names are part of the plugin identity hosts and installers use.

## 3. One test for every plugin

- Move the EQ's banned-token check into the shared library from task S1 (for example `shared/Source/WordingCheck.h`), so every plugin's tests use the same walker and the same rules.
- It walks an editor's component tree (buttons, labels, combo-box items, slider text, tooltips, popup-menu items where they can be built without showing them) and also checks every parameter's display name. It fails on any banned token and on any standalone capital letter other than `Q`.
- The banned list is shared and covers at least: `SC`, `AG`, `OS`, `GR`, `Dyn`, `FREQ`, `THRESH`, `Atten`, `Sidechain`, `In`, `Out` (as whole words), `TPDF`, plus every abbreviation you replaced in this task.
- Each plugin's test executable runs it against its own editor at the default and minimum sizes.

## Tests

- The shared wording test passes for all three editors and all parameter display names.
- State round-trip and preset-loading tests still pass (display-name changes must not affect them).
- Every existing test still passes.

## Out of scope

- Changing fonts or sizes (task S1) or how controls are drawn (task S2).
- Renaming products, bundles, folders or parameter IDs.
