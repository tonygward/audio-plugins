# Studio One VST3 smoke test

A manual check for each build that changes the plugin's identity, parameters or state. AnalogCompressorTests covers the same ground in code. This test confirms what only a real host can show.

Don't run any system-wide install script during development. Copy the bundle by hand into the VST3 folder, or point Studio One at the build folder: **Options → Locations → VST Plug-Ins → Add** `analog-compressor\build\SongsmithAnalogCompressor_artefacts\Release\VST3`.

1. **Scan.** Restart Studio One. *Songsmith Analog Compressor* appears under **Songsmith Studio** (Effects → Dynamics), and the scan log doesn't show it as blocklisted.
2. **Mono and stereo.** Insert it on a mono audio track and on a stereo track. Both instances open, pass audio, and show their I/O as mono/mono and stereo/stereo in the routing view.
3. **Compression.** Play a loop at about -18 dBFS. Raise Input: the needle (GR mode) and the GR readout under it move, and the sound gets denser. Output changes the level but not the GR. Gain Reduction Off returns GR to 0.
4. **Controls.** Move Input and Output and hear the level change. Ratio, Attack, Release, Gain Reduction Off and Oversampling switch without clicks. All Buttons lights all four ratio buttons; pressing one ratio leaves it.
5. **Host bypass.** Use Studio One's own bypass button on the insert. It stays in sync with the plugin's Bypass parameter, and the fade is click-free.
   With Oversampling at 2x or 4x, Studio One shows 4 or 6 samples of plugin delay. A track with the plugin stays phase-aligned with a duplicate track without it, whether the plugin is bypassed or not.
6. **Automation.** Show automation for the track. Input, Output, Attack, Release, Ratio, All Buttons, Gain Reduction Off, Bypass and Oversampling are listed. Record a move on Input and play it back.
7. **Recall.** Set every control away from its default, save the song, close it and reopen it. Every control comes back as it was.
8. **Several instances.** Run four instances at once. Each keeps its own settings.
9. **Unload.** Remove the instances and close the song. Studio One doesn't crash or hang.

Record the Studio One version, the Windows version, and pass or fail for each step in the PR.

## Editor checks (step 4)

1. **Sizes.** Size menu 100 %, 150 %, 200 %: the window resizes, everything stays sharp, nothing overlaps. Reopen the song: the size comes back.
2. **Meter.** GR, +4, +8: with a -18 dBFS RMS tone and Output 0, +4 reads about 0 VU and +8 about -4 VU. The needle swings smoothly (about 300 ms), whatever the buffer size.
3. **Gestures.** Record automation while clicking ratio buttons, All, GR Off and Bypass, and while dragging and double-clicking knobs. Each click writes one clean step, and the knobs write smooth ramps.
4. **Values.** Hover isn't needed: each knob shows its value (Attack and Release show the time too).
