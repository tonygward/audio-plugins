# Songsmith Studio style guide

How every Songsmith Studio plugin looks, so they read as one family. The code for all of this lives in the shared UI library, [`shared/`](../shared/). New plugins link it; they don't copy fonts or look-and-feel code.

## Shared UI library

- `shared/CMakeLists.txt` defines `songsmith_ui`: the embedded fonts (binary data) and the headers below. A plugin adds it from its own `CMakeLists.txt`, so it still builds on its own:

  ```cmake
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared ${CMAKE_BINARY_DIR}/shared)
  target_link_libraries(MyPlugin PRIVATE songsmith_ui)
  ```

- `shared/Source/SongsmithType.h`: the typeface, the type scale and the title, in `Songsmith::UI`.
- `shared/Source/SongsmithLookAndFeel.h`: `Songsmith::UI::LookAndFeelBase`. Each plugin's look-and-feel derives from it and adds only its colours and special drawing (meters, faders, the EQ graph).
- `shared/Tests/SongsmithUITests.h`: checks compiled into every test executable. They fail if the typeface doesn't load, or if any label, button or combo box in an editor is smaller than the type scale allows.

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

## Accent colours

Each plugin has one accent colour, used for the product name in the title and for lit controls.

| Plugin | Accent |
|---|---|
| Songsmith EQ | Songsmith yellow `#F2C94C` (also the total curve) |
| Songsmith Analog Compressor | Amber `#E8A33D` |
| Songsmith Maximiser | Songsmith yellow `#F2C94C` |

The EQ and the maximiser share their accent today; a separate colour for one of them is a decision for a later task.
