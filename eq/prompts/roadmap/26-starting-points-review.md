You are an experienced C++ DSP engineer and mix engineer working on **Songsmith EQ** (by Songsmith Studio, formerly TW Pro EQ), a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/prompts/roadmap/done/20-starting-point-templates.md` (the task that created the templates), `eq/Source/PresetManager.h/.cpp` (`FactoryBand`, `FactoryPreset`, the `off()` and `dynamic()` helpers, and how the current preset is saved as `presetName` + `presetFactory`), `eq/Source/PresetBar.cpp`, `eq/Source/EQParams.h` and the `StartingPointTests` and factory-preset tests in `eq/Tests/EQTests.cpp`.
- Then read the source material under **Grounding**. Read the transcripts themselves, not only the summaries: the summaries are where task 20 went wrong.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter IDs, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`.
- Keep every factory preset at its current **index**: hosts can store program numbers. New templates go at the end.
- Sessions save the loaded preset as `presetName` + `presetFactory`. If you rename a template, check that a session saved with the old name still shows the right preset (add a name alias if needed) and add a test for it.
- This task should not change the saved-state layout. If it must, bump `stateVersion` and add a migration test.

**Real-time safety**
- No allocation, locks, logging or file I/O in `processBlock` or `Engine::process`. Loading a template must not click.

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests in the existing `juce::UnitTest` style.
- **Do not run `install.ps1`.**

**Finish**
- Tick the checkbox for this task in `eq/ROADMAP.md` and update `README.md` (the "Starting points" bullet: count and list).
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, the before → after table for every band you changed, every value you chose differently from this prompt (and why), test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 26: Review the starting points against the transcripts and Tony's own mixes

**Branch:** `roadmap/26-starting-points-review`

## Why

Task 20 built the templates mostly from the short 2026 "5-Minute Mix" videos and the summary in `techniques/joe-gilder.md`. Tony listened to them and says the starting points don't look right, especially **acoustic guitar**. Checking the sources shows why:

- The fullest acoustic video, **`2021-12-02 - How I EQ Acoustic Guitar.md`** ("three cuts and a boost"), wasn't in task 20's reading list. The summary in `techniques/joe-gilder.md` is based on it, but the prompt then told the agent to prefer the 4-minute 2026 video, which only covers two moves.
- So the shipped **"Acoustic guitar body"** template has the low cut and the air shelf switched off, and no 500 Hz cut at all. That's the move Joe calls "cut the plastic" in 2021 and 2023, and the one Tony makes on every acoustic track.
- Task 20 used none of Tony's own mixes, which are documented in detail in `C:/dev/music-recording`.

This task fixes the acoustic templates with concrete values, then audits the other ten templates the same way.

## Grounding

