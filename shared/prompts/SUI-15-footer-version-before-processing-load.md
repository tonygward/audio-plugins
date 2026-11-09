---
id: SUI-15
status: ready
execution: build-now
priority: 2
model: sonnet
effort: low
dependencies: ["VER-01"]
conflicts: []
---

# SUI-15 — Version label immediately before Processing Load

**Source request:** [SUI-15 GitHub issue](https://github.com/tonygward/songsmithstudio/issues/3). **Execute now**, after VER-01.

## Goal
Display the live product version **bottom left**, directly **before the Processing Load button** in the shared footer of every existing Songsmith plugin.

## Work
1. Reuse the authoritative version established by VER-01; never introduce a second hardcoded version.
2. Display discreet `v0.5.0` in the footer in this exact order: **version → Processing Load button**. Preserve all existing Processing Load, CPU/latency and bypass functionality.
3. Implement across EQ, Analog Compressor and Maximiser; use existing shared UI conventions and correct rendering with editor size, DPI, accessibility and supported platforms.
4. Add automated layout/version binding checks and capture screenshots where feasible. No DSP, parameter, preset or session-breaking changes.

## Acceptance criteria
- Each plugin shows the correct product version at the bottom left immediately before Processing Load.
- Version follows future intentional changes to the authoritative version.
- Footer works without overlap/cutoff at minimum and high-DPI editor sizes.
- Applicable tests and native builds pass; report PR and release status.
