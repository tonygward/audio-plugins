You are an experienced build engineer working on the **Songsmith Studio** plugins, a set of JUCE VST3/AU/Standalone plugins in one repo: `C:/dev/audio-plugins`. Follow the repo's `CLAUDE.md` (design principles and working rules) throughout.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

# Task S7: One version number for every plugin

**Branch:** `suite/S7-one-version-number`

## Why

Every plugin has its own version, and the suite has another one. Today Songsmith EQ is 0.5.0, the Analog Compressor and the Maximiser are 0.1.0, and the Songsmith Studio installer is 1.0.0 (`installer/version.txt`). Each has its own tag scheme: `v0.5.0`, `compressor-v0.1.0`, `maximizer-v0.1.0` and `studio-v1.0.0`. That's confusing for users ("which version do I have?") and makes every release a guessing game. Tony wants **one version number** for the whole family. Every plugin, every installer and every release share it, and there's one tag and one GitHub release per version.

## Read first

- `eq/CMakeLists.txt`, `analog-compressor/CMakeLists.txt`, `maximiser/CMakeLists.txt`: the `project(... VERSION x.y.z)` lines.
- `installer/version.txt`, `installer/windows/build-installer.ps1`, `installer/macos/build-installer.sh`, and their `test-installer` scripts.
- Each plugin's `installer/build-installer.ps1`, `installer/macos/build-installer.sh` and `test-installer` scripts (they read the version from `project()`).
- `.github/workflows/build.yml`: the zip steps read the version from `project()` with `sed`, and the `paths:` filters decide what starts a build.
- `eq/CHANGELOG.md`, `maximiser/CHANGELOG.md`, `TECHNICAL.md`, `README.md`.
- The existing releases: `gh release list` and `gh release view studio-v1.0.0`.

## Requirements

### One place for the version

- Add a root **`VERSION`** file holding one `x.y.z` line. It's the only place a version number is written.
- Each plugin's `CMakeLists.txt` reads it (`file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/../VERSION ...)`) and passes it to `project(... VERSION ...)`. Each plugin must still configure and build on its own from its own folder. Fail with a clear message if the file is missing or isn't `x.y.z`.
- Every installer script (per-plugin and suite, Windows and macOS) and the CI zip steps read `VERSION` instead of parsing `project()` or `installer/version.txt`. Delete `installer/version.txt`.
- Add `VERSION` to **both** `paths:` lists in `build.yml`, or a version bump won't start a build.
- Anything else that shows a version (editor, About text, README) uses the same number. Search for hard-coded versions (`0.5.0`, `0.1.0`, `1.0.0`) outside changelogs and `done/` prompts.

### The number

- Use the smallest `x.y.0` that is higher than **every** version already released for any plugin or the suite (check the tags), and not lower than the current `installer/version.txt`. With today's releases that's **1.1.0**. If another task has already bumped `installer/version.txt` to an unreleased 1.1.0, keep 1.1.0.
- Versions only ever go up. That matters for Logic's Audio Unit cache and for the installers' upgrade checks. Moving Songsmith EQ from 0.5.0 to 1.1.0 is an upgrade, so that's fine.
- **Don't change anything that identifies a plugin** (see `CLAUDE.md`): plugin codes, bundle IDs, product names, `AppId`s, package identifiers. `stateVersion` is separate from the product version. Leave it alone, and make sure older sessions still load (the existing state tests).

### Installers

- Per-plugin and suite installers all carry the shared version. Upgrades from every released version (EQ 0.5.0, compressor and maximiser 0.1.0, suite 1.0.0) must still be recognised as upgrades.
- Check the installer tests' "older version" cases (for example the suite test's 0.9.0 build) still build a version lower than `VERSION`. If a test hard-codes a version, derive it from `VERSION` instead.

### Releases and changelog

- From now on there's **one tag per version, `vX.Y.Z`**, and one GitHub release titled **"Songsmith Studio vX.Y.Z"**. It has the suite installers plus each plugin's own installer and zip, for Windows and macOS. Old tags stay as they are.
- Start a root **`CHANGELOG.md`** for Songsmith Studio, with a section per version and a heading per plugin inside it. Add an entry for this version: "Every Songsmith plugin now shares one version number." End each existing per-plugin changelog with a line pointing to the root one, and don't add new entries to them.
- In `TECHNICAL.md`, add a short **Releasing** section that matches what the overnight runner does (`shared/prompts/overnight-prompt.md`):
  - Bump `VERSION`: minor for new features, patch for fixes only.
  - Add the changelog entry and merge.
  - Wait for `main`'s CI build of the merge commit.
  - Download its installer and zip artifacts (`gh run download`).
  - `gh release create vX.Y.Z --target <merge commit>` with those files and plain-language notes.

### Words

- Plain words, British spelling, no abbreviations (`docs/style-guide.md`). Say "version 1.1.0" or "v1.1.0", never "ver.".

## Tests

- Build every plugin in Release and run `EQTests`, `AnalogCompressorTests` and `MaximiserTests`. All pass.
- Add a check that fails if the version ever drifts. It can be a CI step or a test. It should fail if a `CMakeLists.txt`, installer script or workflow contains its own `x.y.z` version instead of reading `VERSION`, or if a built plugin reports a different version (`JucePlugin_VersionString`).
- Windows installer tests (temp folders only) pass for all three plugins and the suite. macOS installers are tested on CI.
- **Never install into the real system folders on the dev machine**, and never run `install.ps1` or any installer outside the test scripts' temp folders.

## Finish

- Wait for CI, including the macOS and suite installer jobs. Check that the artifact file names all carry the new version.
- Update `README.md` and `TECHNICAL.md` wherever they mention separate plugin versions or `installer/version.txt`.
- Open the PR with what changed, the chosen version and why, test output, and "Not yet verified in a DAW". Move this prompt to `done/` and update `shared/prompts/README.md` in the same PR.
- After merging, publish the **v1.1.0** release as described in the Releasing section. It's the first release under the new scheme.
