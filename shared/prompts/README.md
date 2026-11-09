# Songsmith suite prompts

## Current rules — 9 November 2026
[Execution and delivery policy](../../docs/songsmith-execution-policy.md) is authoritative.

- Board Todo order decides overnight pickup; Doing requests immediate work behind the active job. Never reject due to issue repository, prompt location or ID prefix. Validate the actual view order; no priority/ID fallback.
- One task per invocation and one active job. Stop new work at >=60% verified weekly Claude usage, including scoped meters; unknown usage blocks pickup.
- Automatically approve/merge only after current-head required CI/tests pass. Trusted workflows automatically increment the beta third-component build/patch number; major/minor increments are deferred.
- Comment on actual pickup, progress, failure, merge and deployment. Verify matching beta website deployment and both installer downloads, post the final completion comment, then close the issue and finally set its board card Done.
- Archived prompts mean merged implementation, not verified delivery. No duplicate implementation while a release is pending.
- AUTO-11 is the implementation specification for board pickup; it is not yet evidence of a working board reader. Infrastructure/website tasks require their scoped execution path and credentials, not a location-based rejection.

## Canonical issue and prompt links
Every implementation item needs a unique stable ID, issue and canonical prompt linked both ways. Follow [the issue/prompt contract](../../docs/backlog-issue-prompt-contract.md). RUN NOW means requested, not proof of pickup.

