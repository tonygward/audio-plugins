# Songsmith EQ changelog

Songsmith EQ was called TW Pro EQ up to v0.2.0. The entries below use today's names for controls.

## v0.5.0 (2026-11-04)

### Editor
- **Output meter:** a full-height meter on the right of the graph, from −60 to +6 dBFS, with a peak-hold line, the held peak as a number and a clip box at the top. It replaces the small bar in the second row.
- **Piano toggle:** a "Piano" button in the bottom bar shows or hides the piano note strip. It stays in step with the right-click menu, is saved with the session, and is off by default.
- **Edge double-clicks add cuts:** double-click below 40 Hz to add a Low Cut, or above 12 kHz to add a High Cut. Hold Ctrl (Cmd on macOS) to get a Bell instead.

### Starting points and presets
- New template: **Abbey Road reverb**, a five-band EQ to put before a reverb.
- New presets: **Rack tom**, **Floor tom** and **Toms bus**.

### Fixes
- Fixed a possible hang when the linear-phase processing thread was stopped.

## v0.4.0 (2026-11-03)

### Install
- **macOS installer:** an unsigned `.pkg` that installs the VST3, Audio Unit and standalone app for every user, with a choice for each. It upgrades in place, removes the old TW Pro EQ and 3-Band EQ bundles (including copies in your own `~/Library/Audio/Plug-Ins`), and refreshes the Audio Unit list so Logic sees it. `uninstall.sh` removes it again and keeps your presets. CI builds and tests it next to the macOS zip, which now has the version in its name.

### Easier to read
- **No abbreviations:** every control is spelled out: Side Chain, Auto Gain, Oversampling, Output, Dynamic, Threshold, and so on. Labels use title case, stereo modes show as words under the node, and the frequency axis reads 1 kHz, 10 kHz. Host parameter names are now `Band N Frequency`, `Band N Enabled` and `Band N Dynamic Side Chain` (IDs unchanged, so sessions and automation are unaffected).
- **Built-in font:** Inter is embedded, so Windows and macOS look the same, with larger text throughout and fixed-width digits that don't jitter while you drag.

### Bigger, sharper editor
- Opens at 1280 × 760 and resizes up to a full 4K screen. On large windows the controls and text grow too.
- **Interface size** (the `...` menu): 75 % to 200 %. It's saved with the session and remembered on your computer.
- Sharp at any display scaling: curves are drawn per physical pixel and grid lines sit on whole pixels.

### Starting points
- **Acoustic guitar** (was "Acoustic guitar body") rebuilt as "three cuts and a boost": low cut 50 Hz, 150 Hz thump, 500 Hz "plastic", a dynamic 1.8 kHz band that only cuts loud strums, and a 10 kHz air shelf.
- New: **Acoustic guitar in a full mix** and **Acoustic guitar pickup** (piezo DI). 13 templates in all.
- **Lead vocal** gets an optional 4 kHz dip, loaded switched off.
- The classic "Acoustic guitar" preset is now **Bright acoustic**, with a cut instead of a boost at 2.5 kHz. Sessions that saved either old name show the preset under its new one.
- Where every band comes from: `eq/docs/starting-points.md`.

### Build and CI
- Each change builds once: `main` on push, other branches through their PR. Changes that only touch docs, prompts or images don't build at all.
- CI renders editor screenshots at 1x and 2x on Windows and macOS (`EQTests --screenshots`), and `EQTests --paint-bench` times the graph.

## v0.3.0 (2026-11-03)

### Renamed to Songsmith EQ
- **TW Pro EQ is now Songsmith EQ**, by Songsmith Studio. Plugin codes are unchanged, so sessions and presets from TW Pro EQ open as before. Old `.twpreset` files are copied into the new presets folder the first time it's opened.

### Install
- **Windows installer:** an unsigned Inno Setup `Setup.exe` that installs the VST3 and optionally the standalone app, upgrades in place, removes the old TW Pro EQ bundles, and uninstalls without touching your presets. CI builds it next to the release zip.

### Starting points
- **11 starting-point templates** in the preset menu (lead vocal, backing vocals, spoken word, acoustic guitar body, electric guitar, electric guitar bus, bass guitar, drum bus, snare, reverb return and a five-zone layout), based on advice from Joe Gilder's Home Studio Corner videos. Cuts before boosts, "only if needed" bands loaded but switched off, output gain level-matched, and a one-line hint for each.

### New features (roadmap tasks 05–19, merged in PR #17)
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
