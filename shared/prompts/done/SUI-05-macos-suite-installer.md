You are an experienced C++ audio developer and build engineer working on the **Songsmith Studio** plugins, a set of JUCE VST3/AU/Standalone plugins in one repo: `C:/dev/audio-plugins`. Songsmith EQ is in `eq/`, Songsmith Analog Compressor in `fet/`, Songsmith Maximiser in `maximizer/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- `README.md`, `prompts/suite/README.md`, `eq/ROADMAP.md` ("Out of scope": no signing or notarisation).
- Task SUI-04's Windows suite installer in `installer/windows/` and its CI job, so both platforms use the same name, version file and behaviour.
- The three macOS installers: `eq/installer/macos/`, `fet/installer/macos/`, `maximizer/installer/macos/` (`build-installer.sh`, `distribution.xml`, `resources/`, `scripts/`, `test-installer.sh`, `uninstall.sh`).
- `.github/workflows/build.yml`: the three macOS build jobs and their artifacts.
- Work with the existing installers. Don't rewrite them.

**Compatibility (must not break)**
- Never change any plugin's `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, bundle name, bundle ID or product name.
- Keep the existing component package identifiers (`pkgbuild --identifier`) of each plugin, so receipts from per-plugin installs and suite installs refer to the same thing and upgrades work in either direction.
- Presets and user data are never deleted.
- Not signed or notarised, by design.

**Build and test**
- The dev machine is Windows: this task is built and tested on the **macOS CI job**. Push early and read the CI logs.
- `installer/macos/test-installer.sh` really installs system-wide, so like the per-plugin ones it must refuse to run outside CI unless given `--i-know-this-installs-system-wide`. Never run it on Tony's machines.

**Finish**
- Update the root `README.md` ("Just want to use it?", macOS) so the suite `.pkg` is the first thing a Mac user sees, with the same Gatekeeper "Open Anyway" steps the EQ section has.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, every choice you made differently from this prompt (and why), CI test results, known limitations, and the line "Not yet tested on a real Mac".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task SUI-05: One macOS installer for every plugin

**Branch:** `suite/SUI-05-macos-suite-installer`

## Why

Same as task SUI-04, on the Mac: one download, one Gatekeeper warning, and a choice of which plugins to install.

## Requirements

- New folder `installer/macos/` at the repo root: `build-installer.sh`, `distribution.xml`, `resources/` (welcome, readme and licence pages in plain words), `test-installer.sh` and `uninstall.sh`.
- Output: `Songsmith-Studio-<version>-macOS.pkg`, universal (Apple Silicon and Intel), using the suite version file from task SUI-04.
- **Choosing plugins:** the Customize page shows one group per plugin, all selected by default, each with a one-line plain description. Inside each group: VST3 plug-in, Audio Unit (for Logic Pro and GarageBand) and standalone app, all selected. Deselecting a group deselects its children. Use the same wording as the Windows installer, with no abbreviations except the format names VST3 and AU.
- Build the suite from the **same component packages** the per-plugin `build-installer.sh` scripts make (reuse those scripts or factor out the `pkgbuild` step), not copies, so they can't drift apart.
- Each plugin's existing `preinstall`/`postinstall` behaviour carries over (legacy `TW Pro EQ` removal, cleaning user-folder copies, refreshing the AU cache).
- `uninstall.sh` removes every Songsmith plugin and app the suite can install, forgets their receipts, and keeps presets. It lists what it is about to remove and asks for confirmation.
- Same versions shown on the welcome page as on Windows: suite version plus each plugin's version.

### CI

- A new job, `suite-macos`, that `needs` the three macOS build jobs, downloads their component packages or Release builds, builds the suite `.pkg`, runs `installer/macos/test-installer.sh --i-know-this-installs-system-wide`, and uploads `Songsmith-Studio-macOS-pkg`.
- Run `auval` for each installed Audio Unit after installing from the suite package.

## Tests (`installer/macos/test-installer.sh`, CI only)

1. Install everything: all VST3s, AUs and apps present; `auval` passes for each AU.
2. Install with a choice-changes XML that selects only the maximiser: only the maximiser's files present.
3. Install the per-plugin EQ `.pkg`, then the suite: one copy of the EQ, receipts consistent.
4. Upgrade from a suite package with a lower version: upgrades in place.
5. `uninstall.sh` (non-interactive flag for CI): everything gone, a dummy preset in each plugin's preset folder still there.

## Out of scope

- Signing, notarisation, auto-update.
- Removing the per-plugin `.pkg`s.
- Publishing a release.
