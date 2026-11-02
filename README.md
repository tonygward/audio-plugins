# audio-plugins

Audio plugins built with [JUCE](https://juce.com) and CMake.

| Plugin | Folder | Formats |
|---|---|---|
| **TW Pro EQ**: parametric EQ with a spectrum analyser | [`eq/`](eq/) | VST3, Standalone |

---

## TW Pro EQ

### Just want to use it?

Pre-built **Windows** binaries are on the [Releases page](https://github.com/tonygward/audio-plugins/releases).

1. Download `TW-Pro-EQ-<version>-win64.zip`.
2. Extract it and copy the `TW Pro EQ.vst3` folder into `C:\Program Files\Common Files\VST3`.
3. Rescan plugins in your DAW.

There's no pre-built Mac binary yet. Mac users need to build from source (see below).

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

> ⚠️ The Mac build hasn't been tested yet. If something breaks, please open an issue.

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

The plugin ends up at:

```
eq/build/ThreeBandEQ_artefacts/Release/VST3/TW Pro EQ.vst3
```

#### Install

Copy the plugin into your user VST3 folder (no admin rights needed):

```sh
mkdir -p ~/Library/Audio/Plug-Ins/VST3
rm -rf ~/Library/Audio/Plug-Ins/VST3/"TW Pro EQ.vst3"
cp -R build/ThreeBandEQ_artefacts/Release/VST3/"TW Pro EQ.vst3" ~/Library/Audio/Plug-Ins/VST3/
```

Then rescan plugins in your DAW.

**Logic Pro and GarageBand** only load **Audio Unit (AU)** plugins, not VST3. To build an AU as well, change the `FORMATS` line in `eq/CMakeLists.txt` to:

```cmake
FORMATS VST3 AU Standalone
```

After rebuilding, copy `build/ThreeBandEQ_artefacts/Release/AU/TW Pro EQ.component` into `~/Library/Audio/Plug-Ins/Components/`.

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
