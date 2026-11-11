# Songsmith Plugins changelog

Every Songsmith plugin, installer and release shares one version number, kept in the `VERSION` file. Each version has one release on the [Releases page](https://github.com/tonygward/audio-plugins/releases), tagged `vX.Y.Z`. Older releases keep the tags they had (`v0.5.0`, `compressor-v0.1.0`, `maximizer-v0.1.0`, `studio-v1.0.0`), and the plugin changelogs ([Songsmith EQ](eq/CHANGELOG.md), [Songsmith Maximiser](maximiser/CHANGELOG.md)) hold their earlier entries.

## Unreleased

- Songsmith Maximiser now shows just one loudness reading, Integrated LUFS, in its own column to the right of the Release dial. The Short Term and Momentary readings and the loudness band along the bottom are gone, so the window is shorter and a little wider. Hover over the number for an explanation, and click it (or press Reset) to start measuring again. Around −14 LUFS is a useful reference for some streaming masters, not a rule. The sound, the measurement itself and your saved sessions are unchanged; you do not need to do anything.

## 0.5.12

- Songsmith Maximiser now has the same subtle brushed-metal faceplate as Songsmith Analog Compressor, and its Input, Gain Reduction and Output meters sit in darker recessed housings with clearer scale marks, a brighter zero line and a more visible clipping warning. This is a visual change only: the sound, the meter readings and your saved sessions are unchanged, and there is nothing you need to do.

## 0.5.11

- The **Guidance** button and its card are gone from Songsmith EQ, Songsmith Analog Compressor and Songsmith Maximiser. Hover over the plugin name at the top left instead to read a short explanation of what the plugin does and one thing to listen for. Nothing else changes: the meters, the Frequency Guide, the loudness readings and every control work as before, and there is nothing you need to do. The preset menu in Songsmith Maximiser stays right after the name.

## 0.5.10

- Every Songsmith plugin now has the same title bar: the plugin name, a **Preset** menu, **Undo**, **Redo**, and **Bypass** at the far right. Nothing else sits there, so you always know where to look.
- Songsmith Analog Compressor and Songsmith Maximiser have working **Undo** and **Redo** for the changes you make in the plugin window and for loading a preset (also Ctrl+Z, and Ctrl+Y or Ctrl+Shift+Z). They never undo your DAW's automation. Songsmith EQ keeps the Undo and Redo it had.
- Songsmith Maximiser's starting points are now in the **Preset** menu in the title bar. **Peak Mode** and **Dither** moved under the Release knob, next to the output settings. They work exactly as before, and your saved sessions keep their settings.
- Songsmith Analog Compressor no longer has an **Interface size** menu. Drag the bottom-right corner of the window to resize it; your window size is still remembered. **Oversampling** moved down beside Ignore Below, with the other processing controls.
- Songsmith EQ's **Scale** control moved out of the title bar into the second row of controls, beside Auto Gain and Output. It still scales every band's gain together, and your saved sessions and automation keep their value. The band count moved there too, when there is room.

## 0.5.9

- Songsmith Analog Compressor's **Low Cut** dropdown is now a round **Ignore Below** knob, beside Gain Reduction Off, like the Input, Output, Attack and Release knobs. It shows Off, 60 Hz, 80 Hz, 100 Hz, 150 Hz or 200 Hz. It still only changes what the compressor listens to, so the bass you hear is unchanged. Double-click the knob to turn it Off. Your saved sessions, presets and automation keep the setting they had, and there is nothing you need to do.

## 0.5.8

- Songsmith Analog Compressor has a new **2:1** ratio button, below 4:1, for gentle compression that evens out louder parts without squashing the sound. It suits acoustic guitar, vocals and bass. It is a Songsmith addition: the original 1176 has no 2:1 button. Your existing sessions, presets and automation keep the ratio they had (4:1, 8:1, 12:1, 20:1 or All), so nothing sounds different until you press 2:1. There is nothing you need to do.

## 0.5.7

- Songsmith Analog Compressor has a new **Low Cut** control, beside Gain Reduction Off. It stops deep bass from making the compressor turn the whole sound down. It only changes what the compressor listens to, so the bass you hear is not filtered. Choose Off, 60, 80, 100, 150 or 200 Hz. It is a Songsmith addition, not part of the original 1176 design. It starts Off, including in sessions you saved earlier, so nothing sounds different until you choose a setting. If a kick or bass makes the whole mix duck, try 80 or 100 Hz; otherwise leaving it Off is fine.

## 0.5.6

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.5

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.4

- Songsmith EQ, Songsmith Analog Compressor and Songsmith Maximiser each have a new **Guidance** button. Press it when you would like a short, plain explanation of what you are hearing, what to listen for and an optional next step. Facts the plugin measured, such as Gain Reduction, are shown separately from listening suggestions. It never opens by itself, never changes a setting, and says nothing when no audio is playing. **Turn Guidance Off** in the card hides the advice until you turn it on again. Nothing in your saved sessions changes, and doing nothing is often the right choice.
- Songsmith Maximiser now has a loudness meter below the controls. It shows **Integrated**, **Short Term** and **Momentary** loudness in LUFS, the unit streaming services use. It measures what plays through the plugin, as it plays, so play your song through from the start and read Integrated at the end. It is not an analysis of a whole file. It starts again by itself each time the host starts playing from a stop; **Reset**, or a click on the Integrated number, starts again whenever you like. A dash means there is not yet enough sound to measure. The window is taller to make room, and nothing in your saved sessions changes. There is nothing you need to do.

## 0.5.3

- The version number at the bottom left of each plugin is now fully visible instead of being cut off with dots, and it stays readable at every interface size. There is nothing you need to do.

## 0.5.2

- Every plugin now shows its version (for example "v0.5.1") at the bottom left, just before the Processing Load button, so you can tell which release you are running. There is nothing you need to do.

## 0.5.1

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.0

**New version number:** Songsmith Plugins now report version 0.5.0 in every plugin and installer. Earlier beta builds were numbered 1.3.x; this is a deliberate reset to show that the suite is still in beta, not a loss of features.
- **Your sessions and presets are safe:** plugin identifiers and settings are unchanged, so projects and presets open exactly as before.
- **Installing over an earlier beta:** the 0.5.0 installers replace an installed 1.3.x build. You do not need to uninstall first.

## 1.3.8

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 1.3.7

### Songsmith Analog Compressor
- **Drag to resize:** you can now drag the corner of the compressor window to make it larger, from its normal size up to double. The window keeps its proportions, and the Interface size menu still works. The size you choose is remembered when you reopen the window and with your session. Loading a preset never changes the window size, and older sessions open at the size they were saved with, so you don't need to do anything.
- **Clearer help:** every button and menu now has a short tooltip, including Preset, Interface size and the meter buttons. The **All** button explains that it presses all four ratio buttons at once.

## 1.3.6

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 1.3.5

### Songsmith EQ
- **Asks before replacing a saved preset:** if you use Save As with a name you have already used, the EQ now asks whether to replace that preset. Nothing is replaced unless you choose Replace, so you don't need to do anything.
- **Clearer reverb preset name:** the Starting Points entry for putting an EQ before a reverb is now called Reverb Send Clean-up. It sounds exactly the same, and sessions that used it still load it.

### Songsmith Analog Compressor
- **Safer preset saving:** the compressor now asks before replacing one of your saved presets, and keeps its built-in starting points protected, so a name such as Lead vocal can't be saved over. If you try one, choose a different name.
- **Oversampling now starts Off:** a new Analog Compressor adds no delay and uses the least processing power, which suits many tracks at once and recording. If you compress hard and want a cleaner top end, choose 2x or 4x from the Oversampling menu. Saved sessions keep the setting they were saved with, so you don't need to do anything.

### Songsmith Maximiser
- **True peak is now the default:** a new Maximiser starts in True peak mode, so the peaks between samples stay under the Output Ceiling, which is what streaming and broadcast checks measure. It uses a little more processing power than Sample peak. Sessions you have already saved keep the peak mode they were saved with and sound the same, so you don't need to do anything. If you prefer the lighter Sample peak mode, you can still choose it.

## 1.3.4

### Songsmith EQ
- **Clearer Frequency Guide hints:** the hover text now speaks directly to you: what a region might sound like, when to listen for it in your mix, and whether a small adjustment may help. The Plastic and Cheap regions on Acoustic Guitar say that the band may need no adjustment. Nothing about your sound, presets or saved sessions changes, and you don't need to do anything.

## 1.3.3

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 1.3.2

### Songsmith EQ
- **Instrument icons in the Frequency Guide:** each instrument in the list now has a small picture beside its name, so you can find the one you want at a glance.
- **The output meter follows your track:** it shows two bars (Left and Right) on a stereo track and one bar on a mono track. It switches by itself if you move the plugin to a different kind of track.
- **A CLIP warning that stays lit:** if the output reaches full scale, a red CLIP label above the meter stays on until the audio stops, so a brief clip is not missed. It clears by itself after a short stretch of silence. There is nothing you need to do unless you want to turn the level down.
- **Starting Points are grouped by instrument:** the list is easier to scan, with new Mid Tom and High Tom variations. The duplicate Factory section is gone; every starting point is still available under its instrument. Your saved sessions and presets load as before.

### Other
- Songsmith Analog Compressor and Songsmith Maximiser include the latest behind-the-scenes improvements. Nothing you need to do.

## 1.3.1

Beta maintenance checkpoint, authorised on 7 November 2026 in PR #113.

- Songsmith EQ and Maximiser no longer set the obsolete JUCE splash-screen option. This removes build warnings without suppressing other compiler warnings or changing audio processing, parameters or saved sessions.
- Development infrastructure: [AUTO-01](shared/prompts/AUTO-01-matrix-autonomous-development.md) adds restricted manual build-task staging and validation. The supervised fixture still requires the existing Windows account credential; its registration and runtime validation are follow-ups, not beta release gates. Autonomous development remains disabled.
- Includes the existing 1.3.0 Interface and Frequency Guide features; EQ-38 instrument icons are not included. Not yet verified in a DAW.

Install the suite using the single installer for your platform, with one selectable tick box per plugin. The Windows installer is unsigned and may show SmartScreen; the unsigned macOS package may require Open Anyway in Privacy & Security. Uninstalling keeps presets and user data. Manual Windows automation setup is not required to use the plugins.

## 1.3.0

**Delivery traceability:** this release is the agreed **Interface and Frequency Guide** milestone. Prompts: [EQ-37 movable band panel](eq/prompts/done/37-draggable-band-panel.md) (#87), [ACOMP-08 Analog Compressor editor refinements](analog-compressor/prompts/done/08-1176-editor.md) (#88) and [EQ-32 Instrument Frequency Guide](eq/prompts/done/32-frequency-guide.md) (#89). Nothing outside this scope is in the release.

### Songsmith EQ
- **Move the band controls:** drag the floating band panel by its background or title to put it where you want. It always stays inside the graph, and when you open Dynamic it moves just enough to show every new control. It stays where you put it when you choose another band, and opens beside its band again after you close it. Dragging the panel never changes a band or a setting.
- **Frequency Guide:** an optional overlay, off by default. Press **Frequency Guide** above the graph and choose an instrument: Acoustic Guitar, Nylon Guitar, Electric Guitar, Bass Guitar, Vocal, Kick Drum, Snare Drum, Toms, Drum Bus or Any Instrument. Soft coloured regions appear behind the curve, each with a plain word (Rumble, Thump, Plastic, Air and so on). Hover a region to read what it adds, what too much sounds like, and when it is usually moved. The regions are places to listen, not rules, and the guide only draws: it never changes a band or the sound. It is saved with your session. The words come from Joe Gilder's Home Studio Corner videos and Tony's own notes; the guide is not endorsed by Joe Gilder. See [the Frequency Guide notes](eq/docs/frequency-guide.md).
- **A wider smallest window:** the controls for the guide need more room, so the smallest Songsmith EQ window is now 980 pixels wide at 100 % (it was 760). A window you saved smaller opens at the new smallest size.

### Songsmith Analog Compressor
- **A larger needle meter** with bigger scale numbers, about 45 % more face than before. The meter modes and how the needle moves are unchanged.
- **A textured faceplate** with a fine brushed grain, a lit edge and four screws, drawn so it stays sharp at every interface size.
- **Clearer ratio buttons:** they now read 4:1, 8:1, 12:1 and 20:1, each with a plain tooltip. **All Buttons** is now called **All**; it still presses every ratio button at once.
- **Room for a future control:** a place beside Gain Reduction Off is kept free for a detector Low Cut control. Nothing is there yet.
- The window is a little wider at every interface size to make room for the larger meter.

### Compatibility and known limits
- The sound is unchanged in every plugin. No parameter, saved setting or preset changed, so your sessions and presets load as before; a session saved before this version opens with the Frequency Guide off.
- These changes were checked by the automated tests on Windows and macOS. They have **not** been tried in Studio One, Cakewalk or Logic.
- The installers are not signed. Windows may show a SmartScreen warning, and macOS may ask you to allow the installer in System Settings → Privacy & Security.

## 1.2.0
**Delivery traceability:** this release is the agreed **UI refinements** milestone. Prompts: [EQ-33 filter shape icons](eq/prompts/done/33-filter-shape-icons.md) (#80), [EQ-34 Linear Phase tooltip](eq/prompts/done/34-linear-phase-tooltip.md) (#79), [EQ-35 Piano button placement](eq/prompts/done/35-piano-footer.md) (#81), [EQ-36 Dynamic tooltip](eq/prompts/done/36-dynamic-disabled-tooltip.md) (#78), [SUI-12 Processing Load footer](shared/prompts/done/SUI-12-processing-load-footer.md) (#81) and [SUI-13 consistent Bypass](shared/prompts/done/SUI-13-consistent-bypass.md) (#82). Nothing outside this scope is in the release.

### Every plugin
- **Bypass is in the same place and the same colour everywhere:** the Bypass button is at the top right of Songsmith EQ, Songsmith Analog Compressor and Songsmith Maximiser, at about the same size. When it is switched on it turns red, and red always means the sound is bypassed. In the Analog Compressor it moved from the lower right to the top right. Bypass works exactly as before, including automation.
- **Processing Load is at the far left of the strip along the bottom.** Switch it on and one line appears beside it, for example "CPU 0.35%, Latency 0.0ms (0 samples)". Both figures are measured live. If no audio is playing it says "CPU Unavailable" and still shows the latency. **What changed:** the latency used to be on the strip all the time; now it appears with Processing Load. Switch it off and the line disappears. Your Processing Load choice is still remembered on your computer, not in your sessions.

### Songsmith EQ
- **Piano button at the bottom right,** on the same strip, next to the piano notes it shows along the bottom of the graph. It stays at the right whether or not Processing Load is on.
- **Filter shapes:** each filter type (Bell, Low Shelf, High Shelf, Low Cut, High Cut and Notch) has a small picture of its curve next to its name, in the band panel, its drop-down list and the right-click menu.
- **Clearer Dynamic button:** when Dynamic is greyed out, hover over it to see why. It names the filter type, or Linear phase mode, or both, and says what to change (Dynamic works on Bell, Low Shelf and High Shelf, in Zero latency mode).
- **Linear phase explained:** the processing mode box now says when Linear phase is worth trying, why it adds delay and that Zero latency is the better choice for recording and most mixing.

### Compatibility and known limits
- The sound is unchanged. No parameter, saved setting or preset changed, so your sessions and presets load as before.
- These changes were checked by the automated tests on Windows and macOS. They have **not** been tried in Studio One, Cakewalk or Logic.
- The installers are not signed. Windows may show a SmartScreen warning, and macOS may ask you to allow the installer in System Settings → Privacy & Security.

## 1.1.0
**Delivery traceability:** Shared-suite prompts [SUI-06 — one installer entry per plugin](shared/prompts/done/SUI-06-one-entry-per-plugin.md), [SUI-07 — one version number](shared/prompts/done/SUI-07-one-version-number.md), [SUI-08 — suite branding](shared/prompts/done/SUI-08-songsmith-plugins-brand.md), and [SUI-11 — latency and processing load](shared/prompts/done/SUI-11-latency-and-processing-load.md). SUI-11 is a retrospectively archived specification; delivery verification belongs to the original implementation PR and CI. The shared UI and installer foundations are documented in [SUI-01–SUI-05](shared/prompts/README.md). No new feature or version bump results from this documentation correction.


### Every plugin
- **One version number:** every Songsmith plugin now shares one version number. Songsmith EQ moves up from 0.5.0, and the Songsmith Analog Compressor and Songsmith Maximiser move up from 0.1.0, so each of them upgrades cleanly over what you have installed.
- **Named Songsmith Plugins:** the family is now called Songsmith Plugins in the installers, the welcome pages, the Settings → Apps entry and the release notes. Each plugin keeps its own name, and Songsmith Studio stays the publisher. Upgrading from Songsmith Studio 1.0.0 replaces the old entry, so you still see one, and your projects and presets are untouched.
- **One installer entry per plugin:** the installers offer one tick box per plugin. Each plugin comes with its standalone app, and on a Mac with its Audio Unit too.

- **Latency and Processing Load:** every editor has a strip along the bottom. Latency shows the delay the plugin reports to your recording software, in samples and milliseconds. Processing Load is optional and off until you switch it on; it shows how much of the time available for each block of audio this plugin uses. Neither changes the sound, and the switch is not saved in your sessions or presets.

### Songsmith EQ
- The strip along the bottom is new, and the window is 28 px taller by default so the graph keeps its size. The sound is unchanged. Your sessions and presets load as before.

### Songsmith Analog Compressor
- The strip along the bottom is new. The sound is unchanged. Your sessions and presets load as before.

### Songsmith Maximiser
- The strip along the bottom is new. The sound is unchanged. Your sessions and presets load as before.
