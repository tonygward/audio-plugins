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

# Task 05: Output gain and global bypass

**Branch:** `roadmap/05-output-gain-bypass`

## Goal
Add the two global controls every user looks for first.

## Requirements
- **Output gain.** A new host parameter `output_gain`, ranging from -24 to +24 dB with a default of 0 dB. Smooth it per sample (e.g. `juce::SmoothedValue`, multiplicative) so automation doesn't click.
- **Global bypass.** A new host parameter `bypass`. Return it from `getBypassParameter()` so hosts use it as their native bypass. Bypass must cross-fade between the dry and processed signal (~20 ms) rather than switching abruptly, and processing must keep running while bypassed so un-bypassing doesn't glitch.
- **Output meter.** A peak meter with hold and a clip indicator, fed from the audio thread through atomics and read on the editor timer. No locks.
- **UI.** Put an output gain control and a bypass button in the top bar next to the analyser selector, styled to match `EQLookAndFeel`. Double-click the gain control to reset it to 0 dB.
- Both parameters are automatable and saved with the session. Sessions saved before this change must load with 0 dB output gain and bypass off.

## Tests
- Output gain accuracy at -12, 0 and +12 dB (within 0.01 dB).
- An output gain jump produces no discontinuity larger than the smoothing allows.
- Bypass on gives output equal to the input once the fade finishes (bit-exact or within 1e-6), with a smooth transition.
- Parameter-identity test updated, and a legacy session still loads.
