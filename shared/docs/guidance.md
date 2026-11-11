# Plugin title tooltip (SUI-17)

SUI-17 replaced the ADV-01 **Guidance** button and card in Songsmith EQ, Songsmith Analog Compressor and Songsmith Maximiser. Hovering over the plugin name now shows one short explanation. Code: `shared/Source/SongsmithGuidance.h` (`titleTooltipFor`, `TitleHint`). Tests: `shared/Tests/SongsmithGuidanceTests.h`, compiled into all three test executables.

## What the musician sees

| Plugin | Tooltip |
|---|---|
| Songsmith EQ | Shape the tone by boosting or reducing selected frequencies. Make small changes while listening to the whole mix. |
| Songsmith Analog Compressor | Evens out louder and quieter parts and can add analogue-style character. Listen for lost punch if you compress too hard. |
| Songsmith Maximiser | Controls peaks and raises loudness. Compare at the same listening level, so that louder does not automatically seem better. |

The wording is fixed. It answers what the plugin does and gives one listening suggestion. It never depends on a meter, so it cannot present anything as measured, and it names no loudness target, no streaming service and no recording format.

## How the title is made hoverable

The title is painted (`Songsmith::UI::drawTitle`), not a component. Each editor therefore places a transparent `TitleHint` component exactly over the title rectangle from `Songsmith::UI::Header::layout`, the same rectangle the title is painted into. The hint lives inside the scaled content, so it follows the interface size on every scale. Blank space beside the title is not part of it and shows no tooltip. The title looks exactly as before.

- Mouse: the existing tooltip window shows the text after the usual delay.
- Screen readers and keyboard: the hint is a focusable accessible label whose name is the plugin name and whose description is the tooltip text. A sighted keyboard user does not see the tooltip text; it is available to assistive technology only.

## Header layout

The Maximiser **Preset** menu stays in the title bar, immediately after the name, as set out by SUI-18 (name, Preset, Undo, Redo, Bypass at the far right). SUI-17 added no header controls. The hint takes only the name's rectangle, so it never overlaps the Preset menu.

## What was removed, and what was kept

Removed: the Guidance button, the card, the Measured / What It Does / What To Listen For / Try This messages, the "Turn Guidance Off" switch, the guidance-only calculations in the editors (recent reduction, "audio in the last three seconds") and the `Guidance.xml` setting. `Guidance.xml` was never part of sessions or presets, so there is no `stateVersion` change and old sessions load unchanged. An old `Guidance.xml` file left in the machine's settings folder is simply no longer read.

Kept: every live reading in its own place (the Analog Compressor Gain Reduction meter, the Maximiser meters and loudness readings, the EQ Frequency Guide), and every parameter tooltip. Nothing here runs in `processBlock`.

No other code uses the old guidance messages, so none was kept.

## Tests

`SongsmithGuidanceTests` covers: a tooltip for each plugin; short plain full sentences; plain words (shared wording rule); British spelling; no forbidden claims (mud, streaming, targets, LUFS, bit depth, measured or detected); no invented tooltip for an unknown name; the hint carrying the tooltip, accessible name and description, and keyboard focus; the hint covering the title but not the space beside it.

## Not yet verified

- Layout and screenshots: no local build was possible in this run. Before and after screenshots (`--screenshots`) must come from the hosted build, checked at 75 % and 200 % interface size, with the title hint aligned over the name.
- Tooltip display, keyboard focus and screen reader behaviour on Windows and macOS hosts: not checked.
- Studio One, Cakewalk and other DAWs: not checked.
