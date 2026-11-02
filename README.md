# audio-plugins

[![Build](https://github.com/tonygward/audio-plugins/actions/workflows/build.yml/badge.svg)](https://github.com/tonygward/audio-plugins/actions/workflows/build.yml)

Audio plugins built with [JUCE](https://juce.com) and CMake.

| Plugin | Folder | Formats |
|---|---|---|
| **TW Pro EQ**: parametric EQ with a spectrum analyser | [`eq/`](eq/) | VST3, AU (macOS), Standalone |

---

## TW Pro EQ

### Features

- Up to 24 bands: Bell, Low Shelf, High Shelf, Low Cut, High Cut and Notch
- Low Cut and High Cut slopes of 6, 12, 18, 24, 36 or 48 dB/oct (Butterworth; raise Q for a resonant peak at the cutoff)
- **Stereo modes per band:** Stereo, Left, Right, Mid or Side (a small L/R/M/S badge marks the node). The "Curve" selector shows the combined curve for one channel
- Click-free parameter smoothing, undo/redo, and a response curve drawn from the real DSP coefficients
- Pre/post spectrum analyser, plus an optional **sidechain spectrum** (orange) to spot masking: route another track to the sidechain input and pick "Sidechain: spectrum" or "+ overlap" (shades where both tracks are within 6 dB above -60 dBFS). The selector appears once the sidechain receives signal; the sidechain is never heard
- **Solo a band:** Alt-click (and drag) a node to hear only the region it affects while the mouse is held, or latch it with the Solo button in the band panel or the right-click menu
- **Output gain** (±24 dB, double-click to reset) with a peak meter, peak hold and clip indicator (click the meter to clear it)
- **Global bypass** that your DAW uses as its native bypass, with a smooth 20 ms cross-fade
- **Presets:** 8 factory starting points plus your own presets, saved as `.twpreset` files in `%APPDATA%\TW Pro EQ\Presets` (Windows) or `~/Library/TW Pro EQ/Presets` (macOS). Use the arrows and the menu in the top bar; `*` means the settings differ from the loaded preset. Loading a preset is one undo step.

### Just want to use it?

Pre-built **Windows** binaries are on the [Releases page](https://github.com/tonygward/audio-plugins/releases).

1. Download `TW-Pro-EQ-<version>-win64.zip`.
2. Extract it and copy the `TW Pro EQ.vst3` folder into `C:\Program Files\Common Files\VST3`.
3. Rescan plugins in your DAW.

**macOS:** every CI run uploads a `TW-Pro-EQ-macOS-zip` artifact (VST3 + AU, universal) on the [Actions page](https://github.com/tonygward/audio-plugins/actions/workflows/build.yml). It isn't signed or notarised, so after copying the plug-ins (see Install below) you may need to run `xattr -dr com.apple.quarantine` on them. Or build from source.

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
eq\build\ThreeBandEQ_artefacts\Release\VST3\TW Pro EQ.vst3
```

There's also a standalone app in `eq\build\ThreeBandEQ_artefacts\Release\Standalone\`.

#### Install

```powershell
.\install.ps1              # installs the Release build
.\install.ps1 -Config Debug
```

The script asks for administrator rights, copies the plugin into `C:\Program Files\Common Files\VST3`, and removes the old bundle from before the rename (`3-Band EQ.vst3`). It refuses to overwrite a plugin that a running DAW has loaded, so close the DAW first.

To install by hand instead, copy the `TW Pro EQ.vst3` folder into `C:\Program Files\Common Files\VST3`.

#### Run the tests

```powershell
.\build\EQTests_artefacts\Release\EQTests.exe          # unit tests
.\build\EQTests_artefacts\Release\EQTests.exe --bench  # CPU benchmark
```

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
eq/build/ThreeBandEQ_artefacts/Release/VST3/TW Pro EQ.vst3
eq/build/ThreeBandEQ_artefacts/Release/AU/TW Pro EQ.component
```

#### Install

Copy the plug-ins into your user plug-in folders (no admin rights needed). **Logic Pro and GarageBand** load the **AU** (`.component`); most other DAWs load the VST3.

```sh
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/"TW Pro EQ.vst3" ~/Library/Audio/Plug-Ins/Components/"TW Pro EQ.component"
cp -R build/ThreeBandEQ_artefacts/Release/VST3/"TW Pro EQ.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R build/ThreeBandEQ_artefacts/Release/AU/"TW Pro EQ.component" ~/Library/Audio/Plug-Ins/Components/
```

Then rescan plugins in your DAW. If Logic doesn't list the AU straight away, run `killall -9 AudioComponentRegistrar` and reopen Logic. You can check the AU yourself with `auval -v aufx Eq3b Tnyw`.

#### Run the tests

```sh
./build/EQTests_artefacts/Release/EQTests          # unit tests
./build/EQTests_artefacts/Release/EQTests --bench  # CPU benchmark
```

---

### Troubleshooting

- **The DAW doesn't see the plugin:** check the install path, then force a full rescan in the DAW.
- **CMake can't find JUCE:** check that `-DJUCE_DIR` points at the folder that contains JUCE's own `CMakeLists.txt`. If you change it, delete the `build` folder and configure again.
- **The install script says the plugin is in use:** close the DAW and run it again.
