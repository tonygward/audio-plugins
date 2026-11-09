# Editor (step 4)

An original vector editor. Its control layout is informed by the hardware's front panel (UM pp. 8-10), but it doesn't copy the hardware look:

- the large **Input** and **Output** knobs on the left;
- **Attack** above **Release** in the middle;
- the **ratio buttons** 20 / 12 / 8 / 4, top to bottom as on the hardware, plus **All**;
- the needle **meter** with its **GR / +4 / +8** buttons on the right;
- **GR Off** and **Bypass** under the meter;
- **Oversampling** and **Size** in the top bar.

It uses the same embedded Inter fonts, type sizes, title, colours and slightly bevelled controls as every Songsmith plugin, from the shared UI library in `shared/` (fonts in `shared/Resources/Fonts`, SIL OFL 1.1). The bevelled look started here, on this editor's knobs. See `docs/style-guide.md`.

## Scaling

Everything is laid out in design units (960 x 400) inside one component, and that component is scaled as a whole with an affine transform. Text, knobs, strokes and the meter scale together, and JUCE draws them as vectors at the final resolution, so every size is sharp, including Retina and Windows display scaling. Sizes are 100, 125, 150, 175 and 200 %, picked from the **Size** menu. The size is saved with the session; a hand-edited value snaps to the nearest option. Tested at 100, 150 and 200 %: window size, control positions and proportions.

## Controls

- **Readable values** under each knob:
  - Input and Output in dB (`+12.0 dB`);
  - Attack and Release as the dial position plus the time it means (`4.0 · 126 µs`, `4.0 · 235 ms`), from the step 2 mapping.
- **Double-click** a knob for its default: Input 0 dB, Output 0 dB, Attack 4, Release 4.
- **Automation gestures:**
  - knob drags and double-clicks are wrapped by JUCE's slider attachment;
  - each ratio, All, GR Off and Bypass press is one complete gesture (tested: one begin and one end per press);
  - Oversampling uses the combo-box attachment.

## Meter

**Modes** (UG p. 3, UM p. 22). The digital reference is 0 VU = +4 dBm = -18 dBFS RMS. The meter mode is UI state, saved with the session, not automatable.

| Mode | Needle reads | Tested |
|---|---|---|
| GR | -GR (0 = no gain reduction) | 7 dB GR → -7 VU |
| +4 | output RMS + 18 dB (0 VU = -18 dBFS) | -18 dBFS → 0 VU |
| +8 | output RMS + 14 dB (0 VU = -14 dBFS) | -18 dBFS → -4 VU |

- **Scale:** a VU scale from -20 to +3, linear in voltage, with +3 at full scale and 0 VU at 70.8 %. The red section is above 0 VU.
- **Independent display ballistics:** the processor only publishes values (the largest GR since the last read, and the output block RMS). The needle runs its own second-order model (damping 0.82, ω = 13.8 rad/s) on the editor's 30 Hz timer, integrated in 1 ms steps, so it doesn't depend on the audio block size (tested).
- **Calibration:** a step reaches 99 % in 300 ms ± 30 ms with 0.3-1.5 % overshoot. This is the VU meter characteristic (IEC 60268-17), and it is tested.
- **Numeric readout:** the GR value under the needle rises instantly and falls over ~300 ms, so short peaks can be read exactly.

## Screenshots

`AnalogCompressorTests --screenshots <folder> [--scale 2]` renders the editor with audio running: 100, 150 and 200 %, All Buttons, and the +4 meter. CI uploads them as `Songsmith-Analog-Compressor-screenshots-<OS>`.

## Session recall

These are saved in the session (a `UI` child of the state, alongside the parameters):

- the window width (`editorWidth`, SUI-10) and the nearest interface size (`uiScale`; older sessions only have this, and reopen at that size);
- the meter mode.

Loading a preset never changes the window size.

## Resizing (SUI-10)

Drag the window corner where the host allows it. The window keeps the design proportions (960 × 428) between 100 % (the minimum, so text stays at least 12 px) and 200 %. The whole editor is drawn in design units and scaled as one, so it stays sharp. The Interface size menu jumps to 100, 125, 150, 175 or 200 % and shows the nearest step after a drag.

Out-of-range values are snapped or clamped (tested). Every control's value is a host parameter and is recalled with it.
