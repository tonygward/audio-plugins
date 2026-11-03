# TW Pro EQ changelog

## Unreleased (merged in PR #17)

### New features
- **Output gain and bypass:** two new host parameters, smoothed so changes don't click. The top bar now has an output gain bar, a peak meter with hold and clip, and a bypass button.
- **Cut slopes:** low and high cuts go from 6 to 48 dB/oct. Choose the slope in the band panel or from a submenu; the curve shows the real slope.
- **Band solo:** Alt-click a band, or use the Solo button or menu item, to hear just that band's range. It fades in and out over 20 ms, and the soloed region is shaded.
- **Presets:** a preset bar with factory presets and your own preset files. Loading a preset is a single undo step. The top strip now has two rows.
- **Per-band stereo mode:** each band can work on Stereo, Left, Right, Mid or Side. The band panel has a selector, the node shows a badge, and the curve has its own view.
- **Sidechain:** an optional sidechain input that is only analysed, not processed. Its spectrum shows in the display with the overlap highlighted; the selector is in the top strip.
- **Analyser:**
  - Selectable FFT size and speed, and a freeze button.
  - A peak-hold line, cleared by clicking.
  - Tilt and range settings.
  - A settings popup for all of these.
- **Auto gain (AG button):** matches the output's loudness to the input over a 2 s window, within ±12 dB. It stops adjusting below -60 dB, and a readout shows the correction live.
- **Gain scale:** one control scales every band's gain from -100 to 200 %. It's saved in presets, and the curve and nodes follow it.
- **Note labels:**
  - Frequencies also show as note names (A4 = 440 Hz), and you can type a note name to set a frequency.
  - Ctrl/Cmd-drag snaps to semitones.
  - An optional piano strip.
- **Dynamic EQ per band:**
  - Level-dependent gain with a soft knee, attack and release, and an optional sidechain trigger.
  - A Dyn row in the band panel, with the moving gain shaded live on the curve.
- **Linear-phase mode:** a phase mode selector switches the EQ to linear phase. The filter is designed on a background thread and the latency is reported to the host. Dynamic EQ and solo are greyed out in this mode.
- **Oversampling:** optional 2x or 4x, with the extra latency reported to the host. The curve is drawn at the oversampled rate.

### Fixes
- The output-gain ramp mixed in an uninitialised buffer, which could produce NaN.
- An overflow in the display's change check.

### Build, CI and platforms
- GitHub Actions builds on Windows and macOS, runs `EQTests` and uploads the plugins.
- macOS builds an AU, validates it with `auval`, and zips the VST3 and AU.
- Tests that failed on macOS from tiny rounding differences now allow a small tolerance.
- Each feature has its own tests and, where it matters, a CPU benchmark. README and `ROADMAP.md` are updated to match.
