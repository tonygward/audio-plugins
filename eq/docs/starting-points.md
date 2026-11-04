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

### Abbey Road reverb (index 21, task 30)

This template is Tony's own Pro-Q 4 preset, *Abbey Road Reverb Trick* (`Documents/FabFilter/Presets/Pro-Q 4/Tony/Abbey Road Reverb Trick.ffp`, saved 26 Jul 2026), copied band for band. It is not based on Joe's videos. The values come from the decode in `fx-chains/saved-chains.md`, line 307 (*Vocals Reverb (Dual)*: Pro-Q 4 → UADx Pure Plate → ValhallaRoom → Pro-C 2), which shows this is the EQ in front of Tony's vocal reverbs. The `.ffp` itself couldn't be opened when this was written (a OneDrive cloud-only file), so the catalogue decode is the source.

| Band | Songsmith EQ | Pro-Q 4 (`saved-chains.md:307`) |
|---|---|---|
| 1 | Low Cut 377.28 Hz, 12 dB/oct | Low Cut 377.28 Hz, default slope |
| 2 | Bell 594.4 Hz +0.54 dB, Q 1.92 | Bell 594.40 Hz +0.54 dB, Q 1.923 |
| 3 | High Cut 17.056 kHz, 12 dB/oct | High Cut 17056 Hz, Q 1.004, default slope |
| 4 | Bell 10.094 kHz +0.41 dB, Q 2.16 | Bell 10094 Hz +0.41 dB, Q 2.158 |
| 5 | Bell 2 kHz −3 dB, Q 3.02 | Bell 2000 Hz −3.00 dB, Q 3.016 |

- **Slope:** Pro-Q's default cut slope couldn't be confirmed from the file, so the cuts use 12 dB/oct, as the task says to when it's unconfirmed. Check this against the Pro-Q 4 preset.
- **Cut Q:** the cuts use Songsmith EQ's neutral cut Q (0.707, no resonance bump). Pro-Q's Q 1.0 on a cut is its own neutral default.
- **Output gain:** +1.6 dB, measured the way task 20 does it (pink noise, K-weighted), because the cuts take about 1.6 dB of loudness out. On a reverb send, loudness matching means little. Tony runs Pro-Q's Auto Gain here.
- **The technique:** the "Abbey Road reverb trick" EQs the send **before** the reverb: high-pass at about 600 Hz and low-pass at about 10 kHz. Tony's version is gentler: 377 Hz and 17 kHz, plus a −3 dB dip at 2 kHz to keep harshness and sibilance out of the tail. The two tiny bells (+0.5 dB at 594 Hz and 10 kHz) mark the textbook points. The hint suggests raising the low cut towards 600 Hz for the textbook sound.
- **Compared with "Reverb return" (index 17):** that template goes **after** the reverb, following Joe (*2021-09-09, High-Pass Your Reverb, 5:18*: "the reverb can sometimes produce low frequencies even if there's not a lot … going into it and that's why I put the EQ after"). It's a 200 Hz 18 dB/oct cut and an optional 4.8 kHz dip. Abbey Road shapes what goes **into** the reverb. The two can be used together. `mixing/reverb-template.md` (line 34) suggests keeping Tony's pre-reverb Pro-Q 4 and adding a high-pass after. `mixing/between-you-and-me.md` (line 99): "every reverb filtered, usually high-passed at 200–500 Hz and low-passed at 10–12 kHz".
- **Joe on "Abbey Road":** the transcripts mention it only as a big drum room (*2019-01-22, My Favorite Reverb, 2:38*), not this EQ trick.
- **The name:** it names a well-known technique. It doesn't claim any connection with the studio.
- **Cut first:** boosts 0.95 dB against a 3 dB cut, so the template passes "boosts ≤ cuts" without an exception.

## Tom presets (task 31)

