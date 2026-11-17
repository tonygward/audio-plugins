# Optical Leveller presets

The four starting points are in `optical-leveller/Source/OptPresets.cpp`. Each one is named for what it is used for. They are **Songsmith starting points**, not copies of anyone's settings, and they are level-matched: Gain is set so a steady -18 dBFS signal comes out about as loud as it went in, so switching the plugin on and off compares the evenness and not the loudness (test: `Every factory starting point is level-matched at -18 dBFS`).

| Preset | Peak Reduction | Gain | Mode | What it is for |
|---|---|---|---|---|
| Lead Vocal Levelling | 55 | +4.0 dB | Compress | Even out a lead vocal without squashing it |
| Bass Guitar Levelling | 62 | +5.5 dB | Compress | Even out the notes of a bass guitar |
| Acoustic Guitar Smoothing | 35 | +0.5 dB | Compress | Very light smoothing for strummed or picked guitar |
| Drum Room Control | 50 | +3.5 dB | Limit | Hold the loudest hits of a drum room and bring up the room |

Names follow the suite rule: plain words describing the use, no model names, no tutorial titles and no playful names. Model choice would be recorded in preset data and tutorial attribution in these notes, not in the title.

## Research and sources

Written before the settings were tuned, as the task requires.

- **What the controls do.** Peak Reduction sets the amount of compression; the unit has no ratio control, so the main decision is how much gain reduction to apply. Gain restores the level. Compress and Limit are two firmness settings. Source: Universal Audio's [LA-2A specification](https://help.uaudio.com/hc/en-us/articles/206356233-Teletronix-LA-2A-Classic-Leveling-Amplifier) and [LA-2A plug-in tips](https://www.uaudio.com/blog/la-2a-collection-tips-tricks).
- **Settings people commonly use.** Universal Audio's tips describe a bass setting with a low to medium Peak Reduction and Gain at about 40 % on their scale; this is one example from one source, not a recommended default, and Songsmith's own scales are different.
- **Joe Gilder's vocal and bass levelling tutorials.** The task asks for preset research to start with these. **They could not be retrieved** when this research was done: searches found his general compression teaching (for example release as a "tone knob") and his *Understanding Compression* course, but no page or video with LA-2A vocal or bass settings that could be read and cited. **No setting here is taken from them, and none is claimed to be.** This research item is still open: read the tutorials, then record in this file what they say and whether any starting point should change.

## What each starting point is based on

- **Lead Vocal Levelling and Bass Guitar Levelling**: moderate Peak Reduction in Compress, so the needle moves by a few decibels on the loudest notes. The values are design starting points chosen for a few decibels of reduction at typical levels, with Gain matched to the reduction at -18 dBFS.
- **Acoustic Guitar Smoothing**: very light, so the needle barely moves. Consistent with the other Songsmith compressors' acoustic guitar starting points, which use gentle, steady smoothing.
- **Drum Room Control**: Limit, because a drum room's loud hits are short and large, which Limit catches harder.

## Before this is release-ready

The task asks for a real host check and level-matched listening notes before calling the plugin release-ready. Neither has been done yet.

- [ ] Load the VST3 in a real host (Studio One, Reaper) and confirm: it appears as **Songsmith Optical Leveller**, the editor opens and resizes, presets load, Bypass works, saved sessions reopen, and automation of Peak Reduction, Gain and Mode records and plays back.
- [ ] Level-matched listening notes for each starting point on real material (vocal, bass, acoustic guitar, drum room): is the levelling audible, does Compress sound smooth, does Limit pump, is the Gain match right?
- [ ] Read Joe Gilder's vocal and bass levelling tutorials and record what they say (see above).
