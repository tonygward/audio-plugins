You are an experienced C++ DSP engineer working on **TW Pro EQ**, a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- Before changing anything, read `eq/ROADMAP.md`, `eq/Source/EQParams.h`, `eq/Source/PresetManager.h/.cpp`, `eq/Source/PresetBar.cpp`, `eq/Source/EQEditController.h/.cpp`, `eq/Source/PluginProcessor.h/.cpp` and `eq/Tests/EQTests.cpp`.
- Then read the source material listed under **Grounding** below. The values in this prompt come from it. Where this prompt and the transcripts disagree, follow the transcripts and say so in the PR.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove existing parameter IDs, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`.
- Keep the existing 8 factory presets unchanged and in the same order. Add the new templates **after** them, so any host program indices stay the same.
- This task should not change the saved-state layout. If it turns out that it must, bump `stateVersion` and add a migration test.

**Real-time safety**
- No allocation, locks, logging or file I/O in `processBlock` or `Engine::process`.
- Loading a template must not click. Let the existing smoothing handle it.

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests in the existing `juce::UnitTest` style.
- **Do not run `install.ps1`.**

**Finish**
- Tick a new checkbox in `eq/ROADMAP.md` (add one under a fitting heading) and update `README.md`.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, every value you chose differently from this prompt (and why), test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task 20: Starting-point templates based on Joe Gilder's EQ approach

**Branch:** `roadmap/20-starting-point-templates`

## Goal

Add a set of factory templates, one per common source, that closely follow the EQ advice Joe Gilder (Home Studio Corner) gives in his videos. Each template should behave the way he works:

- **Subtractive first.** Cut what's too loud before boosting anything ("cut first, then boost if needed").
- **Small moves.** Gentle cuts of a few dB. The only deep cuts are narrow ones aimed at a specific ringing frequency.
- **Every move has a reason.** A move that he makes only when a recording needs it goes in the template **bypassed** (band used but disabled). It's in the right place and ready to switch on, but does nothing by default.
- **Level-matched.** Comparing with the template on and off should not just be a loudness comparison.
- **A starting point, not a recipe.** He repeatedly warns against copying his frequencies. The templates say where to look; the user moves each band by ear, in the mix, not in solo.

## Grounding

The source material is in `C:/dev/music-recording`:

- `JOE-GILDER.md`: a summary of his philosophy.
- `techniques/joe-gilder.md`: a summary per instrument, with timestamps.
- `transcripts/Joe Gilder • Home Studio Corner/`: the full transcripts. The most relevant are:
  - **5-Minute Mix series (Sept 2026), his current approach:**
    - `2026-09-06 - Frequency Fluency.md`
    - `2026-09-07 - Do This Every Time You Use EQ.md`
    - `2026-09-08 - How to EQ Bass Guitar.md`
    - `2026-09-09 - How to EQ Electric Guitar.md`
    - `2026-09-10 - How to EQ Acoustic Guitar.md`
    - `2026-09-15 - Easy Way to EQ and Compress Drums.md`
    - `2026-09-21 - How to EQ Vocals.md`
    - `2026-09-22 - The Devil Frequency.md`
  - **Vocals:**
    - `2026-03-25 - 3-Zone Vocal EQ.md`
    - `2026-03-13 - High Pass AKA Low Cut.md`
    - `2026-03-25 - GilderVox- Mud Remover.md`
    - `2021-01-15 - 3 Vocal EQ Rules.md`
    - `2023-05-24 - The Best Vocal EQ Formula.md`
    - `2024-06-12 - How to Use Dynamic EQ (Plus an Announcement).md`
    - `2018-08-30 - Mix Background Vocals in 30 Seconds.md`
  - **Drums:**
    - `2022-09-19 - Desert Island Drum EQ Setting.md`
    - `2023-08-09 - Simple Guide to Drum EQ.md`
    - `2018-04-16 - Thick Snare With Only 3 EQ's.md`
  - **Guitars:**
    - `2022-01-03 - Magic Frequency for Electric Guitar.md`
    - `2016-07-07 - How to Tame Harsh Electric Guitars.md`
    - `2018-02-16 - How to Mix Acoustic Guitar - EQ.md`
  - **Reverb and general:**
    - `2021-09-09 - High-Pass Your Reverb.md`
    - `2020-04-17 - 3 Rules of #EQ.md`

