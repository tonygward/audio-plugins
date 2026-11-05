You are an experienced C++ audio developer and UI engineer working on **Songsmith EQ** (by Songsmith Studio), a JUCE VST3/AU/Standalone parametric EQ. Repo: `C:/dev/audio-plugins`, plugin in `eq/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- `CLAUDE.md` (design principles), `docs/style-guide.md` (type scale, colours, plain words), `eq/ROADMAP.md` and `eq/docs/starting-points.md`.
- The editor: `eq/Source/PluginEditor.cpp` (`layoutContent()`, the top bar), the graph and analyser drawing, and how task 28's piano toggle is drawn, saved with the session and tested (`eq/prompts/done/28-piano-strip-toggle.md`).
- The sources in the **Source register** below. Tony's notes are also local, in `C:/dev/music-recording`.
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter IDs, and never change `PLUGIN_MANUFACTURER_CODE` / `PLUGIN_CODE`.
- The guide is editor state, like the piano toggle: saved with the session, **not** an automatable parameter. If the saved-state layout changes, bump `stateVersion` and add a test that older sessions still load with the guide off.

**Real-time safety**
- The guide is drawing only. Nothing in `processBlock` or `Engine::process` changes.

**Build and test (from `eq/`)**
- `cmake --build build --config Release`. If `build/` is missing, first run `cmake -B build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- The build must succeed with no new warnings.
- `.\build\EQTests_artefacts\Release\EQTests.exe` must pass every test. Add tests in the existing `juce::UnitTest` style.
- **Do not run `install.ps1`.**

---

# Task 32: Instrument frequency guide

**Branch:** `roadmap/32-frequency-guide`

Build the guide with **Acoustic Guitar** first, then add the other instruments **in the order of the list below**. Each instrument is a table of regions (data), so once the overlay works, each new one is mostly data and tests. If you run out of time, stop after a finished, tested instrument and list the rest in the PR. The design write-up below is Tony's: follow it. Where it conflicts with `CLAUDE.md` or the style guide (for example plain words, no abbreviations, the bevelled control look), `CLAUDE.md` wins.

The concept picture shows things Songsmith EQ doesn't have (A/B, undo and redo, a share button, an instrument card). Only build the **Frequency Guide** toggle, the instrument selector and the coloured regions with their labels and tooltips. Fit them into the current editor layout.

---

## Design: Songsmith EQ — Instrument Frequency Guide

**Status:** proposed feature · **Product:** Songsmith EQ · **Date:** 4 November 2026

> **Purpose:** Help musicians understand *what they are hearing* before asking them to learn frequency numbers. The guide is an optional visual overlay, not an automatic EQ or a substitute for listening.

## Concept screenshot

![Concept mockup of Songsmith EQ with an acoustic-guitar instrument selector, frequency-guide toggle, coloured tonal regions, EQ curve and spectrum analyser](32-frequency-guide.png)

*Figure 1 — Design reference, not a screenshot of an implemented feature. The interface, colour bands, text and EQ curve are illustrative. “Crunch” belongs to electric guitar, not acoustic: use the regions in the list below, not the ones in the picture.*

## Intended experience

A musician opens **Songsmith EQ**, turns on **Frequency Guide**, and chooses the instrument. A subtle, see-through set of regions appears *behind* the EQ curve and the analyser. Instead of memorising that a problem is at 500 Hz, the musician reads **Plastic** and learns what to listen for. Instead of memorising 8–12 kHz, they read **Air**.

The musician stays in control. The guide **does not change the EQ**, add bands, guess which frequencies are bad, or suggest that boosting a labelled region always helps. Presets and every technical control stay as they are.

**Core principle:** *Musical words first; frequencies when useful.*

## Where the words come from

The words are Joe Gilder's wherever he has given one, so what the guide says matches what Tony has learned from his videos. In *Frequency Fluency* (2026-09-06, 1:32–3:36) Joe splits the range into five groups and gives each **two words**: what it adds when it's right, and what it sounds like when there's too much.

| Group | Roughly | When it's right | Too much |
|---|---|---|---|
| Lows | 0–80 Hz | Thump | Boomy |
| Low mids | 80–300 Hz | Warm | Muddy |
| Mids | 300 Hz–1 kHz | Full | Muffled, lo-fi |
| High mids | 1–5 kHz (up to 8 kHz) | Clarity | Annoying, nasal |
| Highs | above 5–8 kHz | Air | Harsh |

