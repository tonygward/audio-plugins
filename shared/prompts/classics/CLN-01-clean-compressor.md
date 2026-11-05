# CLN-01 · Digital · Clean Compressor

**Recommended model:** Opus for DSP design/validation; Sonnet for implementation.  
**Estimated effort / usage:** High (relative, not a token estimate).  
**Model escalation:** Use Sonnet for implementation, UI and test work. Never switch to Opus silently: record why, scope its use to the difficult design/validation step, then return to Sonnet.  

Follow `CLAUDE.md` and the [suite backlog](../README.md). This is a standalone plugin for Songsmith Plugins.

Build a clean digital compressor with threshold, ratio, attack, release and output gain. Define the knee and detector behaviour, verify static curves and timing, and test bypass, latency and automation. Start preset research with Joe Gilder’s compression tutorials and audition on vocals, bass and drums.

## Deliver and verify

Use the shared UI and follow the [preset requirements](../SUI-09-preset-library.md) and [editor requirements](../SUI-10-resize-and-plain-language.md), including saving and reloading custom presets. Give the plugin stable identities. Ship through the same VST3, Audio Unit and standalone build and suite installer workflow as the existing plugins.

Write the reference and preset source notes before tuning the final settings. Verify DSP behaviour with automated checks, test saved state and automation, and record a real host check and level-matched listening notes before calling the plugin release-ready.