All in `C:/dev/music-recording`. Where Joe's videos and Tony's own settings agree, use them with confidence. Where they disagree, prefer **what Tony actually does on his own recordings** for the acoustic templates (it's his plugin and his guitars), and Joe's most detailed video for everything else. Cite the file and timestamp for every value in the PR.

**Tony's own acoustic mixes**
- `fx-chains/songs/acoustic-moods.md`, lines 30–50: the decoded Pro-Q 4 settings on Tony's steel-string tracks (`Tony`, `Tony (2)`, `Tony 2`). Every track has: Low Cut 50 Hz; Bell 510 Hz, −1 dB, Q 2; a **dynamic** Bell at 1750 Hz, Q 7, range −2.5 dB; High Shelf 12.9 kHz, +1 dB.
- `fx-chains/acoustic-template/01.png`–`06.png`: screenshots of Tony's saved "Acoustic" chain. In `04.png` (the second Pro-Q 4, preset "Acoustic") read the nodes against the grid. They're roughly: steep Low Cut about 55 Hz, Bell about 165 Hz at −1 dB, Bell about 500 Hz at about −3.5 dB (medium-narrow), High Shelf about 12.8 kHz at about +1.7 dB. Measure them yourself from the image and say what you got.
- `mixing/acoustic-moods.md`, "Steel guitar rhythm": high-pass at 100–120 Hz under a bass, a dynamic cut at 200–350 Hz for boom, a gentle 1–2 dB dip at 1.5–2.5 kHz to leave room for a lead, and a shelf lift above 8 kHz.
- `instruments/tony/gibson-j45-standard.md`, `takamine-esf-40c.md`, `yamaha-apx-3.md`, `simon-patrick-woodland-12-cedar.md`: per-guitar notes. The J-45 gets woolly at 200–300 Hz (try a dynamic cut first). The piezo pickups quack at 1–3 kHz (dynamic cut, not static).

**Joe Gilder transcripts** (`transcripts/Joe Gilder • Home Studio Corner/`)
- Acoustic: `2021-12-02 - How I EQ Acoustic Guitar.md` (the main one), `2026-09-10 - How to EQ Acoustic Guitar.md`, `2023-09-27 - Easy Acoustic Guitar Mixing.md`, `2019-09-19 - How to EQ Muddy Acoustic Guitar.md`, `2019-05-30 - How to Mix Acoustic Guitar Challenge - Part 2.md`, `2018-02-16 - How to Mix Acoustic Guitar - EQ.md`, `2010-03-03 - Mixing Acoustic Guitar - HomeStudioCorner.com.md`.
- For the audit of the other templates, start from task 20's list and add at least: `2019-01-10 - How to EQ Bass.md`, `2023-08-23 - Mixing Bass Guitar (3 Things You Should Know).md`, `2021-12-09 - How to EQ Electric Guitar.md`, `2019-01-15 - How to EQ Thin Electric Guitars.md`, `2021-06-10 - EQ Ringing in the Snare Drum.md`, `2024-05-29 - How to EQ Vocals (Most People Get This Wrong).md`, `2023-08-02 - 5 Tips to Nail the Vocal EQ Every Time.md`, `2023-11-15 - Vocal EQ Avoid These Two Traps.md`, `2021-10-14 - My Take on Dynamic EQ.md`. List the folder and use anything else relevant.
- `JOE-GILDER.md` and `techniques/joe-gilder.md` are summaries. Use them to find things, not as the source.

The transcripts are auto-generated captions. Where a number is garbled, say so rather than guessing.

## 1. Acoustic guitar (do this first)

What the sources say, side by side:

| Move | Joe 2021 | Joe 2026 | Joe 2023 | Tony's tracks | Tony's template |
|---|---|---|---|---|---|
| Low cut | 40–50 Hz, "insurance", always | only with a reason | 60–80 Hz | 50 Hz | ~55 Hz, steep |
| Thump | ~150 Hz: find by boosting, then dip; don't remove | 150–200 Hz, "think of it as a drum" | 100–300 Hz | — | ~165 Hz, −1 dB |
| Plastic / "tornado" mids | 500–800 Hz, small; the move he uses least | — | ~500 Hz | 510 Hz, −1 dB, Q 2 | ~500 Hz, ~−3.5 dB |
| Cheap / harsh | — | 1–2 kHz, "closer to 2K", a little | 1–5 kHz | **dynamic** 1750 Hz, Q 7, −2.5 dB | — |
| Air | gentle shelf at 8 kHz+, "don't overdo it" | — | 8–10 kHz, not always | +1 dB shelf, 12.9 kHz | ~+1.7 dB shelf, ~12.8 kHz |

Shipped today (`Acoustic guitar body`): Low Cut 45 Hz **off**; Bell 170 Hz −3 dB Q 1.4; Bell 1.8 kHz −1.5 dB Q 1.4 (static); High Shelf 8 kHz +1.5 dB **off**.

**Replace it with "Acoustic guitar"**, Joe's "three cuts and a boost" with Tony's settings where they're more specific:

| Band | Type | Freq | Gain / range | Q | State | Why |
|---|---|---|---|---|---|---|
| 1 | Low Cut, 12 dB/oct | 50 Hz | — | — | **on** | Joe 2021 and 2019, Tony on every track |
| 2 | Bell | 150 Hz | −3 dB | 1.4 | on | The thump. Hint tells the user to sweep it between 120 and 200 Hz |
| 3 | Bell | 500 Hz | −1.5 dB | 2.0 | **on** | "Cut the plastic". Missing today |
| 4 | Bell, **dynamic** | 1.8 kHz | range −2.5 dB | 4 | on | Tony's dynamic band; Joe's "closer to 2K". Pick the threshold so it only acts on loud strums of typical material, and justify it |
| 5 | High Shelf | 10 kHz | +1.5 dB | default | **on** | Joe 2021, Tony on every track. Between Joe's 8 kHz and Tony's 12.9 kHz |

Hint (under about 100 characters), for example: "Sweep the 150 Hz thump by ear and dip it, don't remove it. The 2 kHz band only cuts loud strums."

**Add, at the end of the list:**

- **"Acoustic guitar in a full mix"**, from `mixing/acoustic-moods.md` and the J-45 notes, for a strummed rhythm part under bass and a lead: Low Cut 110 Hz (12 dB/oct); dynamic Bell 250 Hz, range −3 dB, Q 1.4 (boom); Bell 2 kHz, −1.5 dB, Q 1.2 (room for the lead); High Shelf 10 kHz, +1.5 dB.
- **"Acoustic guitar pickup"**, from the Yamaha APX-3 and Takamine notes, for a piezo DI: Low Cut 100 Hz; dynamic Bell 2 kHz, range −3 dB, Q 1.0 (the 1–3 kHz quack); High Shelf 9 kHz, +1.5 dB. These come from Tony's notes, not Joe's videos, so they're allowed even though task 20 said "only sources Joe covers". Say so in the PR.

**The classic factory preset "Acoustic guitar"** (index 6, from task 08) boosts 2.5 kHz by 1.5 dB. Both Joe ("too much 1–5K makes it sound cheap") and Tony (a cut at 1.75 kHz) say the opposite. Keep its index, rename it so it isn't confused with the new template (for example "Bright acoustic"), and change its 2.5 kHz band to a −1.5 dB cut. Update the "8 classic presets are unchanged" test to a commented exception for this one preset.

Recompute every changed template's output gain so pink noise comes out within ±0.5 dB, as task 20's test requires. With the air shelf now on, check the "boosts ≤ cuts" test still passes; dynamic bands count by their range.

## 2. Audit the other templates

For each of the other ten templates (Lead vocal, Backing vocals, Spoken word, Electric guitar, Electric guitar bus, Bass guitar, Drum bus, Snare, Reverb return, Five-zone start):

1. Re-read the transcripts for that source, plus any of Tony's own chains for it in `fx-chains/songs/*.md`, `fx-chains/cakewalk/*.md` and `mixing/*.md` (for example Dan's bass chain in `fx-chains/songs/acoustic-moods.md`).
2. Compare every band (type, frequency, gain, Q, on/off, dynamic) with what the sources say.
3. Change a value only when the sources clearly support the change. Otherwise leave it and note the disagreement.

