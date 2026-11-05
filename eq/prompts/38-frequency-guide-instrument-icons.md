# EQ-38 — Instrument icons in Frequency Guide

**Priority:** 2 — early EQ UI polish, next suitable UI batch after v1.3.0.
**Status:** To do. **Recommended model:** Sonnet. **Estimated effort / usage:** Low. **Escalation:** Opus not expected; document any unavoidable architecture blocker before switching. **Branch:** `roadmap/38-frequency-guide-instrument-icons`.
**Depends on:** Shipped [EQ-32](done/32-frequency-guide.md), Instrument Frequency Guide.

## Goal
In Songsmith EQ's Frequency Guide, put a small, recognisable **instrument icon beside the instrument name** — for example, a guitar silhouette beside **Acoustic Guitar**. Make the ten-item instrument selector easier and faster to scan, without sacrificing the existing plain-language names.

## Scope
- Show a clear, consistent small icon immediately to the **left** of each instrument label in the selector list. Show the selected instrument's icon next to its label when the selector is closed, where the existing JUCE control permits.
- Cover all ten shipped guide entries: Acoustic Guitar, Nylon Guitar, Electric Guitar, Bass Guitar, Vocal, Kick Drum, Snare Drum, Toms, Drum Bus, Any Instrument.
- Use recognisable vector or bundled assets and consistent stroke, dimensions, padding and alignment at 75–200% UI scaling and across Windows/macOS. Prefer an appropriate guitar variant for nylon vs steel/electric; avoid arbitrary emojis, font-dependent glyphs or platform-specific rendering.
- Keep the icons **beside the words**, not as replacements. For abstract choices such as Drum Bus and Any Instrument, use clear grouping/general-purpose pictograms.
- Follow the existing colour palette, contrast and hover/selected states. Keep proximity to the Frequency Guide toggle, avoid widening the EQ window or truncating names. Retain keyboard navigation/accessibility and existing menu behaviour.
- The icon must not cover the graph, analyser, EQ curves or band controls.

## Non-goals / compatibility
Pure presentation. Do not change frequency ranges, guide copy, DSP, parameters, presets, state property names, saved session formats, audio thread or instrument ordering. No release/version bump in this prompt; the next milestone decides it.

## Validation / acceptance
1. All ten instruments show an identifiable icon and their existing full labels, including the selected item in the collapsed selector where supported.
2. No clipping, overlap, missing icons or layout regression at minimum/default window widths and 75%, 100%, 200% scaling; verify Windows/macOS.
3. Existing instrument selection and session recall behave identically; tests for icon mapping and selection where practical.
4. Release build and EQ tests pass with no new warnings; no new processing or CPU cost during audio rendering.
5. Keep useful before/after screenshots in the PR and/or README, documenting any custom-selector trade-off.

## Implementation procedure
Read `CLAUDE.md`, `docs/style-guide.md`, `eq/Source/FrequencyGuide.h`, guide controls in `eq/Source/PluginEditor.cpp`, `eq/docs/frequency-guide.md`, and `eq/prompts/README.md`. Start from current main, implement on a feature branch, test, open a draft PR and follow the repository's existing PR/CI merge rules. Do not install plugins system-wide.

**Traceability:** EQ-38 follows shipped EQ-32. Mark done and move to `done/` only after the implementation is merged; link the PR and release checkpoint in the roadmap/changelog at delivery.
