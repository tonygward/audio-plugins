# Songsmith EQ roadmap

Songsmith EQ was called TW Pro EQ up to v0.2; the notes below keep the names used at the time.

## Where it stands (v0.2.0)

- 24-band bank with Bell, Low Shelf, High Shelf, Low Cut, High Cut and Notch filters
- Click-free parameter smoothing, type cross-fades and enable/disable fades (SVF engine)
- A response curve drawn from the coefficients the DSP actually uses
- Pre/post spectrum analyser
- Undo/redo, a versioned state with v1 → v2 migration, and persisted editor size
- 525 automated checks plus a CPU benchmark (`EQTests`)

The next steps are less about DSP accuracy and more about what mixing engineers expect from an EQ every day.

---

## Phase 1: Basics people miss straight away (v0.3)

Small, low-risk changes that make the plugin usable on real mixes.

- [x] **Output gain and global bypass.** These are host parameters, with an output meter.
- [x] **Steeper cut slopes.** Offer 6, 12, 18, 24, 36 and 48 dB/oct for Low Cut and High Cut by cascading SVF stages.
- [x] **Solo a band.** Alt-click a node to hear only that band, using a band-pass around its frequency. This helps find problem frequencies.
- [x] **Presets.** Save and load from a user folder, plus a few factory starting points (vocal clean-up, low-end tidy-up, air).
- [x] **Build checks on every push.** A GitHub Actions job that builds and runs `EQTests` on Windows and macOS.

Suggested split: slopes plus output gain and bypass in one PR, and the build checks in a separate PR.

## Phase 2: Reach more users (v0.4)

- [x] **AU format and a tested macOS release.** Logic and GarageBand users can't load the plugin today.
- [x] **Stereo modes per band:** Stereo, Left, Right, Mid and Side. This is the biggest jump in usefulness for mastering and bus work, and it fits the per-channel SVF state the engine already has.
- [x] **Rename to Songsmith EQ** (by Songsmith Studio), ready for a Songsmith family of plugins such as Songsmith Analogue EQ. Plugin codes stay the same, so sessions and presets carry over.
- [x] **Windows installer.** An unsigned Inno Setup `Setup.exe` that installs the VST3 (and optionally the standalone app), upgrades in place, and uninstalls cleanly without touching user presets. Built on CI next to the release zip.
- [ ] **macOS installer.** An unsigned `.pkg` (built with `pkgbuild`/`productbuild`) that installs the VST3, AU and standalone app system-wide, upgrades in place, removes the old TW Pro EQ bundles, and comes with an uninstall script that leaves user presets alone. Built and tested on CI next to the macOS zip.

## Phase 3: Better analyser and workflow (v0.5)

- [x] **Sidechain analyser.** Show a second track's spectrum to spot masking, for example kick against bass.
- [x] **Analyser controls:** freeze, peak hold, tilt (4.5 dB/oct slope) and resolution/speed settings.
- [x] **Auto gain.** Optionally match output loudness so a boost doesn't just sound better because it's louder.
- [x] **Scale the whole curve.** One knob that scales every band's gain together.
- [x] **Piano-roll frequency labels** and note snapping while dragging.
- [x] **Starting-point templates.** One per common source (vocals, guitars, bass, drums, reverb returns), based on Joe Gilder's EQ advice: cut first, small moves, "only if needed" bands loaded bypassed, and loudness-matched.
- [ ] **Review the starting points.** Rebuild the acoustic guitar templates from Joe's full "three cuts and a boost" video and Tony's own acoustic mixes, add "in a full mix" and "pickup" variants, and audit the rest with every band's source written down in `eq/docs/starting-points.md`.
- [ ] **Plain wording and better fonts.** No abbreviations anywhere (`SC` becomes `Side Chain`, `AG` becomes `Auto Gain`, and so on), an embedded open-licence font so Windows and macOS match, and readable text sizes.
- [ ] **Higher-resolution, scalable UI.** A bigger default size, up to 4K, a UI size setting from 75% to 200%, a proportional layout, and sharp drawing at any display scaling.

## Phase 4: The big DSP features (v1.0)

Each of these is a project on its own. Do them in this order:

1. [x] **Dynamic EQ per band:** threshold, ratio, attack and release, with an option to use the sidechain. The engine's ramped-parameter design already suits gain changing over time.
2. [x] **Linear-phase mode.** This needs FFT convolution, latency reporting to the host, and a rework of the tail and latency handling.
3. [x] **Optional oversampling** for high-frequency bells near Nyquist at 44.1/48 kHz, with latency reported to the host.

---

## Things to watch along the way

- **Keep IDs stable.** Every new per-band setting (slope, stereo mode, dynamics) adds host parameters. Add them all to the fixed 24-band bank in one go per release, bump `stateVersion` each time, and keep the migration tests running so older sessions still load.
- **CPU.** Before Phase 4, record what the benchmark harness measures for 3, 12 and 24 bands. Dynamic EQ and linear phase will both cost more, so keep a baseline to compare against.
- **Testing in Studio One.** Each release should include a manual check: save, reopen, automate, and run several instances.

## Out of scope

- CLAP format
- Code-signing the installers, and macOS notarisation (unsigned installers are fine; the SmartScreen and Gatekeeper warnings are documented)
