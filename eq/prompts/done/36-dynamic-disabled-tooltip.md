# EQ-36 — Context-sensitive Dynamic EQ disabled tooltips

**Status:** Done in #78 · **Priority 1 (early UI usability fix)**  
**Model:** Sonnet  
**Scope:** Songsmith EQ Band Panel — Dynamic button only  
**Evidence:** 5 November 2026 screenshot: Band 4, Low Cut, greyed-out Dynamic button.

## Purpose
When **Dynamic** is disabled (greyed out), hovering or focusing it should explain **why it is disabled** and **which filter types support dynamic EQ**, rather than reusing the normal explanation. This is a small UX/documentation correction, not a DSP feature.

## Verified current behaviour
- `EQ::typeHasGain()` in `eq/Source/EQParams.h` is true only for **Bell, Low Shelf and High Shelf**.
- `BandPanel.cpp` disables the Dynamic toggle when `hasGain` is false or linear-phase processing is selected.
- The existing tooltip currently only mentions linear-phase mode, otherwise it describes the enabled feature even when the button is disabled for Low Cut, High Cut or Notch.

## Required tooltip copy and precedence
Choose tooltip based on the current selected filter and processing mode; update live as either changes.

1. **Unsupported filter type, minimum requirement:**  
   `Dynamic EQ isn't available on Low Cut filters. Use Bell, Low Shelf or High Shelf to enable dynamic processing.`  
   Substitute **High Cut** or **Notch** as appropriate. This must work for all three unsupported types.
2. **Supported filter type, but Linear Phase enabled:**  
   `Dynamic EQ isn't available in Linear Phase mode. Switch to the normal EQ mode to use it with Bell, Low Shelf or High Shelf filters.`  
   Verify the actual mode label in UI and use that exact plain-language label.
3. **Both unsupported filter type and Linear Phase enabled:** explain **both restrictions** rather than suggesting that switching filter type alone will enable Dynamic:  
   `Dynamic EQ isn't available on Low Cut filters or in Linear Phase mode. Select Bell, Low Shelf or High Shelf and turn off Linear Phase.`
4. **Enabled:** preserve the existing useful short description, e.g. `Dynamic EQ changes this band's gain in response to the sound level in its frequency range.`

## Accessibility and mechanics
- Crucially, the tooltip **must show while the button is greyed out**. JUCE may not display tooltips for a disabled component; investigate the actual interaction and implement a non-interactive hover/focus overlay or equivalent if necessary. Do not re-enable the action just to make the tooltip work.
- Keep greyed-out visual and click behaviour unchanged; no invisible hit target should trigger an action.
- Support mouse hover and keyboard-accessible explanation as appropriate to the existing plugin's focus/navigation model.
- Make no changes to filter eligibility, DSP, parameter IDs, latency, state migration or existing session compatibility.
- Use British English and shared Songsmith Inter text and style rules.

## Acceptance criteria
- Bell, Low Shelf and High Shelf: enabled in normal mode; ordinary explanatory tooltip.
- Low Cut, High Cut and Notch: disabled, **hoverable explanation names that filter and lists Bell, Low Shelf, High Shelf**.
- Linear Phase selected: disabled tooltip explains the mode restriction, also both restrictions when applicable.
- Tooltip updates immediately when filter type or processing mode changes and never gives a false route to enabling Dynamic.
- Verify at minimum/default/max editor sizes and common Windows/macOS DPI scales. Add tests for tooltip selection and disabled hover behaviour.
- Capture **real** before/after screenshots (including tooltip over disabled Dynamic) in PR and refresh relevant README screenshots.

## Delivery
Follow `CLAUDE.md`. Run early alongside UI refinements, before the larger EQ guide or compressor DSP tasks; avoid needless overlap with EQ-33 filter icons. Branch from current `main`; PR-only merge; test EQ and don't install system-wide. Preserve ID **EQ-36** through delivery, move file to `done/` only once merged, and log PR/release checkpoint without guessing. No independent suite version bump.
