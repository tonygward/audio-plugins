# 1176LN references and design claims

Songsmith Analog Compressor is an original plugin *inspired by* the UREI/Universal Audio 1176LN. The design reference is the **Rev D** blackface 1176LN. This file lists what was read, what each source says, and which design decisions come from a published source and which are our own modelling choices. Every material design claim in the code points back to a row here.

## Sources actually read

| Key | Document | URL | What it really is |
|---|---|---|---|
| **UG** | *1176LN User's Guide*, Universal Audio part 1176-M01, Revision 1.02 (2000) | <https://media.uaudio.com/assetlibrary/1/1/1176ln_manual.pdf> | The UA reissue manual. Its "Theory of Operation" (pp. 7-11) has the block diagram (Fig. 1), input section (Fig. 2), gain-reduction stage (Fig. 3), FET divider (Fig. 4) and gain-reduction control circuit (Fig. 5). The reissue is "patterned ... on the D/E versions" (letter, p. ii). |
| **UM** | *Model 1176LN Solid-State Limiting Amplifier*, Universal Audio part 65-00046 | <https://medias.audiofanzine.com/files/urei-1176ln-manual-473204.pdf> | Despite the "UREI" file name, this is the later UA reissue manual, not an original UREI technical manual. It has "Front Panel" (pp. 8-10), revision history (pp. 17-18), "The Technical Stuff" (pp. 20-22), the same theory of operation and "Specifications" (p. 38). |
| **MA** | Mason Audio, "UREI 1176 DIY" | <https://www.masonaudio.org/diy/comp1176> | DIY build notes. Section 2 is the revision overview. Section 9 has measured ratios. **The only full schematic it shows is Rev F** (Fig. 4, gyraf.dk), plus mnats Rev H/J boards. No Rev D schematic is hosted there. |
| **VK** | Vintage King revision guide | <https://vintageking.com/pages/urei-universal-audio-1176ln-compressor-limiter> | **Not read**: the site returned HTTP 429 (rate limited). The revision facts below come from UM and MA instead. |

No original UREI Rev D factory schematic was available. The circuit figures in **UG** are redrawn by Universal Audio for its D/E-based reissue. Component values taken from them are labelled "UG Fig. n (UA redraw of D/E)". Values from the Rev F schematic in **MA** are **not** used for Rev D behaviour.

## Revision identification (Rev D)

| Claim | Source |
|---|---|
| Rev C (Sept 1970) introduced the black faceplate and the LN (low noise) circuit, built as an epoxy module on the Rev B board. | UM p. 17. UG p. 9 ("LN Circuitry") |
| Rev D integrates the LN circuit on the main board without the module. | UG p. 9 and footnote 1. UM p. 17 |
| Rev E = Rev D + 220 V operation + a 10 MΩ resistor across the ratio switch to avoid pops. | UG p. 9 footnote 1. UM p. 18 |
| Revisions A-F use an input attenuator followed by a transformer. Rev G uses a differential op-amp input. | UG p. 8. UM p. 18 |
| Rev D/E output: Darlington pair + Class A stage (2N3053), as in the UA 1108 preamp. Rev F and later use push-pull Class AB (1109 style). | UG p. 9 ("Output Amplifier"). UM p. 18. MA §2 |
| The FET compression circuit stayed "mostly unchanged" across revisions. The differences are in the amplifier stages. | MA §2 |

## Control semantics (published)

| Control | Published behaviour | Source |
|---|---|---|
| Input | Fixed threshold. Input sets the relative threshold and the total amount of gain reduction. | UG p. 3 "Input Level". UM p. 8 (1), p. 20 |
| Output | Sets the output level after gain reduction (make-up). | UG p. 3. UM p. 8 (2), p. 22 |
| Attack | 20-800 µs. Fully **clockwise is fastest**. Fully counter-clockwise ("OFF") disables compression while the signal still passes through the circuitry. | UG pp. 3-4. UM p. 8 (3) |
| Release | 50 ms-1.1 s. Fully clockwise is fastest. | UG p. 4. UM p. 8 (4) |
| Ratio | 4:1, 8:1, 12:1, 20:1. Higher ratios also raise the threshold. | UG p. 3. UM p. 9 (5), p. 21 |
| Knee | "The 1176LN utilizes soft knee compression and limiting." | UM p. 21 "Knee" |
| All Buttons | All four ratio buttons pressed at once. Distortion "increases radically due to a lag time on the attack of initial transients". Attack and release times keep changing, and the bias points change. The ratio "goes to somewhere between 12:1 and 20:1". | UM p. 9, p. 22 "About All-Button Mode" |
| Meter | VU meter: GR, +4 or +8 (0 VU = +4 or +8 dBm output). In GR mode, 0 = no gain reduction. | UG p. 3. UM p. 22 |
| Stereo | With the 1176SA link, attack and release interact, and the fastest attack doubles. | UG p. 4 "Stereo Operation" |

