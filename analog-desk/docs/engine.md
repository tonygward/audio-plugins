# Songsmith Analog Desk: engine reference

What the engine does, which parts follow published descriptions of the SSL 4000 E channel strip, which are Songsmith design choices, and the test that checks each. The engine is `Source/DeskEngine.h`; the tests are in `Tests/DeskEngineTests.cpp`. This is the first step of DESK-01 (issue #328): the engine and its tests. The plugin, its editor, presets and installers are not built yet.

## Hardware reference, and how far it is verified

The reference is the SSL 4000 E series channel strip, as described in Solid State Logic's [Channel Strip Guide](https://solidstatelogic.com/channel-strip-guide) and Waves' SSL E-Channel manual. Songsmith Analog Desk is an original design, not a copy, and is not affiliated with or endorsed by Solid State Logic.

**Source status. Please read this before relying on any number.** Most of the 4000 E's published figures could not be read when this was written:

- SSL's guide PDF (`SSL Channel Strip Guide v1 2025.pdf`) could not be read as text, and the Universal Audio manual refused automated access.
- The SSL guide *page* gives only qualitative points for the 4000 E: an 18 dB per octave high-pass filter, a four-band parametric EQ, and dynamics with a feed-forward VCA, a fixed fast attack of 3 ms, an adjustable attack from 3 to 30 ms, a soft knee and a logarithmic release in the later revision. It also says the later EQ has "wider cut and boost ranges (±18 dB vs. ±15 dB)" without saying which figure belongs to which unit.
- Waves' manual (seen only as a search summary, not read in full) gives the high band as 1.5 kHz to 16 kHz and the high-mid band as 600 Hz to 7 kHz.

Every other value below is a **Songsmith design choice**. None of them has been checked against a real SSL unit, and the engine has not been compared with a measurement of one. The ranges are meant to be sensible for a channel strip of this kind and are listed so they can be corrected.

## Signal flow

```
input trim -> drive -> high-pass and low-pass -> [dynamics] -> four-band EQ -> [dynamics] -> output trim
```

The dynamics (a gate or expander, then a compressor) sit either before the EQ (the default) or after it, as the strip's own switch does. The EQ and the dynamics each have an independent bypass. No latency, no look-ahead.

## Published or described behaviour used as targets

| Behaviour | Value | Test |
|---|---|---|
| High-pass slope | 18 dB per octave | `Filters` |
| Four-band EQ with sweepable mids | LF, LMF, HMF, HF | `Equaliser` |
| High band frequency range | 1.5 kHz to 16 kHz (source: Waves, summary only) | `Equaliser` |
| High-mid band frequency range | 600 Hz to 7 kHz (source: Waves, summary only) | `Equaliser` |
| Attack range up to 30 ms | the compressor's attack goes to 30 ms | `Compressor attack and release` |
| Soft-knee option | yes | `Compressor` |
| Dynamics before or after the EQ | switchable | `Dynamics order` |
| Gate and expander | both | `Gate and expander` |

## Songsmith design choices

| Choice | Value | Test |
|---|---|---|
| Low-pass | 12 dB per octave (second-order Butterworth), 3 kHz to 22 kHz, or off | `Filters` |
| High-pass range | 16 Hz to 350 Hz, or off; third order (a first-order and a second-order stage) | `Filters` |
| Every EQ band's range | ±15 dB (the published figures conflict) | `Equaliser` |
| Low band | 30 to 450 Hz, a shelf, or a bell | `Equaliser` |
| Low-mid band | 200 Hz to 2.5 kHz, a bell, Q 0.5 to 2.5 | `Equaliser` |
| High band | a shelf or a bell | `Equaliser` |
| Shelf slope | 1 (the standard shelf) | `Equaliser` |
| Trims | ±20 dB | `Identity` |
| Drive | a smooth symmetric soft clip, `tanh (g x) / g`, with `g` from 1 to 8; 0 is exactly transparent and quiet sounds keep their level | `Drive` |
| Drive aliasing | first-order antiderivative correction, so harmonics fold back less than from a plain clip | `Drive` |
| Compressor | stereo-linked, feed-forward, in decibels; threshold -40 to 0 dBFS, ratio 1 to 20, attack 0.1 to 30 ms, release 0.1 to 4 s, make-up 0 to 20 dB | `Compressor` |
| Compressor soft knee | 6 dB, quadratic, centred on the threshold | `Compressor` |
| Detector | the louder channel's peak, rising at once, letting go over 10 ms | `Compressor`, `Stereo link` |
| Gate and expander | a downward expander: a gate turns the sound down at 10:1 below its threshold, an expander at 2:1, each limited by Range (0 to 40 dB), with attack, hold and release, and a 2 dB hysteresis to open again | `Gate and expander` |
| Compressor structure | the same as the Songsmith Bus Compressor's (see `bus-compressor/docs/engine.md`), with continuous ratio and times | `Compressor` |
| Colour from the console's circuits | not modelled beyond the drive | none |
| Parameter smoothing in the engine | the trims, drive and make-up are smoothed over 20 ms; filter and EQ coefficients change at once, so the plugin must smooth those controls | `Block size` |

## Other checks

- Identity: a new strip leaves the sound exactly as it is, and each stage that is switched off does nothing.
- The filter and EQ responses the engine reports (`responseDb`) match the sound that comes out of the audio path.
- Block-size independence, with everything switched on.
- Damaged input (not a number, infinite) leaves the output finite, and silence in, once the filters have settled, gives silence out, across 40 random combinations of every setting.
- Mono and stereo: with one channel the second is not touched, and two identical channels stay identical.

## Not done yet (the rest of DESK-01)

The plugin itself (processor, parameter IDs and saved state, the editor with the shared title bar, preset menu, Undo and Redo, Bypass and the Output and Gain Reduction meters), factory presets, CI and installer wiring, the version and release checks, Matrix scope, and the README. The engine has not been verified against real SSL hardware or a measurement of it, and it has not been listened to.
