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

New parameters (ID version 2; state version 2): `peak_mode` (Sample peak / True peak, default Sample peak) and `auto_release` (default off). A version-1 session loads with both at their defaults, so it sounds exactly as before. Tested in "Version-1 sessions load…".

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

- **Editor:** a vector UI laid out at 720 × 420 and scaled as a whole. The window resizes from 75 % to 300 % at a fixed aspect ratio, and its size is saved with the session. It uses the shared Songsmith UI library (`shared/`) for its embedded Inter font (SIL OFL), type sizes, title, colours and the slightly bevelled controls shared by every Songsmith plugin; see `docs/style-guide.md`. Controls: Threshold and Output Ceiling faders (the L1 calls it "Out Ceiling"; every word is spelled out here), **Link**, Release with **Auto Release**, **Peak** (sample / true peak), **Dither**, Bypass, and the Input, Gain Reduction (0–24 dB, filling downwards like the L1's attenuation meter) and Output meters. `MaximiserTests --screenshot file.png [--scale 2]` renders it.
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

## Starting points, validation and packaging (step 05)

- **Starting points** (`Source/StartingPoints.h`, the editor's "Starting point" menu): all conservative, all with a ceiling at or below −1 dB, and Dither always off. Loading one leaves Bypass and Link alone.

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

  Publishing releases stays manual.
- **CI artifacts** (all separate from the EQ's):
  - `Songsmith-Maximiser-Windows-setup`, `-Windows-zip`, `-macOS-pkg` and `-macOS-zip`;
  - the raw `Songsmith-Maximiser-<OS>` builds;
  - the editor screenshots.
- **Host checks:** see [`docs/host-checks.md`](docs/host-checks.md) for Studio One and Logic (scan, mono/stereo, automation, recall, offline render, latency compensation, multiple instances), listening checks, and the L1 comparison procedure. The L1 comparison hasn't been done: no L1 instance was available.
