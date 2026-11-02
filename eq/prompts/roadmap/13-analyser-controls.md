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

# Task 13: Analyser controls (freeze, peak hold, tilt, resolution, speed)

**Branch:** `roadmap/13-analyser-controls`

## Goal
Make the analyser a proper mixing tool.

## Requirements
- **Freeze:** stop updating and keep the current spectrum on screen.
- **Peak hold:** a thin line holding the maximum level per bin, with a reset (click the analyser area or use a menu item).
- **Tilt:** 0, 3, 4.5 or 6 dB/oct around 1 kHz so pink-ish material looks flat. The default is 4.5.
- **Resolution:** FFT size of 2048, 4096 or 8192. Changing it must not allocate on the audio thread. Preallocate the largest size, or reallocate safely on the message thread while capture pauses.
- **Speed:** slow, medium or fast decay/averaging.
- **Range:** the analyser's dB floor and ceiling, separate from the EQ display range.
- Put these in a compact analyser settings popup next to the existing analyser selector. Save them with the UI state (not as host parameters).
- Keep CPU low and only do the work while the editor is open.

## Tests
- Tilt: a white-noise or pink-noise reference shows the expected slope change, within ±0.5 dB/oct.
- Each FFT size passes the existing calibration and frequency-accuracy tests.
- Peak hold keeps the maximum after the signal drops.
- Freeze keeps the data unchanged while new audio arrives.
- Settings survive saving and restoring.
