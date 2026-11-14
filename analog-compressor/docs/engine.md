# Feedback engine (step 2)

`Source/FETEngine.h/.cpp` holds the clean gain-reduction engine. It has no colour, no oversampling and no JUCE dependency, so tests can drive it on its own. The processor wraps it:

```
input --> Input drive (smoothed) --> [ Engine: gain cell + feedback detector ] --> Output trim (smoothed) --> bypass fade --> out
```

## Where the detector sits

In the hardware, the side chain is fed from the pre-amp output, after the FET gain cell and **before the Output control** (UG p. 10: "taken from the output of the preamplifier section, just before the volume control potentiometer (R23)"). The 1176 is therefore a feedback compressor (UG p. 7, p. 9).

The engine does the same. It detects the signal *after* its own gain reduction, and the Output trim comes afterwards. Two consequences follow, and both are tested:

- **Input drives compression.** More Input means more level into a fixed threshold (UG p. 3, UM p. 20).
- **Output never changes gain reduction** (`Processor with engine` tests).

The fixed pre-amp gain between the FET and the detector is folded into the thresholds below.

## Loop evaluation

Gain reduction `G` is in dB, ≥ 0. The applied gain is `10^(-G/20)`. For each sample:

1. **Level.** `L` is the peak of `|x|` over the channels, in dBFS (floor -150 dB). This is the full-wave rectifier (CR2/CR3, UG p. 10). `x` is the sample *before* the gain cell, so the output level is `L - G`.
2. **Gain computer.** The steady-state target is `k · knee(L - G - T)`, with `k = ratio - 1`. A detector on the output that turns overshoot into GR with gain `k` gives an input/output slope of `1/(1+k) = 1/ratio`.
3. **Ballistics.** One-pole smoothing, like the capacitor C22 charged through the attack pot and discharged through the release pot (UG p. 10).
   - **Attack**, in dB: `G[n] = G[n-1] + a · (target(G[n]) - G[n-1])`, with the attack coefficient, while the target is above `G[n-1]`.
   - **Release** (ACOMP-18), in the gain cell's control voltage. The cell is a divider, R5 over the FET (UG p. 8, Figs. 3-4), so its gain is `1 / (1 + u)` with `u` = R5 × the FET's conductance, which in its linear region follows the gate voltage on the timing capacitor. The capacitor discharges exponentially, so `u = 10^(G/20) - 1` decays exponentially: `u[n] = u[n-1] + a · (u(target) - u[n-1])`, with the release coefficient. Near 0 dB, `u` is proportional to `G`, so shallow release is exactly as before. Deep gain reduction recovers at an almost steady number of dB per second instead, so heavy compression lets go more slowly, which is the 1176's programme-dependent release. It adds no constants. It is a modelling choice: the FET's conductance is only roughly linear in gate voltage, and no measured hardware release curve was available.
4. **Zero-delay solve.** The target depends on `G[n]` itself, because the detector sees the output of this very sample. The equation is solved for `G[n]` with a safeguarded Newton iteration (bisection fallback, at most 8 steps, usually 1-3). The function is monotonic in both the attack (dB) and release (`u`) forms, so there is exactly one root, and it is bracketed by `G[n-1]` and the explicit step. The release form runs in double precision with `expm1` / `log1p`, because `10^(G/20) - 1` loses its digits in single precision at small `G`.

**Why not a one-sample delay in the loop?** At 20:1 (`k = 19`) and the fastest attack, `a·(1+k)` is far above 2, so a delayed loop oscillates. The implicit solve is stable at every setting, at any sample rate.

## Stereo linking

There is one gain for all channels. It is computed from the louder channel (max of `|x|`), so the stereo image never shifts. This plays the role of the 1176SA stereo link (UG p. 4).

The hardware link doubles the fastest attack, and it interacts with the controls of both units. **That is not modelled:** both channels use the same knobs and the same nominal times. This is a modelling choice.

## Knob mapping (modelling choice)

The published ranges are 20-800 µs attack and 50 ms-1.1 s release, with clockwise faster (UG p. 1, UM p. 8). The mapping between them is logarithmic over the printed dial positions 1-7:

```
attack(k)  = 800 µs · (20/800)^((k-1)/6)      1 → 800 µs, 4 → 126 µs, 7 → 20 µs
release(k) = 1.1 s  · (0.05/1.1)^((k-1)/6)    1 → 1.1 s,  4 → 235 ms, 7 → 50 ms
```

The hardware uses a 25 kΩ attack pot (R55) and a 5 MΩ release pot (R56) with fixed resistors around them (UG Fig. 5, the UA redraw of D/E). The real taper is not published, so the log taper is a choice.

## Ratio, threshold and knee (modelling choice)

| Button | Ratio | Threshold (peak dBFS, Input 0 dB) | Knee width |
|---|---|---|---|
| 4 | 4:1 | -19.5 | 10 dB |
| 8 | 8:1 | -17.0 | 8 dB |
| 12 | 12:1 | -15.5 | 7 dB |
| 20 | 20:1 | -13.8 | 6 dB |
| 2 (Songsmith addition) | 2:1 | -22.0 | 10 dB |

These are the published facts the table follows:

- Higher ratios raise the threshold (UM p. 9, p. 21).
- The knee is soft (UM p. 21).
- The threshold is set by a bias on the rectifier diodes for each ratio (UG p. 10).

The numbers themselves are not published. The thresholds are spread so that switching up a ratio at the same Input gives more limiting without a jump in loudness. Higher ratios get narrower knees, because the 12 and 20 buttons are described as limiting (UM p. 9). The 2:1 row has no hardware counterpart: it is a Songsmith choice, added in ACOMP-11 ([two-to-one-ratio.md](two-to-one-ratio.md)). All Buttons uses its own values ([all-buttons.md](all-buttons.md)).

