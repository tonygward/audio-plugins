You are an experienced C++ audio developer working on **TW Pro EQ**, a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages. Keep purely mechanical renames (class names, file paths) in their own commits, separate from behaviour changes, so the PR is easy to review.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/CMakeLists.txt`, `eq/install.ps1`, `.github/workflows/build.yml`, `eq/Source/PresetManager.h/.cpp`, `eq/Source/PluginProcessor.h/.cpp`, `eq/Tests/EQTests.cpp` and `README.md`.
- Then find every use of the old names: `grep -rn "TW Pro EQ\|TWProEQ\|twproeq\|TW-Pro-EQ\|twpreset\|TonyWard\|ThreeBandEQ" --exclude-dir=build .`

**Compatibility (must not break)**
- **Never change `PLUGIN_MANUFACTURER_CODE` (`Tnyw`) or `PLUGIN_CODE` (`Eq3b`).** JUCE builds the VST3 class ID and the AU identity from these. Keeping them is what lets existing Studio One / Logic sessions open with the renamed plugin. Add a comment next to them explaining this, and that a future Songsmith plugin (e.g. Songsmith Analogue EQ) shares `Tnyw` but needs its own `PLUGIN_CODE`.
- Never rename or remove parameter IDs, and don't change the saved-state layout or `stateVersion`. A session saved by TW Pro EQ must load in Songsmith EQ with identical settings.
- User presets must not be lost (see "Presets" below).
- Don't change DSP or UI behaviour. This task is a rename.

**Build and test (from `eq/`)**
- The CMake target is renamed in this task, so delete and reconfigure the build: `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`, then `cmake --build build --config Release`.
- The build must succeed with no new warnings.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests in the existing `juce::UnitTest` style.
- **Do not run `install.ps1`.** Tests must never touch the real `%APPDATA%` presets folder; use temporary folders as the existing preset tests do.

**Finish**
- Tick the checkbox for this task in `eq/ROADMAP.md` and update `README.md`.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, every choice you made differently from this prompt (and why), test results, known limitations, and the line "Not yet verified in a DAW: check an old TW Pro EQ session opens with Songsmith EQ".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 21: Rename TW Pro EQ to Songsmith EQ

**Branch:** `roadmap/21-rename-songsmith`

## Goal

Rebrand the plugin from **TW Pro EQ** to **Songsmith EQ**, made by **Songsmith Studio**. It's the first of a family: **Songsmith Analogue EQ** is planned next. So set the names up as a brand plus a product, not as one-off strings scattered through the code.

Someone who used TW Pro EQ should notice only the new name: their sessions open, their presets are still there, and they don't end up with two copies of the plugin in their DAW.

## Names

| What | Old | New |
|---|---|---|
| Product name (DAW list, window title, bundle) | `TW Pro EQ` | `Songsmith EQ` |
| Company / vendor name (DAW vendor column) | `TonyWard` | `Songsmith Studio` |
| Bundle ID | `com.tonyward.twproeq` | `com.songsmithstudio.songsmitheq` |
| CMake project and plugin target | `ThreeBandEQ` | `SongsmithEQ` (artefacts move to `build/SongsmithEQ_artefacts/`) |
| Processor / editor classes | `ThreeBandEQAudioProcessor(Editor)` | `SongsmithEQAudioProcessor(Editor)` |
| VST3 bundle | `TW Pro EQ.vst3` | `Songsmith EQ.vst3` |
| AU component | `TW Pro EQ.component` | `Songsmith EQ.component` |
| Release zips | `TW-Pro-EQ-<version>-win64.zip`, `TW-Pro-EQ-macOS.zip` | `Songsmith-EQ-<version>-win64.zip`, `Songsmith-EQ-macOS.zip` |
| User presets folder | `%APPDATA%\TW Pro EQ\Presets`, `~/Library/TW Pro EQ/Presets` | `%APPDATA%\Songsmith Studio\Songsmith EQ\Presets`, `~/Library/Songsmith Studio/Songsmith EQ/Presets` |
| Preset file extension | `.twpreset` | `.sspreset` (still read `.twpreset`) |
| Roadmap title | `TW Pro EQ roadmap` | `Songsmith EQ roadmap` |

The vendor-level presets folder (`Songsmith Studio\<product>`) leaves room for Songsmith Analogue EQ to sit alongside.

Define the brand and product names in one place (CMake variables passed through, or `JucePlugin_Name` / `JucePlugin_Manufacturer`), and use those in code instead of new string literals. The test target's hard-coded `JucePlugin_Name` in `CMakeLists.txt` should come from the same variable.

Leave the repo name (`audio-plugins`) and the `eq/` folder as they are.

## Presets

- On first use (when `PresetManager` first needs the user folder), if the new folder doesn't exist and the old `TW Pro EQ\Presets` folder does, **copy** (don't move) its contents into the new folder. Leave the old folder in place, so going back to an older build still works. Do this off the audio thread, and make a failed copy harmless: log nothing on the audio thread and fall back to an empty folder.
- The preset browser lists both `.sspreset` and `.twpreset` files. New saves use `.sspreset`. Overwriting a `.twpreset` file may keep its extension; choose the least surprising behaviour and say what you chose.
- Update the "This file is not a TW Pro EQ preset" message and any similar text.
- Factory presets and starting-point templates are built in and don't change.

## Removing the old bundle

Installing `Songsmith EQ.vst3` next to an old `TW Pro EQ.vst3` would show the same plugin twice (same class ID). So:
- `install.ps1`: install `Songsmith EQ.vst3`, and add `TW Pro EQ.vst3` to `$legacyBundles` next to `3-Band EQ.vst3`, so it is checked for being in use and removed.
- README install instructions (Windows and macOS): tell people upgrading to delete `TW Pro EQ.vst3` (and on macOS `TW Pro EQ.component`) by hand if they install from the zip.

## CI

Update `.github/workflows/build.yml` for the new artefact paths, bundle names, zip names and artifact names. The `auval -v aufx Eq3b Tnyw` line stays the same because the codes don't change. Make sure CI still passes on Windows and macOS.

## Docs

- `README.md`: rename throughout. Add a short note near the top: "Songsmith EQ was called TW Pro EQ before v0.3. Sessions and presets carry over." Keep the Joe Gilder credit on the starting points as it is.
- `eq/ROADMAP.md`: retitle, and keep its history ("TW Pro EQ" in past-tense notes is fine).
- Don't rewrite the existing roadmap prompts in `eq/prompts/roadmap/done/`. They're a record of what was asked at the time.
- `eq/prompts/roadmap/22-windows-installer.md` already uses the new names. Check it matches what you actually did (paths, folder names, extension) and fix any mismatch.

## Tests

- **Session compatibility:** state saved by the processor before the rename still loads identically. Use a saved-state blob from the current tests or create a fixture before renaming, and assert every parameter value round-trips.
- **Identity:** a test that pins the manufacturer and plugin codes (`Tnyw`, `Eq3b`), so a later change to them fails loudly.
- **Preset migration**, using temporary folders:
  - Old folder with presets and no new folder → files copied, old folder untouched.
  - Both folders exist → nothing copied, nothing overwritten.
  - Neither exists → new folder works and is empty.
- **Both extensions:** a `.twpreset` and an `.sspreset` in the folder are both listed and both load.
- No remaining `TW Pro EQ` / `TWProEQ` / `ThreeBandEQ` in `eq/Source`, `eq/Tests` or `eq/CMakeLists.txt`, except in legacy-handling code (old folder name, old extension, old bundle name) with a comment. Check this with grep and list the leftovers in the PR.

## Out of scope

- Building the installer (task 22).
- Songsmith Analogue EQ itself, or any shared-library restructuring for multiple plugins beyond naming.
- New artwork, logos or UI restyling.
- Renaming the GitHub repository.
