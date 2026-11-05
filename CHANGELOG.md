# Songsmith Plugins changelog

Every Songsmith plugin, installer and release shares one version number, kept in the `VERSION` file. Each version has one release on the [Releases page](https://github.com/tonygward/audio-plugins/releases), tagged `vX.Y.Z`. Older releases keep the tags they had (`v0.5.0`, `compressor-v0.1.0`, `maximizer-v0.1.0`, `studio-v1.0.0`), and the plugin changelogs ([Songsmith EQ](eq/CHANGELOG.md), [Songsmith Maximiser](maximiser/CHANGELOG.md)) hold their earlier entries.

## 1.2.0

**Delivery traceability:** this release is the agreed **UI refinements** milestone. Prompts: [EQ-33 filter shape icons](eq/prompts/done/33-filter-shape-icons.md) (#80), [EQ-34 Linear Phase tooltip](eq/prompts/done/34-linear-phase-tooltip.md) (#79), [EQ-35 Piano button placement](eq/prompts/done/35-piano-footer.md) (#81), [EQ-36 Dynamic tooltip](eq/prompts/done/36-dynamic-disabled-tooltip.md) (#78), [SUI-12 Processing Load footer](shared/prompts/done/SUI-12-processing-load-footer.md) (#81) and [SUI-13 consistent Bypass](shared/prompts/done/SUI-13-consistent-bypass.md) (#82). Nothing outside this scope is in the release.

### Every plugin
- **Bypass is in the same place and the same colour everywhere:** the Bypass button is at the top right of Songsmith EQ, Songsmith Analog Compressor and Songsmith Maximiser, at about the same size. When it is switched on it turns red, and red always means the sound is bypassed. In the Analog Compressor it moved from the lower right to the top right. Bypass works exactly as before, including automation.
- **Processing Load is at the far left of the strip along the bottom.** Switch it on and one line appears beside it, for example "CPU 0.35%, Latency 0.0ms (0 samples)". Both figures are measured live. If no audio is playing it says "CPU Unavailable" and still shows the latency. **What changed:** the latency used to be on the strip all the time; now it appears with Processing Load. Switch it off and the line disappears. Your Processing Load choice is still remembered on your computer, not in your sessions.

### Songsmith EQ
- **Piano button at the bottom right,** on the same strip, next to the piano notes it shows along the bottom of the graph. It stays at the right whether or not Processing Load is on.
- **Filter shapes:** each filter type (Bell, Low Shelf, High Shelf, Low Cut, High Cut and Notch) has a small picture of its curve next to its name, in the band panel, its drop-down list and the right-click menu.
- **Clearer Dynamic button:** when Dynamic is greyed out, hover over it to see why. It names the filter type, or Linear phase mode, or both, and says what to change (Dynamic works on Bell, Low Shelf and High Shelf, in Zero latency mode).
- **Linear phase explained:** the processing mode box now says when Linear phase is worth trying, why it adds delay and that Zero latency is the better choice for recording and most mixing.

### Compatibility and known limits
- The sound is unchanged. No parameter, saved setting or preset changed, so your sessions and presets load as before.
- These changes were checked by the automated tests on Windows and macOS. They have **not** been tried in Studio One, Cakewalk or Logic.
- The installers are not signed. Windows may show a SmartScreen warning, and macOS may ask you to allow the installer in System Settings → Privacy & Security.

## 1.1.0
**Delivery traceability:** Shared-suite prompts [SUI-06 — one installer entry per plugin](shared/prompts/done/SUI-06-one-entry-per-plugin.md), [SUI-07 — one version number](shared/prompts/done/SUI-07-one-version-number.md), [SUI-08 — suite branding](shared/prompts/done/SUI-08-songsmith-plugins-brand.md), and [SUI-11 — latency and processing load](shared/prompts/done/SUI-11-latency-and-processing-load.md). SUI-11 is a retrospectively archived specification; delivery verification belongs to the original implementation PR and CI. The shared UI and installer foundations are documented in [SUI-01–SUI-05](shared/prompts/README.md). No new feature or version bump results from this documentation correction.


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
