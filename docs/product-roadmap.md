# Songsmith Studio — product roadmap (working ideas)

_Last updated: 4 November 2026. Planning document, not a delivery promise._

## Vision and guardrails

**Professional sound. Musical controls. No engineering degree required.**

Build a coherent suite of roughly **6–12 useful audio plugins** for home-recording musicians, plus a separate standalone whole-song analysis/mastering application if demand warrants it. The emphasis is on readable, consistent interfaces; a small number of trustworthy, contextual starting points; transparent DSP; and reliable installation and DAW integration.

**Status convention:** Built = implemented in repository, not necessarily validated by musicians or hosts. Proposed = discussed but not committed. Exploratory = research/design opportunity, not scheduled. Never represent conceptual features as shipped. This roadmap is a prioritised backlog, not a requirement to build every item.

## Current foundation — built, validation ongoing

- **Songsmith EQ:** multiband parametric EQ, dynamic and linear-phase modes, analyser, musician-focused starting points, scalable shared UI.
- **Songsmith Analog Compressor:** 1176LN Rev D-inspired feedback FET dynamics, All Buttons, colour model, oversampling; **not a circuit-level or hardware-verified clone**.
- **Songsmith Maximiser:** L1-style lookahead limiting, sample/true-peak choices, Songsmith auto release, TPDF dither; **not sonically verified against Waves L1/L2/L4**.
- Shared Songsmith visual language, plain-English labels, Windows suite installer and universal macOS PKG installer.

## P0 — make the current three trustworthy before broad distribution

