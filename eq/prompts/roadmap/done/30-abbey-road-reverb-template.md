You are an experienced C++ DSP engineer and mix engineer working on **Songsmith EQ** (by Songsmith Studio, formerly TW Pro EQ), a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/prompts/roadmap/done/20-starting-point-templates.md` and `26-starting-points-review.md`, `eq/docs/starting-points.md`, `eq/Source/PresetManager.h/.cpp` (`FactoryBand`, `FactoryPreset`, the `off()` and `dynamic()` helpers, how the current preset is saved as `presetName` + `presetFactory`, and the "Reverb return" template), `eq/Source/PresetBar.cpp`, `eq/Source/EQParams.h` and the `StartingPointTests` and factory-preset tests in `eq/Tests/EQTests.cpp`.
- Then read the sources under **Grounding**.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter IDs, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`.
- Keep every factory preset at its current **index**: hosts can store program numbers. New templates go at the end.
- This task should not change the saved-state layout. If it must, bump `stateVersion` and add a migration test.

**Real-time safety**
- No allocation, locks, logging or file I/O in `processBlock` or `Engine::process`. Loading a template must not click.

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests in the existing `juce::UnitTest` style.
- **Do not run `install.ps1`.**

**Finish**
- Tick the checkbox for this task in `eq/ROADMAP.md`, update the "Starting points" bullet in `README.md` (count and list), and add a section for the new template to `eq/docs/starting-points.md` with the source of every band.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, the band table, every value you chose differently from this prompt (and why), test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 30: "Abbey Road reverb" template, from Tony's Pro-Q preset

**Branch:** `roadmap/30-abbey-road-reverb`

## Why

The "Abbey Road reverb trick" is the classic way to EQ a reverb send: put the EQ **before** the reverb, high-pass it at around 600 Hz and low-pass it at around 10 kHz, so the reverb has no mud and no sizzle and sits behind the vocal. Tony has his own version saved as a FabFilter preset and uses it on his vocal reverb chains. Songsmith EQ should ship it as a template.

## Grounding

**Tony's preset (the main source):** `C:/Users/tony/OneDrive/Documents/FabFilter/Presets/Pro-Q 4/Tony/Abbey Road Reverb Trick.ffp` (saved 26 Jul 2026). It's a plain-text INI file. Read it yourself; don't change it. There's an older Pro-Q 3 version next to it in `Pro-Q 3/Tony/` (binary, Jul 2024): mention it, but use the Pro-Q 4 one.

How to read it: `Band N Frequency` is **log2(Hz)**; `Band N Q` is normalised, **Q = 0.025 × 1600^value** (so 0.5 is Q 1.0); `Band N Gain` is dB; `Band N Shape` is 0 Bell, 2 Low Cut, 4 High Cut; `Slope=2` is Pro-Q's default slope (check what that is in Pro-Q 4 and say; if you can't confirm it, use 12 dB/oct and flag it). Only bands with `Used=1` matter. `Auto Gain=1` is on.

Decoded, it is:

| Band | Shape | Frequency | Gain | Q |
|---|---|---|---|---|
| 1 | Low Cut, default slope | 377 Hz | — | — |
| 2 | Bell | 594 Hz | +0.54 dB | 1.92 |
| 3 | High Cut, default slope | 17.06 kHz | — | — |
| 4 | Bell | 10.09 kHz | +0.41 dB | 2.16 |
| 5 | Bell | 2.00 kHz | −3.0 dB | 3.02 |

Check this decode against the file and say if anything differs. The same values appear, decoded by Tony's catalogue, in `C:/dev/music-recording/fx-chains/saved-chains.md` (the "Vocals Reverb (Dual)" chain: Pro-Q 4 → UADx Pure Plate → ValhallaRoom → Pro-C 2), which confirms it's the EQ in front of his vocal reverbs.

Also read: `C:/dev/music-recording/mixing/reverb-template.md` and `mixing/between-you-and-me.md` ("every reverb filtered, usually high-passed at 200–500 Hz and low-passed at 10–12 kHz"). Search `C:/dev/music-recording/transcripts/` for "Abbey Road" and reverb EQ, and cite anything Joe Gilder says about it.

Note that Tony's version is gentler than the textbook trick: a 377 Hz low cut rather than 600 Hz, and a 17 kHz high cut rather than 10 kHz, with a −3 dB dip at 2 kHz to keep sibilance and harshness out of the reverb. The two tiny bells (+0.5 dB at 594 Hz and 10 kHz) look like they mark the textbook 600 Hz / 10 kHz points.

## The template

Add **"Abbey Road reverb"** at the end of the list, matching Tony's preset band for band (all five bands, on, in that order, so it looks the same as his Pro-Q). Use the closest slope Songsmith EQ offers to Pro-Q's default. Hint (under about 100 characters), for example: "Put this before the reverb. Raise the low cut towards 600 Hz for a cleaner, thinner tail."

- It's a reverb-send EQ, so loudness matching doesn't mean much (Tony has Pro-Q's Auto Gain on). Compute the output gain the same way task 20 does for the other templates so the test passes, and say what it came to.
- The "boosts ≤ cuts" test should already pass (two small boosts, one 3 dB cut and two filters). If it doesn't, add a commented exception citing Tony's preset.
- Compare it with the existing "Reverb return" template and say in the PR how they differ. Leave "Reverb return" as it is.

## Tests

- The template loads, uses five bands with the types, frequencies (within 1%), gains and Q values above, and produces finite output at 44.1, 48 and 96 kHz.
- Level matching and "boosts ≤ cuts", as for every template.
- Every existing factory preset keeps its index.

## Out of scope

- Reading `.ffp` files at runtime, or a general FabFilter preset importer. The values are copied in by hand.
- Putting "Abbey Road" anywhere except the template name and hint. It's the name of a well-known technique, not a claim of affiliation; don't use any logo or say it comes from the studio.