Where his older and newer advice differ, **prefer the 2026 material**. Note any conflicts you resolve in the PR.

## His recurring landmarks

- **Five zones** ("frequency fluency"):

  | Zone | Range | Good | Too much |
  |---|---|---|---|
  | Lows | 0–80 Hz | thump | boomy |
  | Low mids | 80–300 Hz | warm | muddy |
  | Mids | 300 Hz–1 kHz | full | muffled / lo-fi |
  | High mids | 1–5 kHz | clarity | annoying, nasal |
  | Highs | 5 kHz+ | air | harsh |

- **About 200 Hz** is mud on vocals and on acoustic and electric guitars.
- **About 360–400 Hz** is boxy on drums. On electric guitar the same area is "magic warmth" when boosted.
- **About 2.6 kHz** is the "devil frequency": a whistly ringing that comes out when singers push. Cut it narrow and fairly deep.
- **8–12 kHz** is vocal air. He boosts it last and rarely, and only after cutting the lows and low mids.
- **Low cut** at 100–120 Hz on vocals is one of the few moves he makes by default. On reverb returns he cuts at about 200 Hz. Elsewhere he cuts lows only when there's a reason.

## Templates to add

The values below are starting points taken from the transcripts. **Bypassed** means the band is used but disabled. **Q** values are suggestions: wide is about 0.7–1.0, medium about 1.2–1.8, narrow about 5–8. Slope indices refer to `slopeOrders` in `EQParams.h`. Choose the low-cut slopes yourself (12 or 18 dB/oct is typical) and keep them gentle.

1. **Lead vocal**, from his 3-zone vocal EQ plus the devil frequency:
   - Low Cut, 120 Hz.
   - Bell, 200 Hz, −3 dB, wide. Mud.
   - Bell, 2.6 kHz, −4 dB, narrow. The devil frequency. Make it a **dynamic** band if the dynamic-EQ parameters fit: cut only when the vocal pushes. He shows both a static and a dynamic version.
   - High Shelf, 10 kHz, +2 dB, **bypassed**. Air, only if the vocal is still dark after the cuts.
2. **Backing vocals**, from "Mix Background Vocals in 30 Seconds":
   - Low Shelf, about 200 Hz, −4 dB.
   - Bell, about 500 Hz, −3 dB, wide.
   - High Shelf, about 8 kHz, +2 dB. For BVs he does boost the top.
3. **Spoken word**, from his voiceover chain:
   - Low Cut, 100 Hz.
   - Bell, 200 Hz, −3 dB.
   - Bell, 2.6 kHz, −2 dB, narrow.
4. **Acoustic guitar**, from the 2026 video:
   - Low Cut, 45 Hz, **bypassed**. He now uses it only when there's excess low end.
   - Bell, about 170 Hz, −3 dB, medium. The body "thump", centred between 150 and 200 Hz. Don't remove it entirely.
   - Bell, about 1.8 kHz, −1.5 dB, medium. "Plastic".
   - High Shelf, about 8 kHz, +1.5 dB, **bypassed**. Air or pick detail, from the 2018 video.
5. **Electric guitar**:
   - Low Cut, 80 Hz.
   - Bell, about 175 Hz, −2.5 dB. Low-mid mud.
   - Bell, about 2.5 kHz, −2 dB, wide. Aggression or harshness.
6. **Electric guitar bus**:
   - Bell, 360 Hz, +1.5 dB, wide. His "magic frequency": warmth without mud.
   - Bell, about 3.4 kHz, −6 dB, very narrow, **bypassed**. The whistling-guitar notch from 2016. Its position is specific to that recording, so the band is there to sweep.
