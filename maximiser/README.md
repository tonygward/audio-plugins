# Songsmith Maximiser

A look-ahead brickwall maximiser built from the Waves L1 manuals' published behaviour. It is a sibling of `eq/` (JUCE + CMake).

## Build and test

```
cmake -S maximiser -B maximiser/build -A x64 -DJUCE_DIR=C:/dev/JUCE
cmake --build maximiser/build --config Release --parallel 8
maximiser/build/MaximiserTests_artefacts/Release/MaximiserTests.exe
```

## Identity and parameters (step 01)

- VST3, Standalone, plus AU on Apple. AU: `aufx Mxlm Tnyw`. Bundle ID: `com.songsmithstudio.songsmithmaximiser`.
- Stable IDs (version 1): `threshold` (-30..0 dB, default 0), `ceiling` (-30..0 dB, default 0), `release` (0.01..1000 ms, log control, default 1 ms), `bypass` (host bypass, 20 ms cross-fade).
- **Finite dB floor (design choice):** the manual shows silence and full attenuation as minus infinity. Here every level/gain conversion stops at -120 dB, so meters, readouts and state never hold -inf or NaN (`Max::gainToDb`, `Max::dbToGain`; tested in "Finite dB floor").
- State is versioned (`version` = 1). Missing parameters restore to their defaults; out-of-range values are clamped; unknown IDs are ignored.
- Step 01 processing is transparent. The limiter arrives in step 02.

## Limiter (step 02)

`Source/BrickwallLimiter.h` holds the look-ahead sample-peak limiter. It can be tested without the plug-in.

- **Gain law (from the manual):** below Threshold the gain is a constant Output Ceiling − Threshold (dB). Above it, the limiter adds attenuation (≤ 0 dB) so no sample exceeds the ceiling. Tested in "T = -6, C = -1" and "Threshold / ceiling interaction".
- **Stereo:** one shared gain, driven by the louder channel, so the stereo image never shifts. Tested in "Stereo image".
- **Latency and Processing Load readouts:** the Processing Load switch at the far left of the strip along the bottom of the editor shows, when on, this instance's CPU use and the reported latency, for example "CPU 0.35%, Latency 1.8ms (84 samples)" at 48 kHz. The switch is a machine setting, not part of the session. Tested in "Latency and Processing Load".
- **Look-ahead (design choice):** 1.5 ms, rounded up to whole samples. That is 67 samples at 44.1 kHz, 72 at 48 kHz and 144 at 96 kHz. It is also the reported latency. Bypass delays the dry signal by the same amount, so switching bypass never shifts the audio in time. Tested in "Look-ahead and reported latency" and "Bypass is time-aligned".
- **Gain scheduler (design choice):** the needed gain is `r = min(1, ceiling / peak)`. It goes through an instant-attack release envelope, then a sliding minimum over L + 1 samples, then a box-car mean over the same L + 1 samples. Each term of that mean is a minimum over a window that contains sample n − L, so the gain at the moment the delayed sample n − L is output is ≤ r[n − L]. The ceiling therefore holds at the exact sample it protects, with no clipper. A final `min(g, r[n−L])` removes float rounding in the running sum. The attack is a linear ramp over the look-ahead. Tested in "Impulses at every block offset", "Automation never breaks the ceiling" and "Variable block sizes".
- **Release law (design choice):** the gain recovers exponentially towards unity, with time constant = Release. After one Release time, 63 % of the gain reduction has recovered. The result then passes through the same 1.5 ms box-car, so even 0.01 ms recovers over at least about 1.5 ms. Tested in "Release law".
- **Parameter smoothing:** Threshold and Output Ceiling glide over 20 ms. Each sample is protected by the ceiling that was current when it was detected.
- **Reset:** `AudioProcessor::reset()` clears the delay line and the envelope. Tested in "Reset clears…".

## True peak and auto release (step 03)

New parameters (ID version 2; state version 2): `peak_mode` (Sample peak / True peak, default True peak for new instances) and `auto_release` (default off). A version-1 session loads as Sample peak with manual release, so it sounds exactly as before. Version 2 and 3 sessions keep the peak mode they saved. Tested in "Version-1 sessions load…".