These are **Tony's own settings** from his 2021 album *Between You and Me*, not Joe Gilder's advice, so they're allowed to boost more than the other templates. `mixing/between-you-and-me.md` ("Lessons", 5) says saved chains for vocals, backing vocals, toms and cymbals kept the album consistent. All line numbers are in `music-recording/fx-chains/songs/tony-ward.md`.

**The EQ alone won't recreate the album sound.** The tom chains also had the SSL's compressor and gate (ratio 3–10:1, gate range 40 dB), Neutron 3's compressor and transient shaper, and L1+ limiters. Those are out of scope here.

**Translating the Waves SSL E-Channel.** Each SSL band becomes one Songsmith EQ band:

| SSL band | Model |
|---|---|
| High-pass | 18 dB/oct (Waves manual) → Low Cut, 18 dB/oct, Q 0.707 |
| LF, LF Type On | Bell, Q 0.7 (assumed) |
| LF, no Type | Low Shelf, Q 0.707 |
| LMF / HMF | Bell at the saved Q; HMF Q 1.0 (assumed, see below) |
| HF, HF Type On | Bell, Q 0.7 (assumed) |
| HF, no Type | High Shelf, Q 0.707 |

- **What isn't saved or documented:** the E-Channel's LF and HF bell Q is fixed and isn't in its saved state. The HMF Q wasn't saved either (its range is 0.1–3.5). The Waves manual excerpts found don't give these numbers, and no measured curve was available. So the bell Qs are assumptions: Q 0.7 for the broad LF/HF bells, and Q 1.0 for the HMF. With one band per SSL band, the summed curve matches the SSL **under those assumptions**. It hasn't been checked against a measured SSL curve.
- **What would change most if the assumptions are wrong:**
  - the width of the 163 Hz and 10.3 kHz bells;
  - the 4.07 kHz band's width (±1 to 2 dB at 2 and 8 kHz if the real Q is 0.7 or 1.5).

### Rack tom (index 22)

| Band | Setting | Source |
|---|---|---|
| Low Cut 162 Hz, 18 dB/oct | SSL HP Frq 162 Hz | lines 1114, 1123, 2295, 2315 |
| Bell 163 Hz +4.6 dB, Q 0.7 | LF Gain 4.6 dB, LF Frq 163 Hz, LF Type On | same |
| Bell 670 Hz −11.9 dB, Q 1.7 | LMF Gain −11.9 dB, 0.67 kHz, Q 1.70 | same |
| Bell 4.07 kHz +5.9 dB, Q 1.0 | HMF Gain 5.9 dB, 4.07 kHz (Q not saved) | same |
| Bell 10.32 kHz +3.3 dB, Q 0.7 | HF Type On, 10.32 kHz | 2295, 2315 (+3.3); 1123 (+3.0); 1114 (+4.3) |

- **HF gain:** +3.3 dB. It's the median of the four occurrences (+4.3, +3.0, +3.3, +3.3). The *You Want More* tracks are from the 2023 remix, but the High Tom and Toms bus in *Gone from Here* (album-era) have the same EQ, so the shape is the album's.
- **The resonant bump:** the 162 Hz high-pass with a +4.6 dB bell at 163 Hz makes a bump right at the cut. It's kept because it's on the album. Decide whether you want it on new songs. Lowering the bell, or moving the cut down to about 100 Hz, removes it.
- **Output gain:** −2.6 dB (pink noise, K-weighted).
- **Cut first:** it boosts more than it cuts (+13.8 dB against −11.9 dB). It's a commented exception in the test.

### Floor tom (index 23)