## Published specifications

| Spec | Value | Source |
|---|---|---|
| Attack | 20-800 µs | UG p. 1. UM p. 38 |
| Release | 50 ms-1.1 s | UG p. 1. UM p. 38 |
| Frequency response | ±1 dB, 20 Hz-20 kHz | UG p. 1 |
| Gain | 45 dB ±1 dB | UG p. 1 |
| Distortion | < 0.5 % THD, 50 Hz-15 kHz, with limiting, at 1.1 s release. +22 dBm output at ≤ 0.5 % THD | UG p. 1. UM p. 38 (UM prints "> 0.5 %", which is a typo given the UG wording) |
| Signal-to-noise | > 81 dB, input at the threshold of limiting, 30 Hz-18 kHz | UG p. 1. UM p. 38 |
| Input / output impedance | 600 Ω (bridged-T input control) / 600 Ω load | UG p. 1 |

## Circuit topology (published)

| Claim | Source |
|---|---|
| Signal path: input attenuator + transformer → FET gain-reduction stage → pre-amp → Output control → output amplifier and transformer. | UG p. 7 Fig. 1 "Overview" |
| **Feedback** compressor: the side chain samples the signal after gain reduction, from the pre-amp output **just before the Output control (R23)**. | UG p. 7, p. 9, p. 10 |
| The FET (Q1) is the lower leg of a divider with R5 (27 kΩ). A higher gate voltage gives a lower drain-source resistance and less gain. | UG p. 8, Figs. 3-4 (UA redraw of D/E) |
| The ratio buttons set the side-chain level and the rectifier bias, which sets the threshold for each ratio. | UG p. 10 "Gain Reduction Control Circuit" |
| Full-wave rectifier (CR2, CR3) into a smoothing capacitor. R55 (attack pot, 25 kΩ) sets the charge rate and R56 (release pot, 5 MΩ) the discharge rate. | UG p. 10, Fig. 5 (UA redraw of D/E) |
| The LN circuit keeps the FET in its linear region to reduce distortion. | UG p. 9 |
| The output transformer is part of the output amplifier's feedback network. | UG p. 9. UG p. 12 "History" |
| Q-bias calibration: with a sine wave at 0 dB output, turn R59 until the output drops 1.0 dB. This sets the threshold. | UG p. 5 |
| Measured ratios on one DIY unit: 4 → 5.1, 8 → 8.2, 12 → 11.5, 20 → 20.5. These are one builder's measurements, not a UREI spec. | MA §9 |

## Modelling choices (not published)

These are our own decisions. Later steps add rows.

| Decision | Why |
|---|---|
| Input is a dB drive control from -24 to +48 dB, default 0 dB. | The hardware Input is an attenuator into a fixed threshold. A plugin at digital levels needs gain above unity to reach the threshold. |
| Output is a trim from -36 to +24 dB, default 0 dB. | Make-up gain range for a plugin. The hardware knob is an attenuator into the output amplifier. |
| Attack and Release are 1-7 knob positions (the numbers printed on the dial), not times. | Keeps the hardware's "clockwise is faster" feel. The position-to-time mapping is defined and tested in step 2. |
| Attack "OFF" is its own automatable switch, **Gain Reduction Off** (`grOff`), not a knob position. | The knob keeps its setting when the switch is toggled, and automation can use the switch on its own. |
| All Buttons is its own switch (`allButtons`), separate from Ratio. | The selected ratio is kept when All Buttons is switched off again. |
| Host bypass is a dedicated `bypass` parameter with a 20 ms cross-fade, as in Songsmith EQ. | Matches the EQ. It is distinct from Gain Reduction Off, which keeps the coloured path. |
| Engine (step 2): zero-delay feedback loop, stereo max-linked, log knob taper, ratio thresholds -19.5/-17/-15.5/-13.8 dBFS with 10/8/7/6 dB knees, 20:1 calibrated to 1 dB GR at -18 dBFS RMS. | See [engine.md](engine.md). Follows published topology (UG pp. 7-10) and published direction (higher ratio, higher threshold; soft knee: UM p. 21). The numbers are ours. |
| The 1176SA attack-doubling and knob interaction when linked are not modelled. | One plugin instance links internally with shared controls. |
