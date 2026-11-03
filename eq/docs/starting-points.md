# Starting points: where every band comes from

This is the review from task 26, for Tony to check. Each template lists its bands and, for each band, the source and a one-line reason. Joe's videos are in `music-recording/transcripts/Joe Gilder • Home Studio Corner/` and are cited as *date, timestamp*. Tony's own mixes are in `music-recording/` and are cited by file. "Off" means the band is loaded but switched off: use it only if needed.

The rule for conflicts: for the acoustic templates, **what Tony does on his own recordings wins**. For everything else, Joe's most detailed video wins. A value was only changed where the sources clearly support the change. Where they disagree and nothing changed, the disagreement is noted.

The transcripts are auto-generated captions. No number used here was garbled: each was checked in the caption text.

---

## Acoustic guitar (index 11, renamed from "Acoustic guitar body")

Joe's "three cuts and a boost", using Tony's values where they're more specific.

| Band | Type | Freq | Gain / range | Q | State | Source and reason |
|---|---|---|---|---|---|---|
| 1 | Low Cut, 12 dB/oct | 50 Hz | — | — | **on** | Joe *2021-12-02, 2:07* ("usually 40 to 50 hertz… an insurance policy"). Tony uses a 50 Hz low cut on every track (`fx-chains/songs/acoustic-moods.md`, *Tony*, *Tony (2)*, *Tony 2*). |
| 2 | Bell | 150 Hz | −3 dB | 1.4 | on | The thump. Joe *2021-12-02, 3:39* ("around 150… find it with a boost, then pull it down") and *2023-09-27, 11:28* (150, moved to 168). Tony's template has 164 Hz at −1 dB. Joe's deeper −3 dB is kept because the hint says to sweep and set it by ear. |
| 3 | Bell | 500 Hz | −1.5 dB | 2.0 | **on** | "Cut the plastic". Joe *2021-12-02, 5:55* ("closer to 500 hertz… the tornado"; "the one I do the least", so it's small). Tony's tracks have 510 Hz, −1 dB, Q 2. This band was missing before. |
| 4 | Bell, **dynamic** | 1.8 kHz | range −2.5 dB, threshold −27 dB | 4 | on | "Cheap" upper mids. Tony's tracks have a dynamic band at 1750 Hz, Q 7, range −2.5 dB. Joe *2026-09-10, 4:09* says "1 to 2K… closer to 2K". The Q is 4 rather than 7 to cover Joe's wider range. The threshold is set so pink noise at −20 dBFS RMS (normal playing) moves it by at most 0.13 dB, and at −6 dBFS RMS (loud strums) it reaches the full −2.5 dB. EQTests checks both. |
| 5 | High Shelf | 10 kHz | +1.5 dB | 0.71 | **on** | "Boost the air". Joe *2021-12-02, 6:58* ("a little bit of a boost at around 8k or above… don't overdo it"). Tony has +1 dB at 12.9 kHz on his tracks and +1.6 dB in his template. 10 kHz sits between them. |

Output gain 0.0 dB: it measures within 0.01 dB of loudness-matched on pink noise.

**Tony's saved "Acoustic" chain** (`fx-chains/acoustic-template/04.png`). Reading the nodes against the grid gave Low Cut ≈ 54 Hz (steep); Bell ≈ 163 Hz, −1.0 dB; Bell ≈ 506 Hz, −3.4 dB (narrow); High Shelf ≈ 12.9 kHz, about +1.7 dB at the plateau. The decoded preset in `fx-chains/songs/roy.md` (*Acoustic 1*) confirms this: 54.8 Hz at 18 dB/oct; 164 Hz, −0.99 dB, Q 8.2; 510 Hz, −3.40 dB, Q 6.3; 12.9 kHz, +1.63 dB.

**Conflicts resolved**
- Low cut: Joe's 2026 video says "only with a reason" and 2023 says 60–80 Hz. Tony uses 50 Hz everywhere and Joe's 2021 video says 40–50 Hz. **50 Hz, on.**
- 500 Hz depth: Tony's tracks use −1 dB, his template −3.4 dB, and Joe says "a little". **−1.5 dB.**
- Joe calls *different* ranges "plastic": 500–800 Hz in 2021, 1–2 kHz ("closer to 2K") in 2026, 1–5 kHz in 2023. The template covers both: a static cut at 500 Hz and a dynamic one at 1.8 kHz.
- Joe *2023-09-27, 12:47* is against dynamic EQ for a consistent problem ("Easy, Turbo"). Band 4 is dynamic anyway because it's Tony's move, and the harshness only comes with loud strums.

## Acoustic guitar in a full mix (new, index 19)

A strummed rhythm part under a bass and a lead. This one is from Tony's notes, not Joe's videos (allowed by the task).

| Band | Type | Freq | Gain / range | Q | State | Source and reason |
|---|---|---|---|---|---|---|
| 1 | Low Cut, 12 dB/oct | 110 Hz | — | — | on | `mixing/acoustic-moods.md` "Steel guitar rhythm": high-pass at 100–120 Hz, to leave room for the bass. The J-45 notes say 100–120 Hz in a full mix. |
| 2 | Bell, **dynamic** | 250 Hz | range −3 dB, threshold −24 dB | 1.4 | on | Same section: "a dynamic cut at 200–350 Hz to control boom". `gibson-j45-standard.md`: woolly at 200–300 Hz, "try a dynamic cut first". |
| 3 | Bell | 2 kHz | −1.5 dB | 1.2 | on | Same section: "a gentle 1–2 dB dip at about 1.5–2.5 kHz to leave a pocket" for the lead. |
| 4 | High Shelf | 10 kHz | +1.5 dB | 0.71 | on | Same section: "a shelf lift above 8 kHz for shimmer". |

Output gain +0.2 dB (measures +0.03 dB).

## Acoustic guitar pickup (new, index 20)

A piezo DI. This one is from Tony's guitar notes, not Joe's videos (allowed by the task).

| Band | Type | Freq | Gain / range | Q | State | Source and reason |
|---|---|---|---|---|---|---|
| 1 | Low Cut, 12 dB/oct | 100 Hz | — | — | on | `yamaha-apx-3.md`: "a high-pass at 100–120 Hz is safe for rhythm". `takamine-esf-40c.md`: 80–100 Hz. |
| 2 | Bell, **dynamic** | 2 kHz | range −3 dB, threshold −26 dB | 1.0 | on | Both guitars' notes: the piezo "quacks in the 1–3 kHz range"; use "a dynamic EQ band… rather than a static cut". The wide Q covers 1–3 kHz. |
| 3 | High Shelf | 9 kHz | +1.5 dB | 0.71 | on | `yamaha-apx-3.md`: "a gentle high-shelf lift above 8 kHz brings back some air". |

Output gain −0.1 dB (measures +0.01 dB).

## Bright acoustic (classic preset, index 6, renamed from "Acoustic guitar")

This is not a template. It was renamed so it isn't confused with the template above. Its **2.5 kHz band changed from +1.5 dB to −1.5 dB**: Joe *2023-09-27, 7:10* says "too much in the 1 to 5K range… makes it sound cheap", and Tony cuts at 1.75 kHz. Its other bands are unchanged: Low Cut 80 Hz, 200 Hz −3 dB, 9 kHz shelf +2 dB. Sessions that saved it as "Acoustic guitar" at index 6 now show "Bright acoustic".

---

## The other ten templates

### Lead vocal (index 8)

| Band | Type | Freq | Gain | Q | State | Source and reason |
|---|---|---|---|---|---|---|
| 1 | Low Cut, 12 dB/oct | 120 Hz | — | — | on | Unchanged. Joe *2023-08-02, 2:40* says "as high as 80 Hz". The template's 120 Hz came from task 20's sources. **Disagreement noted:** 120 Hz is a little high for a deep voice, but no video clearly supports changing it. |
| 2 | Bell | 200 Hz | −3 dB | 0.9 | on | Joe *2024-05-29, 8:53*: "hovering around just right at 200 Hz… a quick little pull down". |
| 3 | Bell, dynamic | 2.6 kHz | −2 dB, range −4 dB, threshold −30 dB | 6 | on | The "devil frequency": *2024-05-29, 10:38* ("2.6k… on just about every vocal"), and *11:09* (notch it "or use dynamic EQ"). Checked: it's dynamic, with a sensible threshold and range. |
| 4 | High Shelf | 10 kHz | +2 dB | 0.71 | off | Unchanged. *2023-08-02, 7:51*: the mistake is overdoing the highs, so it's off unless the vocal is still dark. |
| 5 | Bell | 4 kHz | −2 dB | 6 | off | **New.** *2024-05-29, 12:12* ("a harmonic up here at like 4K, I'm going to pull that one down") and *2023-11-15, 6:43* (notches "at 4K and 2 and 1/2k"). Off, because not every vocal has it. |

### Backing vocals (index 9)

Unchanged: Low Shelf 200 Hz −4 dB · Bell 500 Hz −3 dB, Q 0.8 · High Shelf 8 kHz +2.5 dB. *2018-08-30, 2:11* ("boost the top end") and *3:19* (high-pass at 200 Hz after the plate reverb). The low shelf at 200 Hz is the in-channel version of that 200 Hz roll-off.

### Spoken word (index 10)

Unchanged: Low Cut 100 Hz · Bell 200 Hz −3 dB, Q 0.9 · Bell 2.6 kHz −2 dB, Q 6. *2018-03-08* gives no frequencies (it uses a preset). The values follow the vocal videos above: *2023-08-02, 2:40* "any… spoken word vocal… gets a high pass filter", plus the 200 Hz and 2.6 kHz moves from *2024-05-29*.

### Electric guitar (index 12)

Unchanged: Low Cut 80 Hz · Bell 175 Hz −2.5 dB, Q 1.2 · Bell 2.5 kHz −2 dB, Q 0.9. *2026-09-09, 2:05* ("down here in the lows, like below 80 hertz") and *3:40* ("a little too bright for me, so I pulled down this range"). *2021-12-09, 1:32* puts the essentials at "150 up to 400 for the warmth and 3 to 5k for the crunch". No change is clearly supported.

### Electric guitar bus (index 13)

Unchanged: Bell 360 Hz +1.5 dB, Q 0.8 · Bell 3.44 kHz −7.5 dB, Q 8 (off). *2026-09-09, 3:40* ("something magical… around 360 hertz… on the overall guitar bus") and *2019-01-15, 5:13* (a 360 Hz boost for warmth). This is still the one template allowed more boost than cut.

### Bass guitar (index 14)

Unchanged: Bell 130 Hz −3 dB, Q 1.4 · Bell 50 Hz +2 dB, Q 0.9 · Bell 2 kHz +1.5 dB (off).
- *2026-09-08, 3:48*: "around 120. 120 to 150… weird gross muddiness", then "50 Hz… bump that up", then "1 to 3K… if I need a little more clickiness".
- *2019-01-10, 7:29*: "right at one hundred and forty Hertz… I'm gonna pull that down", then boost the low lows (*11:10*).
- **Disagreement noted:** the summary in `techniques/joe-gilder.md` says 200–250 Hz and 500 Hz. That comes from *2020-10-23, How to Mix #Bass*. The two EQ-specific videos (2019, the most detailed, and 2026, the newest) both say 120–150 Hz, so 130 Hz stays. Tony's own bass chain (*Dan*, `acoustic-moods.md`) is a different shape: a dynamic dip at 70 Hz, 250 Hz −1.5 dB at Q 5, and a dynamic dip at 3 kHz. It's one bass in one song, so it isn't used for a general template.

### Drum bus (index 15)

Unchanged: Bell 360 Hz −4 dB, Q 0.8 · Low Shelf 80 Hz +2 dB (off) · Bell 130 Hz −3 dB, Q 5 (off) · Bell 3 kHz +1.5 dB. *2026-09-15, 3:34* ("it's 360 Hz") and *4:35* ("around 3K and boost that just a little"). *2018-04-16, 1:02* cuts "400 Hz… the devil" on the drum bus and adds "a low shelf… a couple dB below 200". That's the off low shelf. It stays off because the 2026 video doesn't use it.

### Snare (index 16)

Unchanged: Bell 190 Hz −3 dB, Q 1.4 · Low Shelf 200 Hz +2 dB. This matches *2018-04-16, 3:43–4:14*: "something happening just below 200… bring it down a little bit", then "a low frequency shelf… boost the overall low end… because we cut it we're now boosting a more balanced sound". *2021-06-10, 6:10* (ringing at 319 Hz) is a notch for one particular drum, so it isn't added.

### Reverb return (index 17)

Unchanged: Low Cut 200 Hz, 18 dB/oct · Bell 4.8 kHz −3 dB (off). *2021-09-09, 3:44*: "always a high pass filter on my reverbs… around 200 hertz", rolled up to 500 Hz on drums at *4:14* (the hint says so). Tony's own acoustic reverb return cuts at 377 Hz (`fx-chains/songs/roy.md`, *Acoustci Reverb*), which is within that range.

### Five-zone start (index 18)

Unchanged: five flat bells at 50 Hz, 160 Hz, 550 Hz, 2.6 kHz and 10 kHz. It's a teaching layout from task 20 (*2026-09-06, Frequency Fluency*). There's nothing to audit because every gain is 0 dB.
