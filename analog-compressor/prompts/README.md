# Songsmith Analog Compressor prompts

There is one self-contained prompt per build step. They were run in order by the overnight run of 2026-11-03: each step branched from the integration branch `compressor/overnight-2026-11-03`, got a draft PR, and was merged once CI was green. Finished prompts are in [`done/`](done/). The code is in [`analog-compressor/`](../). (The prompts still say `fet/` and `FETTests`: the folder and test executable were renamed afterwards.)

Every task has a stable **ACOMP-##** reference (for example ACOMP-05), distinct from its GitHub PR number. See [traceability rules](../../shared/prompts/README.md#prompt-identity-and-traceability).

| Prompt | Status | PR |
|---|---|---|
| [01 plugin shell](done/01-plugin-shell.md) | Done | #36 |
| [02 feedback compressor](done/02-feedback-compressor.md) | Done | #41 |
| [03 analogue colour](done/03-analogue-colour.md) | Done | #43 |
| [04 All Buttons and editor](done/04-all-buttons-and-editor.md) | Done | #47 |
| [05 presets, validation and packaging](done/05-presets-validation-packaging.md) | Done | #50 |
| [08 1176-inspired editor](done/08-1176-editor.md) | Done · Sonnet | #88 (shipped in 1.3.0) |
| [06 oversampling off by default](done/06-oversampling-off-by-default.md) | Done · Sonnet | |
| [07 detector Low Cut](done/07-detector-low-cut.md) | Done · Sonnet | [#160](https://github.com/tonygward/audio-plugins/issues/160) (closed) |
| [09 Guided compressor calibration](09-guided-compressor-calibration.md) | To do | [#264](https://github.com/tonygward/audio-plugins/issues/264) |
| [10 Switchable FET compressor models](10-switchable-fet-models.md) | To do | [#265](https://github.com/tonygward/audio-plugins/issues/265) |
| [11 Add 2:1 compression ratio](done/11-two-to-one-ratio.md) | Done · Sonnet | [#199](https://github.com/tonygward/audio-plugins/issues/199), PR #207 (shipped in 0.5.8) |
| [12 Ignore Below rotary Hz control](done/12-ignore-below-rotary-hz.md) | Done · Sonnet | [#200](https://github.com/tonygward/audio-plugins/issues/200), PR #215 (shipped in 0.5.9) |
| [13 Preset research for the Songsmith Compressor](done/13-joe-gilder-grounded-presets.md) | Done | [#233](https://github.com/tonygward/audio-plugins/issues/233), PR #245 (in 0.5.15) |
| [14 Ratio descriptions in per-button tooltips](done/14-ratio-button-tooltips.md) | Done | [#234](https://github.com/tonygward/audio-plugins/issues/234), PR #247 (in 0.5.15) |
| [15 Oversampling at the far left](done/15-oversampling-far-left.md) | Done | [#236](https://github.com/tonygward/audio-plugins/issues/236), PR #248 (in 0.5.15) |
| [16 Continuous Ignore Below up to 500 Hz](done/16-ignore-below-continuous-500hz.md) | Done | [#237](https://github.com/tonygward/audio-plugins/issues/237), PR #249 (in 0.5.18) |
| [18 Release in the control-voltage domain; engine runs while bypassed](done/18-control-voltage-release-and-bypass.md) | Done | [#280](https://github.com/tonygward/audio-plugins/issues/280) |

The board decides the order of the remaining prompts; see the [execution policy](../../docs/songsmith-execution-policy.md). (ACOMP-08, 06 and 07 ran in that order on 5 November 2026, keeping their original IDs and filenames.)

Still to do by hand before a release: the Studio One and Logic checks in [`analog-compressor/docs/daw-checks.md`](../docs/daw-checks.md), and the listening checklists in `analog-compressor/docs`.
