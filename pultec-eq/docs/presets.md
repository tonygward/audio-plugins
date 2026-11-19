# Songsmith Passive EQ: where the starting points come from

The nineteen factory starting points (six for the Tone Shaper and, since PEQ-04, two each for the Mid Shaper and the Frequency Filter, and since PEQ-05 three more for each mode; see [the settings tables](#the-settings)), written down before the plugin is called release-ready, as the prompt asks. They are Songsmith starting points, not copies of anyone's settings. Names follow the suite rule: each is named for the job it does, and keeps hardware names, tutorial titles and educators' names out of the title.

## Sources used

- Published practice for this kind of equaliser, as set out in [Vintage King's guide to starting settings](https://vintageking.com/blog/presets-pultec-eqp-1a-program-equalizer/): 60 Hz for the thump of a kick drum without crowding the bass, 20 or 30 Hz for more bass power, and a gentle cut at 100 Hz to clear the low end for the kick and bass.
- The well-known pairing of a low boost with a low cut at the same frequency, described in [MusicRadar's article on the low-end trick](https://www.musicradar.com/how-to/pultec-low-end-trick), which is why Low Boost and Low Cut are separate controls here.
- The published panel values in [reference.md](reference.md).

Joe Gilder's tone-shaping tutorials, which the prompt names as the starting point for preset research, have **not** been reviewed. No value below comes from them and there is no attribution. When they are reviewed, any value taken from them is recorded here, and the educator's name stays in these notes and in optional credits, never in a title or tooltip.

Only part of the Vintage King article was available when this was written, so the vocal and acoustic guitar settings below are Songsmith's own choices from general practice (a small, broad lift of the top for air, a small low cut to reduce boom), not figures from that page.

## The settings

| Starting point | Low | High boost | High cut | Why |
|---|---|---|---|---|
| Bass Weight | 30 Hz, boost 5.0, cut 2.0 | none | none | 30 Hz for bass power; the cut sits higher and keeps the low middle tidy |
| Kick Drum Weight | 60 Hz, boost 4.0, cut 3.0 | none | none | 60 Hz for kick thump; a larger cut to keep it from sounding boomy |
| Lead Vocal Air | 100 Hz, cut 1.5 | 12 kHz, +3.5, bandwidth 3.0 (broad) | none | a broad, small lift of the top, with a little less low rumble |
| Acoustic Guitar Brightness | 100 Hz, cut 2.5 | 8 kHz, +3.0, bandwidth 4.0 | none | a small lift for string clarity, with less body boom |
| Mix Bus Polish | 30 Hz, boost 2.0 | 16 kHz, +2.5, bandwidth 2.0 (broad) | none | very small changes at both ends |
| Soften Harsh Highs | none | none | 10 kHz, cut 3.0 | takes the edge off a bright recording |

Amounts are small on purpose: the hardware's ranges go much further, and these are places to start from.

PEQ-04 added four starting points for the new modes. They are Songsmith starting points from general practice, not from any named source, and have not been listened to level-matched:

| Starting point | Mode | Settings | Why |
|---|---|---|---|
| Lead Vocal Presence | Mid Shaper | Mid Dip 500 Hz, 3 dB; High-Mid Boost 3 kHz, +4 dB | a broad lift around 3 kHz for clarity, with a little less boxiness around 500 Hz |
| Reduce Boxy Mids | Mid Shaper | Mid Dip 300 Hz, 4 dB | takes out the cardboard-box build-up on guitars, drums and vocals |
| Clear Low Rumble | Frequency Filter | High-Pass 80 Hz | removes rumble below the useful range |
| Soften Top Hiss | Frequency Filter | Low-Pass 10 kHz | tames hiss and a brittle top end |

PEQ-05 added nine use-case starting points, three for each mode. The ticket gives the frequencies and a reference knob position; the dB values below are tuned against Songsmith's own response, which is what the tests measure. They are Songsmith starting points from general practice and the ticket's examples, and have **not** been listened to level-matched.

| Starting point | Mode | Settings (committed values) | What the response does |
|---|---|---|---|
| Punchy Kick | Tone Shaper | Low 60 Hz, Low Boost +9.5 dB, Low Cut 3.0 dB | about +2 dB at 60 Hz, more below, and a little under 0 dB around 200 Hz |
| Deep, Tight Bass | Tone Shaper | Low 30 Hz, Low Boost +9.0 dB, Low Cut 3.5 dB | about +1.5 dB at 30 Hz, a small cut above 60 Hz that tightens the notes. Choose 60 Hz for a punchier alternative |
| Mix Bus Air | Tone Shaper | High Boost 16 kHz, +2.0 dB, bandwidth 1.5 (broad) | a small broad lift at the top, the middle untouched |
| Bass Definition | Mid Shaper | Mid Dip 300 Hz, 4.0 dB; Low-Mid Boost 1 kHz, +3.0 dB | less mud at 300 Hz, more definition at 1 kHz |
| Clear Lead Vocal | Mid Shaper | Mid Dip 300 Hz, 2.0 dB; High-Mid Boost 3 kHz, +2.5 dB | a gentle dip at 300 Hz and a small lift at 3 kHz |
| More Attack on a Soft-Sounding Part | Mid Shaper | Mid Dip 500 Hz, 4.5 dB; Low-Mid Boost 700 Hz, +2.0 dB; High-Mid Boost 3 kHz, +3.0 dB | less wool at 500 Hz, more bite at 3 kHz |
| Rumble Cleanup | Frequency Filter | High-Pass 50 Hz, Low-Pass Off | 3 dB down at 50 Hz and everything above untouched. Choose 80 Hz for a stronger clean-up |
| Tighter Rhythm Guitar | Frequency Filter | High-Pass 150 Hz, Low-Pass 5 kHz | a narrower band, 3 dB down at each end |
| Telephone Voice or Guitar | Frequency Filter | High-Pass 500 Hz, Low-Pass 3 kHz | a thin, small sound, 3 dB down at 500 Hz and 3 kHz |

Choices that differ from the ticket, and why:

- **Low Boost and Low Cut on the two bass starting points.** The ticket suggests Low Boost 5 with Low Cut at or slightly above the same knob position. In Songsmith's response the cut is a broad shelf that sits an octave and a half above the boost, so a cut that equals or exceeds the boost cancels the lift: Low Boost 5.0 with Low Cut 5.5 measured **-2.4 dB at 60 Hz**, a cut where a lift was wanted. The committed values keep the boost clearly above the cut so there is a real lift at the chosen frequency and a small cut above it. The hardware's dial positions are not Songsmith's decibel values, so this is a Songsmith tuning, not a copy of any dial setting.
- **New filter steps.** The ticket's Low Cut 150 Hz and 500 Hz, and High Cut 3 kHz, were not steps of the Frequency Filter, so they were added: the high-pass is now Off, 30, 50, 80, 120, 150, 200, 500 Hz and the low-pass Off, 3, 5, 7, 10, 15 kHz. The new steps sit in order between the old ones, so saved filter positions are moved to the same frequency when a session (state version 2) or a preset file (version 1) is loaded. A test checks every old step.
- **One preset menu list.** The menu still has one "Starting points" list; the earlier presets were never split into instrument groups, so the new ones follow in the same list.
- **Long name.** "More Attack on a Soft-Sounding Part" is the ticket's name. Names may now be up to 40 characters, and small joining words ("a", "on", "or") stay lower case in the Title Case check.

## What the tests check

- Each of the nine PEQ-05 starting points restores its whole sound from any mode (nothing is left from the previous mode), survives a saved and reopened session, and is one Undo step; its response does what its name says (`PEQ-05: ...` tests).
- Every starting point sets every sound parameter, leaves Bypass alone, and is available as a host program (`The starting points set every sound parameter, leave Bypass alone, and are the host's programs`).
- No Tone Shaper or Mid Shaper starting point lifts or cuts more than 6 dB anywhere between 20 Hz and 20 kHz (same test, using the engine's own response). A Frequency Filter starting point is a filter, so it is checked to leave 400 Hz to 4 kHz alone (within 0.5 dB) instead.
- Names are short, in Title Case and free of hardware and educator names; each has a listening cue that stands on its own (same test).
- Every word on screen, including the listening cues, passes the plain-words check with no abbreviations (`Plain words: ...`).

## Before this is release-ready

- [ ] Load the plugin in a real host (VST3 in a DAW, Audio Unit in Logic Pro) and check that it opens, resizes, saves and reloads a session, and responds to automation.
- [ ] Listen to each starting point with the plugin switched in and out at the same listening level, and write short listening notes here. There is no output level control, so every boost makes the sound louder: match the level first.
- [ ] Review Joe Gilder's tone-shaping tutorials and record any change to a starting point here, with the source.
- [ ] Decide whether a modelled valve or transformer colour is wanted. It is not modelled now.
