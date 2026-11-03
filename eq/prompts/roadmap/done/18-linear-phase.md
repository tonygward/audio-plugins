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

# Task 18: Linear-phase mode

**Branch:** `roadmap/18-linear-phase`

## Goal
An optional global linear-phase processing mode for mastering, alongside the existing zero-latency minimum-phase mode.

## Requirements
- A new host parameter `phase_mode` (choice): Zero latency (current), Linear phase. Optionally add a "Natural phase" mode if there's time. The default is Zero latency, so legacy sessions are bit-identical.
- **Linear-phase design:** build a symmetric FIR whose magnitude matches the combined EQ magnitude, using the same `getMagnitudeDb` maths the display uses. Sample it on a dense grid, set zero phase, IFFT, window, and then apply it with partitioned FFT convolution (`juce::dsp::Convolution` is fine if it supports runtime IR swaps without allocating on the audio thread; otherwise write a uniformly partitioned convolver).
- **Rebuilding the IR:** do it on a background thread when parameters change, with rate limiting, and swap it in using a lock-free handover plus a short cross-fade between the old and new IR outputs. Never allocate or lock on the audio thread.
- **Latency:** report it with `setLatencySamples` and change it only when the mode or sample rate changes. Pick an FIR length that gives good low-frequency accuracy at each sample rate (a "Low / Medium / High" quality choice is welcome). Document the latency for each choice.
- Fix `getTailLengthSeconds` for linear-phase mode.
- Make switching modes as smooth as possible. A short fade when latency changes is acceptable. Document what the host sees.
- Dynamic EQ (if Task 17 exists) may be unavailable in linear-phase mode. If so, grey those controls out and say so in the UI and in the PR.

## Tests
- Linear-phase magnitude matches minimum-phase magnitude within ±0.2 dB from 30 Hz to 18 kHz for a range of curves.
- The impulse response is symmetric (linear phase), within tolerance.
- Reported latency equals the measured delay.
- Rapid parameter changes stay finite and don't click badly (bounded discontinuity).
- Zero-latency mode is bit-identical to `main`.
- Benchmark every quality setting.
