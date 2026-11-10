# Guidance (ADV-01)

Optional, calm help in Songsmith EQ, Songsmith Analog Compressor and Songsmith Maximiser. Code: `shared/Source/SongsmithGuidance.h`. Tests: `shared/Tests/SongsmithGuidanceTests.h`, compiled into all three test executables.

## What the musician sees

A **Guidance** button opens a small card. The card shows, in order:

1. **Measured:** facts the plugin measured (gain reduction, ceiling, the selected band, a validated loudness reading). Only present when there is something measured.
2. **What It Does:** what matters and why.
3. **What To Listen For:** a listening suggestion, never a measurement.
4. **Try This (optional):** a next step only when warranted. "You do not need to do anything" is a normal answer.
5. **Turn Guidance Off / On.** The choice is kept in `Guidance.xml` in the machine's Songsmith Studio settings folder, next to `Diagnostics.xml`. It is not a parameter and is not saved in sessions or presets, so there is no `stateVersion` change and old sessions load unchanged.

The card opens only when the button is pressed. It never appears by itself, never repeats and never changes a setting.

## Where the button sits (control proximity)

| Plugin | Position | Reason |
|---|---|---|
| EQ | Third row, directly after the Frequency Guide instrument menu | The advice builds on the Frequency Guide's regions and the selected band. |
| Analog Compressor | Left of the numeric Gain Reduction readout, under the meter mode buttons (the reserved Low Cut slot stays free) | The advice is about that reading. |
| Maximiser | Top right of the Loudness band, above Reset | The advice talks about reduction and the loudness readings beside it. |

## What each plugin measures, and what it does not claim

| Plugin | Measured or read from controls | Not claimed |
|---|---|---|
| EQ | Selected band frequency and gain; the Frequency Guide region for the chosen instrument | That a spectrum proves muddiness. Wording is a listening suggestion. |
| Analog Compressor | Largest recent gain reduction (meter), actual ratio, All Buttons, Bypass, whether audio is playing | Any universal gain reduction target. It asks for a comparison at the same loudness. |
| Maximiser | Largest recent gain reduction, actual ceiling, Short Term loudness once valid, whether audio is playing | That any service requires one LUFS number. No target is ever named. |

"Audio is playing" means the plugin's existing input or output meter saw a level above -60 dB in the last three seconds. With silence, or with Bypass on, the card says there is nothing to say and gives no advice.

Out of scope: recording-format advice (sample rate, bit depth). A plugin cannot know the original file's bit depth from a host buffer. That belongs to the standalone whole-file adviser (REC-01).

## Sources

The EQ wording uses the Frequency Guide's existing region text (see `eq/docs/frequency-guide.md`). The compressor and Maximiser sentences are new Songsmith wording. No named educator, recorded setting or source is quoted on screen. The tests do not check provenance of the Frequency Guide text.

## Real-time safety

Nothing runs in `processBlock`. The editors read values their meters already publish, on the message thread, and the card is built only on a click.

## Tests

`SongsmithGuidanceTests` covers: silence and missing evidence give no advice and no measured facts; the EQ, compressor and Maximiser trigger conditions; plain words (shared wording rule); the three questions answered; forbidden claims (mud, streaming services, targets, bit depth); British spelling; length limits; the preference default, saving and restoring; the card's headings and switch; the button building nothing until pressed; messages being pure functions of plain values (no parameter or audio access).

## Not yet verified

- Editor layout and screenshots: the three layouts were changed without a local build. Before and after screenshots (`--screenshots`) must be produced by the hosted build and checked at minimum width, especially the EQ third row.
- Studio One, Cakewalk and macOS: not checked.
- Interaction between the callout and the host window on each DAW: not checked.
