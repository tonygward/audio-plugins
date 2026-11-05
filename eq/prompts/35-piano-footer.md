# EQ-35 — Move Piano button to far right of EQ footer

**Status:** To do  
**Model:** Sonnet  
**Scope:** Songsmith EQ only.  
**Dependency:** Coordinate with [SUI-12](../../shared/prompts/SUI-12-processing-load-footer.md).

## Rationale — standing Songsmith UI rule
**Controls must be near the thing they affect.** The Piano toggle controls the piano-note strip drawn along the bottom of the EQ graph, so its natural home is at the **bottom-right next to that strip**, not in a distant top toolbar or unrelated control group. This placement is intentional functional proximity, not merely footer symmetry. See the [shared style guide](../../docs/style-guide.md#control-proximity) and `CLAUDE.md`.

## Goal
Place the existing **Piano** toggle at the **far right of the bottom status strip** in Songsmith EQ. Processing Load occupies the far left under SUI-12; the diagnostics text appears directly beside it only when toggled on.

## Required behaviour
1. Move the existing Piano button (introduced by EQ-28) to the right edge of the bottom strip, aligned and padded consistently with the shared component.
2. Keep the existing toggle's function, state persistence, keyboard accessibility, tooltip and semantics: it shows/hides the piano note strip without changing DSP or project compatibility.
3. Footer layout from left to right: `[Processing Load] [CPU …, Latency … (… samples)]` (conditional) → flexible spacer → `[Piano]` (right aligned). When Processing Load is off, its text disappears and Piano remains pinned to the right.
4. Eliminate any duplicate Piano control in the old location. Do not obscure the graph, output meter, scale, resize handle, or overlay controls.
5. At minimum editor width, higher scaling and Windows/macOS HiDPI, neither control nor text should overlap or clip. If necessary shorten only by using responsive layout, not hiding the Piano button.
6. Keep style consistent with the shared bevelled controls and Inter typography.

## Acceptance tests
- Button stays at the far right, visually adjacent to the bottom piano-note strip it controls, across resizes, whether Processing Load is on or off.
- Clicking Piano toggles the existing note overlay; reopen/session recall remains unchanged.
- Screenshot of the footer in both Processing Load states, and before/after screenshots in the PR.
- EQ build and UI tests green; Studio One/Cakewalk verification described accurately.

## Delivery
Branch from latest `main`, PR-based merge, no direct push; implement after or alongside SUI-12, avoiding conflicting bottom-bar edits. Update EQ README, roadmap and local prompt table; move to `done/` only when PR merges. Preserve parameter IDs and suite version discipline.