Things already worth checking: Bass guitar (the summary says Joe cuts 200–250 Hz and 500 Hz, the template cuts 130 Hz; the 2026 video says 120–150 Hz. Which video is newer and more detailed?); whether Lead vocal's 2.6 kHz band is dynamic with sensible threshold and range; and whether the Snare template matches the "Thick Snare With Only 3 EQ's" moves.

Write the result to **`eq/docs/starting-points.md`**: one section per template with a table of its bands, and for each band the source file, timestamp or line, and a one-line reason. Note the conflicts you resolved. This file is for Tony to review, so keep it plain and short. Link it from the README's "Starting points" bullet.

## Tests

- Every template loads, uses the expected number of bands, and produces finite output at 44.1, 48 and 96 kHz (existing test, updated counts).
- Level matching within ±0.5 dB for every template (existing test, new values).
- "Boosts ≤ cuts" still holds, with commented exceptions only where a source justifies one.
- The acoustic template's bands match the table above (or your cited changes), including band 4 being dynamic and bands 1 and 5 being on.
- A session saved with the old name "Acoustic guitar body" at its old index still shows that preset as loaded.
- The renamed classic preset loads at index 6.

## Out of scope

- Compression, de-essing, saturation or anything other than EQ.
- UI changes beyond the preset names and hints.
- Putting Joe Gilder's or Tony's name in preset names or the plugin UI. The README credit stays as it is.