- Real-world Windows **Studio One** and **64-bit Cakewalk** smoke tests (Dave's setup), macOS Logic/AU checks, automation/preset recall, mono/stereo, multiple instances, offline bounce, latency compensation, installer upgrade/uninstall. Record actual results separately from automated test claims.
- Listening tests against reference plugins with precise level matching: Songsmith FET vs a known 1176; Maximiser vs Waves L1/L2 and modern alternatives; EQ against familiar production tools. Test vocals, acoustic and distorted electric guitars, drums and complete mixes; note weaknesses before adding new algorithms.
- Validate limiter worst-case true-peak overshoot (the present docs report up to 0.28 dB on full-band noise), transparency at different gain-reduction levels, auto-release pumping and stereo imaging.
- Independent usability trials: Dave and 5–10 other musicians; measure completion without assistance, comprehension of controls, sonic results, installation pain points. Fix blockers.
- Confirm installer/package identity and backwards compatibility. Commercial release later requires licence review, privacy/support policy, appropriate code signing and Apple notarisation.

## P1 — musician-first workflow improvements (proposed)

### EQ: instrument frequency guide
- Optional, unobtrusive overlay on EQ graph identifying common tonal regions for selected source (acoustic guitar, electric guitar, bass, voice, drums). Examples: *Rumble*, *Warmth*, *Mud*, *Plastic*, *Presence*, *Crunch*, *Harshness*, *Air*; labels/frequencies vary by instrument.
- Start from documented Joe Gilder terminology and the user's own sessions; mark suggestions as starting points, never universal facts. Tooltip explains what listening change to seek.
- Overlay is educational only (no sound change); off by default. Consider hover-to-learn, click-to-add-band only after usability testing. Ensure accessible contrast, smooth resizing, no busy visual clutter.

### Across the suite
- Small, curated presets, source-specific listening cues, informative default values, plain English throughout. Avoid an unmanageable preset catalogue.
- Consider a progressive interface: **Start** (simple musical task), **Shape** (musical controls), **Advanced** (full technical controls); validate whether extra layers actually improve usability.
- Whole-chain workflow presets grounded in real recordings, with safe level matching; never automatically apply excessive compression to distorted guitars.

## P1 — separate standalone app: Songsmith Loudness Analyser / Mastering (proposed)

**Distinct from the 6–12 plugins.** First ship an **analysis-only desktop app** for Windows/macOS; mastering/export are later options.

- Drag/drop or open finished WAV/AIFF/FLAC; fast offline whole-file scan.
- BS.1770-compliant **integrated LUFS**, loudness range (LRA), and independently checked maximum **true peak (dBTP)**. Display duration/format and understandable explanations.
- **Streaming guidance, not a 'Spotify pass certificate'**: services normalise playback and have changing guidance; -14 LUFS isn't a mandatory acceptance threshold or universal mastering objective. Explain expected playback gain *as an estimate* and avoid changing dynamic songs solely to hit a number.
- Compare before/after only if processed; clear distinction between measured file facts and platform estimates.
- Later: optional mastering with reusable Songsmith processing engines, gain-matched A/B, safe preview, explicit export, independent re-analysis of exported file, optional batch album report.
- Keep file reads/writes off the real-time audio callback; develop reusable analysis/DSP libraries shared with the Maximiser, not a VST host inside an application.

## P2 — additional plugins (candidate portfolio; order subject to user testing)

| Candidate | Musician outcome | Notes |
|---|---|---|
| Optical Leveller (LA-2A-inspired) | Smooth out vocals/bass | Original model; benchmark real-world compression character |
| Reverb | Give recordings a space | Rooms, plate, hall; high-priority everyday tool |
| Delay | Add depth or rhythmic echo | Tempo sync, filtering, feedback |
| De-Esser | Tame sharp S sounds | Clear vocal-focused interface |
| Saturator | Add warmth/edge | Oversampling where useful; safe level matching |
| Gate / Expander | Reduce noise/bleed | Musically sensible controls for home recordings |
| Transient Shaper | Change punch/sustain | Candidate for later if demand exists |
| Guitar Amp / Cabinet | Get usable DI guitar tones | Large undertaking; cabinet IR/content licensing and validation |
| Channel Strip | Mix a track with one coherent workflow | More valuable after core components mature |

**Portfolio size is a target, not a quota.** Pick next plugins from musician demand and difficulty, not a fixed checklist.

## P2 — integrated Songsmith Channel Strip (proposed)

- One musician-facing plugin using **shared internal DSP**, not nested VST3 hosting. Optional modules: EQ, fast FET compressor, optical leveller, saturation, eventually limiter and effects.
- Guided starting points such as *Classic vocal*, *Acoustic guitar*, *Heavy rhythm guitar*; user can bypass/reorder appropriate stages; one preset saves the entire chain.
- **Reverb workflow:** separate dry path and wet-only parallel path, optionally EQ **before** reverb (Abbey Road-style filtering) and/or after it; distinguish clearly from a shared DAW send bus. Do not misrepresent one instance per track as a global reverb send.
- Explore musician controls such as *Tone*, *Punch*, *Smoothness*, *Output*, with advanced panel available if needed.

## P3 — Maximiser evolution (research, not yet essential)

- Verify improvements by controlled listening and loudness-matched comparison, not by imitating feature counts.
- Add integrated/short-term LUFS metering and gain-matched bypass if helpful, but whole-track measurement belongs primarily in the standalone app.
- Evaluate better programme-dependent release, controlled soft clipping/oversampling, stereo link flexibility, and more robust true-peak margin.
- Optional noise-shaped dither to extend current TPDF dither, with independently documented tests.
- Treat Waves **L1/L2/L3/L4** as behaviour/market references only; do not claim proprietary ARC, circuit, or adaptive clipping algorithms are cloned. L3-style multiband limiting and L4-style upward compression/multiple modes are exploratory and should justify their complexity.

## Exploratory — assistance and commercialisation

- **Easy Mix / Learn & Lock:** audition/sample analysis and reversible, level-matched suggestions for EQ/dynamics; do not claim AI-style mastering or one-click professional results without evaluation.
- Potential tiers/packaging: Essentials (three), Studio (6–9), Complete (up to 12); price points remain hypotheses, not market evidence.
- Consider free standalone analyser as a discovery channel; no price or licence promise.
- Collect Neil's product/architecture and go-to-market assessment; use Dave for beginner-unassisted workflow testing. Early beta feedback precedes feature expansion and commercial launch.
- Review JUCE and all dependency licences, support/update mechanism, Windows code signing, macOS Developer ID signing/notarisation, trademark/branding and positioning.

## Decision rules

1. Finish/verify the existing foundation before expanding.
2. Build the smallest improvement that removes an observable musician pain point.
3. Keep standalone mastering, plugins, and channel strip separate release decisions.
4. No release or compatibility claims without measured tests and documented real-DAW checks.
5. Revisit this roadmap after independent user testing; close or demote ideas without evidence.
