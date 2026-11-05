# S11 · Latency and Processing Load in every plugin

**Prompt ID:** S11  
**Scope:** Shared suite  
**Status:** Done — shipped in suite version 1.1.0  
**Canonical archive:** `shared/prompts/done/S11-latency-and-processing-load.md`  
**Implementation reference:** [Style guide — Latency and Processing Load strip](../../../docs/style-guide.md#latency-and-processing-load-strip)  
**Release reference:** [Changelog — 1.1.0](../../../CHANGELOG.md#110)

> Retrospective archival of the S11 acceptance requirements previously embedded in `shared/prompts/README.md`. This file is a record of completed work, **not** a new implementation instruction. Do not claim that its creation constitutes fresh testing.

**Model:** Sonnet; Opus review only if safe timing or publication from the audio thread proves difficult.

Follow `CLAUDE.md` and the suite editor requirements. Add a consistent diagnostics view to every current and future Songsmith plugin.

- **Latency:** Show the processing delay this instance reports to the host, in samples and milliseconds at the current sample rate. Refresh it when the sample rate or processing mode changes. Use a plain tooltip such as “The delay this plugin adds. Your recording software may compensate for it.”
- **Processing Load:** Offer optional per-instance timing, off by default. Estimate the percentage of the audio block's time budget used by this instance. Use a smoothed readout with enough precision for light loads, and show unavailable when there is no recent measurement. Explain in a tooltip that the value describes this plugin and varies with the computer, settings and buffer size.
- Keep timing overhead low and benchmark it. Avoid allocations, locks, logging and UI updates in the audio callback. Publish measurements safely and refresh the display on the UI thread. Keep diagnostic settings separate from sound presets.
- Check displayed delay against reported samples and sample rate in every latency-changing mode. Verify the load calculation at different block sizes, behaviour when processing stops, and UI at supported window sizes. Test VST3, Audio Unit and standalone where supported.

**Verification record:** The documentation and changelog report completion. For actual build and host-test evidence, consult the implementation PR/commit and CI; this archive is not itself proof of those tests.
