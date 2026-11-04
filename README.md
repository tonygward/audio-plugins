# audio-plugins

[![Build](https://github.com/tonygward/audio-plugins/actions/workflows/build.yml/badge.svg)](https://github.com/tonygward/audio-plugins/actions/workflows/build.yml)

Audio plugins built with [JUCE](https://juce.com) and CMake.

| Plugin | Folder | Formats |
|---|---|---|
| **Songsmith EQ**: parametric EQ with a spectrum analyser | [`eq/`](eq/) | VST3, AU (macOS), Standalone |

---

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
- **Interface size** (the `...` menu → Interface size): 75 %, 100 %, 125 %, 150 %, 175 % or 200 %. Everything scales together (text, controls, nodes, lines), and the window resizes to match. The setting is saved with the session and remembered on your computer, so new instances open at the size you last chose. The window opens at 1280 × 760 (at 100 %) and can be resized from 760 × 440 up to 3840 × 2400 (a full 4K screen); on large windows the controls and text step up in size too, while the graph gets most of the extra room. Drawing stays sharp with Windows display scaling and on Retina screens
- **Scale** (top bar): scales every band's gain together, -100 % to +200 % (0 % = flat, negative inverts; cuts and notches are unaffected). Double-click for 100 %. It's automatable and stored in presets
- Spectrum analyser, before or after the EQ ("Analyser: before EQ" / "after EQ"), with settings (the `...` button): freeze, peak hold (click the graph to reset), tilt 0/3/4.5/6 dB/oct, resolution low/medium/high (2048, 4096 or 8192 points), slow/medium/fast speed and dBFS range. Plus an optional **side chain spectrum** (orange) to spot masking: route another track to the side chain input and pick "Side Chain: spectrum" or "Side Chain: spectrum and overlap" (shades where both tracks are within 6 dB above -60 dBFS). The selector appears once the side chain receives signal; the side chain is never heard
- **Notes:** frequencies show the nearest note and cents (`440 Hz · A4 +0¢`); type note names such as `A2`, `C#5` or `Eb3` into the frequency field; hold Ctrl (Cmd on macOS) while dragging to snap to semitones; click **Piano** (third row of controls, off by default) or right-click the empty graph to show a piano strip along the frequency axis
- **Solo a band:** Alt-click (and drag) a node to hear only the region it affects while the mouse is held, or latch it with the Solo button in the band panel or the right-click menu
- **Output gain** (±24 dB, double-click to reset) and a tall output meter on the right of the graph (-60 to +6 dBFS scale, peak hold with the held peak as a number, clip indicator at the top: click the meter to clear it)
- **Global bypass** that your DAW uses as its native bypass, with a smooth 20 ms cross-fade
- **Auto Gain** (top bar): matches the output loudness to the input (K-weighted, 2 s window, ±12 dB, frozen on silence) so you judge EQ moves at equal loudness; the button shows the correction, for example "Auto Gain +1.2 dB"
- **Starting points** (in the preset menu): 13 templates for common sources: lead vocal, backing vocals, spoken word, acoustic guitar, electric guitar, electric guitar bus, bass guitar, drum bus, snare, reverb return, a five-zone teaching layout, acoustic guitar in a full mix, and acoustic guitar pickup (piezo DI). They're based on advice from Joe Gilder's [Home Studio Corner](https://homestudiocorner.com) videos, and they aren't endorsed by him. Where every band comes from: [`eq/docs/starting-points.md`](eq/docs/starting-points.md). They work the way he does: cuts before boosts, small moves, "only if needed" bands loaded but switched off, and output gain set so turning the EQ on and off compares tone, not loudness. Hover over the preset name for a one-line hint. Treat the frequencies as places to start listening, and move each band to what you hear in the mix
- **Presets:** 8 factory presets ("Acoustic guitar" is now "Bright acoustic") plus your own presets, saved as `.sspreset` files in `%APPDATA%\Songsmith Studio\Songsmith EQ\Presets` (Windows) or `~/Library/Songsmith Studio/Songsmith EQ/Presets` (macOS). Presets saved by TW Pro EQ (`.twpreset`) still load: the first time you open the preset menu or use the preset arrows, they're copied from the old `TW Pro EQ\Presets` folder, which is left as it was. Use the arrows and the menu in the top bar; `*` means the settings differ from the loaded preset. Loading a preset is one undo step.

### Just want to use it?

Pre-built **Windows** downloads are on the [Releases page](https://github.com/tonygward/audio-plugins/releases).

**Installer (recommended)**

1. Download `Songsmith-EQ-<version>-win64-setup.exe` and run it.
2. Windows SmartScreen will probably say it "protected your PC". The installer isn't code-signed (signing needs a paid certificate), so Windows doesn't recognise the publisher. Click **More info**, then **Run anyway**.
3. Click through Setup. It installs the VST3 plugin into the standard VST3 folder and, if you leave it ticked, the standalone app. Close your DAW first: if the plugin is loaded, Setup stops and changes nothing.
4. Rescan plugins in your DAW.

Running a newer installer upgrades in place. Upgrading from TW Pro EQ? Setup removes the old `TW Pro EQ.vst3` for you, and your sessions and presets carry over. To uninstall, use **Settings → Apps → Installed apps → Songsmith EQ**. Your presets are kept.

**Zip (manual install)**

1. Download `Songsmith-EQ-<version>-win64.zip`.
2. Extract it and copy the `Songsmith EQ.vst3` folder into `C:\Program Files\Common Files\VST3`. Upgrading from TW Pro EQ? Delete `TW Pro EQ.vst3` from that folder too. Your DAW sees it as the same plugin, so leaving it there lists the plugin twice.
3. Rescan plugins in your DAW.

**macOS installer (recommended)**

1. Download `Songsmith-EQ-<version>-macOS.pkg` (from the Releases page, or the `Songsmith-EQ-macOS-pkg` artifact of any CI run on the [Actions page](https://github.com/tonygward/audio-plugins/actions/workflows/build.yml)). It's a universal installer for Apple Silicon and Intel Macs.
2. Quit your DAW, then open the `.pkg`. macOS will probably refuse at first and say it can't check the installer for malicious software. That's because it isn't signed or notarised by Apple (that needs a paid developer account), not because anything is wrong with it. Try to open it once, then go to **System Settings → Privacy & Security**, scroll down and click **Open Anyway**. (On older macOS, right-click the `.pkg` and choose **Open**.)
3. Click through the installer and enter your password. It installs, for every user on the Mac:
   - the **VST3** plug-in into `/Library/Audio/Plug-Ins/VST3`,
   - the **Audio Unit** (for Logic Pro and GarageBand) into `/Library/Audio/Plug-Ins/Components`,
   - the **standalone app** into `/Applications`.

   Click **Customize** to leave any of them out.
4. Restart your DAW, or rescan its plug-ins.

Running a newer installer upgrades in place. Upgrading from TW Pro EQ? The installer removes the old `TW Pro EQ` plug-ins (and any copies in your own `~/Library/Audio/Plug-Ins`), and your sessions and presets carry over. Plug-ins installed by the `.pkg` don't need the `xattr` step below. To uninstall, see [Uninstalling](#uninstalling) below.

**macOS zip (manual install)**

Download `Songsmith-EQ-<version>-macOS.zip` (VST3 + AU, universal; also the `Songsmith-EQ-macOS-zip` CI artifact) and copy the plug-ins into place as described under [Install](#install-1) below. The zip isn't signed or notarised either, so after copying the plug-ins you may need to run `xattr -dr com.apple.quarantine` on them. Or build from source.

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

The version comes from `project(... VERSION ...)` in `CMakeLists.txt`. The script is `installer\SongsmithEQ.iss`. The test never touches your real VST3 folder or presets. CI builds both files on every push (artifacts `Songsmith-EQ-Windows-setup` and `Songsmith-EQ-Windows-zip`), ready to attach to a release by hand.

---

### macOS

The Mac build (VST3, AU and Standalone, universal arm64 + x86_64) is built and tested on every push by the GitHub Actions workflow, including Apple's `auval` Audio Unit validation. It hasn't been checked by hand in Logic Pro yet.

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

The version comes from `project(... VERSION ...)` in `CMakeLists.txt`. The installer's layout and pages are in `installer/macos/` (`distribution.xml`, `resources/`, and the `preinstall`/`postinstall` scripts in `scripts/`). It isn't signed or notarised. CI builds it on every push and uploads it as the `Songsmith-EQ-macOS-pkg` artifact, next to the zip, ready to attach to a release by hand.

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

---

### Fonts

The editor uses [Inter](https://rsms.me/inter/) (Regular, Medium and SemiBold, v4.1) by The Inter Project Authors, embedded in the plugin so Windows and macOS look the same. Inter is licensed under the SIL Open Font License 1.1; the licence is in [`eq/Resources/Fonts/OFL.txt`](eq/Resources/Fonts/OFL.txt).
