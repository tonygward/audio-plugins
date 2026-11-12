# ACOMP-14 — Ratio button tooltips

**GitHub issue:** https://github.com/tonygward/audio-plugins/issues/234
**Execution:** Backlog, board-ordered; do not assume Matrix pickup.
**Objective:** Replace the permanent ratio-help sentence beneath the Analog Compressor controls with hover tooltips on each ratio button, improving clarity without visual clutter.

## Current behaviour
In the compressor editor (e.g. version 0.5.12 screenshot), when 4:1 is selected the bottom edge shows a persistent guidance sentence: “4:1 with a slower attack so each pick comes through. Listen for the strum to sit steady without the attack going soft.” There may already be shorter ratio hover hints; inspect and replace/extend rather than duplicating.

## Required behaviour
Remove the always-visible ratio guidance text and its dedicated layout area; add consistently styled hover tooltips to all six ratio choices. Exact proposed strings:

| Button | Tooltip |
|---|---|
| 2:1 | Gentle compression. Smooths out volume differences while keeping the performance natural. Great for acoustic guitar and vocals. |
| 4:1 | Balanced compression. Controls louder notes while keeping the sound lively and punchy. A good everyday starting point. |
| 8:1 | Strong compression. Keeps volume more consistent and makes louder notes stand out less. Useful for bass, drums and powerful vocals. |
| 12:1 | Very strong compression. Firmly controls loud peaks and creates a more compressed, upfront sound. |
| 20:1 | Limiter-like compression. Heavily controls loud peaks for a dense, aggressive sound. Not a guaranteed brick-wall limiter. |
| All | Aggressive 1176-style compression. Produces a distinctive, energetic sound with extra character. Great for parallel drums and special effects. |

These are listening-oriented descriptions of typical applications, not promises that ratio alone determines gain reduction.

## Implementation
- Locate the persistent ratio/help caption and eliminate it, removing any now-unused layout reservation.
- Apply JUCE's standard tooltip API and the existing Songsmith tooltip look-and-feel to **each individual ratio button**.
- Tooltip should work whether or not its button is currently selected, and should disappear on mouse exit.
- Preserve the selected-button highlight, labels, DSP processing, automation IDs and ordering, saved states, presets, undo/redo and bypass.
- Work with ACOMP-11's 2:1 button addition; avoid reordering ratio parameters or changing All Buttons semantics.
- Keep concise visual layout at supported UI scales; no stale text from one ratio on another.
- Update UI tests and documentation as appropriate.

## Tests / acceptance
- Each of six ratios exposes the correct unique tooltip; blank area when no tooltip.
- There is no always-visible description at the bottom of the compressor.
- Hover over nonselected and selected ratios works with no clipped text.
- Existing ratio, preset, automation, saved-session and DSP regression tests still pass.
- Run release build and CI according to the repository's normal policy. Increment build/version under the active policy and update changelog.
- Evidence comment on issue: commits/PR, checks, merge, published R2 installers, verified beta website. Only then close and move Done.

## Dependencies / risks
- ACOMP-11 ratio addition may touch the same controls: avoid merge conflicts and preserve parameter compatibility.
- Small UI-only change; don't broaden into compressor algorithm or preset overhaul.