7. **Bass guitar**, from the 2026 video:
   - Bell, about 130 Hz, −3 dB, medium. The 120–150 Hz mud, where the second harmonic is louder than the fundamental.
   - Bell, 50 Hz, +2 dB, wide. The "hug" you feel more than hear.
   - Bell, about 2 kHz, +1.5 dB, **bypassed**. Click and clarity, only if missing.
8. **Drum bus**, from the 2026 video plus "Desert Island":
   - Bell, 360–400 Hz, −4 dB, wide. Boxy, the one move he makes "almost always". Pick one value and justify it.
   - Low Shelf, about 80 Hz, +2 dB, **bypassed**. More kick thickness.
   - Bell, about 130 Hz, −3 dB, narrow, **bypassed**. Surgical kick boom.
   - Bell, about 3 kHz, +1.5 dB. Snap on kick and snare. He boosts "just a little".
9. **Snare**, from "Thick Snare With Only 3 EQs":
   - Bell, 400 Hz, −4 dB.
   - Bell, 200 Hz, −2 dB.
   - Low Shelf, about 200 Hz, +2 dB.

   Check the transcript for the exact third move.
10. **Reverb return**, from "5 Rules of Reverb" and "High-Pass Your Reverb":
    - Low Cut, 200 Hz.
    - Bell, about 4.8 kHz, −3 dB, medium, **bypassed**. Tames the metallic edge of a spring reverb.
11. **Five-zone start**: a teaching template with five flat (0 dB) bells at the centres of his five zones, roughly 50 Hz, 160 Hz, 550 Hz, 2.6 kHz and 10 kHz. It's for "find which choir section is too loud and turn it down".

If a transcript gives a clearly better value than one above, use it and cite it in the PR. Don't add templates for sources he doesn't cover.

## Requirements

- Extend `FactoryBand` (and `getFactoryPreset`) so a factory band can be **bypassed** (`enabled = false`) and can carry **dynamic-EQ settings** (`dynOn`, threshold, range, attack, release) when needed. Existing presets must produce identical `PresetData`.
- **Level matching.** For each template, set the preset's output gain so that pink noise through the template, with bypassed bands off, comes out within ±0.5 dB of the input level, measured as RMS or K-weighted. Compute these values offline or in a test helper, hard-code them, and have the test enforce them.
  - If the auto-gain feature from Task 14 does this better, explain the choice. Templates still need sensible output-gain values when auto gain is off.
- **Presentation in the preset menu.** Put the templates in their own sub-section, e.g. "Starting points", below the existing factory presets. Use plain source names such as "Lead vocal" or "Drum bus".
  - **Don't put Joe Gilder's name or brand in preset names or the plugin UI.** Credit him in `README.md` instead, saying the templates are "based on advice from Joe Gilder's Home Studio Corner videos" with a link. This avoids implying he endorses the plugin.
- **A one-line hint for each template**, shown somewhere unobtrusive (a tooltip on the preset name, or the status line) while that template is loaded. Examples:
  - "Find the mud by ear and move the 200 Hz band to where it actually is."
  - "Air band is off: switch it on only if the vocal is still dark after the cuts."

  Keep each hint under about 100 characters, and in his spirit: listen in the mix, make the smallest change that fixes it, level-match.
- Loading a template is one undo transaction, like other presets.

## Tests

- Every template loads, uses the expected number of bands, and produces finite output at 44.1, 48 and 96 kHz.
- Bypassed bands load as used but disabled, and they have no effect on the output (a null test against the same template with those bands removed).
- Level matching: each template's pink-noise output is within ±0.5 dB of its input.
- The 8 existing factory presets produce exactly the same `PresetData` as before.
- **Subtractive check:** in every template, the sum of the active boosts in dB is no larger than the sum of the active cuts. This encodes his "cut first" rule. If a template genuinely breaks the rule (BVs might), allow an explicit, commented exception.

## Out of scope

- Compression, de-essing processors, saturation, or anything other than EQ.
- Templates for sources his transcripts don't cover.
- Changing the existing factory presets.
