You are an experienced C++ DSP engineer and mix engineer working on **Songsmith EQ** (by Songsmith Studio, formerly TW Pro EQ), a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/prompts/roadmap/done/20-starting-point-templates.md` and `26-starting-points-review.md`, `eq/docs/starting-points.md`, `eq/Source/PresetManager.h/.cpp` (`FactoryBand`, `FactoryPreset`, the `off()` and `dynamic()` helpers, how the current preset is saved as `presetName` + `presetFactory`, and the "Drum bus" and "Snare" templates), `eq/Source/PresetBar.cpp`, `eq/Source/EQParams.h` and the `StartingPointTests` and factory-preset tests in `eq/Tests/EQTests.cpp`.
- Then read the sources under **Grounding**: the settings themselves, not only the summaries.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter IDs, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`.
- Keep every factory preset at its current **index**: hosts can store program numbers. New presets go at the end. If task 30 (Abbey Road reverb) is already on `main`, go after it.
- This task should not change the saved-state layout. If it must, bump `stateVersion` and add a migration test.

**Real-time safety**
- No allocation, locks, logging or file I/O in `processBlock` or `Engine::process`. Loading a preset must not click.

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests in the existing `juce::UnitTest` style.
- **Do not run `install.ps1`.**

**Finish**
- Tick the checkbox for this task in `eq/ROADMAP.md`, update the "Starting points" bullet in `README.md` (count and list), and add a section per new preset to `eq/docs/starting-points.md` with the source (file and line) of every band.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, the band table for each preset with its source lines, every value you chose differently from this prompt (and why), test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 31: Tom presets from *Between You and Me*

**Branch:** `roadmap/31-tom-presets`

## Why

Tony's 2021 album *Between You and Me* used a **saved tom chain** to keep the toms consistent across the songs (`mixing/between-you-and-me.md`, "Lessons" 5: "saved chains for vocals, backing vocals, toms and cymbals, which kept the album consistent"). Songsmith EQ has no tom preset. Add Tony's tom EQ as presets so he can use it on new songs.

These are **Tony's own settings**, not Joe Gilder's advice, so they're allowed to boost more than the starting-point templates. Say so in the PR and in `starting-points.md`.

## Grounding

All in `C:/dev/music-recording`.

- `fx-chains/songs/tony-ward.md`, section `## _Between You and Me` (line ~561 to ~2344): every tom track and the Toms bus in each album song. Skip *Happy Blues in 7-4* and *Tell Me That You Love Me*: they aren't on the album. `mixing/between-you-and-me.md` says which song files are closest to the release (the 2023 remixes of *Grains of Sand* and *You Want More* are later work; note it if you use them).
- `fx-chains/saved-chains.md`: the saved "Toms" chain and the "Anguish - Floor/Mid/High Tom" and "Anguish - Toms" chains.
- For comparison only (made after the album): Tony's Pro-Q 4 presets `C:/Users/tony/OneDrive/Documents/FabFilter/Presets/Pro-Q 4/Tony/Drums - Floor Tom.ffp` and `Drums - Mid Tom.ffp` (Jan 2025). Plain-text INI: `Frequency` is log2(Hz), `Q = 0.025 × 1600^value`, `Gain` is dB, `Shape` 0 Bell / 2 Low Cut / 4 High Cut. Mention in the PR how they compare with the album settings, but don't base the presets on them.

### What the album files show

A first read found a clear repeated EQ. Check each value against the file and correct it if this is wrong.

**Rack toms (Mid Tom and High Tom)** — the Waves SSL E-Channel settings repeat exactly on the High Tom in *Gone from Here* (line ~1114) and on the Mid and High Tom in *You Want More* (lines ~2295, ~2315), and the same values sit on the Toms bus in *Gone from Here* (~1123):

