# Songsmith Plugin Analyzer prompts

Prompts for the **Songsmith Plugin Analyzer**: a VST3 pair (a Reference before a plugin, a Result after it) that measures the frequency response another plugin produces. It is not the standalone Songsmith Audio Analyzer (`audio-analyzer/`), which checks finished files.

Every task has a stable **ANALYZER-##** reference, distinct from its GitHub PR number. See [traceability rules](../../shared/prompts/README.md#prompt-identity-and-traceability). The board decides order; see the [execution policy](../../docs/songsmith-execution-policy.md). Matrix archives merged prompts into `done/`.

| Prompt | Status | Issue |
|---|---|---|
| [01 Plugin Analyzer](done/01-plugin-analyzer.md) | Merged in 0.5.55 | [#331](https://github.com/tonygward/audio-plugins/issues/331) |
