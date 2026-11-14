# Songsmith Plugins — technical guide

Audio plugins built with [JUCE](https://juce.com) and CMake.

| Plugin | Folder | Formats |
|---|---|---|
| **Songsmith EQ**: parametric EQ with a spectrum analyser | [`eq/`](eq/) | VST3, AU (macOS), Standalone |
| **Songsmith Analog Compressor**: FET feedback compressor inspired by the 1176LN, see [`analog-compressor/README.md`](analog-compressor/README.md) | [`analog-compressor/`](analog-compressor/) | VST3, AU (macOS), Standalone |
| **Songsmith Maximiser**: look-ahead brickwall maximiser with a loudness meter, see [`maximiser/README.md`](maximiser/README.md) | [`maximiser/`](maximiser/) | VST3, AU (macOS), Standalone |
| **Songsmith Compressor**: clean, conventional compressor with Threshold, Ratio, Attack, Release and Knee, see [`compressor/README.md`](compressor/README.md) | [`compressor/`](compressor/) | VST3, AU (macOS), Standalone |

Every plugin shares one look: the same typeface, type sizes and title, from the shared UI library in [`shared/`](shared/). See the [style guide](docs/style-guide.md).

### Latency and Processing Load

Every editor has a strip along the bottom, the same in every plugin: the **Processing Load** switch at the far left and, only while it is on, one readout beside it such as `CPU 0.35%, Latency 0.0ms (0 samples)`. In Songsmith EQ the **Piano** toggle sits at the far right of the same strip, beside the piano-note strip it controls (see the [style guide](docs/style-guide.md#latency-and-processing-load-strip)):

- **Latency:** the delay the plugin reports to your recording software, in milliseconds at the current sample rate and in samples, for example "1.8ms (84 samples)". It updates when you change a mode that adds delay (linear phase, oversampling) or when the sample rate changes.
- **Processing Load:** off until you switch it on. It shows how much of the time available for each block of audio this plugin uses, for example "3.2%". It describes this plugin only, and varies with the computer, the settings and the buffer size. It says "CPU Unavailable" while no audio is being processed.

The Processing Load switch is remembered on your computer in a `Diagnostics.xml` file beside each plugin's presets folder. It is not part of your sessions or presets. Timing costs about 70 ns per block when on and next to nothing when off. The checks are in `shared/Tests/SongsmithDiagnosticsTests.h` and each plugin's "Latency and Processing Load" tests.

### Fonts

Every editor uses [Inter](https://rsms.me/inter/) (Regular, Medium and SemiBold, v4.1) by The Inter Project Authors, embedded in the plugin so Windows and macOS look the same. One copy of the font files is kept in [`shared/Resources/Fonts`](shared/Resources/Fonts). Inter is licensed under the SIL Open Font License 1.1; the licence is in [`shared/Resources/Fonts/OFL.txt`](shared/Resources/Fonts/OFL.txt).

## Install every plugin at once

**Songsmith Plugins** is one installer with every plugin in it: `Songsmith-Plugins-<version>-win64-setup.exe`, from the [Releases page](https://github.com/tonygward/audio-plugins/releases) (CI builds it in the packaging build on each version bump commit, and on pull requests that change shared or installer files, as the `Songsmith-Plugins-Windows-setup` artifact; a pull request that changes only one plugin builds only that plugin). This is the one to use.

1. Run it. Windows SmartScreen will probably say it "protected your PC", because the installer isn't code-signed: click **More info**, then **Run anyway**.
2. Every plugin is ticked. Untick any you don't want. Each plugin comes with a standalone app that runs without recording software. The page shows how much disk space each one needs.
3. Close your recording software first: if a plugin is loaded, Setup stops and changes nothing.
4. Rescan plugins in your recording software.

Running it again (or a newer version) remembers what you ticked. Unticking a plugin that's installed removes it. If you installed a plugin with its own installer before, the suite replaces that install, so each plugin is installed once and there's one **Songsmith Plugins** entry in **Settings → Apps**. Uninstalling it there removes every plugin and keeps your presets.

To build it: build the four plugins (Release), then run `.\installer\windows\build-installer.ps1` (Inno Setup 6, see below). `.\installer\windows\test-installer.ps1` builds it and tests installing, choosing plugins, upgrading, replacing a per-plugin install, a plugin in use and uninstalling, all inside a temp folder. Every plugin and the installer share the one version in the `VERSION` file at the top of the repository.

**On a Mac**, the same suite is `Songsmith-Plugins-<version>-macOS.pkg` (CI artifact `Songsmith-Plugins-macOS-pkg`), a universal installer for Apple Silicon and Intel (the CI builds on pull requests are Apple Silicon only to save time; the release comes from the universal build of `main`). It isn't signed or notarised, so the first time macOS blocks it: try to open it, then go to **System Settings → Privacy & Security** and click **Open Anyway**. Everything is selected; click **Customize** to leave out a plugin. Each plugin comes with its VST3 plugin, its Audio Unit (for Logic Pro and GarageBand) and a standalone app that runs without recording software. It replaces a plugin you installed with its own installer, so there's one copy. To uninstall, run `installer/macos/uninstall.sh`: it lists what it will remove, asks first, and keeps your presets.

To build it on a Mac: build the four plugins (Release), then run `installer/macos/build-installer.sh`. It builds each plugin's own `.pkg` and makes the suite from the same component packages, so the two can't drift apart. `installer/macos/test-installer.sh` installs for real, system-wide, so it only runs on CI (or with `--i-know-this-installs-system-wide`).

---

## Releasing

Every plugin, installer and release shares one version number, kept in the `VERSION` file (one line, `x.y.z`). Nothing else holds a version: the plugins' `CMakeLists.txt`, every installer script and the CI zip steps read that file, and `installer/check-version.ps1` (run by CI) fails if anything starts holding its own, or if a built plugin reports a different version. There is one tag and one GitHub release per version, `vX.Y.Z`, titled **Songsmith Plugins vX.Y.Z**. Tags from before one shared version (`v0.2.0` to `v0.4.0`, `compressor-v0.1.0`, `maximizer-v0.1.0`, `studio-v1.0.0`) stay as they are.

Releasing is automatic; nobody edits `VERSION` or publishes by hand. Each source merge to `main` is followed by a `[beta-version]` commit that raises the third part of `VERSION` and moves the `## Unreleased` notes in `CHANGELOG.md` into the new version. That commit gets the one full packaging build, and the release is published from it with exactly two files, `Songsmith-Plugins-X.Y.Z-win64-setup.exe` and `Songsmith-Plugins-X.Y.Z-macOS.pkg`, then copied to the beta website. Details, switches and recovery: [release checkpoints](docs/automatic-release-checkpoints.md) and [pipelines](docs/pipelines.md).

What you do in a pull request: if musicians will notice the change, add plain-language bullets under `## Unreleased` at the top of `CHANGELOG.md`.

### What keeps the name Songsmith Studio

**Songsmith Plugins** is the name of the family. **Songsmith Studio** is the publisher, so it stays wherever a name identifies something that already exists on a computer or in a session: the VST3 and Audio Unit manufacturer (hosts list the plugins under Songsmith Studio), the `Program Files\Songsmith Studio` folder, the preset folders (`%APPDATA%\Songsmith Studio`, `~/Library/Songsmith Studio`), the installers' application and package IDs, the plugin codes and bundle IDs, and the release tags already published (`studio-v1.0.0` and the others). Changing any of those would orphan installs, presets or projects.

## Songsmith EQ

Songsmith EQ, by Songsmith Studio, was called TW Pro EQ before v0.3. Sessions and presets carry over.

### Features

- Up to 24 bands: Bell, Low Shelf, High Shelf, Low Cut, High Cut and Notch. Double-click the graph to add one: a Low Cut below 40 Hz, a High Cut above 12 kHz, a Bell anywhere else (hold Ctrl, or Cmd on macOS, for a Bell anywhere)
- Low Cut and High Cut slopes of 6, 12, 18, 24, 36 or 48 dB/oct (Butterworth; raise Q for a resonant peak at the cutoff)
- **Oversampling** (top bar: "Oversampling: off / 2× / 4×"): stops high-frequency bells and shelves from "cramping" near Nyquist at 44.1/48 kHz. It adds a few samples of reported latency in zero-latency mode; in linear-phase mode it only changes the filter design
- **Linear-phase mode** (top bar): "Linear phase: low / medium / high" for mastering. It adds latency (at 44.1/48 kHz about 43-46, 85-93 or 171-186 ms), which is reported to your DAW for compensation. Dynamic EQ and solo need zero-latency mode
- **Dynamic EQ per band** (Bell and shelves): turn on **Dynamic** in the band panel for threshold, range (±24 dB), attack and release; the band's gain moves with the level in its own frequency region, or with the side chain (**Side Chain**). The graph shades the live curve and the node shows where the gain sits now
- **Stereo modes per band:** Stereo, Left, Right, Mid or Side (a small "Left", "Right", "Mid" or "Side" label sits under the node). The "Curve" selector shows the combined curve for one channel
- Click-free parameter smoothing, undo/redo, and a response curve drawn from the real DSP coefficients
- **Interface size** (the `...` menu → Interface size): 75 %, 100 %, 125 %, 150 %, 175 % or 200 %. Everything scales together (text, controls, nodes, lines), and the window resizes to match. The setting is saved with the session and remembered on your computer, so new instances open at the size you last chose. The window opens at 1280 × 788 (at 100 %, including the strip along the bottom) and can be resized from 980 × 468 up to 3840 × 2400 (a full 4K screen); on large windows the controls and text step up in size too, while the graph gets most of the extra room. Drawing stays sharp with Windows display scaling and on Retina screens
- **Scale** (second row, left of Auto Gain and Output): scales every band's gain together, -100 % to +200 % (0 % = flat, negative inverts; cuts and notches are unaffected). Double-click for 100 %. It's automatable and stored in presets
- Spectrum analyser, before or after the EQ ("Analyser: before EQ" / "after EQ"), with settings (the `...` button): freeze, peak hold (click the graph to reset), tilt 0/3/4.5/6 dB/oct, resolution low/medium/high (2048, 4096 or 8192 points), slow/medium/fast speed and dBFS range. Plus an optional **side chain spectrum** (orange) to spot masking: route another track to the side chain input and pick "Side Chain: spectrum" or "Side Chain: spectrum and overlap" (shades where both tracks are within 6 dB above -60 dBFS). The selector appears once the side chain receives signal; the side chain is never heard
- **Notes:** frequencies show the nearest note and cents (`440 Hz · A4 +0¢`); type note names such as `A2`, `C#5` or `Eb3` into the frequency field; hold Ctrl (Cmd on macOS) while dragging to snap to semitones; click **Piano** (far right of the strip along the bottom, off by default) or right-click the empty graph to show a piano strip along the frequency axis
- **Frequency Guide** (row 3, above the graph): an optional overlay of see-through coloured regions behind the curve, each with a plain word (Rumble, Thump, Plastic, Air...) and a tooltip on what it adds, what too much sounds like and when it is usually moved. Choose the instrument beside it: Acoustic Guitar, Nylon Guitar, Electric Guitar, Bass Guitar, Vocal, Kick Drum, Snare Drum, Toms, Drum Bus or Any Instrument. It is off by default, only draws (it never changes a band or the sound), and is saved with the session. The words come from Joe Gilder's Home Studio Corner videos and Tony's own notes, and the guide is not endorsed by Joe Gilder. Details, sources and choices: [`eq/docs/frequency-guide.md`](eq/docs/frequency-guide.md).
- **Solo a band:** Alt-click (and drag) a node to hear only the region it affects while the mouse is held, or latch it with the Solo button in the band panel or the right-click menu
- **Output gain** (±24 dB, double-click to reset) and a tall output meter on the right of the graph (-60 to +6 dBFS scale, peak hold with the held peak as a number: click the meter to reset it; a red CLIP warning at the top clears by itself after the output has been silent, see [`eq/docs/output-meter.md`](eq/docs/output-meter.md))
- **Global bypass** that your DAW uses as its native bypass, with a smooth 20 ms cross-fade
- **Auto Gain** (top bar): matches the output loudness to the input (K-weighted, 2 s window, ±12 dB, frozen on silence) so you judge EQ moves at equal loudness; the button shows the correction, for example "Auto Gain +1.2 dB"
- **Starting points** (the **Preset** menu → Starting Points): ready-made settings grouped by instrument (Drums, Guitars, Vocals, Effects, General), such as **Vocals - Lead Vocal**, **Guitars - Acoustic Guitar (Full Mix)**, **Effects - Reverb Send Clean-up** and **Drums - Floor Tom**. Most follow advice from Joe Gilder's [Home Studio Corner](https://homestudiocorner.com) videos, and they aren't endorsed by him; the acoustic guitar variants, the reverb send and the toms come from Tony's own mixes. They work the way he does: cuts before boosts, small moves, "only if needed" bands loaded but switched off, and output gain set so turning the EQ on and off compares tone, not loudness. Hover over the preset name for a one-line hint. Treat the frequencies as places to start listening, and move each band to what you hear in the mix. Every band's source and the full menu: [`eq/docs/starting-points.md`](eq/docs/starting-points.md)
- **Presets:** your own presets are saved as `.sspreset` files in `%APPDATA%\Songsmith Studio\Songsmith EQ\Presets` (Windows) or `~/Library/Songsmith Studio/Songsmith EQ/Presets` (macOS). Presets saved by TW Pro EQ (`.twpreset`) still load: the first time you open the preset menu or use the preset arrows, they're copied from the old `TW Pro EQ\Presets` folder, which is left as it was. Use the arrows and the **Preset** menu in the title bar; `*` means the settings differ from the loaded preset. Loading a preset is one undo step.

### Just want to use it?

Install it with the [Songsmith Plugins installer](#install-every-plugin-at-once) above, from the [Releases page](https://github.com/tonygward/audio-plugins/releases): `Songsmith-Plugins-<version>-win64-setup.exe` on Windows or `Songsmith-Plugins-<version>-macOS.pkg` on a Mac, with Songsmith EQ ticked. Each release has only those two files; there is no separate Songsmith EQ download any more.

Upgrading from TW Pro EQ? The installer removes the old `TW Pro EQ` plug-in, and your sessions and presets carry over.

To install a build you made yourself, see **Install** under [Windows](#windows) or [macOS](#macos) below.

---

### Building from source

#### What you need (both platforms)

- **CMake 3.22 or newer**: <https://cmake.org/download/>
- **Git**
- **JUCE**: the project was developed against JUCE 9.0.5. JUCE isn't included in this repo; clone it next to this one:

  ```sh
  git clone https://github.com/juce-framework/JUCE.git
  ```

  The build finds JUCE through the `JUCE_DIR` CMake option. It defaults to `C:/dev/JUCE`, so on a Mac (or anywhere else) pass `-DJUCE_DIR=/path/to/JUCE` when configuring.

#### Build options

| Option | Default | What it does |
|---|---|---|
| `JUCE_DIR` | `C:/dev/JUCE` | Path to your JUCE checkout |
| `EQ_BUILD_TESTS` | `ON` | Also builds the `EQTests` console app (unit tests and CPU benchmark) |
| `EQ_COPY_PLUGIN` | `OFF` | Copies the plugin to the system plugin folder after each build (on Windows this needs an elevated shell) |

---

### Windows

**Requirements:** Visual Studio 2022 or 2026 with the **Desktop development with C++** workload.

#### Build

From a terminal in the `eq` folder:

```powershell
# Configure. Use "Visual Studio 17 2022" if you have VS 2022.
cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE

# Build
cmake --build build --config Release
```

The plugin ends up at:

```
eq\build\SongsmithEQ_artefacts\Release\VST3\Songsmith EQ.vst3
```

There's also a standalone app in `eq\build\SongsmithEQ_artefacts\Release\Standalone\`.

#### Install

```powershell
.\install.ps1              # installs the Release build
.\install.ps1 -Config Debug
```

The script asks for administrator rights, copies the plugin into `C:\Program Files\Common Files\VST3`, and removes the bundles from before the renames (`3-Band EQ.vst3` and `TW Pro EQ.vst3`). It refuses to overwrite a plugin that a running DAW has loaded, so close the DAW first.

To install by hand instead, copy the `Songsmith EQ.vst3` folder into `C:\Program Files\Common Files\VST3`.

#### Run the tests

```powershell
.\build\EQTests_artefacts\Release\EQTests.exe          # unit tests
.\build\EQTests_artefacts\Release\EQTests.exe --bench  # CPU benchmark
```

#### Building the installer

You need [Inno Setup 6](https://jrsoftware.org/isinfo.php) (`winget install JRSoftware.InnoSetup`). After a Release build, from the `eq` folder:

```powershell
.\installer\build-installer.ps1        # -> build\installer\Songsmith-EQ-<version>-win64-setup.exe
.\installer\build-installer.ps1 -Zip   # also builds the manual-install zip
.\installer\test-installer.ps1         # builds it, then installs, upgrades and uninstalls it inside a temp folder
```

The version comes from the shared `VERSION` file (see [Releasing](#releasing)). The script is `installer\SongsmithEQ.iss`. The test never touches your real VST3 folder or presets. CI builds both as internal test artifacts (`Songsmith-EQ-Windows-setup` and `Songsmith-EQ-Windows-zip`); they are never published. Releases carry only the two suite installers.

---

### macOS

The Mac build (VST3, AU and Standalone, universal arm64 + x86_64) is built and tested by the GitHub Actions workflow on pull requests that change it and in the packaging build on each version bump commit, including Apple's `auval` Audio Unit validation. It hasn't been checked by hand in Logic Pro yet.

**Requirements:**

- **Xcode** from the App Store. Open it once afterwards so it finishes installing its components, then run:

  ```sh
  xcode-select --install
  ```
- **CMake**: `brew install cmake` (with [Homebrew](https://brew.sh)), or the installer from cmake.org.

#### Build

From Terminal in the `eq` folder:

```sh
# Configure. Builds a universal binary for both Apple Silicon and Intel Macs.
cmake -B build -G Xcode \
  -DJUCE_DIR="$HOME/dev/JUCE" \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

# Build
cmake --build build --config Release
```

If you'd rather not use Xcode as the generator, `-G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release` works too.

On a Mac the build produces both plug-in formats (AU is added automatically on Apple platforms):

```
eq/build/SongsmithEQ_artefacts/Release/VST3/Songsmith EQ.vst3
eq/build/SongsmithEQ_artefacts/Release/AU/Songsmith EQ.component
```

#### Install

Copy the plug-ins into your user plug-in folders (no admin rights needed). **Logic Pro and GarageBand** load the **AU** (`.component`); most other DAWs load the VST3.

```sh
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/"Songsmith EQ.vst3" ~/Library/Audio/Plug-Ins/Components/"Songsmith EQ.component"
cp -R build/SongsmithEQ_artefacts/Release/VST3/"Songsmith EQ.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R build/SongsmithEQ_artefacts/Release/AU/"Songsmith EQ.component" ~/Library/Audio/Plug-Ins/Components/
```

**Upgrading from TW Pro EQ:** delete the old copies too (they're the same plugin to your DAW, so it would show up twice). Your sessions and presets carry over.

```sh
rm -rf ~/Library/Audio/Plug-Ins/VST3/"TW Pro EQ.vst3" ~/Library/Audio/Plug-Ins/Components/"TW Pro EQ.component"
```

Then rescan plugins in your DAW. If Logic doesn't list the AU straight away, run `killall -9 AudioComponentRegistrar` and reopen Logic. You can check the AU yourself with `auval -v aufx Eq3b Tnyw`.

#### Run the tests

```sh
./build/EQTests_artefacts/Release/EQTests          # unit tests
./build/EQTests_artefacts/Release/EQTests --bench  # CPU benchmark
```

#### Building the macOS installer

After a Release build, from the `eq` folder (needs Xcode's `pkgbuild` and `productbuild`):

```sh
installer/macos/build-installer.sh   # -> build/installer/Songsmith-EQ-<version>-macOS.pkg
```

The version comes from the shared `VERSION` file (see [Releasing](#releasing)). The installer's layout and pages are in `installer/macos/` (`distribution.xml`, `resources/`, and the `preinstall`/`postinstall` scripts in `scripts/`). It isn't signed or notarised. CI builds it as an internal test artifact (`Songsmith-EQ-macOS-pkg`, next to the zip); it is never published. Releases carry only the two suite installers.

`installer/macos/test-installer.sh` is CI's end-to-end test. It really installs system-wide (and removes legacy plug-ins), so it refuses to run outside CI unless you pass `--i-know-this-installs-system-wide`.

#### Uninstalling

Quit your DAW, then run:

```sh
installer/macos/uninstall.sh
```

It asks for your password, removes `Songsmith EQ.vst3`, `Songsmith EQ.component` and `Songsmith EQ.app`, and makes macOS forget the installer's package receipts. Your presets in `~/Library/Songsmith Studio/Songsmith EQ/Presets` are kept. If you installed by hand into `~/Library/Audio/Plug-Ins`, delete those copies yourself.

---

### Troubleshooting

- **The DAW doesn't see the plugin:** check the install path, then force a full rescan in the DAW.
- **CMake can't find JUCE:** check that `-DJUCE_DIR` points at the folder that contains JUCE's own `CMakeLists.txt`. If you change it, delete the `build` folder and configure again.
- **The install script says the plugin is in use:** close the DAW and run it again.
