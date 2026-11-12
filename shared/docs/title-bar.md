# Suite title bar (SUI-18)

Every Songsmith plugin has the same title bar, laid out by `shared/Source/SongsmithHeader.h`:

**plugin name | Preset | Undo | Redo ............ Bypass (far right)**

Sizes (design units): preset 220, Undo and Redo 60 each, Bypass 84, 26 high. Keyboard order: Preset, Undo, Redo, Bypass. Tests: `checkSuiteHeader` (layout, order, nothing else in the bar) and `checkParameterHistory` in `shared/Tests/SongsmithHeaderTests.h`.

## Where the moved controls went

| Plugin | Control | New place | Why there |
|---|---|---|---|
| Songsmith EQ | Scale (Gain Scale, `gain_scale`) | Second row, left of Auto Gain and Output | A real sound parameter, so it stays visible beside the other gain controls. Its ID, saved value and automation are untouched. EQ-41 may move it to its final toolbox spot. |
| Songsmith EQ | Band count | Second row, left of Scale, when there is room | Hidden at the narrowest widths rather than squeezed. |
| Songsmith Analog Compressor | Interface size menu | Removed | Drag the bottom-right corner; size is still saved, and the old saved interface size still sets the starting width. |
| Songsmith Analog Compressor | Oversampling | Far left of the processing area, under Input (ACOMP-15) | A processing setting, at a predictable place on the left edge. It was briefly below Ignore Below. |
| Songsmith Maximiser | Starting point menu | Title bar, now called Preset | Same place and word as the other plugins. |
| Songsmith Maximiser | Peak Mode and Dither | Under the Release knob | Output settings; both keep their parameters and saved values. |

## Undo and Redo semantics

- EQ keeps its own edit controller (unchanged).
- Analog Compressor and Maximiser use `Header::ParameterHistory`, held by the processor so it survives closing the window. One step is one gesture in the window (a knob drag, a button click, a menu choice) or one preset load, stored as the parameter values before and after.
- Host automation sends no gestures, so it is never recorded and Undo cannot revert it. The tooltip says: "Only changes you made in this window." A host's own Undo is separate.
- Loading a session from the host clears the history. Message thread only; nothing runs in `processBlock`.

## Not yet verified

Built and tested only by hosted CI; not yet checked in a DAW. Screenshots at minimum, typical and high DPI come from `--screenshots` in each test executable and are attached to the PR. Coordinate with SUI-17: the title is painted into `titleBounds` only, so a tooltip can attach to that rectangle and not the whole bar.