- **True-peak detector (design choice):** `Max::TruePeakDetector` uses 8x oversampling with a Kaiser-windowed sinc (24 taps per phase, β 8). For each sample it reports the largest reconstructed magnitude on both sides of that sample, across all channels, and the limiter uses this in place of the sample peak. I chose 8x over 4x because 4x under-read full-band noise by about 0.4 dB here, and the output then overshot by 0.94 dB.
- **Latency:** the detector needs 12 samples. The audio waits for it in **both** modes, so the latency is look-ahead + 12 in either mode: 79 samples at 44.1 kHz, 84 at 48 kHz and 156 at 96 kHz. Switching peak mode never changes the latency the DAW compensates, and never moves the audio. Sample-peak mode skips the filter, so it costs nothing extra.
- **Verification meter:** `Max::TruePeakMeter` is an independent 8x interpolator with 32 taps per phase (β 10). It is longer than the detector, so the tests don't grade the detector against itself. Its accuracy was checked on a full-scale fs/4 tone at 45°, whose samples sit at −3 dBFS while the waveform peaks at 0 dBTP.
- **Measured overshoot** above a −1 dBTP ceiling (T −6), read by the meter:

  | Signal | True-peak mode | Sample-peak mode |
  |---|---|---|
  | 5, 11.025, 15 and 19 kHz tones at 4 phases | ≤ 0.001 dB | up to +3.01 dB |
  | 17 kHz bursts | ≤ 0.001 dB | |
  | ±1 doublet clicks | 0.009 dB | |
  | Full-scale full-band white noise (worst case) | 0.28 dB | |

  The test tolerances are 0.1 dB, and 0.35 dB for white noise.
- **Auto release (Songsmith recovery law; Waves ARC was only a behavioural reference):** a sustained-gain estimate `s` follows the envelope. It falls with a 40 ms time constant and rises with 400 ms. While the gain is below `s`, the limiter is recovering from a transient and uses the manual Release. Once the gain is back at `s`, it recovers with max(Release, 150 ms).

  Measured at 48 kHz, T −8, C −1:

  | Mode | THD (50 Hz, ~7 dB GR) | Pumping (bass between kicks) | Recovery after a 5 ms spike | CPU, sample-peak mode |
  |---|---|---|---|---|
  | Manual 1 ms | 17.3 % | 0.00 dB | 4.4 ms | ~70 ns/sample |
  | Manual 150 ms | 1.9 % | 1.41 dB | 270 ms | ~60 ns/sample |
  | Auto (Release 1 ms) | 3.0 % | 0.26 dB | 4.4 ms | ~50–120 ns/sample |

  Timings are from a Windows dev machine and vary between runs. True-peak mode costs about 290 ns per stereo sample. Tested in "Auto release vs manual".

## Editor and dither (step 04)

