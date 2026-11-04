# Studio One VST3 smoke test

A manual check for each build that changes the plugin's identity, parameters or state. FETTests covers the same ground in code. This test confirms what only a real host can show.

Don't run any system-wide install script during development. Copy the bundle by hand into the VST3 folder, or point Studio One at the build folder: **Options → Locations → VST Plug-Ins → Add** `fet\build\SongsmithAnalogCompressor_artefacts\Release\VST3`.

1. **Scan.** Restart Studio One. *Songsmith Analog Compressor* appears under **Songsmith Studio** (Effects → Dynamics), and the scan log doesn't show it as blocklisted.
2. **Mono and stereo.** Insert it on a mono audio track and on a stereo track. Both instances open, pass audio, and show their I/O as mono/mono and stereo/stereo in the routing view.
3. **Default transparency.** Insert it with default settings on a track playing a loop. Toggle the plugin's **Bypass** button. You hear no change in level or tone (step 1 has no compression yet).
4. **Controls.** Move Input and Output and hear the level change. Ratio, Attack, Release, All Buttons and Gain Reduction Off can be switched without clicks (they don't affect the sound until step 2).
5. **Host bypass.** Use Studio One's own bypass button on the insert. It stays in sync with the plugin's Bypass parameter, and the fade is click-free.
6. **Automation.** Show automation for the track. Input, Output, Attack, Release, Ratio, All Buttons, Gain Reduction Off and Bypass are listed. Record a move on Input and play it back.
7. **Recall.** Set every control away from its default, save the song, close it and reopen it. Every control comes back as it was.
8. **Several instances.** Run four instances at once. Each keeps its own settings.
9. **Unload.** Remove the instances and close the song. Studio One doesn't crash or hang.

Record the Studio One version, the Windows version, and pass or fail for each step in the PR.
