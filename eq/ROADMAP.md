# Songsmith Plugins — roadmap

_Forward-looking ideas; no committed delivery dates._

**Principles stay the same:** professional sound, beginner-friendly controls, plain English, consistent interfaces, useful starting points and reliable operation.

## Roadmap at a glance

**Foundation:** Songsmith EQ, Analog Compressor and Maximiser already exist. Keep improving and validating them before widening the collection.

| Direction | What it means | Status |
|---|---|---|
| **More plugins** | Aim for **6–12** useful processors. Next candidates: **Optical Leveller (LA-2A-inspired)**, **Clean Compressor**, **Pultec-style EQ**, **SSL-style Bus Compressor**. Further ideas: **Reverb**, **Delay**, **De-Esser**, **Saturator**, **Gate / Expander**, **Transient Shaper**, **Guitar Amp / Cabinet**. This is a candidate pool, not a commitment to build all of them. | Planned classics / later ideas |
| **Songsmith Channel Strip** | One easy-to-use plugin combining the **existing, validated processing engines** into practical vocal and instrument chains, including a proposed internal parallel Abbey Road-style reverb path. | Future |
| **Standalone Loudness Analyser** | Drop in a **whole finished song** to check integrated LUFS, true peaks, recording format and streaming guidance; optional mastering/export can follow later. | Future |