| ID | Canonical prompt | Tracking issue | Evidence / status |
|---|---|---|---|
| VER-01 | [Version baseline](done/VER-01-standardise-suite-version-0.5.0.md) | [songsmithstudio #2](https://github.com/tonygward/songsmithstudio/issues/2) | Implementation #152 merged; later beta build numbers advance automatically. Verify delivery before issue Done. |
| SUI-15 | [Footer version label](done/SUI-15-footer-version-before-processing-load.md) | [songsmithstudio #3](https://github.com/tonygward/songsmithstudio/issues/3) | #174 merged; v0.5.2 release and website source manifest exist. Authenticated live delivery still needs recorded verification. |
| SUI-16 | [Fix truncated version text](done/SUI-16-version-label-fully-visible.md) | [songsmithstudio #4](https://github.com/tonygward/songsmithstudio/issues/4) | Restored as a distinct truncation bug, not a duplicate of SUI-15. #177 merged; beta publication/download verification pending at reconciliation. |
| WEB-01 | [Private beta portal](WEB-01-private-beta-portal.md) | [website #21](https://github.com/tonygward/songsmith-website/issues/21) | Reconcile implemented portal against remaining acceptance criteria; do not rebuild it blindly. |
| WEB-02 | [Distribution hardening](https://github.com/tonygward/songsmith-website/blob/main/prompts/WEB-02-release-distribution-hardening.md) | [website #22](https://github.com/tonygward/songsmith-website/issues/22) | Website-owned implementation and end-to-end acceptance. |
| WEB-03 | [Plugin Features section](WEB-03-beta-plugin-features-section.md) | [website #19](https://github.com/tonygward/songsmith-website/issues/19) | Requested website work; verify actual pickup and deployment separately. |

## Open implementation / validation prompts
Order below is an index, not the execution queue. The board decides order.

| ID | Canonical prompt | Tracking issue |
|---|---|---|
| REC-01 | [Standalone file Recording and Streaming Advisor](future/REC-01-file-recording-advisor.md) | [#173](https://github.com/tonygward/audio-plugins/issues/173) |
| ADV-02 | [Reference-mix guided review](future/ADV-02-reference-mix-guided-review.md) | [#172](https://github.com/tonygward/audio-plugins/issues/172) |
| ADV-01 | [Musician-friendly guidance across Songsmith plugins](future/ADV-01-musician-guidance.md) | [#171](https://github.com/tonygward/audio-plugins/issues/171) |
| PEQ-01 | [Pultec-style EQ](classics/PEQ-01-pultec-eq.md) | [#170](https://github.com/tonygward/audio-plugins/issues/170) |
| OPT-01 | [Optical Leveller](classics/OPT-01-optical-leveller.md) | [#169](https://github.com/tonygward/audio-plugins/issues/169) |
| CLN-01 | [Clean Compressor](classics/CLN-01-clean-compressor.md) | [#168](https://github.com/tonygward/audio-plugins/issues/168) |
| BUS-01 | [SSL-style bus compressor](classics/BUS-01-ssl.md) | [#167](https://github.com/tonygward/audio-plugins/issues/167) |
| AUTO-05 | [Isolated Linux development agent with GitHub Actions native builds](AUTO-05-linux-agent-github-actions.md) | [#166](https://github.com/tonygward/audio-plugins/issues/166) |
| AUTO-04 | [Independent test-writing agent and TDD workflow](AUTO-04-independent-tdd-test-agent.md) | [#165](https://github.com/tonygward/audio-plugins/issues/165) |
| AUTO-03 | [Matrix remote heartbeat and backlog observability](AUTO-03-matrix-remote-heartbeat.md) | [#164](https://github.com/tonygward/audio-plugins/issues/164) |
| AUTO-02 | [Codex fallback when Claude allowance is exhausted](AUTO-02-codex-fallback.md) | [#163](https://github.com/tonygward/audio-plugins/issues/163) |
| AUTO-01 | [Matrix autonomous development orchestrator](AUTO-01-matrix-autonomous-development.md) | [#162](https://github.com/tonygward/audio-plugins/issues/162) |
| MAX-07 | [Loudness meter (LUFS)](../../maximiser/prompts/07-loudness-meter.md) | [#161](https://github.com/tonygward/audio-plugins/issues/161) |
| ACOMP-07 | [Low Cut for the compressor detector](../../analog-compressor/prompts/07-detector-low-cut.md) | [#160](https://github.com/tonygward/audio-plugins/issues/160) |
| AUTO-10 | [Verify beta release distribution end to end](AUTO-10-verified-beta-distribution-e2e.md) | [#157](https://github.com/tonygward/audio-plugins/issues/157) |
| AUTO-09 | [Prove end-to-end Matrix development and CI pilot](AUTO-09-container-development-ci-and-merge-pilot.md) | [#156](https://github.com/tonygward/audio-plugins/issues/156) |
| AUTO-08 | [Validate Matrix Linux container deployment](AUTO-08-linux-container-deployment-validation.md) | [#155](https://github.com/tonygward/audio-plugins/issues/155) |
| AUTO-11 | [Board-ordered pickup and delivery lifecycle](AUTO-11-board-driven-pickup.md) | [#175](https://github.com/tonygward/audio-plugins/issues/175) |

AUTO-01 and AUTO-05 are parent tracking epics. AUTO-06/07 are archived code foundations. AUTO-08/09/10 retain runtime, development/CI and distribution evidence respectively. Real Matrix PRs #174/#177 show execution has occurred; they do not prove every isolation/recovery/distribution acceptance criterion.

AUTO-02 provider fallback and future/classic plugin designs remain deferred until intentionally promoted. A board card cannot bypass unmet dependencies or execution permissions.

## Merged implementation archive
These links prevent completed features being selected again. Verify each ticket's delivery separately; the archive is not board Done.

### shared
- [AUTO-06-matrix-queue-and-usage-foundation](done/AUTO-06-matrix-queue-and-usage-foundation.md)
- [AUTO-07-linux-ci-worker-code-foundation](done/AUTO-07-linux-ci-worker-code-foundation.md)
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

### analog-compressor
- [01-plugin-shell](../../analog-compressor/prompts/done/01-plugin-shell.md)
- [02-feedback-compressor](../../analog-compressor/prompts/done/02-feedback-compressor.md)
- [03-analogue-colour](../../analog-compressor/prompts/done/03-analogue-colour.md)
- [04-all-buttons-and-editor](../../analog-compressor/prompts/done/04-all-buttons-and-editor.md)
- [05-presets-validation-packaging](../../analog-compressor/prompts/done/05-presets-validation-packaging.md)
- [06-oversampling-off-by-default](../../analog-compressor/prompts/done/06-oversampling-off-by-default.md)
- [08-1176-editor](../../analog-compressor/prompts/done/08-1176-editor.md)

### maximiser
- [01-maximiser-shell](../../maximiser/prompts/done/01-maximiser-shell.md)
- [02-brickwall-limiter](../../maximiser/prompts/done/02-brickwall-limiter.md)
- [03-true-peak-and-auto-release](../../maximiser/prompts/done/03-true-peak-and-auto-release.md)
- [04-editor-and-dither](../../maximiser/prompts/done/04-editor-and-dither.md)
- [05-validation-and-packaging](../../maximiser/prompts/done/05-validation-and-packaging.md)
- [06-true-peak-by-default](../../maximiser/prompts/done/06-true-peak-by-default.md)
- [overnight-prompt](../../maximiser/prompts/done/overnight-prompt.md)

## Product direction
The current suite comprises EQ, Analog Compressor and Maximiser. Planned classics, guidance tools and Channel Strip remain future work until implemented and released. Keep [the EQ roadmap](../../eq/ROADMAP.md) for product direction, not current queue order or software versions.

## Prompt identity and traceability
Keep IDs stable when archiving. Repair relative links and issue links at the same time; record implementation PRs and verified release/deployment evidence. Preserve historical acceptance requirements but clearly label superseded policy and merged implementation status. Use Sonnet by default and explicitly scope any Opus design escalation.
