# Songsmith suite prompts

This folder holds the prompts for work across the whole suite. The rules for when a prompt runs and how a task finishes are in the [execution policy](../../docs/songsmith-execution-policy.md); what a prompt contains is in the [issue and prompt template](../../docs/backlog-issue-prompt-contract.md). Neither is repeated here.

## Open prompts

This is an index, not the queue: the board decides what runs and in what order. Cards in Backlog wait, whichever folder their prompt is in.

| ID | Canonical prompt | Tracking issue |
|---|---|---|
| ARCH-01 | [Monorepo migration — plugins, Matrix and two websites](ARCH-01-songsmith-monorepo-migration.md) (parent epic, not one Matrix job) | [#194](https://github.com/tonygward/audio-plugins/issues/194) |
| REC-01 | [Standalone file Recording and Streaming Advisor](REC-01-file-recording-advisor.md) | [#173](https://github.com/tonygward/audio-plugins/issues/173) |
| ADV-02 | [Reference-mix guided review](ADV-02-reference-mix-guided-review.md) | [#172](https://github.com/tonygward/audio-plugins/issues/172) |
| PEQ-01 | [Pultec-style EQ](classics/PEQ-01-pultec-eq.md) | [#170](https://github.com/tonygward/audio-plugins/issues/170) |
| OPT-01 | [Optical Leveller](classics/OPT-01-optical-leveller.md) | [#169](https://github.com/tonygward/audio-plugins/issues/169) |
| COMP-01 | [Songsmith Compressor](../../compressor/prompts/01-songsmith-compressor.md) (supersedes CLN-01, #168, withdrawn) | [#235](https://github.com/tonygward/audio-plugins/issues/235) |
| BUS-01 | [SSL-style bus compressor](classics/BUS-01-ssl.md) | [#167](https://github.com/tonygward/audio-plugins/issues/167) |
| AUTO-04 | [Independent test-writing agent and TDD workflow](AUTO-04-independent-tdd-test-agent.md) | [#165](https://github.com/tonygward/audio-plugins/issues/165) |
| AUTO-02 | [Codex fallback when Claude allowance is exhausted](AUTO-02-codex-fallback.md) | [#163](https://github.com/tonygward/audio-plugins/issues/163) |
| AUTO-01 | [Matrix autonomous development orchestrator](AUTO-01-matrix-autonomous-development.md) (parent epic) | [#162](https://github.com/tonygward/audio-plugins/issues/162) |
| AUTO-09 | [Prove end-to-end Matrix development and CI pilot](AUTO-09-container-development-ci-and-merge-pilot.md) | [#156](https://github.com/tonygward/audio-plugins/issues/156) |
| AUTO-08 | [Validate Matrix Linux container deployment](AUTO-08-linux-container-deployment-validation.md) | [#155](https://github.com/tonygward/audio-plugins/issues/155) |
| EQ-41 | [Label the Gain Scale control](../../eq/prompts/41-gain-scale-main-toolbox.md) | [#203](https://github.com/tonygward/audio-plugins/issues/203) |
| EQ-42 | [Accurate clipping indication and peak readout](../../eq/prompts/42-clip-meter-readout.md) | [#232](https://github.com/tonygward/audio-plugins/issues/232) |
| ACOMP-09 | [Guided compressor calibration](../../analog-compressor/prompts/09-guided-compressor-calibration.md) | [#264](https://github.com/tonygward/audio-plugins/issues/264) |
| ACOMP-10 | [Switchable FET compressor models](../../analog-compressor/prompts/10-switchable-fet-models.md) | [#265](https://github.com/tonygward/audio-plugins/issues/265) |
| WEB-01 | [Private beta portal](WEB-01-private-beta-portal.md) (code in `songsmith-website`; Matrix does not build there yet) | [website #21](https://github.com/tonygward/songsmith-website/issues/21) |
| WEB-02 | [Distribution hardening](https://github.com/tonygward/songsmith-website/blob/main/prompts/WEB-02-release-distribution-hardening.md) (prompt kept in `songsmith-website`) | [website #22](https://github.com/tonygward/songsmith-website/issues/22) |
| WEB-03 | [Plugin Features section](WEB-03-beta-plugin-features-section.md) (code in `songsmith-website`) | [website #19](https://github.com/tonygward/songsmith-website/issues/19) |

`classics/` groups the hardware-inspired plugin designs; it is only a folder and holds nothing back. AUTO-05 (the Linux agent container), AUTO-06, AUTO-07, AUTO-10 and AUTO-11 to AUTO-14 are built; their prompts are in `done/` or were withdrawn.

## Merged implementation archive
These links prevent completed features being selected again. An archived prompt means the work merged; the card reaches Done only after delivery.

### shared
- [ADV-01-musician-guidance](done/ADV-01-musician-guidance.md)
- [AUTO-06-matrix-queue-and-usage-foundation](done/AUTO-06-matrix-queue-and-usage-foundation.md)
- [AUTO-07-linux-ci-worker-code-foundation](done/AUTO-07-linux-ci-worker-code-foundation.md)
- [AUTO-11-board-driven-pickup](done/AUTO-11-board-driven-pickup.md)
- [AUTO-12-bounded-claude-agentic-loop](done/AUTO-12-bounded-claude-agentic-loop.md)
- [AUTO-13-ci-pass-issue-comment](done/AUTO-13-ci-pass-issue-comment.md)
- [AUTO-14-pickup-reason](done/AUTO-14-pickup-reason.md)
- [S7-one-version-number](done/S7-one-version-number.md)
- [SUI-01-shared-fonts-and-type](done/SUI-01-shared-fonts-and-type.md)
- [SUI-02-bevelled-controls](done/SUI-02-bevelled-controls.md)
- [SUI-03-plain-words-everywhere](done/SUI-03-plain-words-everywhere.md)
- [SUI-04-windows-suite-installer](done/SUI-04-windows-suite-installer.md)
- [SUI-05-macos-suite-installer](done/SUI-05-macos-suite-installer.md)
- [SUI-06-one-entry-per-plugin](done/SUI-06-one-entry-per-plugin.md)
- [SUI-07-one-version-number](done/SUI-07-one-version-number.md)
- [SUI-08-songsmith-plugins-brand](done/SUI-08-songsmith-plugins-brand.md)
- [SUI-09-preset-library](done/SUI-09-preset-library.md)
- [SUI-10-resize-and-plain-language](done/SUI-10-resize-and-plain-language.md)
- [SUI-11-latency-and-processing-load](done/SUI-11-latency-and-processing-load.md)
- [SUI-12-processing-load-footer](done/SUI-12-processing-load-footer.md)
- [SUI-13-consistent-bypass](done/SUI-13-consistent-bypass.md)
- [SUI-14-neutral-tooltips-and-tonal-language](done/SUI-14-neutral-tooltips-and-tonal-language.md)
- [SUI-15-footer-version-before-processing-load](done/SUI-15-footer-version-before-processing-load.md)
- [SUI-16-version-label-fully-visible](done/SUI-16-version-label-fully-visible.md)
- [SUI-17-title-tooltip-replaces-guidance](done/SUI-17-title-tooltip-replaces-guidance.md)
- [SUI-18-consistent-plugin-title-bars](done/SUI-18-consistent-plugin-title-bars.md)
- [VER-01-standardise-suite-version-0.5.0](done/VER-01-standardise-suite-version-0.5.0.md)

### eq
- [01-three-band-eq](../../eq/prompts/done/01-three-band-eq.md)
- [02-interactive-eq-curve](../../eq/prompts/done/02-interactive-eq-curve.md)
- [03-24-band-eq](../../eq/prompts/done/03-24-band-eq.md)
- [05-output-gain-bypass](../../eq/prompts/done/05-output-gain-bypass.md)
- [06-cut-slopes](../../eq/prompts/done/06-cut-slopes.md)
- [07-band-solo](../../eq/prompts/done/07-band-solo.md)
- [08-presets](../../eq/prompts/done/08-presets.md)
- [09-ci](../../eq/prompts/done/09-ci.md)
- [10-au-macos](../../eq/prompts/done/10-au-macos.md)
- [11-stereo-modes](../../eq/prompts/done/11-stereo-modes.md)
- [12-sidechain-analyser](../../eq/prompts/done/12-sidechain-analyser.md)
- [13-analyser-controls](../../eq/prompts/done/13-analyser-controls.md)
- [14-auto-gain](../../eq/prompts/done/14-auto-gain.md)
- [15-scale-curve](../../eq/prompts/done/15-scale-curve.md)
- [16-note-labels](../../eq/prompts/done/16-note-labels.md)
- [17-dynamic-eq](../../eq/prompts/done/17-dynamic-eq.md)
- [18-linear-phase](../../eq/prompts/done/18-linear-phase.md)
- [19-oversampling](../../eq/prompts/done/19-oversampling.md)
- [20-starting-point-templates](../../eq/prompts/done/20-starting-point-templates.md)
- [21-rename-songsmith](../../eq/prompts/done/21-rename-songsmith.md)
- [22-windows-installer](../../eq/prompts/done/22-windows-installer.md)
- [23-macos-installer](../../eq/prompts/done/23-macos-installer.md)
- [24-ui-wording-fonts](../../eq/prompts/done/24-ui-wording-fonts.md)
- [25-ui-resolution](../../eq/prompts/done/25-ui-resolution.md)
- [26-starting-points-review](../../eq/prompts/done/26-starting-points-review.md)
- [27-vertical-output-meter](../../eq/prompts/done/27-vertical-output-meter.md)
- [28-piano-strip-toggle](../../eq/prompts/done/28-piano-strip-toggle.md)
- [29-edge-click-cuts](../../eq/prompts/done/29-edge-click-cuts.md)
- [30-abbey-road-reverb-template](../../eq/prompts/done/30-abbey-road-reverb-template.md)
- [31-tom-presets](../../eq/prompts/done/31-tom-presets.md)
- [32-frequency-guide](../../eq/prompts/done/32-frequency-guide.md)
- [33-filter-shape-icons](../../eq/prompts/done/33-filter-shape-icons.md)
- [34-linear-phase-tooltip](../../eq/prompts/done/34-linear-phase-tooltip.md)
- [35-piano-footer](../../eq/prompts/done/35-piano-footer.md)
- [36-dynamic-disabled-tooltip](../../eq/prompts/done/36-dynamic-disabled-tooltip.md)
- [37-draggable-band-panel](../../eq/prompts/done/37-draggable-band-panel.md)
- [38-frequency-guide-instrument-icons](../../eq/prompts/done/38-frequency-guide-instrument-icons.md)
- [39-stereo-meter-clip-latch](../../eq/prompts/done/39-stereo-meter-clip-latch.md)
- [40-organise-starting-points](../../eq/prompts/done/40-organise-starting-points.md)
- [44-accurate-highs](../../eq/prompts/done/44-accurate-highs.md)

### analog-compressor
- [01-plugin-shell](../../analog-compressor/prompts/done/01-plugin-shell.md)
- [02-feedback-compressor](../../analog-compressor/prompts/done/02-feedback-compressor.md)
- [03-analogue-colour](../../analog-compressor/prompts/done/03-analogue-colour.md)
- [04-all-buttons-and-editor](../../analog-compressor/prompts/done/04-all-buttons-and-editor.md)
- [05-presets-validation-packaging](../../analog-compressor/prompts/done/05-presets-validation-packaging.md)
- [06-oversampling-off-by-default](../../analog-compressor/prompts/done/06-oversampling-off-by-default.md)
- [07-detector-low-cut](../../analog-compressor/prompts/done/07-detector-low-cut.md)
- [08-1176-editor](../../analog-compressor/prompts/done/08-1176-editor.md)
- [11-two-to-one-ratio](../../analog-compressor/prompts/done/11-two-to-one-ratio.md)
- [12-ignore-below-rotary-hz](../../analog-compressor/prompts/done/12-ignore-below-rotary-hz.md)
- [13-joe-gilder-grounded-presets](../../analog-compressor/prompts/done/13-joe-gilder-grounded-presets.md)
- [14-ratio-button-tooltips](../../analog-compressor/prompts/done/14-ratio-button-tooltips.md)
- [15-oversampling-far-left](../../analog-compressor/prompts/done/15-oversampling-far-left.md)
- [16-ignore-below-continuous-500hz](../../analog-compressor/prompts/done/16-ignore-below-continuous-500hz.md)

### maximiser
- [01-maximiser-shell](../../maximiser/prompts/done/01-maximiser-shell.md)
- [02-brickwall-limiter](../../maximiser/prompts/done/02-brickwall-limiter.md)
- [03-true-peak-and-auto-release](../../maximiser/prompts/done/03-true-peak-and-auto-release.md)
- [04-editor-and-dither](../../maximiser/prompts/done/04-editor-and-dither.md)
- [05-validation-and-packaging](../../maximiser/prompts/done/05-validation-and-packaging.md)
- [06-true-peak-by-default](../../maximiser/prompts/done/06-true-peak-by-default.md)
- [07-loudness-meter](../../maximiser/prompts/done/07-loudness-meter.md)
- [08-faceplate-and-meter-polish](../../maximiser/prompts/done/08-faceplate-and-meter-polish.md)
- [09-integrated-lufs-beside-dial](../../maximiser/prompts/done/09-integrated-lufs-beside-dial.md)

## Product direction
The current suite comprises EQ, Analog Compressor and Maximiser. The planned Songsmith Compressor (COMP-01) has its own `compressor/prompts/` folder until it has source. Planned classics, guidance tools and Channel Strip remain future work until implemented and released. Keep [the EQ roadmap](../../eq/ROADMAP.md) for product direction, not current queue order or software versions.

## Prompt identity and traceability
Keep IDs stable when archiving. Repair relative links and issue links at the same time; record implementation PRs and verified release/deployment evidence. Preserve historical acceptance requirements but clearly label superseded policy and merged implementation status. Sonnet is the default model; Matrix always runs Sonnet.