- **Editor:** a vector UI laid out at 860 × 448 and scaled as a whole. The window resizes from 75 % to 300 % at a fixed aspect ratio, and its size is saved with the session. It uses the shared Songsmith UI library (`shared/`) for its embedded Inter font (SIL OFL), type sizes, title, colours and the slightly bevelled controls shared by every Songsmith plugin; see `docs/style-guide.md`. Controls: Threshold and Output Ceiling faders (the L1 calls it "Out Ceiling"; every word is spelled out here), **Link**, Release with **Auto Release**, **Peak** (sample / true peak), **Dither**, Bypass, and the Input, Gain Reduction (0–24 dB, filling downwards like the L1's attenuation meter) and Output meters. `MaximiserTests --screenshot file.png [--scale 2]` renders it.
- **Link (design choice):** with Link on, moving Threshold or Output Ceiling moves the other by the same amount, so C − T (the gain below threshold) stays exactly the same. The movement stops where either control would leave its range (the common movement range); C − T is never squashed. Link only follows the user's own moves. Host automation of one parameter never drags the other, which is why the two faders are driven by hand rather than by parameter attachments. Link is saved with the session (UI state, not automatable). Tested in "Linked move…" and "Link follows the user, not host automation".
- **Meter ballistics:** these live in the editor only (`Max::MeterBallistics`: instant rise, 20 dB/s fall, 1.5 s peak hold). The audio thread only publishes raw block peaks and the deepest gain reduction. Tested in "Meter ballistics", plus a check that the audio is identical with and without an editor open.
- **Dither (design choice; the legacy IDR controls were only a workflow reference):** new parameter `dither` (Off / 16-bit / 24-bit, ID version 3, state version 3, default Off).
  - The last stage before the bypass mix applies TPDF dither (±1 LSB), rounds to the target bit depth and clamps to the largest code.
  - Each channel has its own xorshift generator, seeded differently, so the noise floor is uncorrelated between channels.
  - There is **no noise shaping**: none was added, because it couldn't be specified and tested independently in this step.
  - Bypass passes the undithered dry signal.
- **Dither headroom:** while dither is on, the limiter's ceiling is lowered by 1.5 LSB: 4.6e-5 at 16 bits (0.0004 dB at −1 dBFS) and 1.8e-7 at 24 bits. Since |dither| < 1 LSB and rounding adds at most ½ LSB, the quantised sample peak never exceeds Output Ceiling. In true-peak mode, the true peak after quantisation can additionally exceed the ceiling by at most 1.5 LSB, on top of the true-peak tolerance.
  - Tested in "Ceiling holds after quantisation": every sample lands on the 16/24-bit grid and stays ≤ the ceiling.
  - Tested in "TPDF noise": the noise power is q²/4, the spectrum stays within 0.1 dB across 8 bands, and the L/R and lag-1 correlations are < 0.02.
  - Tested in "Bypass skips dither…": state recall, and version-2 sessions load with dither off.

## Loudness meter (MAX-07)

A Songsmith addition: the Waves L1 had no loudness meter. Since MAX-09 the editor shows only **Integrated LUFS**, in a column to the right of the Release dial (the only dial; the faders are Threshold and Output Ceiling), level with it and never below it. It measures the output of the plugin: after the limiter and dither, and, when bypassed, what passes through. Hover over it for a plain explanation, including that around −14 LUFS is a useful reference for some streaming masters, not a rule. The window is 860 × 448 at 100 % (it was 720 × 552, with a loudness band along the bottom); it still scales from 75 % to 300 %. Short Term and Momentary are no longer displayed; the measurement code and its tests are kept. Not yet verified in a DAW; before and after screenshots come from the test executable (`--screenshots`).

- **It only hears real-time playback.** It measures what plays through the plugin, as it plays, so it is not an offline analysis of a whole file like a DAW's export analysis. Play the song through from the start and the Integrated reading at the end is the song's loudness. A fast offline bounce is not a measurement. Readings are measurements of the audio that reached the plugin, not a pass or fail against any streaming service.
- **Readings:** Integrated is everything since the last reset (internally Momentary, 400 ms, and Short Term, 3 s, are still computed but not shown). A dash means there is not yet enough sound (or only silence); no invented number is shown. Loudness range is not shown ("LU" is not an allowed short form); it can come later. A short-term history strip was left out to keep the layout uncluttered.
- **Reset:** the **Reset** button and a click on the Integrated number start again. The meter also starts again by itself when the host starts playback from a stop (design choice; read from the host's play head, so the standalone app, which has none, only resets by hand). It is not a parameter, is not automatable and is not saved: the state version is unchanged (still 3).
- **Counting continues with the editor closed.** The readings are kept by the processor and a 10 Hz message-thread timer drains the audio thread's queue, so Integrated carries on while the editor is closed. If the host stalls the message thread for more than about 6.8 minutes the newest 100 ms steps are dropped rather than blocking the audio thread.
- **Layouts:** mono and stereo (the only layouts the plugin supports). Left, right and mono have weight 1.0 as in BS.1770-4, so the same signal reads 3 dB lower in mono than in both stereo channels.
- **Real-time safety:** the audio thread only K-weights and adds up the mean square of each 100 ms step and writes it to a lock-free single-producer, single-consumer queue (`juce::AbstractFifo`). The queue is allocated once when the plugin is created (not in `prepareToPlay`, so a sample-rate change can never race the editor reading it). Gating, windows and the histogram (0.01 LU bins, so Integrated costs the same after an hour as after a minute) run on the message thread.
- **Cost:** two biquad stages per channel per sample, about 10 multiply-adds, plus a few additions per 100 ms. Not yet measured on hardware: use the Processing Load readout to see it on your machine.

| Source | Where it is used | Checked by |
|---|---|---|
| ITU-R BS.1770-4: K-weighting. The standard publishes 48 kHz coefficients only; they are derived here for every rate from the analogue prototype (`kWeightingCoefficients`) | `Source/Loudness.h` | "K-weighting: BS.1770-4 coefficients at 48 kHz" (published coefficients; response at 1 kHz, 10 kHz and 38 Hz; 44.1, 96 and 192 kHz) and "Readings agree at 44.1, 48, 96 and 192 kHz" |
| BS.1770-4: channel weights, −0.691 offset, 400 ms gating blocks every 100 ms, −70 LUFS absolute gate and −10 LU relative gate | `StepMeter`, `LoudnessReadings` | "Mono reads 3 dB lower…", "Gating: …" |
| EBU R 128: Momentary 400 ms, Short Term 3 s, Integrated | `LoudnessReadings` | "Momentary (400 ms) and Short Term (3 s) rise and fall…" |
| EBU Tech 3341: 1 kHz tone at −23 and −33 dBFS (±0.1 LU); gated sequences | `Tests/MaximiserTests.cpp` | "Tech 3341: 1 kHz tone…" and "Gating…". The gated sequences are modelled on cases 3 to 5 from their descriptions, with expected values worked out by hand; they are not the standard's own files |
| Design choices: reset on play from a stop, the queue, nothing saved | `PluginProcessor` | "Integrated counts only audio after a reset", "The processor measures its output…", "The meter is not saved…", "No allocation in processBlock with the meter running" |

**Not yet verified in a DAW** against a reference meter; see `docs/host-checks.md`.

## Title tooltip (SUI-17)

Hover over **Songsmith Maximiser** at the top left for a short explanation of what the plugin does and one listening suggestion. It is fixed wording, not a measurement, and never names a loudness number you must reach. The old Guidance button and card were removed; the meters and loudness readings are unchanged, and the **Preset** menu stays right after the name. See [../shared/docs/guidance.md](../shared/docs/guidance.md). Not yet verified in a DAW.

## Starting points, validation and packaging (step 05)

- **Starting points** (`Source/StartingPoints.h`, the editor's **Preset** menu in the title bar, which reads: name, Preset, Undo, Redo, Bypass; Peak Mode and Dither sit under the Release knob): all conservative, all with a ceiling at or below −1 dB, and Dither always off. Loading one leaves Bypass and Link alone.

  | Starting point | Threshold | Output Ceiling | Gain | Release | Peak | Auto Release |
  |---|---|---|---|---|---|---|
  | Mix bus | −1.5 dB | −1.0 dB | +0.5 dB | 10 ms | Sample | On |
  | Demo loudness | −6.0 dB | −1.0 dB | +5 dB | 3 ms | True | On |
  | Final export | −3.0 dB | −1.0 dB | +2 dB | 5 ms (the manual's 3–7 ms) | True | Off |

  Tested in "Starting points": each holds its ceiling on loud material, with true peak within 0.1 dB.
- **Stress tests** ("Limiter stress"): 120 randomised cases covering:
  - every rate from 44.1 to 192 kHz, mono and stereo;
  - block sizes from 1 to 4096 samples, and all modes, including dither;
  - automation every few blocks;
  - noise up to +24 dBFS, square waves, DC, impulses, sweeps and denormals.

  The output was always finite and never above the ceiling. One extra case drives the input to +40 dBFS.
- **Installers:** `maximiser/installer` is adapted from Songsmith EQ's, with its own identities. Its Inno Setup AppId and its pkg receipt IDs (`com.songsmithstudio.songsmithmaximiser.*`) must never change. CI builds and tests:
  - **Windows:** an Inno Setup installer plus a zip. The test installs per-user into a temp folder, then checks:
    - an upgrade in place;
    - that Setup refuses to install while the plug-in is loaded;
    - uninstall;
    - that a Songsmith EQ install is never touched.
  - **macOS:** a `.pkg` plus a zip. The test installs system-wide on the throwaway runner, runs auval on the installed component, reinstalls, installs with the app deselected, and runs `uninstall.sh`.

  These per-plugin installers are internal CI test files. Releases are published automatically and contain only the two suite installers; see [release checkpoints](../docs/automatic-release-checkpoints.md).
- **CI artifacts** (all separate from the EQ's):
  - `Songsmith-Maximiser-Windows-setup`, `-Windows-zip`, `-macOS-pkg` and `-macOS-zip`;
  - the raw `Songsmith-Maximiser-<OS>` builds;
  - the editor screenshots.
- **Host checks:** see [`docs/host-checks.md`](docs/host-checks.md) for Studio One and Logic (scan, mono/stereo, automation, recall, offline render, latency compensation, multiple instances), listening checks, and the L1 comparison procedure. The L1 comparison hasn't been done: no L1 instance was available.
