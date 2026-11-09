---
id: SUI-15
status: done
execution: build-now
priority: 2
model: sonnet
effort: low
dependencies: ["VER-01"]
conflicts: []
---

# SUI-15 — Version label immediately before Processing Load

**Source request:** [SUI-15 GitHub issue](https://github.com/tonygward/songsmithstudio/issues/3). **Implementation merged in [PR #174](https://github.com/tonygward/audio-plugins/pull/174)**. Archived to prevent repeat coding; issue completion requires verified beta delivery.

## Goal
Display the live product version **bottom left**, directly **before the Processing Load button** in the shared footer of every existing Songsmith plugin.

## Work
1. Reuse the authoritative version established by VER-01; never introduce a second hardcoded version.
2. Display the current live version (for example `v0.5.3`) in the footer in this exact order: **version → Processing Load button**. Preserve all existing Processing Load, CPU/latency and bypass functionality.
3. Implement across EQ, Analog Compressor and Maximiser; use existing shared UI conventions and correct rendering with editor size, DPI, accessibility and supported platforms.
4. Add automated layout/version binding checks and capture screenshots where feasible. No DSP, parameter, preset or session-breaking changes.

## Acceptance criteria
- Each plugin shows the correct product version at the bottom left immediately before Processing Load.
- Version follows future automatic beta build-number changes to the authoritative version.
- Footer works without overlap/cutoff at minimum and high-DPI editor sizes.
- Applicable tests and native builds pass; report PR and release status.

## Current delivery policy
Follow [automatic beta builds and issue completion](../../../docs/songsmith-execution-policy.md). Automatic approval/merge requires exact-head green checks. Keep the tracking issue open until matching beta website deployment and both installer downloads are verified; post the final completion comment, then close the issue and finally set board Done. Automatic third-component build increments apply; major/minor increments are deferred.
