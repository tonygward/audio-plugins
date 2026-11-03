# Songsmith Analog Compressor

A feedback FET compressor inspired by the Rev D blackface 1176LN, built with JUCE and CMake. **Work in progress:** this is the plugin shell (step 1). It has parameters, saved state, mono and stereo layouts, and a debug editor. The compressor engine comes in step 2.

- Formats: VST3 and Standalone, plus AU on macOS.
- Identity: manufacturer `Tnyw`, plugin `Sfc1`, AU `aufx Sfc1 Tnyw`. Never change these once released.
- Parameters (stable IDs): `input`, `output`, `attack`, `release`, `ratio`, `allButtons`, `grOff`, `bypass`. See `Source/FETParams.h`.
- Sources and design claims: [docs/references.md](docs/references.md).
- Manual DAW check: [docs/smoke-test.md](docs/smoke-test.md).

## Build and test (Windows)

From the `fet` folder:

```powershell
cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE
cmake --build build --config Release
.\build\FETTests_artefacts\Release\FETTests.exe
```

The plugin is built to `build\SongsmithAnalogCompressor_artefacts\Release\VST3\Songsmith Analog Compressor.vst3`. During development, don't install it system-wide. Load it from the build folder instead (see the smoke test).

On macOS, build with `-G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DJUCE_DIR=...`.
