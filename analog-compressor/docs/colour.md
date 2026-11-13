# Analogue colour, oversampling and latency (step 3)

Step 3 adds the parts of the 1176LN sound that aren't gain reduction. The clean engine (`FET::Engine`, step 2) is unchanged and is still what the tests drive directly. `SongsmithAnalogCompressorProcessor::setColourEnabled (false)` turns the colour off for tests, which leaves the clean engine alone.

## What the technical sources separate

The UA manuals split the hardware into stages that colour the sound for different reasons:

| Stage | Source | What it does to the sound | Where it is in the code |
|---|---|---|---|
| Input attenuator + input transformer | UG p. 8 Fig. 2 | Sets the level into the FET. Not modelled as colour (see below). | Input drive (linear) |
| **FET gain cell** (the FET as the lower leg of a divider with R5) | UG p. 8 Figs. 3-4, p. 9 "LN Circuitry", p. 12 | FETs are non-linear. The more the FET conducts (deeper GR), the more its drain-source resistance follows the signal across it. The LN circuit keeps it near its linear region. | `FET::FetCell` |
| Pre-amp | UG p. 7 Fig. 1 | Fixed gain into the detector and the Output control. Folded into the thresholds. | — |
| **Class A output amplifier** (Darlington + 2N3053, the 1108 design) | UG p. 9 "Output Amplifier", UM p. 18, p. 22 "About Class A" | Single-ended Class A: no crossover distortion, but asymmetric (even and odd harmonics) and soft as it nears its limit. | `FET::OutputStage::amp` |
| **Output transformer** (part of the amplifier's feedback network) | UG p. 9, p. 12 "History" | Core saturation at low frequencies and high levels, plus the low-frequency roll-off of the coupling. | `FET::OutputStage::processSample` |

The order follows the hardware (UG p. 7 Fig. 1): Input → gain cell (+ FET colour) → [detector] → Output control → output amplifier → transformer. The Output control drives the output amplifier, so turning Output up drives the amplifier harder. It still never changes the gain reduction.

### Schematic revision check

No Rev D factory schematic was available (see [references.md](references.md)). The only full schematic in the Mason Audio archive is **Rev F**, whose push-pull Class AB output stage is exactly what Rev D does *not* have (UG p. 9, UM p. 18). **No component values from it are used.** The UA figures (UG Figs. 2-5) are redrawings of the D/E circuit. They name the parts (R5 = 27 kΩ, Q1, 2N3053), but they don't give the device curves needed to derive distortion from the components. So every colour constant below is calibrated to published *performance* figures, not derived from component values.

## Calibration points

The digital reference is the same as in step 2: **0 VU = +4 dBm = -18 dBFS RMS**, so **+22 dBm = 0 dBFS RMS**.

| Published figure (UG p. 1, UM p. 38) | Model | Measured (`AnalogCompressorTests --colour`) |
|---|---|---|
| Output of +22 dBm at no more than 0.5 % THD | Output stage soft ceiling at 2.9 (about +9 dBFS peak). The second-order asymmetry term is 0.005. | **0.55 %** at 0 dBFS RMS, 1 kHz (H2 -49.5 dB, H3 -47.3 dB) |
| (operating level) | | **0.045 %** at -18 dBFS RMS (H2 -67 dB) |
| < 0.5 % THD 50 Hz-15 kHz with limiting, at the 1.1 s release setting | FET 2nd-order term `0.05 · (1 - gain) · y²` | 0.25 % at 8 dB GR, **0.49 % at 17 dB GR**, 0.75 % at 26 dB GR (4:1, 1 kHz, release knob 1 = 1.1 s) |
| ±1 dB, 20 Hz-20 kHz | Coupling high-pass at 8 Hz, transformer flux corner at 20 Hz | -0.64 dB at 20 Hz, -0.11 dB at 50 Hz, 0.00 dB at 10, 16 and 20 kHz (44.1/48/96 kHz, every oversampling setting) |

**Assumptions where no hardware data exists:**

- The FET distortion is purely second order and proportional to how hard the FET conducts (`1 - gain`). It is zero with no gain reduction, because the FET is pinched off.
- The transformer's core term is a cube of a 20 Hz low-passed "flux" signal, with coefficient 0.05.
- The input transformer is treated as clean. Its effect isn't separated in any source we could read.

All of these are tagged as modelling choices in `FETColour.h` and listed in [references.md](references.md).

## Gain Reduction Off (Attack "OFF")

This is the hardware's Attack fully counter-clockwise: "disables compression altogether; however, signal continues to pass through the 1176LN circuitry … to add the color" (UM p. 8). The plugin does the same:

- the engine fades GR to 0 over 10 ms;
- the FET term vanishes, because the FET doesn't conduct;
- the output amplifier and transformer stay in the path, driven by Input and Output.

It is **distinct from host bypass**, which is bit-exact (delayed by the latency when oversampling is on). Both are tested.

## Measurements

`AnalogCompressorTests --colour` prints three tables, all with a mono processor and a sine at an FFT bin centre (2¹⁵ points, 4-term Blackman-Harris window, measured after 1.5 s of settling):

- **THD:** harmonics 2 to Nyquist, relative to the fundamental.
- **Alias:** every other bin from 20 Hz to 20 kHz (excluding DC and ±4 bins around the fundamental and each harmonic), relative to the fundamental.
- **Level-matched:** the compressed run is measured first. Then the GR-off run uses the Output trim to bring the output amplifier to the same fundamental level (within 0.3 dB, tested). The two rows compare colour, not loudness.

### Harmonics and aliasing versus drive (4:1, input -18 dBFS RMS, Windows)

At 44.1 kHz, 1 kHz tone, level-matched:

| Drive | GR | THD with compression | THD with GR off | Alias with compression, off / 2x / 4x | Alias with GR off, off / 2x / 4x |
|---|---|---|---|---|---|
| +6 dB | 8.1 dB | 0.25 % | 0.035 % | -74 / -91 / -108 dB | -121 / -119 / -117 dB |
| +18 dB | 16.9 dB | 0.49 % | 0.051 % | -72 / -89 / -106 dB | -124 / -123 / -117 dB |
| +30 dB | 25.9 dB | 0.75 % | 0.072 % | -72 / -89 / -107 dB | -122 / -118 / -117 dB |

The same at 48 kHz, 1 kHz tone, has compression aliasing of -71 / -86 / -108 dB (+6 dB drive), -70 / -85 / -109 dB (+18 dB) and -70 / -87 / -109 dB (+30 dB).

At 7 kHz, where the harmonics fold back:

| fs | Drive | Alias with compression, off / 2x / 4x | Alias with GR off, off / 2x / 4x |
|---|---|---|---|
| 44.1 kHz | +18 dB | -80 / -83 / -85 dB | -123 / -121 / -116 dB |
| 44.1 kHz | +30 dB | -80 / -83 / -85 dB | -116 / -120 / -116 dB |
| 48 kHz | +18 dB | -70 / -82 / -95 dB | -121 / -121 / -119 dB |
| 48 kHz | +30 dB | -70 / -83 / -94 dB | -115 / -119 / -116 dB |

What the numbers say:

- At these output levels (-14 to -20 dBFS), the **output stage alone** is clean enough that its aliasing sits at the measurement floor (about -115 to -125 dB) at every setting. It only matters when Output drives it hard (about 0 dBFS and above).
- Almost all aliasing comes from the **compression itself**. The fast gain modulation (the detector follows the rectified waveform, as the hardware does) has a wide spectrum.
  - **2x** gains about 15-17 dB on a 1 kHz tone, and 3-13 dB on a 7 kHz tone.
  - **4x** is 5-38 dB better than off in every case measured: 34-38 dB on a 1 kHz tone, but only about 5 dB on a 7 kHz tone at 44.1 kHz.
- **Since ACOMP-18** (release in the gain cell's control-voltage domain, [engine.md](engine.md)) the figures above were re-measured. The new release law ripples less within each cycle, so odd-order distortion fell by 3-8 dB (H3 on a 1 kHz tone about -82 to -85 dB, was -74 to -82). It also moved where the folded components land: with oversampling off, aliasing on a 1 kHz tone rose by 4-10 dB to about -70 to -74 dB, which 4x now removes almost completely. Every figure stays at least 69 dB below the tone.
- THD barely changes with oversampling. The harmonics are the same, and only the folded parts go.

## Oversampling (evaluated)

The new `oversampling` parameter (state version 2, version hint 2) has three settings: Off, 2x and 4x. It uses JUCE polyphase IIR half-band filters in maximum quality, with integer latency. The whole chain runs at the higher rate: engine, FET term, Output trim, output stage. Input drive is applied before upsampling, since it is linear. The engine's times are in seconds, so its timing doesn't change with the rate domain. The tests measure attack and release at 88.2, 96, 176.4 and 192 kHz, and the settled GR through the processor at each setting.

| Setting | Latency (44.1 / 48 / 96 kHz) | CPU, stereo 48 kHz, colour on | CPU, clean |
|---|---|---|---|
| Off | 0 samples | 0.6 % | 0.5 % |
| 2x | **4 samples** | 1.3 % | 1.1 % |
| 4x | **6 samples** | 2.3-2.6 % | 2.0 % |

CPU is the percentage of one core (`AnalogCompressorTests --bench`, Windows Release).

**Default: Off.** Step 3 first chose 4x because it cut compression aliasing by 8-38 dB (5-38 dB since ACOMP-18). The default is now Off, because a compressor often runs on many tracks at once and on tracks being recorded, where zero latency and low processing power matter more than aliasing, which is hard to hear at normal settings. Turn on 2x or 4x for a cleaner top end when compressing hard. The measurements above are unchanged. Sessions saved with version 2 keep the setting they saved (including 4x). Version 1 sessions load as Off, which is how they sounded. Factory presets never store oversampling.

**Default: 4x** (a decision made without a human). It's the only setting that reliably reduces the compression aliasing, by 8-38 dB in every measured case. The cost is about 2.4 % of a core per stereo instance and 6 samples of latency (0.13 ms at 48 kHz). 2x halves the CPU, but gains only 2-4 dB on some material. Off has zero latency, for tracking or for lots of instances.

Changing the setting during playback fades the output out over 10 ms, switches, and fades back in (tested click-free). The new latency is reported to the host from the message thread within 100 ms.

## Latency and aligned bypass

- The reported latency is the oversampler's rounded latency. It is **4 samples at 2x and 6 at 4x at every sample rate**, and 0 when off. A 200 Hz sine shows the same delay within 0.6 samples (tested at 44.1 and 48 kHz).
- The bypass dry signal is delayed by the same amount through a ring buffer. A bypass cross-fade therefore never combs: the level stays within ±1 dB through the fade (tested). With oversampling on, a fully bypassed plugin outputs the input exactly, delayed by the reported latency, so the host's delay compensation keeps tracks aligned.
- `getTailLengthSeconds()` is still 0.

## Listening checklist additions (level-matched, as in engine.md)

1. **GR Off colour:** on a DI bass at -18 dBFS, toggle Gain Reduction Off against Bypass. The difference is subtle at 0 VU. Push Output +12 dB and pull the DAW fader down 12 dB: the low end thickens and the attack gets slightly blunted.
2. **Compression colour:** vocal at 20 dB GR, release fully slow. A/B against the clean build (or a reference compressor) at matched level. A light 2nd-harmonic edge appears on loud syllables only.
3. **Oversampling:** hi-hats or cymbals with 15-20 dB GR and fast attack and release. Off vs 4x: Off adds a faint grainy haze above 10 kHz that 4x removes. Switch during playback and listen for clicks (there should be none).
