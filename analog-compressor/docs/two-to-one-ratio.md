# 2:1 ratio (ACOMP-11)

A fifth ratio button, **2:1**, for gentle compression on acoustic guitar, vocals and bass. It is a **Songsmith design choice**: the 1176 hardware this plugin is inspired by has 4, 8, 12 and 20 (plus All Buttons) and no 2:1 button.

## Behaviour

| | 4:1 | 2:1 |
|---|---|---|
| Ratio above the knee | 4:1 | 2:1 |
| Threshold (peak dBFS) | -19.5 | -22.0 |
| Knee width | 10 dB | 10 dB |

The threshold and knee are Songsmith choices. Because 2:1 starts a little lower, it eases in earlier but removes less gain at loud levels: above about -14.5 dBFS peak it takes off less gain than 4:1. Attack and release timing use the same model as the other ratios, and Ignore Below, oversampling, stereo link, bypass and latency are unchanged because 2:1 is only another row in the engine's gain-computer table (`FETEngine.h`, index 4). Later model variants (ACOMP-10) can reuse that row rather than add a second gain computer.

## Compatibility design

A JUCE choice parameter stores an index, but hosts automate it as a **normalised** value, `index / (count - 1)`. With four items the legacy values are 0, 1/3, 2/3 and 1. If 2:1 were appended as a fifth item, the same stored host values would mean 0, 1/4, 1/2 and 1, so recorded automation for 12:1 would land on 8:1/20:1 and, worst of all, an old 20:1 (1.0) would select 2:1. Inserting 2:1 at the front would also renumber every ratio.

So the `ratio` choice is **unchanged** (four items, same ID, same default 4:1), and 2:1 is a separate on/off parameter:

- ID `ratioTwoToOne`, version hint 4, default Off.
- While it is on, it takes over from `ratio`. The stored `ratio` value is left alone, so turning 2:1 off returns to the previous ratio.
- The editor button pressing 2:1 turns on the switch and leaves All Buttons; pressing 4, 8, 12 or 20 turns the switch off and sets `ratio`; All Buttons lights only the four hardware buttons.
- State version is now 4. Sessions from versions 1 to 3 have no `ratioTwoToOne`, and the loader fills missing parameters with their defaults, so they load with 2:1 Off and their saved ratio and All Buttons.
- Presets store `ratioTwoToOne` as the last preset value. Older preset files lack it and load it Off. Factory starting points turn it Off.

What is preserved: saved ratio indexes, normalised host automation for `ratio`, All Buttons, default 4:1, presets. What is not: a host cannot automate "2:1" through the `ratio` lane; it has its own automation lane called "Ratio 2:1". Hosts differ in how they treat newly added parameters, and this has not been checked in a real DAW.

## Tests

`TwoToOneTests` in `Tests/AnalogCompressorTests.cpp`, plus the pinned parameter table, cover: the stored index and normalised value for each of the four legacy choices (and that replaying those values never turns 2:1 on); engine constants for the four hardware ratios; measured 2:1 slope (1 dB out per 2 dB in) above the knee through the running engine; being gentler than 4:1 at loud levels; the switch overriding and restoring `ratio`; ratio switching during playback staying finite; sessions from state versions 1-3 loading with their ratio and 2:1 Off; recall of a session and a preset with 2:1; older preset files; the button, its tooltip, spacing and All Buttons interaction.

## Status

Shipped in 0.5.8 (PR #207). The tests above run in the plugin's CI build. The ratio column holds six buttons, 34 design units high with a 6 unit gap. Not yet verified in a DAW, and there are no listening notes yet.
