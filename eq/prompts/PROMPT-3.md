# Claude Code Prompt 5 — Advanced 24-Band Parametric EQ

You are an experienced C++ DSP engineer specialising in professional audio equalisation, JUCE and real-time VST3 development.

We have an existing working JUCE-based Windows VST3 equaliser with an interactive graphical EQ curve and a real-time FFT spectrum analyser.

JUCE is installed at `C:/dev/JUCE`.

**Your task is to extend the plugin into an advanced 24-band parametric EQ with professional-quality interaction, accurate filter processing and reliable automation.**

Do not redesign the application from scratch.

## 1. Analyse the current implementation

Before writing code:

- Examine the existing EQ filters and parameter architecture.
- Identify whether filter processing uses JUCE IIR, custom biquads or another implementation.
- Inspect how nodes map to processor parameters.
- Inspect parameter state persistence and automation identifiers.
- Preserve compatibility with existing projects and presets wherever technically possible.
- Identify potential CPU or thread-safety problems before proceeding.

Produce a short implementation plan.

## 2. Expand EQ architecture to 24 bands

Support up to 24 independently configurable EQ bands.

Each band must support:

- Enabled / disabled state
- Frequency
- Gain
- Q / bandwidth
- Filter type
- Independent graphical node
- Visual selection state
- DAW automation

Frequency range:

20 Hz to 20 kHz, constrained appropriately by the sample rate and filter design.

Gain range:

-30 dB to +30 dB, with a default visible editing range of ±24 dB.

Q range:

Approximately 0.1 to 40, subject to stable filter implementation.

All 24 bands must be available, but inactive bands should have minimal CPU cost.

## 3. Filter processing

Implement or extend the EQ DSP using stable, properly tested filter algorithms.

For this stage, support:

- Bell / peaking EQ
- Low shelf
- High shelf
- Low cut / high-pass
- High cut / low-pass
- Notch

Requirements:

- Accurate centre frequency and gain.
- Predictable bandwidth and Q behaviour.
- Stable filter coefficients at extreme values.
- Correct processing at 44.1, 48, 88.2, 96 and 192 kHz.
- Sensible handling of frequencies near Nyquist.
- No denormal-related CPU spikes.
- No NaN or infinite output.
- Proper filter state resetting.
- Independent processing for left and right channels.

Use well-established digital filter formulations.

Do not introduce linear-phase processing or dynamic EQ yet.

## 4. Avoid clicks during parameter changes

Filter updates must not introduce audible discontinuities.

Requirements:

- Smooth gain changes.
- Smooth frequency changes.
- Smooth Q changes.
- Avoid abruptly replacing coefficients while audio is processing.
- Prefer coefficient interpolation or another stable smoothing method where appropriate.
- Consider the limitations of naive coefficient interpolation.
- Avoid unnecessary work per sample or per audio block.
- Support fast automation without audio glitches.

Implement a clearly defined strategy for smoothing and explain why it is safe for the selected filter structure.

## 5. Accurate combined EQ curve

The GUI must display the actual combined frequency response of the active filters.

Requirements:

- Calculate response using the same coefficients and filter topology used by the DSP.
- Correctly combine filter magnitudes.
- Display overlapping boosts and cuts accurately.
- Handle narrow notches and high-Q peaks.
- Preserve visual accuracy across different sample rates.
- Update smoothly while nodes are dragged.
- Show a selected band's individual frequency response as a secondary curve.
- Keep the main combined response clearly visible.

Avoid drawing an approximate decorative curve unrelated to the actual filter processing.

## 6. Advanced node interaction

Implement professional graphical EQ editing.

Mouse behaviour:

- Double-click empty graph space to create a band.
- Click a node to select it.
- Drag horizontally to adjust frequency.
- Drag vertically to adjust gain.
- Use the mouse wheel over a node to adjust Q.
- Double-click an existing node to reset its parameters.
- Right-click a node to open its context menu.
- Delete key removes or disables the selected band.
- Shift-drag enables fine adjustment.
- Support sensible interaction for overlapping nodes.

