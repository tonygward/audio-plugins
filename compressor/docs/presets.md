# Songsmith Compressor presets

The four factory presets are in `Source/CompPresets.h`. They are starting points for a first listen, named for what they are used for, not copies of anyone's settings.

| Preset | Threshold | Ratio | Attack | Release | Knee | Where the numbers come from |
|---|---|---|---|---|---|---|
| Acoustic Guitar | -30 dB | 1.3:1 | 30 ms | 50 ms | 12 dB | Follows the idea in Joe Gilder's 2018 video on gentle, steady compression: a low threshold, a ratio around 1.2 to 1.5, about 30 ms attack and 50 ms release ([video](https://www.youtube.com/watch?v=d7vHaydnTuQ&t=190s), [notes](https://github.com/tonygward/music-recording/blob/main/techniques/joe-gilder.md)). The threshold and knee are Songsmith choices, because the right threshold depends on the recording. |
| Lead Vocal Control | -24 dB | 3:1 | 8 ms | 120 ms | 9 dB | Songsmith starting point. |
| Drum Bus Glue | -18 dB | 2:1 | 30 ms | 100 ms | 6 dB | Songsmith starting point. |
| Gentle Level Smoothing | -20 dB | 1.5:1 | 20 ms | 150 ms | 12 dB | Songsmith starting point. |

All four use Auto Makeup and Mix at 100%.

- **Names:** the Acoustic Guitar preset keeps that plain name. The tutorial's own wording and the presenter's name stay in this note and are not shown in the plugin.
- **Not endorsed:** nothing here says Joe Gilder or any other educator endorses the plugin.
- **Not verified by ear:** no listening check on real guitar, vocal or drum recordings has been done, so the settings other than the published ratio and timing are unverified. They are meant to be adjusted by ear, and each preset's tooltip says so.
- **Preset research** for further presets continues under ACOMP-13 (#233).

Tests: `Factory presets` in `Tests/CompressorTests.cpp` check the names are unique and plain, the values are in range, Acoustic Guitar has the published ratio and timing, and loading a preset is one Undo step and leaves Bypass alone.
