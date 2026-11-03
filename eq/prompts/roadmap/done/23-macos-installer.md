You are an experienced C++ audio developer and macOS release engineer working on **Songsmith EQ** (by Songsmith Studio, formerly TW Pro EQ), a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/CMakeLists.txt`, `.github/workflows/build.yml` (especially the macOS steps: `auval`, the `ditto` zip), the Windows installer in `eq/installer/` (`SongsmithEQ.iss`, `build-installer.ps1`, `test-installer.ps1`) as the model to follow, `eq/Source/PresetManager.cpp` (where user presets live), and the macOS parts of `README.md`.
- The development machine is **Windows**. You can't build or run the macOS installer locally, so the macOS CI job (`macos-latest`) is where it gets built and tested. Push early to the draft PR and iterate on CI results. Write the macOS scripts so they can also be run by hand on a Mac.
- Work with the existing build. Don't restructure the CMake project.

**Compatibility (must not break)**
- Never change `PLUGIN_MANUFACTURER_CODE` (Tnyw) / `PLUGIN_CODE` (Eq3b), `BUNDLE_ID`, `PRODUCT_NAME`, or the bundle names `Songsmith EQ.vst3`, `Songsmith EQ.component` and `Songsmith EQ.app`. Hosts identify the plugin by these, and saved sessions depend on them.
- Keep the existing macOS zip (`Songsmith-EQ-macOS-zip` artifact). The installer is offered alongside it, not instead of it.
- Don't touch the Windows installer or `install.ps1`, except for shared docs.
- Don't touch DSP, parameters or `stateVersion`. This task changes packaging only.

**Build and test (Windows, from `eq/`)**
- `cmake --build build --config Release` must still succeed with no new warnings, and `.\build\EQTests_artefacts\Release\EQTests.exe` must still pass. This is a sanity check; the real tests for this task run on the macOS CI job.
- **Do not run `install.ps1`.** Never write to the real `C:\Program Files\Common Files\VST3` or `%APPDATA%\Songsmith Studio`.

**Finish**
- Tick the checkbox for this task in `eq/ROADMAP.md` and update `README.md`.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, every choice you made differently from this prompt (and why), CI results, known limitations, and the line "Not yet verified on a real Mac outside CI".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 23: macOS installer

**Branch:** `roadmap/23-macos-installer`

## Goal

Give Mac users a normal installer instead of "unzip, copy two bundles into `~/Library/Audio/Plug-Ins`, then run `xattr` in Terminal". A user who has never opened Terminal should be able to double-click the installer, click Continue a few times, open Logic (or rescan in their DAW) and see Songsmith EQ.

The installer is **not signed or notarised**. That's a deliberate scope decision (see "Out of scope" in `ROADMAP.md`). Gatekeeper will block it on first open; document the way past that rather than working around it.

## Tooling

- Build a flat **`.pkg`** with Apple's own `pkgbuild` and `productbuild` (both preinstalled with Xcode on `macos-latest`). No third-party packaging tools.
- Put the scripts in `eq/installer/macos/`: a `build-installer.sh` that reads the version from `project(... VERSION x.y.z)` in `eq/CMakeLists.txt` (as `build-installer.ps1` does), checks the Release build exists, builds one component package per format and combines them with `productbuild --distribution`. Keep the `distribution.xml` and any `pre`/`postinstall` scripts in that folder, readable and commented.
- Output goes to `eq/build/installer/Songsmith-EQ-<version>-macOS.pkg`.
- Component package identifiers: `com.songsmithstudio.songsmitheq.vst3`, `.au` and `.app`. These are package receipts, not the plugin's `BUNDLE_ID`; leave a comment saying never to change them, since `pkgutil` uses them to recognise an existing install.

## What the installer does

- **Choices** on the "Customize" page:
  - **VST3 plugin** → `/Library/Audio/Plug-Ins/VST3/Songsmith EQ.vst3` (on by default).
  - **Audio Unit** → `/Library/Audio/Plug-Ins/Components/Songsmith EQ.component` (on by default).
  - **Standalone app** → `/Applications/Songsmith EQ.app` (on by default).
- **System-wide install** into `/Library` (admin password prompt), which every host scans. Universal binary (arm64 + x86_64), as CI already builds. Set a sensible minimum macOS version in `distribution.xml`, matching the build's deployment target.
- **Upgrades:** installing over an older version replaces each bundle cleanly. Remove the old bundle in a `preinstall` script before the payload is laid down, so stale files from a previous version don't linger. Make sure `pkgbuild` doesn't treat the bundles as relocatable (`BundleIsRelocatable` false in a component plist), or an upgrade can install into a copy the user moved elsewhere.
- **Legacy bundles:** remove `TW Pro EQ.vst3` / `TW Pro EQ.component` and `3-Band EQ.vst3` / `3-Band EQ.component` from `/Library/Audio/Plug-Ins`. Also remove those legacy names and any user-level `Songsmith EQ.vst3` / `.component` from the console user's `~/Library/Audio/Plug-Ins` (the README told people to install there), since the same plugin ID would show up twice. Find the user with `stat -f%Su /dev/console`, not `$USER`. Never touch anything else in those folders.
- **AU cache:** after installing the Audio Unit, refresh the AU registry (for example `killall -9 AudioComponentRegistrar`, ignoring failure) so Logic picks up the new version without a reboot.
- **Plugin in use:** macOS doesn't lock loaded bundles, so a running host keeps the old code until it's restarted. The welcome text should say to quit the DAW first, and the conclusion text should say to restart it.
- **Uninstall:** a `.pkg` has no uninstaller. Ship `eq/installer/macos/uninstall.sh` (also included on the installer's Readme or in the release notes) that removes the three bundles and forgets the package receipts with `pkgutil --forget`. It must **not** delete the user's presets in `~/Library/Songsmith Studio/Songsmith EQ/Presets` (or the old TW Pro EQ folder). Those belong to the user.
- **Pages:** welcome (what gets installed, quit your DAW, upgrading from TW Pro EQ keeps sessions and presets), licence if the repo has one, conclusion (restart or rescan the DAW). Keep it short. Use an icon or background only if the project already has artwork; don't invent any.
- Payload files must be owned by `root:wheel` with normal permissions, and the bundles must not carry quarantine or other extended attributes.

## CI and releases

- Add steps to the macOS job in `.github/workflows/build.yml`, after `auval` and the zip: build the installer, test it, and upload it as an artifact named `Songsmith-EQ-macOS-pkg`.
- The macOS zip step should name its file with the version, `Songsmith-EQ-<version>-macOS.zip`, matching the Windows zip and the assets on the v0.3.0 release.
- Don't add automatic GitHub release publishing. Releases stay manual; just make the artifacts ready to attach.

## Tests

`EQTests` can't cover the installer, so add `eq/installer/macos/test-installer.sh`, run by CI. The GitHub runner is a throwaway machine with passwordless `sudo`, so the test may install for real there. **It must refuse to run outside CI** (check `CI=true` / `GITHUB_ACTIONS`) unless it's given an explicit `--i-know-this-installs-system-wide` flag, so nobody runs it on their own Mac by accident.

- Check the package before installing: `pkgutil --payload-files` (or `--expand-full`) lists exactly the three bundles at the right paths, with no `.DS_Store` or build leftovers, and `installer -pkg ... -showChoicesXML` lists the three choices.
- Seed fake legacy bundles (`TW Pro EQ.vst3` / `.component` in `/Library/...` and in `~/Library/...`), a stale file inside an existing `Songsmith EQ.vst3`, and a dummy preset file in the presets folder.
- `sudo installer -pkg <pkg> -target /`: check it exits 0, the three bundles exist, the legacy bundles and the stale file are gone, and the preset file is untouched.
- `auval -v aufx Eq3b Tnyw` passes against the **installed** component.
- Install again over itself: succeeds, same file list as after the first install.
- Install with a choices XML that deselects the standalone app: the plug-ins install and the app is not added.
- Run `uninstall.sh`: the bundles and receipts are gone, the presets file and the `Plug-Ins` folders are still there.
- If you have time, run the installed VST3 through `pluginval` (strictness 5). If it's too slow or flaky, leave it out and say so.

## Docs

- `README.md`, "Just want to use it?" (macOS): lead with the installer (download `...-macOS.pkg`, open it, follow the steps, restart the DAW). Keep the zip and `xattr` instructions underneath as the manual option.
- Explain the Gatekeeper block plainly and why it appears (the installer isn't signed or notarised): on recent macOS, try to open it once, then **System Settings → Privacy & Security → Open Anyway**. Note that plug-ins installed by the `.pkg` don't need the `xattr` step.
- Add a short "Building the macOS installer" note to the macOS build section pointing at `build-installer.sh`, and an "Uninstalling" note pointing at `uninstall.sh`.

## Out of scope

- Code signing, notarisation, stapling, or anything that needs an Apple Developer account.
- `.dmg` packaging, auto-update or telemetry.
- Installing presets: factory presets are built into the plugin.
- Any change to the Windows installer.
