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

# Task 12: Sidechain analyser (spot masking)

**Branch:** `roadmap/12-sidechain-analyser`

## Goal
Show the spectrum of another track alongside this one, so users can see masking (e.g. kick against bass).

## Requirements
- Add an optional stereo **sidechain input bus**, disabled by default. Update `isBusesLayoutSupported` so the main bus rules stay the same and the sidechain can be disabled, mono or stereo. Studio One must still load the plugin as a plain stereo effect.
- The sidechain is **analysed only**. It must never reach the output.
- Extend `SpectrumAnalyser` (or add a second instance) to capture the sidechain, again only while a viewer is open, as the existing pre/post capture does.
- **Display:** draw the sidechain spectrum in a distinct colour. Add an optional "collision" highlight where both spectra are within N dB of each other above a level floor. Choose sensible defaults and document them.
- **UI:** a toggle for the sidechain spectrum, shown only when the sidechain bus is connected and receiving signal.
- Save the toggle with the UI state.

## Tests
- The sidechain signal has no effect on the output (bit-exact versus no sidechain).
- The sidechain analyser shows correct level and frequency for a sine (reuse the existing calibration tests).
- Every bus layout combination is accepted or rejected as intended.