**Musician-first improvements:** an optional EQ frequency overlay per instrument (e.g. **Warmth**, **Thump**, **Plastic**, **Crunch**, **Air**) grounded in [Joe Gilder and real recording notes](../eq/docs/starting-points.md); plus optional, contextual guidance in every plugin and the standalone app: **“What matters? Why? Do I need to do anything?”** [Guidance prompts](../shared/prompts/README.md#open-implementation--validation-prompts) are future work, not automatically queued.

**Standing rules:** clear British English and musical preset names; consistent UI; honest, evidence-based audio guidance; backward compatibility and real-time safety; useful screenshots retained in PRs and READMEs; a beta release follows each merged plugin change (see the [execution policy](../docs/songsmith-execution-policy.md)). See [development rules](../CLAUDE.md) and the [prioritised backlog](../shared/prompts/README.md).

## Suite direction (high level)

1. **Standalone classics first.** Develop the **analog emulation** track (1176-inspired FET, optical leveller, Pultec-style EQ and SSL-style compressor) and the **digital** track (EQ, Maximiser and clean compressor). See the [Songsmith Plugins backlog](../shared/prompts/README.md) for the prompts and shared acceptance requirements.
2. **Songsmith Channel Strip.** **Model: Opus for architecture and routing, Sonnet for implementation.** Start after releasing and validating the standalone classics in both tracks. Combine the proven processing engines into practical vocal, guitar and other mixing chains.
   - **Internal Abbey Road reverb bus.** **Model: Opus for routing design, Sonnet for implementation.** Send an adjustable amount of signal to a built-in reverb on a parallel path and blend its return with the dry channel. Start with the existing Abbey Road Reverb Trick EQ preset before the reverb (377 Hz low cut, 2 kHz dip and 17 kHz high cut). Include a send level and a bus bypass so the effect can be used without setting up a separate DAW aux.
3. **Songsmith Loudness Analyser.** **Model: Sonnet with the validated Maximiser measurement engine.** A **separate standalone app** for dropping in an entire finished track and checking integrated LUFS, true peaks and straightforward streaming guidance. Optional mastering/export can be considered later.

Keep validating and improving what already exists before fixing the order of new work.

**Language/content follow-up:** [SUI-14](../shared/prompts/done/SUI-14-neutral-tooltips-and-tonal-language.md) audits operational tooltips, keeps Joe Gilder credits in research/provenance rather than user-facing help, and clarifies subjective guitar terms such as Plastic and Cheap without automatically renaming them.

**Future reference review:** [ADV-02](../shared/prompts/ADV-02-reference-mix-guided-review.md) proposes comparing a musician's own mix with a reference, first by measurable stereo-mix properties and later by instrument/group when stems or track routing are supplied. High-effort research, not scheduled.

**Future cross-product feature — Recording and Mixing Advisor:** optional, context-sensitive, plain-English explanations in each plugin, with whole-file recording-format advice in the separate standalone Loudness Analyser. Each message answers *what matters, why it matters, and whether any action is needed*. See [REC-01](../shared/prompts/REC-01-file-recording-advisor.md) and [ADV-01](../shared/prompts/done/ADV-01-musician-guidance.md).

## Songsmith EQ — proposed instrument guide

- [x] **Optional instrument-specific frequency overlay** (delivered as EQ-32 in 1.3.0; see [docs/frequency-guide.md](docs/frequency-guide.md)): show friendly tonal labels on the EQ graph for acoustic guitar, electric guitar, vocals, bass and drums. Draw on Joe Gilder's notes and the existing EQ starting-point research: **Rumble**, **Thump**, **Plastic**, **Warmth**, **Crunch** and **Air**. Keep this a subtle, optional, educational overlay, off by default and without changing audio. Frequency regions are listening suggestions rather than universal rules. Sources: `tonygward/music-recording/techniques/joe-gilder.md` and `eq/docs/starting-points.md`. Prompt: [`prompts/done/32-frequency-guide.md`](prompts/done/32-frequency-guide.md).


---

**Versioning note:** the EQ version numbers on the historical phases below are not current suite release numbers. New releases use the shared root `VERSION`, aligned to the delivery milestones in [the suite backlog](../shared/prompts/README.md).

## Songsmith EQ — existing development roadmap

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
- [x] **macOS installer.** An unsigned `.pkg` (built with `pkgbuild`/`productbuild`) that installs the VST3, AU and standalone app system-wide, upgrades in place, removes the old TW Pro EQ bundles, and comes with an uninstall script that leaves user presets alone. Built and tested on CI next to the macOS zip.

## Phase 3: Better analyser and workflow (v0.5)

- [x] **Sidechain analyser.** Show a second track's spectrum to spot masking, for example kick against bass.
- [x] **Analyser controls:** freeze, peak hold, tilt (4.5 dB/oct slope) and resolution/speed settings.
- [x] **Auto gain.** Optionally match output loudness so a boost doesn't just sound better because it's louder.
- [x] **Scale the whole curve.** One knob that scales every band's gain together.
- [x] **Piano-roll frequency labels** and note snapping while dragging.
- [x] **Starting-point templates.** One per common source (vocals, guitars, bass, drums, reverb returns), based on Joe Gilder's EQ advice: cut first, small moves, "only if needed" bands loaded bypassed, and loudness-matched.
- [x] **Review the starting points.** Rebuild the acoustic guitar templates from Joe's full "three cuts and a boost" video and Tony's own acoustic mixes, add "in a full mix" and "pickup" variants, and audit the rest with every band's source written down in `eq/docs/starting-points.md`.
- [x] **Plain wording and better fonts.** No abbreviations anywhere (`SC` becomes `Side Chain`, `AG` becomes `Auto Gain`, and so on), an embedded open-licence font so Windows and macOS match, and readable text sizes.
- [x] **Higher-resolution, scalable UI.** A bigger default size, up to 4K, a UI size setting from 75% to 200%, a proportional layout, and sharp drawing at any display scaling.

## Phase 4: The big DSP features (v1.0)

Each of these is a project on its own. Do them in this order:

1. [x] **Dynamic EQ per band:** threshold, ratio, attack and release, with an option to use the sidechain. The engine's ramped-parameter design already suits gain changing over time.
2. [x] **Linear-phase mode.** This needs FFT convolution, latency reporting to the host, and a rework of the tail and latency handling.
3. [x] **Optional oversampling** for high-frequency bells near Nyquist at 44.1/48 kHz, with latency reported to the host.

## Phase 5: Polish from daily use

Requests from using the plugin on real mixes. Prompts 27–31 in [`prompts/done/`](prompts/done/).

- [x] **Vertical output meter on the right** of the graph, full height, with a dB scale, peak hold and clip indicator, in place of the small bar in control row 2.
- [ ] **EQ-39 — early Priority 2 follow-up:** [automatic mono/stereo output meter and sticky red CLIP warning](prompts/done/39-stereo-meter-clip-latch.md). One bar for mono; independent L/R for stereo. Clip on either channel latches until sustained silence. UI and real-time-safe metering only; shipped EQ-27 stays done.
- [ ] **EQ-40 — early Priority 2 preset clean-up:** [group Starting Points by instrument, expose Mid/High Tom, hide Five-zone start and remove duplicate Factory choices](prompts/done/40-organise-starting-points.md); retain old preset indices and session compatibility.
- [x] **Piano notes toggle** in the bottom control bar (row 3), so the piano strip from task 16 isn't hidden in a right-click menu.
- [x] **Edge clicks add cuts.** Double-clicking near the far left of the graph adds a Low Cut, near the far right a High Cut, and a Bell anywhere else.
- [x] **"Abbey Road reverb" template**, copied from Tony's Pro-Q 4 preset "Abbey Road Reverb Trick" (low cut 377 Hz, 2 kHz dip, high cut 17 kHz).
- [x] **Tom presets** (rack tom, floor tom, toms bus) from Tony's saved tom chain on *Between You and Me*.

---

## Things to watch along the way

- **Keep IDs stable.** Every new per-band setting (slope, stereo mode, dynamics) adds host parameters. Add them all to the fixed 24-band bank in one go per release, bump `stateVersion` each time, and keep the migration tests running so older sessions still load.
- **CPU.** Before Phase 4, record what the benchmark harness measures for 3, 12 and 24 bands. Dynamic EQ and linear phase will both cost more, so keep a baseline to compare against.
- **Testing in Studio One.** Each release should include a manual check: save, reopen, automate, and run several instances.

## Out of scope

- CLAP format
- Code-signing the installers, and macOS notarisation (unsigned installers are fine; the SmartScreen and Gatekeeper warnings are documented)
