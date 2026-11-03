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

# Task 07: Solo a band (audition)

**Branch:** `roadmap/07-band-solo`

## Goal
Let users hear only the frequency region a band affects, to find problem frequencies quickly.

## Requirements
- Hold Alt and click/drag a node to audition it. While it is held, the output is a band-pass centred on that band's frequency, and dragging sweeps it. Releasing the mouse returns to normal output. Also add a "Solo" toggle in the band panel and right-click menu that latches.
- Set the band-pass width from the band's Q, but keep it musically useful: clamp to a sensible range so a very wide bell doesn't just play the full signal. For shelves and cuts, audition the region the filter affects, and document what you chose.
- Solo is **not** a host parameter and **not** saved. It is a transient editing state, passed to the audio thread through an atomic. Clear it when the editor closes.
- Fade in and out of solo (~20 ms) so it doesn't click.
- Draw the soloed band clearly in the display, e.g. dim the other nodes and show the band-pass region shaded.
- Solo must not create undo history entries.

## Tests
- Solo active: a tone at the band frequency passes at roughly unity gain, and tones two octaves away are attenuated by at least 18 dB.
- Releasing solo returns bit-identical normal processing once the fade finishes.
- Closing the editor clears solo.