**Calibration point.** This mirrors the "Q bias" procedure (UG p. 5), which sets the threshold so a sine drops 1.0 dB, done at 20:1 (MA §9). The digital reference is 0 VU = -18 dBFS RMS. At 20:1 with Input 0 dB, a 1 kHz sine at -18 dBFS RMS gets 1.0 dB of gain reduction (tested, ±0.25 dB). The other thresholds hang off this one using the table.

The knee is quadratic and centred on the threshold. It is applied to the *output* level, because that is what the feedback detector sees.

## Timing: the measurement

`AnalogCompressorTests --timing` prints the full table. The tests assert the same numbers. The definitions are below and in `Tests/EngineMeasure.h`.

- **Test signal:** a constant-envelope signal whose samples alternate `+A, -A`. The rectified detector input is then a true step, with no ripple, at any sample rate.
- **Attack time:** let the engine settle at `T - 40 dB` (no GR), then step to `T + over`. The attack time is how long GR takes to first reach 63.2 % (1 - 1/e) of its settled value, with linear interpolation between samples.
- **Release time:** let the engine settle at `T + over`, then step down to `T - 40 dB`. The release time is how long the control voltage `u = 10^(GR/20) - 1` takes to first fall to 36.8 % (1/e) of its value at the step (ACOMP-18; for shallow GR this is the same as 1/e of GR in dB).
- **Levels:** `over` = +10, +20 and +30 dB. **Rates:** 44.1, 48 and 96 kHz. **Knobs:** 1, 4 and 7. **Ratios:** the four hardware ratios (the 2:1 slope is tested separately, see [two-to-one-ratio.md](two-to-one-ratio.md)).

Above the knee, the closed loop speeds the attack up by `1 + k` = the ratio, so the engine scales its internal attack constant by the ratio. In the release phase the detector is below threshold, so the loop is open and the release needs no scaling.

Results (Windows, Release build):

| Case | Nominal | Measured |
|---|---|---|
| Attack, knob 1, +20 dB, all ratios and rates | 800 µs | 786-826 µs |
| Attack, knob 4, +20 dB | 126 µs | 112-121 µs |
| Attack, knob 1 or 4, +10 dB (output inside the knee, where the loop gain is lower) | 800 / 126 µs | up to +17 % / +10 % slower |
| Attack, knob 7 (20 µs) at 44.1 kHz | 20 µs | 6-14 µs |
| Attack, knob 7 (20 µs) at 96 kHz | 20 µs | 13-18 µs |
| Release (of `u`), every knob, level and rate | 50 ms-1.1 s | within ±0.1 % |

The tests allow ±25 % or ±1 sample for attack (whichever is larger) and ±5 % for release.

### Sub-sample limitation

One sample lasts 22.7 µs at 44.1 kHz and 20.8 µs at 48 kHz. That is longer than the fastest attack. At knob 7 the gain cell reaches 63 % of its final GR within the first sample of the step, so the measured time comes out below 20 µs. It depends only on how the interpolation lands inside that one sample. Attack settings faster than about 2 samples (roughly knob 6.3 and above at 44.1 kHz) can't be told apart at the base rate. Step 3's oversampling restores resolution there.

## Smoothing and click-free changes

- **Input and Output:** multiplicative smoothing over 20 ms.
- **Ratio, attack and release changes:** these change only the gain computer and coefficients. `G` is a continuous state, so the applied gain cannot step. Tested: the largest sample step on a DC input while switching ratios and attack every 1000 samples is < 0.01.
- **Gain Reduction Off:** fades `G` to 0 with a 10 ms time constant, then snaps to exactly 0 below 0.001 dB. From then on the samples are untouched.

## Metering

The processor exposes the largest GR since the last read (`takeGainReductionDb()`, lock-free). The editor shows it on a needle with VU ballistics, plus a numeric readout (see editor.md).

## CPU

`AnalogCompressorTests --bench` measures stereo at 48 kHz with 512-sample blocks and 18 dB of drive. It uses about 1.1 % of one core (Windows, Release).

## Listening checklist (level-matched)

Use a DAW with sample-accurate gain. After setting each sound, match the output to the bypassed level with the Output trim, within 0.5 dB on a loudness meter. Then A/B with the plugin's Bypass. Judge the compression, not the loudness.

**Lead vocal** (dry, phrase with loud and soft lines):

1. 4:1, attack 3, release 5. Raise Input until GR peaks at 3-5 dB. Soft words are lifted relative to loud ones, and there's no pumping on held notes.
2. Attack 7 vs attack 1. At 7, consonants (t, k, s) get softer. At 1, they come through with more bite.
3. Release 7 vs release 1. At 7, the level recovers between words and you hear breaths come up. At 1, the GR hangs over the next phrase.
4. 4:1 → 20:1 at the same Input. Only the loudest peaks are held, the threshold sits higher, and there's no level jump or click when switching.
5. Toggle Gain Reduction Off and Bypass. Both pass the vocal unchanged in this step (no colour yet), and neither clicks.

**Drums** (stereo kit or room, with a clear snare):

1. 4:1, attack 1 (slow), release 7 (fast). With 6-10 dB of GR, the snare attack pokes through, the decay and room come up, and the kit sounds bigger.
2. Attack 7. The transients are flattened and the kit sounds smaller and denser.
3. Stereo: pan a hi-hat hard left. A loud snare centred makes the hat duck by the same amount on both sides, and the image doesn't shift.
4. Hot input: Input +30 dB on 20:1. There's no crackle or runaway, and peaks are held at a steady level.
5. Variable buffers: change the DAW buffer size (64 → 2048) during playback. The sound doesn't change.
