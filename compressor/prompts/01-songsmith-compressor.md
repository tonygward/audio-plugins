# COMP-01 — Songsmith Compressor

**Product name:** Songsmith Compressor (exactly; never "Songsmith Clean Compressor" in product UI, installer, or plugin metadata).
**Scope:** New independent dynamics plugin.
**Implementation:** Claude Sonnet by default, Opus for explicitly documented advanced DSP/architecture questions.
**Issue:** [COMP-01 #235](https://github.com/tonygward/audio-plugins/issues/235). Supersedes CLN-01 (#168, closed; its prompt was withdrawn) to prevent duplicate implementation.

## User story and value
As a recording musician I want a simple, transparent conventional compressor that can gently smooth acoustic guitars and vocals or assertively control peaks, with an optional **Analog Warmth** button for colour. I should not need to understand 1176 Input/Output threshold conventions, and I should be able to load a sensible instrument preset and mostly leave it alone.

## Existing vs desired
The existing **Songsmith Analog Compressor** is a fast, fixed-threshold, 1176-inspired FET design with selectable analogue models. It cannot faithfully implement Joe Gilder's ~1.3:1, ~30 ms attack 'butter compression'. **Do not extend or break the Analog Compressor** to accommodate conventional compressor behaviour.

Implement a *separate* plugin named **Songsmith Compressor**, reusing appropriately safe shared Songsmith UI/preset/parameter/CI facilities, without copying Analog Compressor's distinctive fast FET dynamics or introducing a separate technology stack.

## MVP DSP and controls
1. Input signal → independent clean dynamics gain computer (feed-forward recommended; choose RMS/peak detector/soft-knee behaviour explicitly) → optional Analog Warmth stage → Output; wet/dry mix semantics must be documented. Lookahead/oversampling only if warranted and latency correctly reported. No unnecessary multiband or external sidechain in v1.
2. **Threshold:** −60 to 0 dB; **Ratio:** continuously adjustable 1:1–20:1 (precision at 1.2–1.5:1); **Attack:** 0.1–100 ms (30 ms usable); **Release:** 10–2000 ms (30/50 ms usable); **Knee:** hard–soft; **Makeup Gain:** user-set gain with optional reliable Auto mode; **Mix:** 0–100% for parallel control. Defaults gentle and musically useful. If a range conflicts with reliable signal behaviour, document and test an explicitly approved adjustment rather than silently narrowing it.
3. **Analog Warmth** is one straightforward Off/On control, **Off by default**, independently introducing subtle, measured harmonic saturation, not a different threshold/ratio/timing model. Minimise tonal shift and level jumps; gain match in/out across normal levels, avoid aliasing through appropriately designed saturation/oversampling with measured CPU/latency consequences. Do not use licensed/proprietary commercial DSP. In bypass, both compressor and colour are bypassed. Clearly label any model-independent coloration.
4. Metering: input/output dBFS and meaningful gain-reduction measurement, sample-peak clip accuracy and consistency with Songsmith EQ improvements (EQ-42), appropriate ballistics, accurate units and tooltips. Do not falsely label sample peak as true peak or make negative peak readout red from latched historical CLIP.
5. **UX:** Songsmith suite visual standard; title bar **Songsmith Compressor → Preset → Undo → Redo → Bypass**. Friendly tooltips; avoid additional complex mode pages and unnecessary buttons. CPU/latency footer in suite convention; resize and HiDPI support; stable VST3 identity, supported AU/standalone outputs when suite builds allow.
6. Factory presets with clear instrument/use-case names; **Acoustic Guitar** should reproduce Joe Gilder's gentle 'butter' principle: low threshold, ~1.2–1.5:1 (e.g. 1.3), ~30 ms attack, ~50 ms release, level matched, sustained gentle gain reduction not pumping. This is a starting point: low threshold and Input signal-dependent GR; avoid inventing a universal fixed threshold or promising constant exact reduction. Source [Joe Gilder 2018 video](https://www.youtube.com/watch?v=d7vHaydnTuQ&t=190s) and [transcript notes](https://github.com/tonygward/music-recording/blob/main/techniques/joe-gilder.md). Name remains Acoustic Guitar; 'butter compression' belongs in docs/attribution, not preset title. Further curated candidates under ACOMP-13 #233: lead vocal control (and optional gentler vocal finishing), bass guitar, electric guitar sustain, backing vocals, drum bus glue; cross-link actual evidence and avoid speculative numeric attribution. Coordinate moving conventional presets to this plugin; Analog Compressor retains authentic FET-oriented presets. No duplicate factory names within one plugin.
7. Preset handling, state, automation, parameter smoothing and undo must follow existing Songsmith conventions. Reserve permanent vendor/product/plugin identifiers and ensure they differ from Analog Compressor; old host sessions unaffected. Use stable versioned state and migration tests.

## Acceptance and automated testing
- New installed plugin visibly named exactly **Songsmith Compressor** and loads alongside **Songsmith Analog Compressor** with distinct binary, manufacturer/plugin identifiers, DAW scan entries and preserved old sessions.
- Transfer curve checked across threshold/ratio 1:1, 1.2, 1.3, 1.5, 2, 4, 10, 20 and several knees; 1:1 objectively produces no gain reduction (Analog Warmth Off).
- Envelope tests for attack 0.1/10/30/100 ms and release 10/30/50/2000 ms, sample rates 44.1/48/96 kHz and buffer boundaries; include very low threshold and near-continuous gentle compression.
- Analog Warmth Off transparent apart from intentional gain reduction; On produces controlled harmonics with no material uncontrolled level change or unacceptable aliasing, no DC offset, safe silence.
- Stereo linking, bypass, fast parameter automation, no clicks/NaNs/denormals, no allocation or locks in audio callback, sensible low CPU and explicitly documented latency.
- Instrument presets including Joe-grounded Acoustic Guitar load/save consistently, maintain level, have explanatory tooltips and accessible names. Evaluate real guitar/vocal/bass audio where available; if not, do not claim listening verification.
- Unit/DSP tests, UI screenshot tests at min/max sizes and HiDPI, Windows/macOS CI, DAW host checks, and preset/session migration checks.
- Consistent installer + beta website product listing, feature summary, Windows EXE/macOS PKG, private R2 and authenticated downloads, plus accurate release notes.

## Dependencies, risks, delivery expectations
- Link CLN-01 #168 as superseded historical design scope, not a second Matrix task. Link ACOMP-13 #233 (preset research; split ownership), EQ-42 #232 (metering consistency), SUI-18 #202 (title bar), SUI-09 (preset curation). Keep implementation small through verifiable child tasks/PRs if necessary; an epic parent should not be independently scheduled along with its children.
- Major risk: delivering a full commercial-quality new plugin in one bounded Matrix run is too large; agent should split into contained, ordered implementation issues before work if required. Distinguish MVP code, tests, hosted build, release and deployment.
- Follow `docs/songsmith-execution-policy.md`: board selection, one active task, <60% verified Claude usage gate, durable claim/start and issue comments, exact-head CI, trusted merge, third-component beta version increments, private R2 and verified beta installers, completion evidence → issue close → board Done.