Provide a clear tooltip showing:

- Frequency in Hz or kHz
- Gain in dB
- Q value
- Filter type

Where possible, update tooltips continuously while dragging.

All interactions must affect the actual DSP and remain automatable.

## 7. Active-band management

Support a maximum of 24 bands without cluttering the interface.

Requirements:

- Start with only the existing active EQ bands visible.
- Allow users to add bands dynamically.
- Allow users to delete or deactivate bands.
- Give every band a consistent identity.
- Preserve parameter IDs needed by the DAW.
- Use a fixed internal bank of 24 automatable bands if necessary, while exposing a dynamic set of active nodes in the UI.
- Allow multiple bands at similar frequencies.
- Distinguish selected and unselected bands visually.

Avoid creating or removing host parameters dynamically during playback.

## 8. Filter type controls

Provide a contextual floating panel for the selected band.

Controls:

- Filter type
- Frequency
- Gain
- Q / bandwidth
- Enabled / bypass
- Numeric editing
- Reset controls

The panel should:

- Be compact and visually consistent.
- Avoid obscuring the main editing area.
- Follow or anchor intelligently near the selected node.
- Display values with sensible precision.
- Support both mouse and keyboard entry.

Filter type changes should avoid audible discontinuities.

## 9. Undo and redo

Implement undo/redo for user-driven EQ editing.

Requirements:

- Adding a band
- Removing a band
- Moving a band
- Changing filter type
- Changing frequency, gain or Q
- Resetting a band

Group each continuous drag operation into a sensible undo transaction.

Use JUCE's UndoManager where appropriate.

Do not fill the undo history with hundreds of individual steps during a single mouse drag.

## 10. Preset and session persistence

Ensure the following survive saving and reopening a Studio One project:

- All 24 bands
- Active/inactive states
- Frequency
- Gain
- Q
- Filter types
- Analyser settings
- Relevant UI state

Requirements:

- Use a robust versioned state representation.
- Respect existing parameter IDs.
- Do not break previously saved sessions unnecessarily.
- Ensure automation remains attached to the correct bands.
- Support multiple instances independently.

Do not require the plugin window to be open for EQ processing.

## 11. CPU efficiency

Optimise for practical use in real mixing sessions.

Requirements:

- Skip disabled bands.
- Avoid audio-thread memory allocation.
- Avoid blocking synchronisation.
- Preallocate filter objects and necessary state.
- Recompute coefficients only when parameters change.
- Avoid rebuilding the entire DSP graph unnecessarily.
- Maintain practical performance with all 24 bands enabled.
- Ensure stable performance at small audio buffer sizes.

Benchmark CPU use for 3, 12 and 24 active bands using a repeatable test harness.

Report actual measurements rather than estimates.

## 12. Automated DSP testing

Create tests that verify the correctness of the filters.

Include:

- 1 kHz bell boost of +6 dB
- 1 kHz bell cut of -6 dB
- Low-shelf response
- High-shelf response
- High-pass response
- Low-pass response
- Narrow notch response
- Multiple overlapping bands
- Neutral / bypassed response
- Extreme Q values
- Rapid parameter changes
- Silence and very low-level input
- Multiple supported sample rates

Compare measured output or analytically calculated responses against expected values, within stated numerical tolerances.

Include tests for state save/restore and preserved parameter identities.

## 13. Build and verify

- Preserve the existing project structure.
- Build the Windows x64 VST3.
- Resolve compilation errors.
- Run automated tests.
- Keep the analyser and GUI functional.
- Report any regressions or remaining limitations.
- Show the location of the compiled `.vst3`.

Do not state that DAW integration has passed unless it has been verified in Studio One.

**Acceptance criteria:** A functioning, efficient 24-band parametric EQ with accurate filtering, an interactive frequency response graph, independent band control, working automation, undo/redo and reliable session persistence.