| Move | SSL E-Channel setting |
|---|---|
| High-pass | 162 Hz |
| LF | +4.6 dB at 163 Hz, bell (`LF Type On`) |
| LMF | −11.9 dB at 670 Hz, Q 1.7 |
| HMF | +5.9 dB at 4.07 kHz (Q not saved: use the E-Channel default and say what it is) |
| HF | +3.0 to +4.3 dB at 10.32 kHz, bell (`HF Type On`); use +3.3 dB, the *You Want More* value, unless the release files say otherwise |

**Floor tom** — *You Want More* (~2304) and *Yesterday's Toy* (~2147):

| Move | SSL E-Channel setting |
|---|---|
| High-pass | none (or 18 Hz) |
| LF | +7.8 dB at 122 Hz, shelf (no `LF Type On`) |
| LMF | −6.6 dB at 930 Hz, Q 1.7 |
| HMF | +5.9 dB at 4.07 kHz |
| HF | +1.6 dB (*You Want More*) or +6.8 dB (*Yesterday's Toy*) at 10.32 kHz, shelf |

**Toms bus** — the Neutron 3 Equalizer on the Toms bus in *Ethereal Engagement* (~985), *Gone from Here* (~1122), *Nothing Left To Give* (~1864) and *You Want More* (~2322) shares a shape: a steep high-pass at about 40–70 Hz, a broad cut of −4.5 to −7 dB around 90–150 Hz, a −8 dB cut near 317 Hz (Ethereal Engagement, Gone from Here), and a steep low-pass at about 10–11 kHz. Neutron's "Band Shelf" and Q values need care: read what each shape means before translating it.

Also look at *Anguish* (Neutron 3 Equalizer on Mid and High Tom: −3.3 dB band shelf at 299 Hz, +2.45 dB at 136 Hz and 1.9 kHz, high-pass 45 Hz, low-pass 13.6 kHz) and *Escape Hatch* (Toms bus: SSL HP 50 Hz, LMF −3.2 dB, HMF +3.8 dB; Neutron −5.75 dB at 734 Hz, +3.68 dB at 3 kHz). Decide whether they're the same idea in a gentler form and say so.

## The presets

Add three presets at the end of the list:

1. **"Rack tom"**: the SSL rack-tom EQ above, translated band for band.
2. **"Floor tom"**: the SSL floor-tom EQ above.
3. **"Toms bus"**: the common shape of the Neutron Toms-bus EQs, using the median of the songs' values where they differ.

Translating from the SSL: the E-Channel's bands aren't textbook bells. Match the **curve**, not the knob numbers: model each SSL band (LF bell, LMF, HMF, HF bell/shelf, 18 dB/oct high-pass) with Songsmith EQ's filters, compare the summed response with the SSL's at 20 Hz–20 kHz (from the Waves manual or a measured curve if you can find one; otherwise state the assumption), and get it within about 1 dB where it matters (60 Hz–12 kHz). Put the comparison in the PR.

- Hints (under about 100 characters), for example: "Tony's rack-tom EQ from Between You and Me. Lower the 670 Hz cut if the tom sounds hollow."
- Output gain: compute it as task 20 does for the templates so pink noise comes out within ±0.5 dB.
- "Boosts ≤ cuts": these presets may fail it (the floor tom boosts +7.8 dB). Add a commented exception for each, citing the album files, rather than changing Tony's values.
- The 162 Hz high-pass with a +4.6 dB bell at 163 Hz makes a resonant bump right at the cut. Keep it (it's what's on the album) and mention it in the hint or `starting-points.md` so Tony can decide whether it's what he wants on new songs.

## Tests

- Each preset loads, uses the expected bands and types, matches the table values (frequency within 1%, gain within 0.1 dB), and produces finite output at 44.1, 48 and 96 kHz.
- Level matching within ±0.5 dB.
- "Boosts ≤ cuts", with the commented exceptions.
- Every existing factory preset keeps its index.

## Out of scope

- Compression, gating, transient shaping or limiting from the tom chains (Neutron's compressor, SSL dynamics, L1+). Note in `starting-points.md` that the album chain also had those, so the EQ alone won't recreate the sound.
- Kick, snare, hi-hat or cymbal presets.
