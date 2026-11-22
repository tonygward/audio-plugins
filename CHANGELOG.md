# Songsmith Plugins changelog

Every Songsmith plugin, installer and release shares one version number, kept in the `VERSION` file. Each version has one release on the [Releases page](https://github.com/tonygward/audio-plugins/releases), tagged `vX.Y.Z`. Releases from before one shared version keep the tags they had (`v0.2.0` to `v0.4.0`, `compressor-v0.1.0`, `maximizer-v0.1.0`, `studio-v1.0.0`), and the plugin changelogs ([Songsmith EQ](eq/CHANGELOG.md), [Songsmith Maximiser](maximiser/CHANGELOG.md)) hold their earlier entries. Versions 1.1.0 to 1.3.8 came first: the suite was then renumbered to 0.5.0, which is why the numbers below 0.5.0 are higher.


## Unreleased

- **Songsmith Optical Leveller: Set Levels.** A new **Set Levels** button under Peak Reduction listens to a typical part of your song and suggests the Peak Reduction that gives the **Target Gain Reduction** you choose. Type the target in dB (**-3.0 dB** means 3 dB of reduction); it is an average over what you played, not a fixed amount at every moment. A small ear lights up while the plugin is listening to your sound. Apply sets Peak Reduction and closes the panel straight away, Undo puts it back, and Cancel changes nothing.
- **Songsmith Optical Leveller: type exact values.** Click the number under **Gain** or **Peak Reduction** to type a value. Peak Reduction now shows one decimal place.
- **Songsmith Optical Leveller: a lighter grey faceplate**, with darker labels so everything stays easy to read. The meters sit on their own dark panel.
- **Songsmith Optical Leveller: Oversampling.** A new **Oversampling** dropdown (Off, 2× or 4×) runs the leveller at a higher rate inside the plugin. It is Off by default, so your existing projects sound exactly as before. Switching it on adds a small delay, which your recording program is told about. Nothing to do unless you want it.
- **Songsmith Analog Compressor: the Set Levels panel shows the same listening ear.** It lights up while the plugin is listening to your sound, and shows Waiting for audio until sound arrives.

## 0.5.73

- **Songsmith EQ: a new Output panel at the far right.** The level meter, the Output Trim knob and the Auto Gain button now sit together in one box headed **Output**. Output Trim is smaller and sits under the meter, with Auto Gain right beside it. Auto Gain no longer puts its number on the button; hover over it to see how much it is adding right now. Your settings, projects and presets sound exactly as before, and Auto Gain stays off unless you switch it on. This is the first plugin to get the new panel; the others follow.

## 0.5.72

- **Songsmith Analog Desk has a new look: a console channel strip.** The controls are now in four modules side by side in the order the sound goes through them (Input and Filters, Dynamics, Equaliser, Output and Meter), on navy-grey metal with steel dividers. The four Equaliser bands are colour coded (low steel, low-mid cyan, high-mid green, high red), and the Gain Reduction and Output meters sit in the Output module. Every control and every setting is still there, and your projects and presets sound exactly as they did.

- **Songsmith Bus Compressor has a new look.** The controls are now grouped on a graphite console panel at the left (Threshold and Make-up Gain on top, Attack, Release and Ratio below, with blue-capped knobs), and the meters have their own recessed zone at the right. There is also a new **Input** level meter beside the Gain Reduction and Output meters, so you can see the level going in, the gain reduction and the level coming out together. The sound, your settings, your presets and your saved projects are exactly as they were.
- **Songsmith Analog Compressor: Set Levels is quicker.** Pressing Apply now sets your Input and Output and closes the Set Levels panel straight away, with no second message to click away. While the panel is open it is slightly see-through, so you can still make out the controls behind it, and it is the only thing you can click, so nothing is changed by accident. Undo still puts your old settings back. Cancel and Escape change nothing.
- **Songsmith EQ: the mouse wheel now does what you would expect.** Scrolling over a High Pass or Low Pass changes its slope (6, 12, 18, 24, 36 or 48 dB/oct), and scrolling over a Bell or a shelf changes its gain (0.5 dB a step, 0.1 dB with Shift). It no longer changes Q, which you can still set by its own control (a Notch still uses the wheel for its width). Scrolling up makes the slope steeper or the gain higher. Nothing in your saved projects changes.
- **The Songsmith Analog Compressor's model dropdown is now four buttons:** **Black**, **Blue**, **Anniversary** and **Lindell**. The one you are using is lit, and hovering over a button tells you what that model is good for. Nothing you saved changes: your projects and presets open on the same model as before, and automation works as it did.
- **Every Songsmith plugin now shows Before and After loudness.** At the bottom of the window, to the right of Processing Load, two readings in LUFS show how loud the sound is going into the plugin and coming out of it, averaged over the last 3 seconds. It says Measuring... for the first 3 seconds after you press play, and a dash when there is no sound. Use it to hear whether a plugin is making things louder or quieter, and trust your ears. It measures that plugin only, not the whole track, and you do not need to do anything.

## 0.5.70

- **Songsmith Compressor: Analog Warmth now has four models.** A dropdown beside the Analog Warmth button lets you choose **Black**, **Blue**, **Anniversary** or **Lindell**, the same four as the Songsmith Analog Compressor, and they use the same code. Hover over the dropdown for what the one you chose is good for. It is greyed while Analog Warmth is off, and it remembers your choice. Your existing projects sound exactly as they did: they keep their original Analog Warmth, shown as **Classic**, until you choose a model. With Analog Warmth off nothing changes at all.

## 0.5.69

This version was never published as its own release. Its notes (listed under Unreleased until the next release) were missing from the notes of 0.5.70.

## 0.5.65

- **Songsmith Maximiser now has Oversampling.** A new dropdown sits right beside Dither: **Off**, **2×** or **4×**. It runs the limiter at a higher rate inside the plugin, for a cleaner top end and more accurate peaks when you limit hard. It uses more processing power and adds a little delay, which your recording software makes up for. Your existing songs are not changed: it is off unless you turn it on, and the output ceiling holds exactly as before at every setting.
- **The Oversampling dropdowns are tidier.** The Songsmith Analog Compressor's was wider than it needed to be; it is now only as wide as its text, like the Songsmith EQ's and the Maximiser's, and all three say the same thing ("Oversampling: off", "2×", "4×").

## 0.5.64

- **Songsmith plugins now tell you when a newer version is available.** If a newer build has been published, a small **Update available** link appears beside the version number at the bottom left of the plugin window. Clicking it opens the download page in your browser. Nothing is installed for you, and you do not need to do anything. It looks at most once a day, in the background, and stays quiet when you are offline or up to date.

## 0.5.63

- **New: Songsmith Analog Desk.** A mixing desk channel strip in one plugin: a high-pass and a low-pass filter, a four-band equaliser, a gate or expander and a compressor, with a little warm Drive at the front. It comes with eight starting points (such as Lead Vocal Presence and Control and Kick Drum Punch) and the same title bar, Undo and Redo, Bypass and meters as the other Songsmith plugins. It is in the Windows and Mac installers with its own tick box, ticked by default: you do not need to do anything, and nothing you already have changes. It has not been tried in a recording program yet, so please tell us what you hear.

## 0.5.62

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.61

- **Songsmith Plugin Analyzer is simpler to use.** The two copies are now called **Before Plugin** (it used to be Reference) and **After Plugin** (it used to be Result), and they find each other on their own, so there is no Session to choose: a Pair With box appears only if they cannot tell which pair they belong to. Press **Start** on the After Plugin to make your first capture. Press it again and it asks whether to **Replace** that capture or **Add to Compare**, and it never replaces one without you choosing. Both curves are then drawn together on one graph (the first as a solid blue line, the second as a dashed orange one), and under the graph each capture has a **Show** switch and a name you can change. The Measure Into, Show, Compare With and Name for the Next Measurement boxes are gone. Projects you saved before still open: a project with older measurement slots opens with its first two curves as the two captures. You do not need to do anything.

## 0.5.60

- **Songsmith Audio Analyzer now shows pictures, not just numbers.** The main one is a True Peak chart: a tall bar for each channel showing the highest peak anywhere in the song, with 0 dBTP marked and the bar turning red above it, plus a small tick for the highest stored sample (a different reading, kept apart). Integrated Loudness is now one large number, and a waveform shows how big the sound is from the start of the song to the end (it shows size, not loudness). Sample rate, bit depth, duration and channels sit in a compact row below. Hover over any chart for a plain explanation. The window is a little taller, and nothing else about how you use it has changed. You do not need to do anything.
- **Songsmith Passive EQ's preset menu is now grouped.** The starting points sit under headings (Drums, Bass, Guitar, Vocals, Mix Bus and Mastering, and Utility), so it is quicker to find the one you want, and your own saved presets come after them under their own heading. The headings are not choices, and the arrow keys skip them. Every starting point has the same name and settings as before, and your saved sessions and presets are unaffected. You do not need to do anything.

## 0.5.59

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.58

- **Songsmith Audio Analyzer now reads MP3 files.** Open, drop or choose an MP3 just as you would a WAV, AIFF or FLAC file, and it shows the file's integrated loudness (LUFS) and true peak (dBTP), sample rate, channels, length, and that it is a compressed (lossy) file with its average bitrate. An MP3 has no bit depth, so that box says "Not applicable" rather than showing a made-up number. The readings are for the sound as it plays back from the MP3, so they can differ a little from the WAV it was made from. You do not need to do anything. On a Mac, Finder can now open MP3 files with the app too.
- **Songsmith Audio Analyzer now has the Songsmith logo as its icon.** The Start menu entry, the taskbar button and the file in its folder on Windows, and the app in Applications on a Mac, show the logo instead of a plain placeholder. You do not need to do anything. If Windows still shows the old plain icon after you update, it is showing a saved picture: sign out and in again, or restart your computer, and it will refresh.

## 0.5.57

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.55

- **New: Songsmith Plugin Analyzer**, a plugin that measures what another plugin does to the sound at each frequency. Put one copy (the Reference) before the plugin you want to measure and one copy (the Result) after it, give both the same Session, start playback and press Start. The Result draws the gain in decibels from 20 Hz to 20 kHz, keeps four named measurements you can lay over each other (for example a hardware-inspired plugin against Songsmith Passive EQ), and saves them as a table with Export CSV. It tells you when it cannot measure honestly: no sound, a very quiet sound, clipping, or processing that changes over time. It measures a fixed equaliser accurately; a compressor or anything that changes with level is shown at the test level only. You do not need to do anything: it is a new, optional tick box in both suite installers (VST3 only, no standalone app), and your other plugins and presets are untouched.

## 0.5.54

- **New: Songsmith Audio Analyzer**, an app (not a plugin) for checking a finished song. Open a WAV, AIFF or FLAC file, or drop one on its window, and it shows the file's integrated loudness (LUFS), true peak (dBTP), sample rate, bit depth and encoding, and duration, with short, optional advice in plain English. If a reading cannot be made it says so, and your file is only ever read, never changed. It needs no recording software. It is a tick box in both Songsmith Plugins installers, selected by default: on Windows it adds a Start menu shortcut, and on a Mac it goes in Applications. If you do not want it, untick it. It has not yet been tried on a real Mac by a person.

## 0.5.53

- Songsmith Passive EQ has nine new starting points in its preset menu: Punchy Kick, Deep, Tight Bass and Mix Bus Air (Tone Shaper); Bass Definition, Clear Lead Vocal and More Attack on a Soft-Sounding Part (Mid Shaper); and Rumble Cleanup, Tighter Rhythm Guitar and Telephone Voice or Guitar (Frequency Filter). Choosing one switches to the right mode and sets every control, so nothing is left over from before. To make three of them possible the Frequency Filter gained a 150 Hz and a 500 Hz High-Pass and a 3 kHz Low-Pass. Sessions and presets you saved with the filter keep exactly the frequency they had, and you do not need to do anything. The new starting points are places to start from, and have not yet been listened to level-matched.

## 0.5.52

- Songsmith Passive EQ now has three modes, chosen from one EQ Mode dropdown in a row of its own under the title bar: Tone Shaper (the equaliser you already know, now the default), Mid Shaper (a low-mid boost, a mid dip and a high-mid boost, each with its own frequency) and Frequency Filter (a high-pass and a low-pass filter). Each mode shows its own controls in the blue panel, keeps its own settings, and the graph shows what you are hearing. There are four new starting points, two for each new mode. Sessions and presets saved before this open as Tone Shaper and sound exactly as they did. The window is a little taller to make room. The new modes are inspired by classic units, not measured copies.

## 0.5.51

- Songsmith Analog Compressor: the four models (Blackface, Blue Stripe, Anniversary Edition and Lindell) are now chosen from one Model dropdown, in a row of their own directly under the title bar, outside the textured panel, with Oversampling at the right of the same row, as in Songsmith EQ. The model in use is shown in the dropdown; hover over it to read what each model is good for. The Slow Attack button on Anniversary Edition now sits centred under Attack and Release with clear space above the panel's lower edge, and the panel's decorative lines no longer cross any label. The window is a little taller. The sound, saved settings, presets and automation are unchanged, and you do not need to do anything.

## 0.5.50

- Every Songsmith plugin now has the level meter at the far right of its window, laid out like the Songsmith Compressor's. Songsmith Optical Leveller, Songsmith Bus Compressor and Songsmith Analog Compressor gain a tall Gain Reduction bar and an Output meter (left and right channels, a clip warning and the highest recent level) at the far right, and their windows are a little wider to make room. Songsmith Maximiser's Gain Reduction and Output meters have moved to the far right to match, and Songsmith Passive EQ's meter now sits at the same distance from the edge. Their older needle and lamp meters stay as they were. The Output meter shows sample peak, not true peak. The Analog Compressor's bar shows no reduction while Bypass is on. The sound, presets and saved settings are unchanged, and you do not need to do anything.

## 0.5.49

- Songsmith Analog Compressor: the four models (Blackface, Blue Stripe, Anniversary Edition and Lindell) are now a row of buttons under the title bar, with Oversampling at the right of the same row, as in Songsmith EQ. The selected model is lit. The model descriptions are no longer printed on the faceplate: hover over any model button to read what it is good for. The main controls have moved down a little to make room; nothing else about them has changed.
- Songsmith Analog Compressor: Set Levels now keeps one small button under Output, and its steps (listening, the suggested Input and Output, Apply and Cancel) appear in a temporary panel above it, so the faceplate no longer changes while you use it. Press Escape or Cancel to close the panel without changing anything. The suggestions work exactly as before and nothing changes until you press Apply. You do not need to do anything.

## 0.5.48

- Songsmith Analog Compressor: each model now has a clearly different faceplate colour: Blackface is dark as before, Blue Stripe is a clear blue, Anniversary Edition is navy blue with white pinstripes, and Lindell is grey. The model name box is quieter, and the names are shorter: Blackface, Blue Stripe, Anniversary Edition and Lindell. Lindell now has a 2:1 ratio button. Your saved sessions and presets are unchanged. If you switch by hand from Lindell or Anniversary Edition to a model without 2:1, a 2:1 ratio becomes 4:1.

## 0.5.47

- Songsmith Passive EQ now has two tall output meters on the right, one for each channel, in the same rounded style as the other plugins, with a clip warning and a held peak number. They show the sound after the equaliser and Bypass, as sample peak, not true peak. The window is a little wider. The sound has not changed. A boost makes the sound louder, so if the clip warning lights, lower the boosts.

## 0.5.46

- Songsmith Optical Leveller now has a grey, textured faceplate, and the Compress and Limit buttons each show a small picture of what they do: Compress bends the level gently, Limit flattens it much more. The Limit tooltip now says plainly that it is a firmer setting of the same smooth action, not a hard ceiling. The sound has not changed, and you do not need to do anything.

## 0.5.45

- Songsmith Passive EQ now has a blue, textured faceplate with a matching soft blue accent, instead of the plain dark grey. Everything works exactly as before, and you do not need to do anything.

## 0.5.44

- The level meters in Songsmith EQ and Songsmith Compressor, and the gain reduction meter in Songsmith Compressor, now share the Maximiser's rounded, tube-like look. Gain reduction in the Compressor is shown in red. The readings and the sound have not changed, and you do not need to do anything.

## 0.5.43

- New plugin: **Songsmith Bus Compressor**, a compressor for gluing a mix or a group of sounds together, inspired by a classic mixing desk bus compressor. It has Threshold, Ratio (2:1, 4:1 or 10:1), Attack (0.1 to 30 ms), Release (0.1 to 1.2 seconds, or Auto) and Make-up Gain, and both channels always get the same gain so the stereo picture holds. A row of lamps shows how much it is turning the sound down. It comes with five starting points (Mix Bus Glue, Finished Mix Gentle Glue, Drum Bus Punch, Drum Bus Heavy Control and Vocal Group Glue) and saved custom presets. It is in both suite installers with its own tick box. It has not yet been tried in a real recording program. You do not need to do anything; tick it in the installer if you want it.

## 0.5.42

Behind-the-scenes groundwork for a future standalone Loudness Analyser: the code that reads a finished WAV, AIFF or FLAC file and explains its format and loudness in plain, optional advice. There is nothing new to see in the plugins and nothing you need to do.

## 0.5.41

- New plugin: **Songsmith Passive EQ**, a gentle tone equaliser inspired by a classic passive design. It boosts and cuts the low end around 20, 30, 60 or 100 Hz, lifts the high end with a choice of seven frequencies and an adjustable width, and turns the very top down with a choice of three frequencies. A curve shows exactly what you are doing to the sound. It comes with six starting points (Bass Weight, Kick Drum Weight, Lead Vocal Air, Acoustic Guitar Brightness, Mix Bus Polish and Soften Harsh Highs) and is in both suite installers, with its own tick box. You do not need to do anything; your sessions and other plugins are unchanged. This has not yet been checked in a DAW.

## 0.5.40

- New plugin: **Songsmith Optical Leveller**, a smooth, simple leveller inspired by a classic optical unit. It has two knobs, Peak Reduction (how much levelling) and Gain (the level afterwards), a Compress or Limit switch, and a needle meter. It reacts in about a hundredth of a second and lets go more slowly the longer and harder the sound has pushed, which keeps vocals and bass even without squashing them. It comes with four starting points (Lead Vocal Levelling, Bass Guitar Levelling, Acoustic Guitar Smoothing and Drum Room Control) and is in both suite installers, with its own tick box. You do not need to do anything; your sessions and other plugins are unchanged. This has not yet been checked in a DAW.

## 0.5.39

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.38

- Songsmith Analog Compressor has a **Model** selector in the lower right. Choose Blackface · Original Songsmith (the original Songsmith sound), Blue Stripe (firmer, faster and grittier), 1176AE · Anniversary Edition (gentler and cleaner, with its own 2:1 and Slow Attack buttons) or Lindell-inspired (softer, lets more of the first hit through). Each model has its own subtle faceplate, and a short line beside the selector says what it suits. These descriptions have not yet been checked by listening, and the Lindell-inspired reference is still to be confirmed.
- Your existing sessions and presets open on Blackface · Original Songsmith and sound exactly as before, so you do not need to do anything. The model is saved with your sessions and presets. Switching from the 1176AE to another model turns 2:1 into 4:1 and switches Slow Attack off; nothing else moves. This has not yet been checked in a DAW.

## 0.5.37

- Songsmith Analog Compressor has an optional **Set Levels** button under the Output knob. Press it, play a loud part of your song for 3 to 12 seconds, and press Finish Listening. It suggests an Input setting that gives a few decibels of gain reduction on the louder peaks, and an Output setting that keeps the level close to what you started with, using your current ratio, Attack, Release and Ignore Below. Nothing changes until you press Apply, Cancel leaves everything alone, and Undo restores your old settings. You do not need to use it, and the knobs still turn freely afterwards. If your settings or automation change while it listens, nothing is written. This has not yet been checked in a DAW.
- The Attack and Release tooltips in Songsmith Analog Compressor now say what each does and that turning the knob clockwise makes it faster. The knobs already worked that way; only the words changed.

## 0.5.36

- Songsmith Analog Compressor has a simpler layout. The Ignore Below slider now sits directly under the Input knob, and Oversampling moved to the lower right, under the meter. Both work exactly as before.
- The gentle 2:1 ratio button is gone from Songsmith Analog Compressor; Songsmith Compressor is the place for gentle compression. You do not need to do anything: a session, preset or automation that used 2:1 sounds exactly as saved, shows a small "Saved 2:1" note, and switches to your choice as soon as you press a ratio.
- The "All ratio buttons on" note is gone. The lit All button and the four lit ratios already show it. This has not yet been checked in a DAW.

## 0.5.35

- Processing Load now really starts off in Songsmith Maximiser and every other Songsmith plugin, even on a computer that had an earlier version installed. Earlier versions switched it on by default and saved that, so it could stay on after updating. That old saved value is now ignored once, so if you switched Processing Load on yourself before, switch it on again and it will be remembered from then on. Your sound, sessions and presets are unchanged. This has not yet been checked in a DAW.

## 0.5.34

- In Songsmith Compressor, switching on Analog Warmth now gives the floating control panel the fine brushed-metal texture you see on Songsmith Analog Compressor and Songsmith Maximiser, so you can see at a glance that warmth is on. Switching it off returns the plain panel straight away, and it follows the button, your host's automation and saved sessions. Only the panel changes: the sound, your settings and your sessions are unchanged, and you do not need to do anything. This has not yet been checked in a DAW.

## 0.5.33

- Factory preset names now use Title Case and say what they are for. In Songsmith Analog Compressor, "Lead vocal", "Acoustic guitar", "Bass" and "Drum room" are now "Lead Vocal Control", "Acoustic Guitar", "Bass Guitar" and "Drum Room". In Songsmith Maximiser, "Mix bus", "Demo loudness" and "Final export" are now "Mix Bus", "Demo Loudness" and "Final Export". The tom presets in Songsmith EQ now describe what they do instead of naming where the settings came from. Only the wording changed: the settings, your saved sessions and your own presets work as before, and the old names stay reserved so you cannot save a preset of your own with one. You do not need to do anything.

## 0.5.32

- Processing Load is now off in every Songsmith plugin until you switch it on, including on a new computer or after a damaged settings file. The Processing Load button is still at the bottom left; switch it on to see the CPU and delay readout. If you chose to switch it on before, that choice is kept. Your sound, sessions and presets are unchanged, and you do not need to do anything.

## 0.5.31

- Songsmith Compressor has a new look. A graph of the last 8 seconds shows the sound going in (grey) and coming out (red), so you can see the compressor working: where red falls below grey, the level is being turned down. A dashed line shows your Threshold, and a Pause button holds the picture. The controls now float in a panel you can drag out of the way; it remembers where you put it. On the right, a Gain Reduction bar shows how much the level is turned down, and the output meter is now the same one as in Songsmith EQ (each channel, a CLIP warning and the held peak). The sound, your settings and your saved sessions are unchanged, and older sessions open with the panel in its usual place. You do not need to do anything. This has not yet been checked in a DAW.

- Songsmith EQ has a new **Accurate Highs** button, beside Oversampling. With it on, bells, and shelves of up to 18 dB, set near the top of the range (around 10 kHz and above) keep the shape the curve shows, instead of getting narrower and weaker as they approach the top, and without the delay that Oversampling adds. It is on in a new Songsmith EQ. Projects you saved before open with it off, so they sound exactly as they did; turn it on if you want. The tom starting points are now 0.5 dB quieter to stay level-matched with it on.

- In Songsmith EQ, a dynamic band set to a low frequency (around 50 to 100 Hz) now follows the level smoothly instead of wobbling slightly with each cycle of the bass, so it sounds cleaner with fast attack settings. Auto Gain is also more accurate in Linear phase and with Oversampling on, because it now compares the same moment of the input and the output. Your settings are unchanged; you do not need to do anything.

- Songsmith Analog Compressor now lets go of heavy compression more like the 1176 it is inspired by: after a lot of gain reduction it recovers at a steadier pace instead of rushing back at first, so heavily compressed parts let go more gradually. Light compression sounds the same as before. The Gain Reduction meter also keeps moving while the plugin is bypassed, so you can see what it would do. Projects you saved may sound slightly different on heavily compressed parts; you do not need to do anything.

- Songsmith Maximiser limits more cleanly: when it turns a loud peak down, the change now starts and finishes gently instead of in a straight line, so hard limiting adds less distortion. It still never goes above your Output Ceiling, and the delay your recording software compensates for is unchanged. Projects you saved will sound very slightly cleaner on loud peaks; you do not need to do anything.
- A new Songsmith Maximiser now starts with **Release** at 100 ms instead of 1 ms, which keeps bass and sustained notes cleaner. Your saved projects and the starting points keep their own Release setting.

## 0.5.25

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.24

- In Songsmith Compressor, there is a new **Analog Warmth** button, off to begin with. Switch it on to add a little smooth, rounded colour to the compressed sound. Quiet and normal levels keep the same loudness; only the loudest peaks are rounded off slightly. It adds no delay and does not change how the compressor works. Your saved sessions and presets are unchanged: they open with Analog Warmth off and sound exactly as before, and loading a preset switches it off. You do not need to do anything. This has not yet been checked in a DAW or by ear on real recordings.

## 0.5.23

- **New plugin: Songsmith Compressor.** A simple, clear compressor with Threshold, Ratio, Attack, Release, Knee, Makeup Gain (or Auto Makeup) and Mix, meters for input, output and Gain Reduction, and four starting presets: Acoustic Guitar, Lead Vocal Control, Drum Bus Glue and Gentle Level Smoothing. It adds no analogue colour, so it is a good choice when you want to even out a sound without changing its tone. It appears as its own tick box in the Windows and macOS installers and as its own plugin in your recording software, next to Songsmith Analog Compressor, which is unchanged. Analog Warmth, an optional colour control, is planned and is not in this version. You do not need to do anything. This has not yet been checked in a DAW.

## 0.5.22

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.21

- In Songsmith EQ, the output meter's peak number now sits directly under the **CLIP** label instead of under the bars. A negative number (for example -0.6) is never red; it turns red only when the held peak reaches 0 dBFS or higher. Before, an earlier clip could make a negative number look red. CLIP itself works as before: it lights when a sample reaches 0 dBFS and stays lit while sound continues. The meter measures sample peak, not true peak. Nothing about the sound changes and you do not need to do anything. This has not yet been checked in a DAW.
- In Songsmith EQ, the control that turns every band's boosts and cuts up or down together now reads **Gain Scale** (it used to say only "Scale", which looked like an interface size control). Its hover help now explains it in plain words. It works exactly as before, and your saved sessions and presets sound the same, so you do not need to do anything. This has not yet been checked in a DAW.

## 0.5.19

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.18

- In Songsmith Analog Compressor, **Ignore Below** is now a horizontal slider instead of a knob with six steps. Pick any whole frequency from 20 to 500 Hz by dragging, with the arrow keys, or by clicking the number and typing it (for example 127 Hz); double-click for Off. It still only changes what the compressor listens to, so the bass you hear is unchanged. Your saved sessions and presets keep exactly the frequency they had (Off, 60, 80, 100, 150 or 200 Hz), and you do not need to do anything. If a project has automation written for the old six steps, it still works; whichever of the old and new controls moved most recently decides the frequency. This has not yet been checked in a DAW.

## 0.5.17

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.16

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.15

- Songsmith Analog Compressor no longer shows a sentence of ratio advice along the bottom of the window. Instead, hover over any ratio button (2:1, 4:1, 8:1, 12:1, 20:1 or All) for a short plain-language description of how it tends to sound and where it is useful. The tip for a loaded starting point now appears when you hover over the preset menu. These are listening suggestions, not guarantees. The sound and your saved sessions are unchanged; you do not need to do anything.

- In Songsmith Analog Compressor, the Oversampling menu has moved to the far left of the controls, under the Input knob, instead of sitting below Ignore Below. It is the same menu with the same choices, and your saved sessions and presets are unchanged. You do not need to do anything.

## 0.5.14

Behind-the-scenes improvements and fixes. There is nothing you need to do.

## 0.5.13

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
