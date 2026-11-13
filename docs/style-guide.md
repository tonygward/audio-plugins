# Songsmith Plugins style guide

How every Songsmith plugin looks, so they read as one family. The code for all of this lives in the shared UI library, [`shared/`](../shared/). New plugins link it; they don't copy fonts or look-and-feel code.

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
- `shared/Source/SongsmithWording.h`: the wording check (below), run by every plugin's tests on its editor, parameter names and presets.
- `shared/Tests/SongsmithControlSheet.h`: renders every control state for screenshots (`controls.png` next to each plugin's editor screenshots).

## Control proximity

**Standing UI rule: a widget belongs near the thing it affects.** Location is part of usability, not merely decoration. Place toggles and controls next to their graph, note strip, meter, readout or processing section so the connection is self-evident. Avoid moving a control to an unrelated global toolbar just to fill space. Group related controls, maintain the spatial relationship at minimum and maximum window sizes and at high DPI, and check both enabled and disabled states. If a control truly affects the entire plugin, a shared toolbar can be appropriate; explain exceptions.

**EQ example:** the Piano toggle belongs at the **bottom-right of the interface beside the piano-note strip** it shows/hides, as required by [EQ-35](../eq/prompts/done/35-piano-footer.md). The Processing Load toggle sits at the **far left of the bottom status strip immediately beside its conditional CPU/latency values**, as required by [SUI-12](../shared/prompts/done/SUI-12-processing-load-footer.md). Consistent proximity takes precedence over arbitrary alignment or symmetry.

**Consistent global Bypass:** Place the Bypass button at the **top-right** of every editor, matching the established EQ/Maximiser layout. Unlike feature-local controls, this whole-plugin action has a predictable suite-wide position. When engaged, the button uses the same **#C9563C** red (the Analog Compressor's existing bypass colour); when not engaged it uses the normal neutral control style. The token is `Songsmith::UI::Colours::bypass` and every editor applies it with `Songsmith::UI::styleBypassButton`; the button is 84 design pixels wide in every plugin. Keep the position stable through scaling. Local toggles (band On/Off, Gain Reduction Off) never use this red. See [SUI-13](../shared/prompts/done/SUI-13-consistent-bypass.md).

For each new or moved control, ask: *Can a musician see what it affects without searching elsewhere?* Test across resizing, scaling, keyboard focus and state changes. Include a representative screenshot in UI PRs.

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

Songsmith EQ's title bar is its first top-bar row (42 px tall, 12 px from the left), laid out by the shared header like the others: name, Preset, Undo, Redo and Bypass, nothing else (see [title bar](../shared/docs/title-bar.md)). It has to fit at the 760 px minimum width. Interface size is in the `...` menu.

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
| `bypass` | `#C9563C` | Global Bypass when engaged (processing is bypassed). Never replaced by a plugin's accent. Black text on it has a contrast of 4.9:1 |
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
- **Faders** (the maximiser's Threshold and Output Ceiling): a 6 px well as the track, filled in the fader's colour (vertical faders from the top, like the L1's); a raised handle with the button bevel and a line in the fader's colour.

## Accent colours

Each plugin has one accent colour, used for the product name in the title, lit buttons, knob pointers and value arcs.

| Plugin | Accent |
|---|---|
| Songsmith EQ | Songsmith yellow `#F2C94C` (also the total curve) |
| Songsmith Analog Compressor | Amber `#E8A33D` |
| Songsmith Maximiser | Songsmith yellow `#F2C94C` |

The EQ and the maximiser share their accent today; a separate colour for one of them is a decision for a later task.

## Latency and Processing Load strip

Every editor has the same strip along the bottom, from the shared library (`shared/Source/SongsmithDiagnostics.h`, class `Songsmith::Diagnostics::Bar`). It is 28 px tall at 100 %, inside the scaled editor, with a rule above it. Text is `Type::control` (Regular, tabular figures). The **Processing Load** switch (a bevelled toggle button) is at the **far left**. Only while it is on, one readout follows directly to its right: `CPU 0.35%, Latency 0.0ms (0 samples)`. The right end is free for a plugin's own control that belongs beside what it affects: Songsmith EQ puts its **Piano** toggle there (`Bar::setTrailingControl`), which stays right-aligned whether or not the readout is showing. New plugins add the strip the same way and put `Songsmith::Diagnostics::LoadMeter loadMeter;` in their processor.

- **Latency** is what the plugin reports to the host, in milliseconds at the current sample rate and in samples ("1.8ms (84 samples)", "1 sample" for one). It is shown only inside the readout, so it appears when Processing Load is on. It follows mode and sample-rate changes. Tooltip: "The delay this plugin adds. Your recording software may compensate for it."
- **Processing Load** is off by default and per instance. It estimates the share of each block's time budget that this instance uses, smoothed over about half a second, with enough digits for light loads ("0.07%", "3.2%", "14%"; a real zero reads "0%"). When the switch is off the readout is hidden completely. When no block has been measured in the last second (for example when the host has stopped) it reads "CPU Unavailable" and still shows the latency; it never shows a made-up zero. "CPU" is the one abbreviation allowed, and only in this readout (`Wording::findBanned`). Tooltip: it describes this plugin, and varies with the computer, the settings and the buffer size.
- The switch is a machine setting in `Diagnostics.xml` next to the plugin's presets folder (never inside it). It is not a parameter and is not saved with sessions or presets. Closing the editor switches timing off.
- Real-time rules: the audio thread only reads the clock and writes two atomics. With the switch off, a block costs one relaxed atomic read. Measured: about 70 ns a block with it on, under 1 ns with it off.

## Wording

Someone who has never used a studio compressor or equaliser should be able to read every plugin. Clarity and simple language go a long way.

- **Spell every word out:** labels, buttons, menu items, combo-box items, tooltips, hints, undo descriptions, text drawn on the panel, the parameter names hosts show, and preset names. "Gain Reduction", not "GR"; "Output", not "Out"; "Side Chain", not "SC".
- **The only short forms allowed:** units (`Hz`, `kHz`, `dB`, `dB/oct`, `dBFS`, `dBTP`, `dBu`, `LUFS`, `ms`, `s`, `%`, `×`); note names (`A4`, `C#5`); key names in tooltips (`Ctrl`, `Alt`, `Cmd`, `Shift`); `Q`, the name of the EQ control, explained in its tooltip; format names in installers and READMEs (`VST3`, `AU`); and the product name Songsmith EQ.
- **Say what a control does,** not what the hardware called it: "how fast it reacts when the sound gets louder", not "attack time constant". When a technical term is the right name (Threshold, Ratio, Dither, Oversampling), keep it and explain it in one short tooltip sentence.
- **Title case for control names** ("Output Ceiling"), sentence case for tooltips, hints and values ("Oversampling: off"). No capitals for emphasis.
- **One word for one thing** across the suite (see the glossary).
- **If a word doesn't fit, change the layout:** a wider control, a second line, or a caption above. Never shorten the word. The tests check that every label and button shows its whole text.
- **British spelling** in UI text and READMEs ("analyser", "colour", "maximiser"). Product names, bundle IDs and folders don't change to fix spelling.

The check (`Songsmith::UI::Wording::findBanned`) fails on a banned word (`SC`, `AG`, `OS`, `GR`, `Dyn`, `FREQ`, `THRESH`, `Atten`, `Sidechain`, `In`, `Out`, `TPDF`, `CPU` and a few more), on a standalone capital letter other than `Q`, and on any word in capitals that isn't an allowed short form.

### Glossary

| Say | Not | Means |
|---|---|---|
| Input | In | The level coming in, or the control that sets it |
| Output | Out | The level going out, or the control that sets it |
| Gain Reduction | GR, Atten, Attenuation | How much a compressor or limiter is turning the level down, in dB |
| Gain Reduction Off | GR Off | The compressor stops compressing but still colours the sound |
| Threshold | Thresh | The level where a compressor, limiter or dynamic band starts to act |
| Output Ceiling | Out Ceiling | The highest level the maximiser's output may reach |
| Ratio | | How strongly sound above the threshold is turned down |
| Attack | Att | How fast it reacts when the sound gets louder |
| Release | Rel | How fast it lets go when the sound gets quieter |
| Auto Release | | A release that adapts to the material |
| Side Chain | SC, Sidechain | A second input that controls the processing |
| Oversampling | OS | Running at a higher sample rate inside the plugin, for cleaner distortion and filters |
| Dither | TPDF | A very quiet noise added before the output is rounded to fewer bits |
| Bypass | Byp | The sound passes through untouched |
| Interface size | Size, UI size | How large the window is drawn: at least 75 % to 200 % (Songsmith EQ 75–200 %, Songsmith Maximiser 75–300 %; Songsmith Analog Compressor is 100–200 % until [ACOMP-17](https://github.com/tonygward/audio-plugins/issues/255)) |
| Starting point | | A factory setting to start from, with a listening hint |
| Preset | | A saved setting, yours or a factory one |

### Known naming questions

- Some factory presets shipped before the preset naming rule in `CLAUDE.md` (for example "Lead vocal", "Abbey Road reverb", "Rack tom"). They are renamed for their use case in [SUI-19](https://github.com/tonygward/audio-plugins/issues/266); until then, new presets follow the rule.
- "Songsmith Analog Compressor" uses the American "analog" while the UI uses British spelling; the product name is part of the plugin's identity in hosts and installers, so it stays.
