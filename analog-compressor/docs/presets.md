# Starting points and how the controls work (step 5)

## How the controls work

This follows the UA 1176LN manual. Page numbers are from UM (UA part 65-00046).

- **Input decides how much it compresses.** The threshold is fixed. Turning Input up pushes more signal over it, so you get more compression. Turning it down gives less (UM p. 8 (1), p. 20). There's no Threshold knob.
- **The ratio buttons decide how firmly.** 4 and 8 compress, and 12 and 20 limit (UM p. 9). Higher ratios also raise the threshold (UM p. 21), so switching from 4 to 20 at the same Input catches only the loudest peaks.
- **Attack and Release get faster clockwise.** This is the opposite of most compressors (UM p. 9, p. 21).
  - **Attack**: 7 is fastest, about 20 µs, and flattens transients. 1 is slowest, about 800 µs, and lets the start of each note through.
  - **Release**: 7 is fastest, 50 ms. Too fast and it pumps and breathes. 1 is slowest, 1.1 s. Too slow and a loud moment holds the next quiet one down (UM p. 8 (4)).
- **Output only matches the level.** Set the compression with Input first. Then turn Output until switching the plugin in and out sounds equally loud, so you hear the compression, not the loudness (UM p. 8 (2), p. 22). Output doesn't change the amount of compression.
- **Gain Reduction Off** (Attack "OFF" on the hardware) stops the compression but keeps the amplifiers' colour (UM p. 8 (3)). **Bypass** takes the plugin out completely.
- The **meter** shows the gain reduction (**Gain Reduction**) or the output level (**Output +4** and **Output +8**).

## Starting points

These are conservative settings for common sources. Each one is level-matched: a -18 dBFS RMS signal comes out within 1 dB of the level it went in, with 1.5-10 dB of peak gain reduction (tested). So switching the plugin in and out compares tone, not loudness. Load one from the **Preset** menu, or from your DAW's preset/program list. Then adjust **Input** to taste and re-match with **Output**.

| Starting point | Input | Output | Attack | Release | Ratio | Listening cue |
|---|---|---|---|---|---|---|
| Lead vocal | +1 dB | +3.7 dB | 3 (234 µs) | 5 (~140 ms) | 4:1 | About 3-5 dB on loud lines. Raise Input for more control. Turn Attack down (counter-clockwise) if the consonants go dull. |
| Acoustic guitar | +2 dB | +3.3 dB | 2 (~430 µs) | 6 (~84 ms) | 4:1 | A slower attack lets each pick come through. Listen for the strum to sit steady without the attack going soft. |
| Bass | +3 dB | +2.4 dB | 3 (234 µs) | 3 (~390 ms) | 8:1 | A moderate release keeps low notes from distorting. Each note should come out even. If it growls, slow the Release. |
| Drum room | +6 dB | +1.4 dB | 2 (~430 µs) | 7 (50 ms) | 8:1 | Slow attack, fastest release: the hits punch through and the room swells between them. For more, try **All**. |

Hover over the **Preset** menu to read the listening cue for the loaded starting point.

## Your presets

- **Preset → Save preset…** saves the current sound as a `.sspreset` file in:
  - Windows: `%APPDATA%\Songsmith Studio\Songsmith Analog Compressor\Presets`
  - macOS: `~/Library/Songsmith Studio/Songsmith Analog Compressor/Presets`

  The workflow is the same as Songsmith EQ's, but in a folder of its own.
- **Replacing a preset is always your choice.** If the name is already used by one of your presets, the plugin asks before replacing it. Starting points are read-only: a name such as "Lead vocal" is refused, so choose your own name.
- **Preset → Open presets folder** shows the folder.
- **What a preset stores:** Input, Output, Attack, Release, Ratio, 2:1, All Buttons, Gain Reduction Off and Ignore Below. It never stores bypass, oversampling or the editor size.
- **Loading** a preset is one automation gesture per control it changes. A preset file with missing values loads them as defaults, and out-of-range values are clamped.
- Installers and uninstallers never touch this folder.
