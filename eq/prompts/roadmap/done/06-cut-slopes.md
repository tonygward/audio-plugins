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

# Task 06: Selectable slopes for Low Cut and High Cut

**Branch:** `roadmap/06-cut-slopes`

## Goal
Low Cut and High Cut currently have a single fixed 12 dB/oct slope from one SVF stage. Make the slope selectable.

## Requirements
- A new per-band host parameter `bandN_slope` (choice): 6, 12, 18, 24, 36 or 48 dB/oct. The default is 12, so existing sessions sound identical.
- Implement the slopes by cascading stages: a one-pole stage (trapezoidal / TPT) for the odd 6 dB part, and 2-pole SVF sections with **Butterworth Q values per section** for the rest. Steeper slopes must stay maximally flat, not just several identical sections stacked. Decide how the user's Q interacts with steep slopes, for example applying Q as resonance only on the last section, and document the choice.
- Preallocate enough stages per band and channel for 48 dB/oct. No allocation when the slope changes.
- A slope change must not click. Reuse the existing type-change cross-fade path.
- The slope parameter only matters for Low Cut and High Cut. For other filter types, hide or disable its control.
- `getMagnitudeDb` and the drawn curve must use the same cascade the DSP uses.
- **UI:** a slope selector in the band panel, a slope entry in the node's right-click menu, and the slope shown in the tooltip.
- Any CPU cost must only apply to cut bands with steeper slopes.

## Tests
- For each slope, measure attenuation one and two octaves past the cutoff: it should match N×6 dB/oct within ±1 dB, and be -3 dB at the cutoff for Butterworth.
- Test at 44.1, 96 and 192 kHz.
- Slope change while audio is running stays finite and has no discontinuity spike.
- A legacy session loads with a 12 dB/oct slope and sounds identical.
- Benchmark with 24 bands at 48 dB/oct.
