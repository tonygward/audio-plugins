# Songsmith Maximiser changelog

## 0.1.0 (unreleased)

First version: a look-ahead brickwall maximiser modelled on the published behaviour of the Waves L1.

- Threshold, Out Ceiling (constant C − T gain below threshold) and Release (0.01–1000 ms).
- One shared stereo gain. 1.5 ms look-ahead, so the ceiling holds at the exact sample it protects.
- Optional true-peak limiting (8x detector) and content-dependent Auto Release.
- Optional final-stage TPDF dither to 16 or 24 bits, off by default.
- A scalable Songsmith editor: Link, I/O and attenuation meters, and three starting points.
- VST3 and Standalone on Windows; VST3, AU and Standalone on macOS. Unsigned installers for both.
