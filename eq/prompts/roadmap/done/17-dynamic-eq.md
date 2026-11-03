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

# Task 17: Dynamic EQ per band

**Branch:** `roadmap/17-dynamic-eq`

## Goal
Each band can optionally change its gain depending on the level in its own frequency region. This covers de-essing, taming resonances and lifting quiet detail.

## Requirements
- **New per-band host parameters** (all 24 bands): `bandN_dyn_on`, `bandN_threshold` (-60 to 0 dBFS), `bandN_range` (-24 to +24 dB, the maximum dynamic gain change), `bandN_attack` (0.1–100 ms), `bandN_release` (10–1000 ms). Ratio is optional. If you add it, document the gain computer. Every default must leave sessions sounding identical (dynamics off).
- Dynamics apply to Bell and shelf types only. For other types, hide or disable the controls.
- **Detector:** a band-pass on the band's own frequency and Q, fed from the band's input (or from the sidechain bus if Task 12 added one and the user picks "external"). Use peak or RMS with attack/release smoothing, and a soft knee. Compute the detector per channel, then link it (max of L/R) by default.
- **Applying it:** feed the dynamic gain into the existing per-band gain ramp. Coefficients must stay valid SVFs, so recompute them from the ramped gain at the existing update interval. Never interpolate raw coefficients. Choose an update interval that is responsive enough for attack times down to 1 ms without blowing up CPU, and document it.
- **Display:** for dynamic bands, show the static target and the live curve (e.g. a translucent area between them) and a small gain-reduction indicator on the node. Send the live gain to the UI through atomics.
- **UI:** dynamic controls in the band panel, collapsed unless dynamics are on, plus a right-click menu toggle.
- CPU must stay reasonable: bands with dynamics off cost nothing extra.

## Tests
- A static tone below the threshold gives the static response.
- A tone above the threshold reaches static gain + range within tolerance once it settles.
- Attack and release timing are within ±20% of their settings.
- Dynamics off is bit-identical to `main`.
- Extreme settings and rapid automation stay finite.
- A legacy session loads.
- Benchmark: 24 dynamic bands at 48 kHz, 64-sample blocks. Report the numbers.
