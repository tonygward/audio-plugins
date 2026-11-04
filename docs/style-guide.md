# Songsmith Studio style guide

How every Songsmith Studio plugin looks, so they read as one family. The code for all of this lives in the shared UI library, [`shared/`](../shared/). New plugins link it; they don't copy fonts or look-and-feel code.

## Shared UI library

- `shared/CMakeLists.txt` defines `songsmith_ui`: the embedded fonts (binary data) and the headers below. A plugin adds it from its own `CMakeLists.txt`, so it still builds on its own:

  ```cmake
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)
  target_link_libraries(MyPlugin PRIVATE songsmith_ui)
  ```

- `shared/Source/SongsmithType.h`: the typeface, the type scale and the title, in `Songsmith::UI`.
- `shared/Source/SongsmithColours.h`: the shared neutral colours (`Songsmith::UI::Colours`).
- `shared/Source/SongsmithLookAndFeel.h`: `Songsmith::UI::LookAndFeelBase`, which draws every button, knob, combo box and slider. Each plugin's look-and-feel derives from it, passes its accent colour, and adds only its special drawing (meters, the EQ graph, the compressor's printed knob scale).
- `shared/Tests/SongsmithUITests.h`: checks compiled into every test executable. They fail if the typeface doesn't load, if any label, button or combo box in an editor is smaller than the type scale allows, if any control isn't drawn by the shared look-and-feel, or if buttons stop looking raised when off and pushed in when on.
- `shared/Tests/SongsmithControlSheet.h`: renders every control state for screenshots (`controls.png` next to each plugin's editor screenshots).

## Typeface

[Inter](https://rsms.me/inter/) v4.1, embedded in every plugin in three weights: Regular, Medium and SemiBold. It is built for screens, stays clear at small sizes, and has tabular figures, so numbers that change while you drag don't jitter. Embedding it means Windows and macOS look the same, and no plugin falls back to a system font. The files and the licence (SIL Open Font License 1.1) are in [`shared/Resources/Fonts`](../shared/Resources/Fonts).

Hierarchy comes from weight and size, not capitals.

## Type scale

Sizes are in px at 100 % interface size; the whole editor scales on top. Code uses the role (`Songsmith::UI::Type::control`), never a number.

| Role | Size | Weight | Used for |
|---|---|---|---|
| `caption` | 11 px | Regular or Medium, tabular figures | Graph grid labels, knob scale labels, meter scales. Lighter colour only. Nothing else may be this small. |
| `hint` | 12 px | Regular (SemiBold for a status word) | Hints, the starting point's listening cue, meter mode captions, small pills on the graph, "All Buttons" |
| `control` | 13 px | Medium for labels, Regular for values | Control labels and values, buttons and menus inside panels, tooltips, slider pop-ups |
| `heading` | 14 px | SemiBold for titles, Medium for captions and buttons | Band and knob titles, section captions ("Ratio", "Meter"), top-bar controls, popup menus, the gain reduction readout |
| `title` | 18 px | SemiBold + Regular | The plugin title |

Buttons and combo boxes pick their size from their height: 26 px or taller (top-bar controls) use `heading`, smaller ones `control`.

Why these numbers: 11, 12 and 13 px are the minimums set for Songsmith EQ (roadmap task 24), which the other two plugins already followed. The compressor's knob titles and top-bar controls were 14 px and the EQ's top bar was 14 px, so `heading` is 14 px; the EQ's band title (15 px) and popup menus (15 px) come down to it. The compressor and maximiser titles were 18 px, so the EQ's title (17 px) goes up to match. The compressor's 12.5 px hint becomes 12 px.

## Title

Every plugin draws its name with `Songsmith::UI::drawTitle`: "Songsmith" in SemiBold in the text colour, a space, then the product name ("EQ", "Analog Compressor", "Maximiser") in Regular in the plugin's accent colour, both at 18 px. It sits at the top left, 24 px from the editor's left edge (`titleInset`), centred in a 52 px title bar (`titleBarHeight`). The compressor's title bar is the model.

Songsmith EQ keeps its title in its first top-bar row (42 px tall, 12 px from the left), because that row also holds the presets, Undo, Redo and the interface size and has to fit at the 760 px minimum width.

## Colours

Every plugin uses the same neutral colours, from `Songsmith::UI::Colours`, plus one accent colour. Nothing else should be a new grey.

| Token | Colour | Used for |
|---|---|---|
| `panel` | `#1E2024` | The panel (bottom of its gradient) |
| `panelLight` | `#2B2D32` | The top of the panel gradient, raised controls, popup menus |
| `recess` | `#121417` | Wells: combo boxes, value fields, knob rings, slider tracks; the title bar (at 60 %) |
| `edge` | `#30343B` | Rules and panel outlines |
| `text` | `#E7E6E3` | Values and labels that matter |
| `textDim` | `#9A9B97` | Captions, hints, scales (at least 4.5:1 on the panel) |
| `highlight` | white at 12 % | The lit edge of a raised or recessed surface |
| `shadow` | black at 45 % | The dark edge of a raised or recessed surface |

The graph, meters and band colours keep their own colours; they show data, not controls.

## Controls

The slightly bevelled look started on the compressor's knobs. It is subtle: controls look physical, not chrome. All of it is in `Songsmith::UI::LookAndFeelBase`, drawn in the editor's design units, so it scales with the interface size, and 1 px edges sit on whole pixels so they stay crisp at 2×.

- **Light source:** the top left. Raised surfaces are lighter at the top and catch a 1 px highlight on their top inner edge; recessed surfaces are in shadow at the top and catch the light on their bottom edge.
- **Panel:** a vertical gradient from `panelLight` at the top to `panel` at the bottom, behind every editor (`drawPanel`). The title bar across the top is `recess` at 60 % (`drawTitleBar`), 52 px tall (the EQ's first top-bar row, 42 px). The EQ's graph keeps its flat background, so the curves stay readable.
- **Buttons, off (raised):** the button colour (`panelLight`) as a vertical gradient, 12 % brighter at the top to 14 % darker at the bottom; a `highlight` line on the top inner edge; a `shadow` line (at 55 %) on the bottom inner edge; a 1 px `shadow` outline; corners 4 px.
- **Buttons, on (pushed in):** filled with the accent (or the button's own "on" colour: the bypass colour for Bypass, a band's colour in the EQ's band panel) as a gradient 22 % darker at the top to 4 % brighter at the bottom; the `shadow` line moves to the top inner edge and the `highlight` to the bottom. Text in a dark colour.
- **Pressed (mouse down):** the pushed-in look in the off colour, 10 % darker.
- **Hover:** the colour 8 % brighter. Nothing moves.
- **Disabled:** flat, the colour at 45 %, the text at 50 %.
- **Knobs:** a recessed ring (`recess`, darker at the top left, lighter at the bottom right); a cap 86 % of the radius (74 % when the ring shows a value arc) filled with a radial gradient from `panelLight` 25 % brighter at the top left to `panel` 40 % darker at the bottom right; a thin black outline at 50 %; a pointer in the knob's accent colour from 35 % to 90 % of the cap. Knobs without a printed scale show their value as an arc in the ring, from the centre for ranges that cross zero. Disabled knobs get a grey pointer.
- **Combo boxes, the EQ's preset name and value fields (wells):** `recess`, 35 % darker at the top over the first 10 px, a `shadow` outline, a `highlight` on the bottom edge, corners 4 px (3 px for slider value boxes). Combo boxes have a thin chevron in `textDim`.
- **Bar sliders** (the EQ's Output and Scale): a well that fills with the value at 55 %, from zero for ranges that cross it.
- **Faders** (the maximiser's Threshold and Out Ceiling): a 6 px well as the track, filled in the fader's colour (vertical faders from the top, like the L1's); a raised handle with the button bevel and a line in the fader's colour.

## Accent colours

Each plugin has one accent colour, used for the product name in the title, lit buttons, knob pointers and value arcs.

| Plugin | Accent |
|---|---|
| Songsmith EQ | Songsmith yellow `#F2C94C` (also the total curve) |
| Songsmith Analog Compressor | Amber `#E8A33D` |
| Songsmith Maximiser | Songsmith yellow `#F2C94C` |

The EQ and the maximiser share their accent today; a separate colour for one of them is a decision for a later task.
