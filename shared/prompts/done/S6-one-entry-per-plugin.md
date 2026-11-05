You are an experienced build engineer working on the **Songsmith Studio** plugins, a set of JUCE VST3/AU/Standalone plugins in one repo: `C:/dev/audio-plugins`. Follow the repo's `CLAUDE.md` (design principles and working rules) throughout.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

# Task S6: One installer entry per plugin

**Branch:** `suite/S6-one-entry-per-plugin`

## Why

The suite installer's Components page lists each plugin twice: "Songsmith EQ", then "Standalone app" under it, and the same for the compressor and the maximiser. The per-plugin installers have the same thing, with "VST3 plugin" and "Standalone app". Tony wants **one tick box per plugin**. Ticking a plugin installs everything that comes with it: VST3 and standalone app on Windows, and also the AU on macOS. The standalone app is small and harmless, so the choice isn't worth the extra lines.

## Read first

- `installer/windows/SongsmithStudio.iss` (`[Components]`, `[InstallDelete]`, `[Icons]`, the `SuiteHad` code and the remembered selection), `build-installer.ps1`, `test-installer.ps1`.
- `installer/macos/distribution.xml`, `build-installer.sh`, `test-installer.sh`.
- The per-plugin installers: `eq/installer/SongsmithEQ.iss`, `analog-compressor/installer/SongsmithAnalogCompressor.iss`, `maximiser/installer/SongsmithMaximiser.iss`.
- `prompts/done/S4-windows-suite-installer.md` and `S5-macos-suite-installer.md`, to see why things are the way they are.

## Requirements

### Windows suite installer

- `[Components]`: exactly three entries, `eq`, `fet` and `max`, with their current descriptions. Remove the `\standalone` sub-components. Each plugin's VST3, standalone app and Start menu shortcut go with its component.
- **Upgrades from suite 1.x:** Inno Setup remembers the previous selection, which may include `eq\standalone` or have a plugin ticked without its standalone app. Both must upgrade cleanly: a ticked plugin gets its app, and nothing errors on the unknown sub-component names. Unticking a plugin still removes its bundle **and** its app (update the `SuiteHad` checks and the `[InstallDelete]` lines).
- Keep the `AppId`, the per-plugin uninstall-and-replace logic, the "close your DAW first" check and the legacy bundle removal exactly as they are.
- Bump `installer/version.txt` (minor version).

### Per-plugin Windows installers

- Remove their Components page: they always install the VST3 and the standalone app. Keep the optional desktop shortcut task. Never change their `AppId`s. An upgrade where the app was unticked before now installs it, which is fine.

### macOS suite installer

- Same idea: three choices, `eq`, `fet` and `max`, each installing the VST3, AU and standalone app. Keep those three choice IDs (scripts use them). Remove the sub-choices from the outline and fold their packages into the parent choice.
- If something stops this (for example the macOS Installer needing a sub-choice to install a package), keep the sub-choices but hide them with `visible="false"`, and say so in the PR.

### Words

- Plain words on every page, no abbreviations (`docs/style-guide.md`). If a page or the README mentions the standalone app as a separate choice, update it, for example "Each plugin comes with a standalone app that runs without recording software."

## Tests

- Windows (`installer/windows/test-installer.ps1`, temp folder only): update every case that passed `eq\standalone` and friends. Add: ticking one plugin installs its VST3 **and** app; upgrading from a suite 1.x build where a plugin was ticked without its app ends with the app installed and one Apps entry; unticking a plugin removes both. Keep the "in use" and "presets survive uninstall" cases.
- Per-plugin installers: their `test-installer.ps1` scripts pass, and each installs the app without a components choice.
- macOS (`installer/macos/test-installer.sh`, runs on CI only): `-showChoicesXML` offers exactly three visible choices. Installing only `max` installs its VST3, AU and app and nothing else.
- **Never install into the real system folders on the dev machine**, and never run `install.ps1` or any installer outside the test scripts' temp folders.

## Finish

- Build every plugin in Release, build and test the installers. Wait for CI, including the macOS installer job.
- Screenshots of the Windows Components page before and after, committed and linked by SHA in the PR, then deleted before merging (see `CLAUDE.md`).
- Update the root `README.md` and each plugin's README where they describe the install choices.
- Open the PR with what changed, test output, and "Not yet tested on a clean Windows machine or Mac". Move this prompt to `done/` and update `prompts/README.md` in the same PR.
