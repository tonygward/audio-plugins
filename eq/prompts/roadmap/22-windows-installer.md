You are an experienced C++ audio developer and Windows release engineer working on **Songsmith EQ** (by Songsmith Studio, formerly TW Pro EQ), a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- This task runs **after task 21** (the rename to Songsmith EQ). If `eq/CMakeLists.txt` still says `TW Pro EQ`, stop and open a draft PR with a "Blocked: task 21 not merged" section.
- Before changing anything, read `eq/ROADMAP.md`, `eq/CMakeLists.txt`, `eq/install.ps1`, `.github/workflows/build.yml`, `eq/Source/PresetManager.cpp` (where user presets live) and the "Just want to use it?" and Windows install sections of `README.md`.
- Work with the existing build. Don't restructure the CMake project.

**Compatibility (must not break)**
- Never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`, `BUNDLE_ID`, `PRODUCT_NAME` or the bundle name `Songsmith EQ.vst3`. Hosts identify the plugin by these, and saved sessions depend on them.
- Keep `install.ps1` working exactly as it does today. It's the developer install path; the installer is for users.
- Keep the existing release zip (`Songsmith-EQ-<version>-win64.zip`). The installer is offered alongside it, not instead of it.
- Don't touch DSP, parameters or `stateVersion`. This task changes packaging only.

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must still pass every test.
- **Do not run `install.ps1`, and do not run the installer you build** without `/DIR` pointing into a temporary folder (see Tests). Never write to the real `C:\Program Files\Common Files\VST3` or `%APPDATA%\Songsmith Studio`.

**Finish**
- Tick the checkbox for this task in `eq/ROADMAP.md` and update `README.md`.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, every choice you made differently from this prompt (and why), test results, known limitations, and the line "Not yet verified on a clean Windows machine".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 22: Windows installer

**Branch:** `roadmap/22-windows-installer`

## Goal

Give Windows users a normal `Setup.exe` instead of "extract the zip and copy a folder into Program Files". A user who has never heard of a VST3 folder should be able to run it, click Next a few times, rescan in their DAW and see Songsmith EQ.

The installer is **not code-signed**. That's a deliberate scope decision (see "Out of scope" in `ROADMAP.md`). Windows SmartScreen will warn on first run; document that rather than working around it.

## Tooling

- Use **Inno Setup 6**. It's free, scriptable, produces a single `.exe`, and is preinstalled on GitHub's `windows-latest` runners (`iscc.exe`). If it's not installed locally, install it with `winget install JRSoftware.InnoSetup` (or record in the PR that you couldn't, and rely on CI).
- Put the script in `eq/installer/SongsmithEQ.iss`. Keep it readable and commented.
- Don't hard-code the version. Pass it in from CMake's `PROJECT_VERSION` (for example `iscc /DAppVersion=0.2.0 ...`), and have a small wrapper, `eq/installer/build-installer.ps1`, that reads the version from `eq/CMakeLists.txt`, checks the Release build exists, and runs `iscc`. Output goes to `eq/build/installer/Songsmith-EQ-<version>-win64-setup.exe`.

## What the installer does

- **Components** the user can tick:
  - **VST3 plugin** (required, always on): installs `Songsmith EQ.vst3` into `{commoncf64}\VST3`. The VST3 path is fixed by the spec, so don't let the user change it.
  - **Standalone app** (on by default): installs the standalone `.exe` into `{autopf}\Songsmith Studio\Songsmith EQ`, with a Start-menu shortcut. Desktop shortcut is an unticked optional task.
- **Admin rights:** writing to Common Files needs elevation, so use `PrivilegesRequired=admin`. 64-bit only (`ArchitecturesAllowed=x64compatible`, `ArchitecturesInstallIn64BitMode=x64compatible`).
- **Stable identity:** a fixed `AppId` GUID (generate it once and leave a comment saying never to change it), so newer installers upgrade in place and appear once in "Installed apps".
- **Upgrades:** installing over an older version replaces the bundle cleanly. Delete the old `Songsmith EQ.vst3` folder before copying, so stale files from a previous version don't linger (the same reason `install.ps1` does it).
- **Legacy bundles:** remove `TW Pro EQ.vst3` and `3-Band EQ.vst3` from the VST3 folder if present, as `install.ps1` does. They have the same plugin ID and would show up twice.
- **Plugin in use:** if a host has the plugin loaded, the files are locked. Use Inno's Restart Manager support (`CloseApplications`) and/or a check in `[Code]` so the user gets a clear "close your DAW and try again" message, not a half-finished install or a "reboot to finish" surprise.
- **Uninstall:** removes the VST3 bundle, the standalone app and shortcuts. It must **not** delete the user's presets in `%APPDATA%\Songsmith Studio\Songsmith EQ\Presets` (or the old `%APPDATA%\TW Pro EQ\Presets`). Those belong to the user.
- **Finish page:** a short note saying to rescan plugins in the DAW. Optionally offer to launch the standalone app.
- Include the licence (if the repo has one) and a short readme on the welcome or info page. Keep the wizard to as few pages as Inno allows with these components.
- Set `AppPublisher` (`Songsmith Studio`), `AppPublisherURL` (the GitHub repo), `AppVersion`, `VersionInfoVersion` and an `UninstallDisplayName`. Use an icon only if the project already has one; don't invent artwork.

## CI and releases

- Add steps to the Windows job in `.github/workflows/build.yml` that build the installer after the tests pass and upload it as an artifact named `Songsmith-EQ-Windows-setup`.
- Also make sure the Windows job produces the release zip in the same shape as the existing `Songsmith-EQ-<version>-win64.zip`, if it doesn't already, so a release has both files from one CI run.
- Don't add automatic GitHub release publishing. Releases stay manual; just make the artifacts ready to attach.

## Tests

The installer can't be covered by `EQTests`, so verify it with a script, `eq/installer/test-installer.ps1`, that CI also runs:

- Build the installer.
- Run it **silently into a temporary directory**: `/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR=<temp>` plus whatever override you add so the VST3 destination can also be redirected for testing (for example a `/VST3DIR=` command-line parameter read in `[Code]`, used only for tests and documented as such). The test must never write to the real VST3 folder.
- Check the expected files exist: the bundle's `Contents\x86_64-win\Songsmith EQ.vst3` DLL and the standalone `.exe`.
- Put a dummy file in a fake presets folder, run the uninstaller silently, and check the plugin files are gone and the presets file is still there.
- Run the installer twice in a row (upgrade over itself) and check it succeeds and leaves no duplicate or stale files.
- If you have time, run the installed VST3 through `pluginval` (strictness 5) on CI. If it's too slow or flaky, leave it out and say so.

## Docs

- `README.md`, "Just want to use it?": lead with the installer (download `...-setup.exe`, run it, rescan). Keep the zip instructions underneath as the manual option. Mention the SmartScreen warning plainly ("More info → Run anyway") and why it appears (the installer isn't signed).
- Add a short "Building the installer" note to the Windows build section pointing at `build-installer.ps1`.
- In `ROADMAP.md`, change the "Out of scope" line from "Signed installers" to "Code-signing the installer", so it's clear an unsigned installer is in scope and signing isn't.

## Out of scope

- Code signing, timestamping or anything that needs a certificate.
- macOS installers (`.pkg`) or notarisation.
- MSI/WiX, auto-update, or telemetry.
- Installing presets: factory presets are built into the plugin.