| Band | Setting | Source |
|---|---|---|
| Low Shelf 122 Hz +7.8 dB | LF Gain 7.8 dB, 122 Hz, no LF Type (shelf) | lines 2147, 2304 |
| Bell 930 Hz −6.6 dB, Q 1.7 | LMF −6.6 dB, 0.93 kHz, Q 1.70 | same |
| Bell 4.07 kHz +5.9 dB, Q 1.0 | HMF 5.9 dB, 4.07 kHz | same |
| High Shelf 10.32 kHz +6.8 dB | HF Gain 6.8 dB, 10.32 kHz, no HF Type (shelf) | line 2147 (*Yesterday's Toy*) |

- **Why *Yesterday's Toy*:** its file is album-era. *You Want More* (line 2304) is the 2023 remix, and there the HF is a +1.6 dB **bell** (`HF Type On`), not a shelf.
- **High-pass:** *Yesterday's Toy* has HP Frq 18 Hz. That's below Songsmith EQ's 20 Hz minimum and inaudible, so it's left out.
- **Output gain:** −4.0 dB.
- **Cut first:** it boosts more than it cuts (+20.5 dB against −6.6 dB). It's a commented exception in the test.

### Toms bus (index 24)

The Neutron 3 Equalizer on the Toms bus has the same shape in four songs. Each value is the median of the four:

| Band | Ethereal Engagement (985) | Gone from Here (1122) | Nothing Left To Give (1864) | You Want More (2322) | Preset |
|---|---|---|---|---|---|
| High-pass (Flat Highpass) | 68.6 Hz | 68.6 Hz | 31.9 Hz | 41.5 Hz | Low Cut **55 Hz**, 48 dB/oct |
| Low cut-down | 130 Hz −4.34, Q 4.92 | 149 Hz −7.08, Q 3.63 | 89 Hz −5.00 (Band Shelf) | 108 Hz −4.53, Q 2.69 (Band Shelf) | Bell **119 Hz −4.8 dB, Q 2.0** |
| 317 Hz cut | −8.02 | −8.02 | — | — | Bell 317 Hz −8 dB, Q 1.4, **off** |
| Low-pass (Flat Lowpass) | 10.95 kHz | 10.95 kHz | 10.45 kHz | 10.13 kHz | High Cut **10.7 kHz**, 24 dB/oct |

- **Q:** the median Q of the bells is 3.6, but two of the four songs use Neutron's "Band Shelf" (a flat-topped band, effectively wider). So Q 2.0 stands for the broad cut the prompt describes.
- **Slopes:** Neutron's Q 40 on the Flat Highpass is the maximum (steepest), so the low cut is 48 dB/oct, Songsmith EQ's steepest. The low-pass Q 19.8–27 is about half way, so the high cut is 24 dB/oct. These are interpretations, not measurements.
- **The 317 Hz cut:** it's on in only two of the four songs, so it's loaded off ("only if needed"). The Q is an assumption, since Neutron's default bell Q wasn't saved.
- **Left out:** the small +2.6 dB band at 89 Hz (Ethereal Engagement, Gone from Here) and the +3 dB band at 1.45 kHz (Gone from Here only) aren't common to the songs.
- **Output gain:** +1.4 dB.
- **Gentler versions of the same idea:**
  - *Anguish* (lines 643–644, Mid and High Tom): −3.3 dB band shelf at 299 Hz, +2.45 dB at 136 Hz and 1.9 kHz, high-pass 45 Hz, low-pass 13.6 kHz.
  - *Escape Hatch* (lines 793–794, Toms bus): −2.9 dB at 117 Hz, −5.75 dB at 734 Hz, +3.7 dB at 3 kHz; SSL HP 50 Hz.
  - Both are the same idea in a gentler form: a high-pass, a low-mid cut-down and a presence lift. They aren't separate presets.
- **The saved chains:** the saved "Toms" chain in `fx-chains/saved-chains.md` (line 303) has no EQ (EKramer DR → L1+). The per-song chains carry the EQ.

**Compared with Tony's later Pro-Q 4 tom presets** (*Drums - Floor Tom*, *Drums - Mid Tom*, Jan 2025): not done. The `.ffp` files are OneDrive cloud-only placeholders that couldn't be opened here, and they aren't decoded in the catalogue.
