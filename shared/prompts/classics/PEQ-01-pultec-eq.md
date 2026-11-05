# PEQ-01 · Analog · Pultec-style EQ

Follow `CLAUDE.md` and the [suite backlog](../README.md). This is a standalone plugin for Songsmith Plugins.

Build a Pultec EQP-1A-inspired tone EQ. Research and test the interaction of simultaneous low boost and cut, the selectable frequency points, and the high boost bandwidth. Validate response curves and document any modeled colour. Start preset research with Joe Gilder’s broad tone-shaping tutorials.

## Deliver and verify

Use the shared UI and follow the [preset requirements](../S9-preset-library.md) and [editor requirements](../S10-resize-and-plain-language.md), including saving and reloading custom presets. Give the plugin stable identities. Ship through the same VST3, Audio Unit and standalone build and suite installer workflow as the existing plugins.

Write the reference and preset source notes before tuning the final settings. Verify DSP behaviour with automated checks, test saved state and automation, and record a real host check and level-matched listening notes before calling the plugin release-ready.
