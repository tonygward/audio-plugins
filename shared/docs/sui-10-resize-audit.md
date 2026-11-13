# SUI-10 audit: resizing and control help

| Plugin | Drag to resize | Saved with session | Preset keeps size | Notes |
|---|---|---|---|---|
| Songsmith EQ | Yes (reference) | Width, height, interface size | Yes | Unchanged. |
| Songsmith Maximiser | Yes, fixed proportions | Width | Yes | Unchanged. |
| Songsmith Analog Compressor | **Added**: 100 % to 200 %, fixed proportions | **Width added** (`editorWidth`); older sessions reopen at their saved interface size | Yes (tested) | Interface size menu kept; it shows the nearest step after a drag. (SUI-18 later removed the menu; dragging is now the only way.) |

Analog Compressor help: every button and menu has a tooltip and an accessible name that matches its visible label (tested). The ratio panel button reads **All**; its tooltip says it presses all four ratio buttons at once.

## Not yet verified

- Dragging in a real host, and at display scaling on Windows and macOS (this change was made without a build).
- Before and after screenshots: run `AnalogCompressorTests --screenshots <folder>` (and with `--scale 2`) on a build and attach them to the PR. The existing 100, 150 and 200 % shots show the layout is unchanged at those sizes.
- EQ and Maximiser tooltips were not re-audited in this change.
