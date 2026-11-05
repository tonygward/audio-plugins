# BUS-01 · Analog · SSL-style compressor

**Recommended model:** Opus for DSP design/validation; Sonnet for implementation.  
**Estimated effort / usage:** High (relative, not a token estimate).  
**Model escalation:** Use Sonnet for implementation, UI and test work. Never switch to Opus silently: record why, scope its use to the difficult design/validation step, then return to Sonnet.  

Follow `CLAUDE.md` and the [suite backlog](../README.md). This is a standalone plugin for Songsmith Plugins.

Build an SSL-style stereo bus compressor as the first standalone SSL emulation. Identify the hardware reference and document the detector, stereo linking and release behaviour. Verify timing and gain reduction with repeatable measurements and listen on mix and drum buses. Start preset research with Joe Gilder’s bus-compression tutorials.

## Deliver and verify

Use the shared UI and follow the [preset requirements](../SUI-09-preset-library.md) and [editor requirements](../SUI-10-resize-and-plain-language.md), including saving and reloading custom presets. Give the plugin stable identities. Ship through the same VST3, Audio Unit and standalone build and suite installer workflow as the existing plugins.

Write the reference and preset source notes before tuning the final settings. Verify DSP behaviour with automated checks, test saved state and automation, and record a real host check and level-matched listening notes before calling the plugin release-ready.
