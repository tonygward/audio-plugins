# Songsmith Plugins changelog

Every Songsmith plugin, installer and release shares one version number, kept in the `VERSION` file. Each version has one release on the [Releases page](https://github.com/tonygward/audio-plugins/releases), tagged `vX.Y.Z`. Older releases keep the tags they had (`v0.5.0`, `compressor-v0.1.0`, `maximizer-v0.1.0`, `studio-v1.0.0`), and the plugin changelogs ([Songsmith EQ](eq/CHANGELOG.md), [Songsmith Maximiser](maximiser/CHANGELOG.md)) hold their earlier entries.

## 1.1.0

**Delivery traceability:** Shared-suite prompts [S6 — one installer entry per plugin](shared/prompts/done/S6-one-entry-per-plugin.md), [S7 — one version number](shared/prompts/done/S7-one-version-number.md), [S8 — suite branding](shared/prompts/done/S8-songsmith-plugins-brand.md), and [S11 — latency and processing load](shared/prompts/done/S11-latency-and-processing-load.md). S11 is a retrospectively archived specification; delivery verification belongs to the original implementation PR and CI. The shared UI and installer foundations are documented in [S1–S5](shared/prompts/README.md). No new feature or version bump results from this documentation correction.


### Every plugin
- **One version number:** every Songsmith plugin now shares one version number. Songsmith EQ moves up from 0.5.0, and the Songsmith Analog Compressor and Songsmith Maximiser move up from 0.1.0, so each of them upgrades cleanly over what you have installed.
- **Named Songsmith Plugins:** the family is now called Songsmith Plugins in the installers, the welcome pages, the Settings → Apps entry and the release notes. Each plugin keeps its own name, and Songsmith Studio stays the publisher. Upgrading from Songsmith Studio 1.0.0 replaces the old entry, so you still see one, and your projects and presets are untouched.
- **One installer entry per plugin:** the installers offer one tick box per plugin. Each plugin comes with its standalone app, and on a Mac with its Audio Unit too.

- **Latency and Processing Load:** every editor has a strip along the bottom. Latency shows the delay the plugin reports to your recording software, in samples and milliseconds. Processing Load is optional and off until you switch it on; it shows how much of the time available for each block of audio this plugin uses. Neither changes the sound, and the switch is not saved in your sessions or presets.

### Songsmith EQ
- The strip along the bottom is new, and the window is 28 px taller by default so the graph keeps its size. The sound is unchanged. Your sessions and presets load as before.

### Songsmith Analog Compressor
- The strip along the bottom is new. The sound is unchanged. Your sessions and presets load as before.

### Songsmith Maximiser
- The strip along the bottom is new. The sound is unchanged. Your sessions and presets load as before.
