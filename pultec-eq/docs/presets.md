# Songsmith Passive EQ: where the starting points come from

The six factory starting points, written down before the plugin is called release-ready, as the prompt asks. They are Songsmith starting points, not copies of anyone's settings. Names follow the suite rule: each is named for the job it does, and keeps hardware names, tutorial titles and educators' names out of the title.

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

## What the tests check

- Every starting point sets every sound parameter, leaves Bypass alone, and is available as a host program (`The starting points set every sound parameter, leave Bypass alone, and are the host's programs`).
- No starting point lifts or cuts more than 6 dB anywhere between 20 Hz and 20 kHz (same test, using the engine's own response).
- Names are short, in Title Case and free of hardware and educator names; each has a listening cue that stands on its own (same test).
- Every word on screen, including the listening cues, passes the plain-words check with no abbreviations (`Plain words: ...`).

## Before this is release-ready

- [ ] Load the plugin in a real host (VST3 in a DAW, Audio Unit in Logic Pro) and check that it opens, resizes, saves and reloads a session, and responds to automation.
- [ ] Listen to each starting point with the plugin switched in and out at the same listening level, and write short listening notes here. There is no output level control, so every boost makes the sound louder: match the level first.
- [ ] Review Joe Gilder's tone-shaping tutorials and record any change to a starting point here, with the source.
- [ ] Decide whether a modelled valve or transformer colour is wanted. It is not modelled now.