He says the boundaries matter less than knowing what each group sounds like, and that he doesn't like "boxy" because he's not sure what it means (3:36), though he does call 360 Hz on drums "the boxy zone" (2026-09-15, 3:04). **Use this two-sided idea in every tooltip:** one sentence on what the region adds, one on what too much sounds like, and when Joe or Tony usually moves it.

**Rules for the words**
- Prefer Joe's own word for that instrument (marked **Joe** below). Next, Tony's word from his own instrument notes and mixes (**Tony**). Only then a Songsmith word (**Songsmith**), and say so in the PR.
- The labels are plain words (`CLAUDE.md` principle 4): no abbreviations, British spelling, one word for one thing across instruments. "Mud" means the same thing on every instrument.
- The frequencies are **listening regions, not rules**. Regions fade into each other rather than looking like hard boxes, and every tooltip says "around" or "about".
- The transcripts are auto-generated captions. Check every number and quote against the caption text before using it, as task 26 did.
- Credit Joe Gilder's Home Studio Corner videos and Tony's notes. Don't present the guide as endorsed by Joe.

## The instruments, in build order

Each instrument lists its regions from low to high. Frequencies are approximate. **Joe** citations are *transcript date, timestamp*, from `C:/dev/music-recording/transcripts/Joe Gilder • Home Studio Corner/`. **Tony** citations are files in `C:/dev/music-recording/`.

### 1. Acoustic Guitar (build first)

A steel-string acoustic recorded with a mic. Joe's *three cuts and a boost*, plus his 2026 update and Tony's own settings.

| Region | Around | Label | What to say | Source |
|---|---|---|---|---|
| Below the guitar | under 40–50 Hz | **Rumble** | Nothing useful from the guitar lives here: air conditioning, foot taps, things you only hear on a subwoofer. A low cut here is insurance. | **Joe** 2021-12-02, 0:31 and 2:07 ("nothing good… below 40 or 50 hertz… an insurance policy"). 2026-09-10, 1:34: only use a low cut when there's a reason. **Tony**: 50 Hz low cut on his tracks (`fx-chains/songs/acoustic-moods.md`) |
| The body knock | 100–200 Hz, often about 150 | **Thump** | The percussive knock of each strum, like a drum inside the guitar. Some of it is the warmth; too much is a constant boom on every chord. Turn it down, never out. | **Joe** 2021-12-02, 2:37–4:42 ("cut the thump", "around 150", "if I get rid of it entirely it would remove all the warmth"); 2026-09-10, 2:34–4:09 ("think of an acoustic guitar as a drum", "between 150 and 200"). **Tony**: 164 Hz, −1 dB (`fx-chains/songs/roy.md`, *Acoustic 1*) |
| Low mid build-up | 500–800 Hz | **Plastic** | Too much makes even a good guitar sound cheap and mid-heavy, "like the sound of a storm". A small cut can clear both the lows and the highs. It's the move Joe makes least. | **Joe** 2021-12-02, 4:42–6:26 ("cut the plastic", "800 hertz… closer to 500", "the one I do the least"). **Tony**: 510 Hz, −1 to −3.4 dB |
| Upper mids | 1–2 kHz, nearer 2 kHz | **Cheap** | Where a guitar can sound like it's made of plastic, especially on hard strums. Pulling a little down here warms the guitar without adding low end. | **Joe** 2026-09-10, 4:09 ("1 to 2K… plasticky… closer to 2K… sounds cheap"). **Tony**: dynamic dip at 1.75 kHz on loud strums (`acoustic-moods.md`) |
| Pick and sparkle | above about 8 kHz | **Air** | Pick detail that helps the guitar cut through a busy mix. Too much and it sizzles: a little goes a long way. | **Joe** 2021-12-02, 6:58–8:08 ("boost the air", "a little bit… around 8k or above", "I literally make a face"). **Tony**: +1 to +1.6 dB at 12.9 kHz |

**Joe uses "plastic" for two ranges:** 500–800 Hz in 2021, and 1–2 kHz in 2026. Keep both, with different words: **Plastic** for 500–800 Hz (his original "three cuts and a boost" name) and **Cheap** for 1–2 kHz (his 2026 word for it). Explain this in the PR. It matches the "Acoustic guitar" starting point in `eq/docs/starting-points.md`, which has a cut in each.

