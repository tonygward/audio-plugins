You are an experienced C++ audio developer and UI engineer working on the **Songsmith Studio** plugins, a set of JUCE VST3/AU/Standalone plugins in one repo: `C:/dev/audio-plugins`. Songsmith EQ is in `eq/`, Songsmith Analog Compressor in `fet/`, Songsmith Maximiser in `maximizer/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- `README.md`, `prompts/suite/README.md`, `eq/prompts/roadmap/done/24-*.md` (the task that set the EQ's wording and font rules) and `eq/prompts/roadmap/done/25-*.md` (scalable UI).
- Each plugin's `CMakeLists.txt`, `README.md` and look-and-feel: `eq/Source/EQLookAndFeel.h`, `fet/Source/FETLookAndFeel.h`, `maximizer/Source/MaxLookAndFeel.h`, and every editor's `paint()` code where text is drawn directly.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter IDs, and never change any plugin's `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, bundle ID or product name.
- Don't change DSP, parameter ranges, `stateVersion` or saved-state layouts. Sessions and presets from every released version must still load.
- Each plugin must still configure and build **on its own** from its own folder (`cmake -S eq -B eq/build`, and the same for `fet` and `maximizer`), because CI and the READMEs work that way.

**Real-time safety**
- Nothing in this task may touch `processBlock` or any engine's process code.

**Build and test (every plugin)**
- For each of `eq`, `fet` and `maximizer`: `cmake --build <plugin>/build --config Release`. If a `build/` folder is missing, first run `cmake -S <plugin> -B <plugin>/build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- Every build must succeed with no new warnings, here and on the macOS CI job.
- Run every test executable and make sure all tests pass: `eq/build/EQTests_artefacts/Release/EQTests.exe`, `fet/build/FETTests_artefacts/Release/FETTests.exe`, `maximizer/build/MaximizerTests_artefacts/Release/MaximizerTests.exe`. Add tests in the existing `juce::UnitTest` style.
- Render before and after screenshots of every editor with each test executable's screenshot option (`--screenshots <folder>` / `--screenshot <file>`, at `--scale 1` and `--scale 2`). Commit them under `docs/screenshots/SUI-01/` and show them side by side in the PR.
- **Do not run any `install.ps1`, installer or system-wide install.**

**Finish**
- Update every README that this task changes.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, every choice you made differently from this prompt (and why), test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task SUI-01: One shared look: fonts, type sizes and a style guide

**Branch:** `suite/SUI-01-shared-fonts-and-type`

## Why

The three plugins were built one after another and each copied what it needed from the EQ. They now have three copies of the Inter font setup, two copies of the font files (`eq/Resources/Fonts` and `maximizer/Resources/Fonts`; `fet` points into `eq/`), and three slightly different sets of text sizes (the compressor uses 11 px knob scales and 12.5 px hints, the maximiser has a `TextSize` scale, the EQ follows task 24's minimums). Tony wants the plugins to look like one family: same typeface, same sizes for the same kind of text, same title treatment. Fix that once, in one place, so new plugins (an Analogue EQ is planned) get it for free.

## Requirements

### 1. A shared UI library

- Create `shared/` at the repo root with its own `CMakeLists.txt` that defines a JUCE-friendly library (an `INTERFACE` library plus a `juce_add_binary_data` target is fine), for example `songsmith_ui`.
- Move the Inter font files and `OFL.txt` to `shared/Resources/Fonts/`, once. Delete the other copies and point every plugin at the shared ones.
- Each plugin pulls it in with `add_subdirectory (${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)` (or equivalent) so it still builds on its own.
- One header, for example `shared/Source/SongsmithType.h`, in a `Songsmith::UI` namespace, holding:
  - the `Weight` enum (Regular, Medium, SemiBold), the cached `typeface (Weight)`, `font (height, weight)` and `numbers (height, weight)` (tabular figures);
  - one **type scale** with named roles, not numbers sprinkled through editors. Start from task 24's rules and the maximiser's `TextSize`, for example: `caption` (grid and scale labels, 11 px, lighter colour only), `hint` (12 px), `control` (13 px labels and values), `heading` (14–15 px band titles, section captions, top-bar controls), `title` (18 px plugin name). Pick the final numbers by comparing the three editors and say why in the PR.
  - a `drawTitle()` helper (or similar) so every plugin draws "Songsmith" + product name the same way, in the same place, at the same size. The compressor's title bar is the model; the per-plugin accent colour may differ.
- A shared base look-and-feel, for example `Songsmith::UI::LookAndFeelBase : juce::LookAndFeel_V4`, that serves the typeface (`getTypefaceForFont`, `setDefaultSansSerifTypeface`) and the fonts for labels, buttons, combo boxes, popup menus, tooltips and slider pop-ups from the type scale. Each plugin's own look-and-feel derives from it and keeps its colours and special drawing (meters, faders, the EQ graph). Task SUI-02 will add the shared control drawing on top of this, so leave room for it.

### 2. Move every plugin onto it

- Replace the three local font setups (`EQColours::font`, `FET::UI::font`, `MaxColours::font` and their typeface caches) with the shared ones. Keep thin aliases if that keeps the diff small, but there must be only one implementation.
- Replace hard-coded heights in editors and `paint()` code with type-scale roles. Search for `font (` and `withHeight (` across all three `Source/` folders.
- Nothing should get smaller than task 24's minimums at 100 % size. Some text will change size slightly to line up across plugins; that is the point, but check that no label clips or wraps at the minimum window size of each plugin.

### 3. Style guide

- Write `docs/style-guide.md`: the typeface and why, the type scale table (role, size, weight, where it's used), the title treatment, the per-plugin accent colours, and the rule that new plugins link `shared/` instead of copying. Keep it short and plain. Tasks SUI-02 (controls) and SUI-03 (wording) will add sections, so give each topic its own heading.
- Link it from the root `README.md`, and move the "Fonts" credit there so it names the shared folder.

## Tests

- A test in each plugin's test executable (or one shared test source compiled into each) that checks the embedded typeface loads and that `font()` for each weight returns that typeface, not a system fallback.
- A test per editor that walks the component tree and fails if any label, button or combo box uses a font height below the type scale's smallest role.
- Every existing test still passes.

## Out of scope

- Changing colours, control shapes or button drawing (task SUI-02).
- Changing any wording (task SUI-03).
- Layout changes beyond what is needed to keep text from clipping.
