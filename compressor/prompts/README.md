# Songsmith Compressor prompts

Prompts for the planned **Songsmith Compressor**: a separate, transparent compressor with an optional Analog Warmth button. It is not the Analog Compressor and must not change it. The plugin has no source yet. This folder holds its prompts until it does, and its code will live beside them in `compressor/`.

Every task has a stable **COMP-##** reference, distinct from its GitHub PR number. See [traceability rules](../../shared/prompts/README.md#prompt-identity-and-traceability). The board decides order; see the [execution policy](../../docs/songsmith-execution-policy.md). Matrix reads this folder like the other plugin folders, and archives merged prompts into `done/`.

| Prompt | Status | Issue |
|---|---|---|
| [01 Songsmith Compressor](01-songsmith-compressor.md) | To do. Supersedes CLN-01 (Clean Compressor, #168, withdrawn) | [#235](https://github.com/tonygward/audio-plugins/issues/235) |

**Before Matrix can build COMP-01:** a new plugin needs a CMake project, CI build and test jobs in `.github/workflows/build.yml`, a place in both suite installers and the version checks. Workflows and installer scripts sit outside what Matrix may edit, so a person sets those up, or reviews them, first.

## How COMP-01 is split

COMP-01 is too large for one run, so it goes in ordered slices. Each one is its own pull request that refers to #235. #235 stays the single tracking issue; the slices are not separate Matrix tasks.

| Slice | What | State |
|---|---|---|
| 1 | Engine: gain computer, soft knee, attack and release, stereo link, make-up, mix, tests ([engine notes](../docs/engine.md)) | Merged |
| 2 | Plugin: JUCE plugin target, identity, parameters, state and tests, editor, four presets, and a CI build and test job | In review |
| 2b | Release wiring: suite installer entries, release checks and the beta website | To do |
| 3 | (folded into slice 2) | Done |
| 4 | Analog Warmth: optional saturation, measured ([engine notes](../docs/engine.md#analog-warmth)) | In review |
| 5 | Host check, listening notes and further presets | To do |
| COMP-02 | Level graph, floating panel and the Songsmith EQ meter ([notes](../docs/display.md)) | In review |