**What the tooltips should also say** (from Tony's notes, short):
- **In a full mix** (rhythm under a bass and a lead), Tony cuts higher: a low cut at 100–120 Hz, boom control at 200–350 Hz, and a small dip at 1.5–2.5 kHz to leave room for the lead (`mixing/acoustic-moods.md`, "Steel guitar rhythm"). A J-45 gets woolly at 200–300 Hz (`instruments/tony/gibson-j45-standard.md`).
- **A pickup (piezo) instead of a mic** "quacks" at 1–3 kHz; a dynamic cut is better than a fixed one (`instruments/tony/yamaha-apx-3.md`, `takamine-esf-40c.md`).
- **A 12-string**: low cut around 100–150 Hz, and its shimmer is mostly 3–10 kHz (`instruments/tony/simon-patrick-woodland-12-cedar.md`).

These go in the tooltip of the region they affect, as one short extra sentence, not as extra regions.

### 2. Nylon Guitar

Chris's nylon-string lead in *Acoustic Moods*. These are **Tony's** notes, not Joe's. The key difference from Acoustic Guitar: **1–3 kHz is the melody here, not a problem**. Don't label it as something to cut.

| Around | Label | What to say | Source |
|---|---|---|---|
| under 80 Hz | **Rumble** | Chair and foot noise. | **Tony** `instruments/chris/nylon.md` (Pro-Q 4, band 1) |
| 180–250 Hz | **Boom** | Soundhole boom when the mic is close. Cut only when it blooms. | **Tony** same, band 2; `mixing/acoustic-moods.md` |
| 400–500 Hz | **Boxy** | Only if it sounds boxy; usually leave it alone. | **Tony** same, band 3 |
| 1–3 kHz | **Melody** | Where the tune speaks. Leave it alone; make room in the other guitars instead. | **Tony** `nylon.md` ("Leave 1–3 kHz alone… that's where the melody speaks"); `mixing/acoustic-moods.md`, point 5 |
| 2–5 kHz | **String Noise** | Squeaks and fret noise. Tame them gently, only where they jump out. | **Tony** `mixing/acoustic-moods.md` (soothe2 / de-esser on squeaks) |
| 10–12 kHz | **Air** | A little lift adds openness. | **Tony** `nylon.md` (Pultec, 12 kHz) |

Melody and String Noise overlap on purpose. Draw them so it's clear they share the range.

### 3. Electric Guitar

| Around | Label | What to say | Source |
|---|---|---|---|
| under 80 Hz | **Boom** | Below the guitar's useful range; Joe usually rolls it off. | **Joe** 2026-09-09, 2:05–2:36 ("below 80 hertz I typically roll that off… there can be boominess") |
| 150–200 Hz | **Mud** | A big tone that thickens up and muddies the mix, a "rumbly" thickness. Common on clean guitars. | **Joe** 2026-09-09, 2:36–3:09 ("150 to 200"). **Tony**: high gain builds up mud at 200–400 Hz (`instruments/tony/schecter-demon-6.md`) |
| about 360 Hz | **Warmth** | A little boost on the guitar bus adds warmth without mud. Don't overdo it. | **Joe** 2026-09-09, 3:40–4:11 ("something magical… around 360"); 2021-12-09, 1:32 ("150 up to 400 for the warmth") |
| 2–3 kHz | **Aggression** | Where the bite is. Too much is really annoying. | **Joe** 2026-09-09, 3:09–3:40 ("high mids are where all the aggression is… 2 to 3K") |
| 3–5 kHz | **Crunch** | The crunch of a driven tone. Too much is spiky: a Telecaster bridge pickup, a Strat's in-between positions. | **Joe** 2021-12-09, 1:32 ("3 to 5K for the crunch"). **Tony**: Tele bridge 3–5 kHz, Strat "quack" 2–4 kHz (`fender-telecaster.md`, `fender-stratocaster.md`) |

Joe also says 80–90 % of an electric guitar's mix tone comes from the recording (2026-09-09, 0:30–1:01). Put that in the instrument's general tooltip.

### 4. Bass Guitar

| Around | Label | What to say | Source |
|---|---|---|---|
| about 50 Hz | **Weight** | The deep part you feel more than hear: Joe's "big warm hug". A small boost here if it's missing. A 5- or 6-string's low B is about 31 Hz: don't cut it. | **Joe** 2026-09-08, 3:48–4:50 ("go down to 50 Hz… bump that up", "nice warmth down at 50"), 1:03 (the "hug"). **Tony**: low B (`instruments/dan/washburn-6-string.md`) |
| 120–150 Hz | **Mud** | "Weird gross muddiness": the octave above the note that's often louder than the note itself. Turn it down first. | **Joe** 2026-09-08, 3:16–3:48; 2019-01-10, 7:29 (140 Hz) |
| 200–500 Hz | **Boxy** | Some basses get boxy here. Listen before cutting. | **Joe** 2020-10-23 (*How to Mix #Bass*: 200–250 Hz and 500 Hz). Older than the two EQ videos, so a gentler region: see `eq/docs/starting-points.md`, "Bass guitar" |
| 1–3 kHz | **Click** | The pick or finger attack that lets notes be heard on small speakers. A fretless's "mwah" lives at 1–2 kHz: don't cut it. | **Joe** 2026-09-08, 4:18 ("1 to 3K… a little more clickiness"); 2019-04-09 (a 3 kHz boost for grit). **Tony**: `instruments/dan/fretless-5-string.md` |

**Tony** adds: an acoustic bass (piezo) has body boom at 100–200 Hz and "quack" at 2–4 kHz (`instruments/dan/acoustic-bass.md`). One sentence in the tooltips.

### 5. Vocal

| Around | Label | What to say | Source |
|---|---|---|---|
| under 100–120 Hz | **Rumble** | Pops from "P" sounds and stand noise. Joe cuts it by default. | **Joe** 2026-09-21, 2:34 ("100, 120 hertz… when the singer sings a P"). Older advice: "as high as 80 Hz" (2023-08-02, 2:40) |
| about 200 Hz | **Mud** | Where the warmth is, but too much and the vocal won't sit on top of the mix. Modern records are thinner here than you think. | **Joe** 2026-09-21, 2:34–4:07; 2024-05-29, 8:53 |
| about 2.6 kHz | **Piercing** | A whistly, piercing ring when the singer pushes; Joe calls it the "devil frequency". A narrow cut lets you turn the vocal up. | **Joe** 2026-09-22, 1:00 ("usually around 2.6 kHz"), 2:02 ("harsh and piercing"), 3:03 ("whistly"); 2024-05-29, 10:38 |
| about 4 kHz | **Harshness** | Sometimes a second harsh ring. Only if you hear it. | **Joe** 2024-05-29, 12:12; 2023-11-15, 6:43 |
| 6–8 kHz | **Sibilance** | The "s" and "t" sounds. A de-esser works better than a fixed cut. | **Joe** `techniques/joe-gilder.md` (de-esser at 6–8 kHz); 2026-09-20 |
| 8–12 kHz | **Air** | A little lift if the vocal is still dark. Most people overdo it. | **Joe** 2026-09-21, 4:07–4:38 ("I call this the air range… be really careful") |

### 6. Kick Drum

| Around | Label | What to say | Source |
|---|---|---|---|
| under 100 Hz, often about 80 | **Weight** | The punch in the chest. Turn up only if it's missing. | **Joe** 2026-09-15, 3:34–4:05 ("thickness in the kick… below 100, probably 80"); 2019-04-09, 1:33 ("the kick is the punch") |
| 60–160 Hz | **Boom** | A ring that hangs on after each hit. Find the one note and cut it narrowly. | **Joe** 2026-09-15, 4:05 ("around 130… a boom boom that I did not care for"); 2014-05-22, 1:33 (77 Hz resonance, cut 8 dB, narrow) |
| 360–400 Hz | **Boxy** | Cardboard-sounding, "woo woo". Joe pulls it down on drums almost always. | **Joe** 2026-09-15, 3:04–3:34; 2010-02-01 (400 Hz) |
| about 3 kHz | **Snap** | The beater's click. A little boost helps the kick through on small speakers. | **Joe** 2026-09-15, 4:35; 2025-05-07 (the "clicky… snap" and "woof" of a kick) |

### 7. Snare Drum

| Around | Label | What to say | Source |
|---|---|---|---|
| just below 200 Hz | **Boom** | Something happening just below 200 Hz. Bring it down, then lift the low end as a whole so it's balanced. | **Joe** 2018-04-16, 3:43–4:14 |
| 360 Hz | **Boxy** | As on the kick. | **Joe** 2026-09-15, 3:34 |
| about 3 kHz | **Snap** | The crack of the stick. A small boost. | **Joe** 2026-09-15, 4:35 |

Joe notches a single ringing note on some snares (2021-06-10, 6:10, 319 Hz). That's one drum, so say in the tooltip that a ring can be anywhere, rather than drawing a region.

### 8. Toms

**Tony's** own settings from *Between You and Me* (`fx-chains/songs/tony-ward.md`, decoded in `eq/docs/starting-points.md`, "Tom presets"). Tony's are bolder than Joe's moves, so the tooltips say "on Tony's album".

| Around | Label | What to say | Source |
|---|---|---|---|
| 100–165 Hz | **Body** | The tone of the drum. Tony boosts it on the album. | **Tony** rack tom +4.6 dB at 163 Hz, floor tom shelf at 122 Hz, toms bus cut at 119 Hz |
| 300–950 Hz | **Boxy** | The cardboard sound; Tony cuts it deeply (−7 to −12 dB). | **Tony** 670 Hz (rack), 930 Hz (floor), 317 Hz (bus) |
| about 4 kHz | **Attack** | The stick on the head. | **Tony** +5.9 dB at 4.07 kHz on both toms |
| above 10 kHz | **Air** | Cymbal spill lives here too; the toms bus cuts above about 10.7 kHz. | **Tony** 10.3 kHz boosts; toms bus high cut |

### 9. Drum Bus

| Around | Label | What to say | Source |
|---|---|---|---|
| under 100 Hz | **Weight** | More kick thickness, only if needed. | **Joe** 2026-09-15, 3:34–4:05 |
| about 130 Hz | **Boom** | Low boom in the kick or snare. Cut narrowly. | **Joe** 2026-09-15, 4:05 |
| about 360 Hz | **Boxy** | "The boxy zone": muffled. Joe pulls it down almost always. | **Joe** 2026-09-15, 3:04–3:34; 2018-04-16, 1:02 (400 Hz, "the devil") |
| about 3 kHz | **Snap** | A little more snap from kick and snare. | **Joe** 2026-09-15, 4:35 |

### 10. Any Instrument

Joe's five groups from *Frequency Fluency* (the table above): Lows, Low Mids, Mids, High Mids, Highs, with the two words for each in the tooltip. This is the guide for anything not in the list (piano, keys, strings) and matches the "Five-zone start" starting point.

### Not in this task

Piano and keys, backing vocals, overheads and cymbals, and separate Acoustic Guitar variants (full mix, pickup, 12-string). The notes don't have enough instrument-specific detail for them yet, or the variants are covered by tooltips. List them in the PR as possible follow-ups.

## Visual and interaction direction

Use the concept picture as the **visual north star**, but fit the feature into the *existing* scalable Songsmith EQ rather than rebuilding its UI.

- **Controls:** `Frequency Guide` toggle (off by default) and an instrument selector listing the instruments above in the same order. Neither changes EQ parameters, automation or audio.
- **Placement:** see-through regions **behind** the live spectrum and the response curve. Bands, draggable nodes, the frequency grid and meters stay readable and clickable.
- **Labels:** short, high-contrast words; a tooltip of one or two sentences on what to listen for. Show the approximate frequency range in the tooltip, as supporting information.
- **Fit:** regions follow the logarithmic frequency axis and work from the minimum size to 4K and Retina. Hover tooltips never cover the node being dragged. When a label doesn't fit its region at a small size, move it or show it on hover; never shorten the word.
- **Accessibility:** don't rely on colour alone. Labels stay legible with colour-vision deficiencies and at small sizes. No animation.
- **Audio:** visual only. Switching the guide or the instrument leaves the EQ, the processing and the output unchanged. It never adds bands.
- **Presets:** choosing a guide is separate from loading a starting point. Loading the acoustic starting point doesn't turn the guide on or change its instrument.

### What the user sees on hover (illustrative copy)

**Thump · around 100–200 Hz · Acoustic Guitar**
"The knock of each strum, like a drum inside the guitar. Some of it is the warmth; too much is a boom on every chord. Turn it down, never out."

**Melody · around 1–3 kHz · Nylon Guitar**
"Where the tune speaks. Leave it alone and make room in the other guitars instead."

**Piercing · around 2.6 kHz · Vocal**
"A whistly ring when the singer pushes. A narrow cut here often lets you turn the vocal up."

## Acceptance checks

1. Turning Frequency Guide on or off, or changing instrument, produces **identical audio** and never changes a band.
2. Acoustic Guitar shows **Rumble, Thump, Plastic, Cheap and Air**, in that order from low to high. Electric Guitar includes **Warmth and Crunch**.
3. Nylon Guitar's 1–3 kHz region is **Melody**, and its tooltip doesn't suggest cutting it.
4. Every region of every instrument built has a source in the PR (Joe with date and timestamp, or Tony with a file), checked against the caption text or the file.
5. At every interface size, labels and nodes stay readable, and the guide never gets in the way of dragging or selecting a band.
6. Someone can find and use the guide without knowing the words "high-pass", "bell", "Q" or "kHz".
7. The README credits the sources and doesn't present the guide as endorsed by Joe Gilder.

## Source register

All local, in `C:/dev/music-recording` (GitHub: `tonygward/music-recording`).

**Joe Gilder transcripts** (`transcripts/Joe Gilder • Home Studio Corner/`, auto-generated captions):
- *2026-09-06 Frequency Fluency*: the five groups and the two-sided words.
- *2026-09-08 How to EQ Bass Guitar*, *2026-09-09 How to EQ Electric Guitar*, *2026-09-10 How to EQ Acoustic Guitar*, *2026-09-15 Easy Way to EQ and Compress Drums*, *2026-09-21 How to EQ Vocals*, *2026-09-22 The Devil Frequency*: the 2026 *5-Minute Mix* series, his newest advice.
- *2021-12-02 How I EQ Acoustic Guitar* ("three cuts and a boost"), *2021-12-09 How to EQ Electric Guitar*, *2019-01-10 How to EQ Bass*, *2019-04-09 How to Mix Kick Drum and Bass*, *2018-04-16 Thick Snare With Only 3 EQ's*, *2014-05-22 Mixing Kick Drum – Cutting Low End*, *2024-05-29 How to EQ Vocals*, *2023-08-02 5 Tips to Nail the Vocal EQ*.
- Summary: `techniques/joe-gilder.md`.

**Tony's notes**
- `instruments/tony/` (J-45, APX-3, Takamine, 12-string, Telecaster, Stratocaster, Demon-6), `instruments/chris/nylon.md`, `instruments/dan/` (basses).
- `mixing/acoustic-moods.md` (instrument roles and carving), `fx-chains/songs/acoustic-moods.md`, `fx-chains/songs/roy.md`, `fx-chains/songs/tony-ward.md` (saved settings).

**This repo**
- `eq/docs/starting-points.md`: how task 26 resolved disagreements between the sources. Where this prompt and that file disagree on a frequency, follow that file and say so in the PR.

## Scope

The guide is visual only. The DSP, the presets and the other plugins are out of scope.

---

## Tests

- Audio is identical with the guide on or off and for every instrument: render the same input through the processor and compare sample for sample. Band parameters are unchanged.
- The toggle and the instrument are saved with the session and restored. An older session (no guide state) loads with the guide off.
- For each instrument built: the regions and labels match the tables above (or the PR explains the change), they run from low to high, and each has a tooltip.
- Nylon Guitar's Melody tooltip doesn't contain the word "cut" as advice.
- Every label and tooltip passes the shared plain-words test.
- At every interface size (75 % to 200 %, and the minimum and 4K sizes from task 25), labels stay readable and don't overlap each other. Band nodes stay on top and can still be clicked and dragged.

## Finish

- Render screenshots (`EQTests --screenshots`, at 1x and `--scale 2`) with the guide off, and on for each instrument built, at the default and minimum sizes. Commit them on the branch, link them in the PR by commit SHA, then delete them before merging (see `CLAUDE.md`).
- Tick this item in `eq/ROADMAP.md`. Add the guide to `README.md` and `TECHNICAL.md` in plain words, and credit the sources (not presented as endorsed by Joe Gilder).
- Push the branch and open a **draft** PR with `gh pr create --draft`. Cover what changed, the final words and regions for each instrument with their sources, which instruments are still to do, every choice made differently from this prompt (and why), test results, known limitations, and "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.
