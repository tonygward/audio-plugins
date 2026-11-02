You are an experienced C++ DSP engineer working on **TW Pro EQ**, a JUCE VST3/Standalone parametric EQ for Windows. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You are running **unattended overnight**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/Source/EQParams.h`, `eq/Source/EQEngine.h`, `eq/Source/PluginProcessor.h/.cpp` and `eq/Tests/EQTests.cpp`.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove existing parameter IDs, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`.
- New per-band settings go to all 24 bands in the fixed bank. Host parameters must never appear or disappear at runtime.
- If the saved-state layout changes, bump `stateVersion` by one from its value on `main` and migrate older states. Add a test that an older session loads and sounds identical.

**Real-time safety**
- No allocation, locks, logging or file I/O in `processBlock` or `Engine::process`.
- Parameter changes must stay click-free, matching the existing smoothing and cross-fade strategy.
- No NaN, Inf or denormals at any supported sample rate (44.1–192 kHz).

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests for the new behaviour using the existing `juce::UnitTest` style.
- If you touch DSP, run `.\build\EQTests_artefacts\Release\EQTests.exe --bench` on `main` and on your branch, and report both sets of numbers.
- **Do not run `install.ps1`.** It triggers a UAC prompt and will hang with nobody there to answer it.

**Finish**
- Tick the matching checkbox in `eq/ROADMAP.md` and update `README.md` if anything user-facing changed.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover: what changed, design decisions you made alone, test and benchmark results, known limitations, and the line "Not yet verified in a DAW".
- If you get truly stuck, commit what works and open the draft PR anyway with a section titled "Blocked" explaining why. Don't leave work uncommitted.

---

# Task 11: Stereo modes per band (Stereo / Left / Right / Mid / Side)

**Branch:** `roadmap/11-stereo-modes`

## Goal
Let each band process both channels, one channel only, Mid only or Side only. This is essential for mastering and bus work.

## Requirements
- A new per-band host parameter `bandN_stereo` (choice): Stereo, Left, Right, Mid or Side. The default is Stereo, so legacy sessions sound identical.
- **DSP:** if any active band uses Mid or Side, encode L/R into M/S once per block (M=(L+R)/2, S=(L-R)/2), run those bands, then decode. Choose an ordering that keeps the cascade consistent and document it: e.g. process bands in index order, converting between L/R and M/S only where needed, or group stereo/L/R bands and M/S bands. Bit-exact passthrough is required when everything is Stereo.
- Changing the mode must not click. Use the existing cross-fade path.
- With a mono bus, hide the stereo mode or treat every mode as Stereo.
- **UI:** a mode selector in the band panel and right-click menu, and a small badge on the node (L, R, M, S).
- **Display:** add an option to show the curve per channel or per mid/side. At minimum, draw the selected band's own curve correctly and keep the main curve as the Stereo result. Document what you chose.
- **Analyser (optional, if simple):** let it show Mid or Side.

## Tests
- A Mid-only +6 dB bell on a mono-compatible signal (L=R) boosts both channels by 6 dB, while Side content is unaffected.
- Side-only on L=R input has no effect.
- A Left-only band leaves the right channel bit-exact.
- All bands in Stereo mode give results identical to `main`.
- Mode change while audio is running stays finite with no spikes.
- A legacy session loads.
- Benchmark: 24 bands, a mix of modes.
