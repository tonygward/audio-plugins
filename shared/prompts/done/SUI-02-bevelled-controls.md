You are an experienced C++ audio developer and UI engineer working on the **Songsmith Studio** plugins, a set of JUCE VST3/AU/Standalone plugins in one repo: `C:/dev/audio-plugins`. Songsmith EQ is in `eq/`, Songsmith Analog Compressor in `fet/`, Songsmith Maximiser in `maximizer/`. JUCE is at `C:/dev/JUCE`.

You may be running **unattended**. Nobody will answer questions. When something is unclear, choose the most sensible option, keep going, and record the decision in the PR description.

## Working rules (apply to every task)

**Git**
- Start from an up-to-date `main`: `git checkout main && git pull`, then create the branch named below.
- Never commit to or push `main`. Never force-push.
- Commit in logical steps with clear messages.

**Read first**
- `README.md`, `prompts/suite/README.md`, `docs/style-guide.md` and the `shared/` library added by task SUI-01.
- Each plugin's look-and-feel: `eq/Source/EQLookAndFeel.h`, `fet/Source/FETLookAndFeel.h`, `maximizer/Source/MaxLookAndFeel.h`, plus any control that paints itself (for example `FETKnob::paint` in `fet/Source/PluginEditor.cpp`, the EQ's `BandPanel`, `PresetBar` and `OutputMeter`, the maximiser's `Meters.h`).
- Work with the existing architecture. Don't rewrite it.

**Compatibility (must not break)**
- Never rename or remove parameter IDs, and never change any plugin's `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, bundle ID or product name.
- Don't change DSP, parameter ranges, `stateVersion` or saved-state layouts.
- Keep every existing keyboard and mouse gesture working (double-click to reset, Alt-click solo, drag, right-click menus).
- Each plugin must still configure and build on its own from its own folder.

**Real-time safety**
- Nothing in this task may touch `processBlock` or any engine's process code.

**Build and test (every plugin)**
- For each of `eq`, `fet` and `maximizer`: `cmake --build <plugin>/build --config Release`. If a `build/` folder is missing, first run `cmake -S <plugin> -B <plugin>/build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE`.
- Every build must succeed with no new warnings, here and on the macOS CI job.
- Run every test executable (`EQTests`, `FETTests`, `MaximizerTests`) and make sure all tests pass. Add tests in the existing `juce::UnitTest` style.
- Render before and after screenshots of every editor with each test executable's screenshot option, at `--scale 1` and `--scale 2`, including buttons in their off, on, hover (if the renderer can show it) and disabled states. Commit them under `docs/screenshots/SUI-02/` and show them side by side in the PR.
- **Do not run any `install.ps1`, installer or system-wide install.**

**Finish**
- Update `docs/style-guide.md` and any README that describes the look.
- Push the branch and open a **draft** PR with `gh pr create --draft`. The PR body should cover what changed, every choice you made differently from this prompt (and why), test results, known limitations, and the line "Not yet verified in a DAW".
- If you get stuck, commit what works and open the draft PR anyway with a "Blocked" section.

---

# Task SUI-02: The compressor's bevelled look on every control, in every plugin

**Branch:** `suite/SUI-02-bevelled-controls`

## Why

Tony likes the slightly bevelled, physical look of the Songsmith Analog Compressor: knobs with a lit cap and a soft highlight from the top left, a recessed well around them, and a panel that shades from lighter at the top to darker at the bottom. The EQ and the maximiser draw flat controls, and even the compressor's own buttons are flatter than its knobs. Every plugin should use one control style, and it should be that one.

## Start by pinning down the look

Before changing anything, render the compressor's editor at `--scale 2` and look closely at what makes it feel bevelled. Today that is mostly in `fet/Source/FETLookAndFeel.h`:

- `drawRotarySlider`: a dark recessed ring, a cap filled with a radial `ColourGradient` from `panelLight.brighter (0.25f)` at the top left to `panel.darker (0.4f)` at the bottom right, a thin dark outline, and an accent-coloured pointer.
- `paintContent` in `fet/Source/PluginEditor.cpp`: a vertical panel gradient and a darker title bar.

Write down the recipe in `docs/style-guide.md` under a "Controls" heading (light source, gradient stops, edge widths, corner radius, how "on", "pressed", "hover" and "disabled" look), then implement it once.

## Requirements

### 1. Shared control drawing

Add the drawing to the shared look-and-feel base from task SUI-01 (`shared/`), so every plugin inherits it:

- **Buttons and toggle buttons** (`drawButtonBackground`, `drawButtonText`, `drawToggleButton` where used): slightly raised when off: a vertical gradient a little lighter at the top, a 1 px highlight on the top inner edge, a 1 px darker edge at the bottom, rounded corners (about 4 px at 100 %). When **on** or **pressed**, it looks pushed in: the gradient reversed or darkened, the highlight moves to the bottom edge, and it fills with the plugin's accent colour (the bypass colour for Bypass, as the compressor does now). Hover brightens slightly. Disabled is dimmed and flat. Keep it subtle: "slightly bevelled", not skeuomorphic chrome.
- **Rotary knobs:** the compressor's knob recipe, with the pointer and any value arc in the plugin's accent colour. The EQ's band panel knobs and the maximiser's Release knob move to it.
- **Combo boxes and the preset bar:** a recessed well (darker than the panel, highlight on the bottom edge, shadow on the top edge), so fields read as "something you put a value into" and buttons read as "something you press".
- **Linear sliders and faders** (maximiser Threshold and Ceiling, EQ output if it uses one): recessed track, raised handle with the same bevel as a button.
- **Panels:** the compressor's top-to-bottom panel gradient and title bar on every editor. The EQ's graph area stays as it is: the graph needs a flat background to stay readable.
- Every value (gradient amounts, edge widths, radii) scales with the editor's interface size, and stays crisp at 2× (draw with float rectangles, half-pixel aligned 1 px lines).
- Colours come from shared neutral tokens (panel, panelLight, recess, text, textDim, edge) plus one accent per plugin. Unify the neutral tokens across the three plugins if they differ only slightly, and list every changed colour in the PR.

### 2. Move every plugin onto it

- Delete the per-plugin copies of button, knob and combo-box drawing that the shared base now covers. Keep only drawing that is truly specific (the EQ graph and nodes, needle and bar meters, the compressor's printed knob scale).
- Check the EQ carefully: it has the most controls (top bar, band panel, preset bar, analyser settings, piano toggle). Every button must get the new look, and none may grow or shrink enough to break row layouts at the minimum window size.

## Tests

- A test per editor that walks the component tree and checks every `Button`, `Slider` and `ComboBox` uses a look-and-feel derived from the shared base (so no control slips back to a plain JUCE look).
- A pixel test on a rendered button in off and on states: the top edge is lighter than the bottom edge when off, and the reverse when on. Keep the tolerance loose; this guards the idea, not exact colours.
- Every existing test still passes, including the compressor's colour tests.

## Out of scope

- Changing wording (task SUI-03) or type sizes (task SUI-01).
- Moving or adding controls.
- Changing how meters or the EQ graph are drawn, beyond the panel background around them.
