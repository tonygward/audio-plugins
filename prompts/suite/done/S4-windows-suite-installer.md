You are an experienced C++ audio developer and build engineer working on the **Songsmith Studio** plugins, a set of JUCE VST3/AU/Standalone plugins in one repo: `C:/dev/audio-plugins`. Songsmith EQ is in `eq/`, Songsmith Analog Compressor in `fet/`, Songsmith Maximiser in `maximizer/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- `README.md`, `prompts/suite/README.md`, `eq/ROADMAP.md` ("Out of scope": no code signing).
- The three Windows installers and their scripts: `eq/installer/SongsmithEQ.iss`, `fet/installer/SongsmithAnalogCompressor.iss`, `maximizer/installer/SongsmithMaximiser.iss`, and each folder's `build-installer.ps1` and `test-installer.ps1`.
- `.github/workflows/build.yml`: the `build`, `fet` and `maximizer` jobs and the artifacts they upload.
- Work with the existing installers. Don't rewrite them.

**Compatibility (must not break)**
- Never change any plugin's `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, bundle name or product name.
- Never change the existing per-plugin installers' `AppId`s. They identify existing installs on users' machines.
- Presets and user data are never deleted, by install, upgrade or uninstall.
- The installer is not code-signed, by design. Don't add signing.

**Build and test**
- Build every plugin in Release: `cmake --build <plugin>/build --config Release` for `eq`, `fet` and `maximizer` (configure first with `cmake -S <plugin> -B <plugin>/build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE` if `build/` is missing).
- You need Inno Setup 6 (`winget install JRSoftware.InnoSetup`).
- Test the installer **only inside a temp folder**, the way the existing `test-installer.ps1` scripts do (`/CURRENTUSER` plus the test-only `/VST3DIR=`). **Never install into the real `Common Files\VST3` folder, never run any `install.ps1`, and never touch real presets.**

**Finish**
- Update the root `README.md` ("Just want to use it?") so the suite installer is the first thing a Windows user sees.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, every choice you made differently from this prompt (and why), test results (paste the test script output), known limitations, and the line "Not yet tested on a clean Windows machine".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task S4: One Windows installer for every plugin

**Branch:** `suite/S4-windows-suite-installer`

## Why

Each plugin has its own installer, so someone who wants all three downloads and runs three setups and clicks through three SmartScreen warnings. Tony wants **one installer that includes every plugin**, where you can untick the ones you don't want.

## Requirements

### The installer

- New folder `installer/windows/` at the repo root with `SongsmithStudio.iss`, `build-installer.ps1` and `test-installer.ps1`, following the per-plugin scripts' patterns and comments.
- Name: **Songsmith Studio** (Setup title "Songsmith Studio plugins"). Output file: `Songsmith-Studio-<version>-win64-setup.exe`. It gets its own new, permanent `AppId`, with the same "NEVER CHANGE THIS" comment the others have.
- **Version:** the suite has its own version in one place (for example `installer/version.txt`, starting at `1.0.0`), and the Setup welcome page and the "Apps" entry list each plugin's own version (read from each plugin's `project(... VERSION ...)` by `build-installer.ps1`).
- **Choosing plugins:** a Components page with one entry per plugin, **all ticked by default**, each with a one-line plain description ("Songsmith EQ: shape the tone of a track"). Under each plugin, its standalone app is a sub-component, ticked by default. The VST3 can't be unticked on its own; untick the plugin instead. Use plain words throughout: no abbreviations on any page (see `docs/style-guide.md` if it exists, otherwise task 24's rule in `eq/prompts/roadmap/done/24-*.md`).
- Show the disk space for each plugin, and refuse to continue if nothing is ticked.
- Same behaviour as the per-plugin installers for each plugin it installs: same bundle locations (`Common Files\VST3`, `Program Files\Songsmith Studio\<plugin>`), same "close your DAW first" check that stops before changing anything if a plugin is loaded, the same removal of legacy bundles (`TW Pro EQ.vst3`, `3-Band EQ.vst3`), and presets left alone.
- **Living with the per-plugin installers:** someone may already have, say, Songsmith EQ from its own installer. When the suite installs a plugin that a per-plugin installer already installed, run that installer's uninstaller silently first (find it by its `AppId` in the registry), so there is only ever one entry in Settings → Apps per install and no duplicate files. Say clearly on the Ready page which existing installs will be replaced.
- **Running it again:** a newer suite installer upgrades in place and remembers which plugins were ticked last time. Unticking a plugin that is installed **removes** it (Inno Setup doesn't do this on its own; handle it in `[Code]`), after saying so on the Ready page.
- **Uninstall:** one entry, "Songsmith Studio", removes everything it installed and leaves presets.
- Keep building the per-plugin installers in CI for now; the suite installer is the one to recommend. Note in the PR whether you think the per-plugin ones can be retired later.

### CI

- A new job in `.github/workflows/build.yml`, `suite-windows`, that `needs` the three Windows build jobs, downloads their Release artefacts (have those jobs upload the VST3 and standalone builds if they don't already), builds the suite installer and runs `installer/windows/test-installer.ps1`. Upload it as the `Songsmith-Studio-Windows-setup` artifact.
- Respect the existing path filters, so docs-only changes still skip builds.

## Tests (`installer/windows/test-installer.ps1`, all inside a temp folder)

1. Install with everything ticked: all three VST3 bundles and standalone apps are present.
2. Install with only the EQ ticked: only the EQ is present.
3. Re-run and tick the compressor too: EQ kept, compressor added, still one Apps entry.
4. Re-run and untick the EQ: EQ removed, compressor kept.
5. Install the per-plugin EQ installer first, then the suite: the per-plugin entry is gone and the EQ is installed once.
6. Upgrade from an older suite version: choices remembered.
7. Uninstall: every bundle and app gone, a dummy preset file in each plugin's preset folder still there.
8. "Plugin is in use": with a bundle file held open, Setup stops and changes nothing.

## Out of scope

- macOS (task S5).
- Code signing, auto-update, downloading plugins from the internet.
- Publishing a release.
