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

# Task 19: Optional oversampling

**Branch:** `roadmap/19-oversampling`

## Goal
Reduce "cramping" of high-frequency bells and shelves near Nyquist at 44.1/48 kHz.

## Requirements
- A new host parameter `oversampling` (choice): Off, 2x, 4x. The default is Off, so legacy sessions are bit-identical.
- Use `juce::dsp::Oversampling` (polyphase IIR for low latency, or FIR for linear phase; pick per mode and document it). Allocate in `prepareToPlay`. If changing the factor needs reallocation, handle it safely, for example by preparing all factors up front.
- Run the `Engine` at the oversampled rate. Its `prepare` must take the higher rate, and coefficients must be computed for it.
- Report the added latency with `setLatencySamples`. If Task 18's linear-phase mode exists, combine the latencies correctly.
- Optionally make it automatic: only oversample when a band sits above ~1/4 of the base sample rate. Only do this if it can't cause glitches.
- **Display:** the drawn curve must reflect the oversampled response (less cramping) so it stays truthful.

## Tests
- At 44.1 kHz, a 16 kHz +12 dB bell with Q 2: with 2x/4x, the measured response is closer to the analogue prototype than with Off. Report the numbers.
- Reported latency equals the measured delay.
- Off is bit-identical to `main`.
- All sample rates stay finite.
- Benchmark 24 bands at Off, 2x and 4x.
