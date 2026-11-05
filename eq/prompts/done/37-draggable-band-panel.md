# EQ-37 — Movable band panel with automatic visibility on Dynamic expansion

**Status:** Done in #87 · **Priority 1 (early EQ usability)**  
**Model:** Sonnet  
**Scope:** Songsmith EQ floating band control panel (`eq/Source/BandPanel.*`, `eq/Source/EQDisplay.*`).

## User requests (5 November 2026)
1. **Click and drag the floating band-controls panel** to reposition it elsewhere within the EQ graph.
2. **Clicking Dynamic may expand the panel:** automatically reposition it as needed so **all newly revealed controls are visible**, not cut off at the graph/window boundary.

## Existing source behaviour
- `EQDisplay::positionPanel()` auto-anchors the panel near the selected graph node and clamps it using `panel.getPreferredHeight()`. It currently avoids moving the panel while hovered or dragged; it has **no explicit draggable-panel mechanism**.
- `BandPanel::getPreferredHeight()` increases when its dynamic controls row appears (`panelHeight + dynamicsRowHeight`). The band panel uses a fixed `panelWidth = 640` design units.
- Repositioning must account for the expanded height even when a pointer is hovering the panel: the existing early-return-on-hover must **not** prevent visibility correction.

## Requirements
1. **Drag handle:** allow mouse dragging from a clearly identifiable panel header/title or empty background area; do not capture drags from knobs, sliders, menus, filter selectors, toggle buttons, or the graph underneath. Give the handle a move cursor and tooltip such as `Drag to move these band controls.`
2. **Initial placement:** retain useful automatic placement near the selected EQ band when first opened. When moved manually, keep the user-selected location during ongoing interaction and band switching. Choose a simple documented reset (e.g. dismiss/reopen or editor reopen); do not introduce new DAW/session parameters.
3. **Always keep all controls accessible:** constrain the entire panel within the usable graph bounds at every stage, including while dragging, window resize, scaling, and changing the selected band. Account for whether Dynamic row is shown.
4. **Dynamic expansion:** on activating **Dynamic**, recalculate the expanded panel dimensions and **automatically move it upward/inward if necessary**, so the entire extra row (Threshold, Range, Attack, Release, gain display and related controls) fits and is clickable. Prefer the smallest adjustment that preserves the user's placement rather than snapping the panel back to its original auto-anchored location. This applies even if the mouse is still over the panel or immediately after the click.
5. **Dynamic collapse:** when disabling Dynamic, avoid unnecessary position jumps; keep the compact panel within bounds. When reopening Dynamic, keep the larger panel in view.
6. **Insufficient space:** where the graph/editor is too small to contain the expanded panel in its current design size, provide a sensible responsive fit/repositioning strategy (or an inner scrolling layout only as a last resort) so no controls are hidden or stranded. Do not let the panel escape off-screen.
7. Preserve panel-versus-node drag separation: moving the panel must never alter frequency/gain/Q, toggle settings, or start a node drag. Normal knob, combo-box, context-menu, button and keyboard operations continue working.
8. Follow suite-wide proximity rule: the floating panel remains associated with the EQ graph and selected band. Preserve DSP, parameters, session/automation compatibility and real-time safety.

## Tests / evidence
- Open a band near each graph edge/corner; show the compact panel without clipping.
- Drag it to the top, bottom and either side, then release: it remains visible at the dropped position and does not jump on refresh.
- **Critical regression:** drag compact panel close to bottom edge, click Dynamic; the expanded panel moves just enough to expose **every** additional control; click off/on repeatedly without clipping or wild jumps.
- Repeat when auto-positioned, manually positioned, hovered, changing band, resizing, and at 1×/2× DPI on Windows/macOS.
- Drag knobs and EQ nodes normally; check no accidental processing changes from panel drag.
- Add layout/interaction regression tests and **real before-and-after screenshots**, including Dynamic expanded at bottom edge, to the PR/README.

## Delivery
Follow `CLAUDE.md`. **Sonnet**, early Priority 1 UI-only work. Branch from latest `main`, PR-based merge, no system-wide installation. Preserve the stable ID **EQ-37**, update the local and shared backlog/PR traceability, move to `done/` only after merge; no independent suite version bump.
