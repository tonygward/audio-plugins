**Tracking issue:** https://github.com/tonygward/audio-plugins/issues/170

# PEQ-01 · Analog · Pultec-style EQ

**Recommended model:** Opus for DSP design/validation; Sonnet for implementation.  
**Estimated effort / usage:** High (relative, not a token estimate).  
**Model escalation:** Use Sonnet after filter behaviour and reference tests are specified. Never switch to Opus silently: record why, scope its use to the difficult design/validation step, then return to Sonnet.  

Follow `CLAUDE.md` and the [suite backlog](../README.md). This is a standalone plugin for Songsmith Plugins.

Build a Pultec EQP-1A-inspired tone EQ. Research and test the interaction of simultaneous low boost and cut, the selectable frequency points, and the high boost bandwidth. Validate response curves and document any modeled colour. Start preset research with Joe Gilder’s broad tone-shaping tutorials.

## Deliver and verify

Use the shared UI and follow the [preset requirements](../done/SUI-09-preset-library.md) and [editor requirements](../done/SUI-10-resize-and-plain-language.md), including saving and reloading custom presets. Give the plugin stable identities. Ship through the same VST3, Audio Unit and standalone build and suite installer workflow as the existing plugins.

Write the reference and preset source notes before tuning the final settings. Verify DSP behaviour with automated checks, test saved state and automation, and record a real host check and level-matched listening notes before calling the plugin release-ready.

**Current execution and delivery:** Follow [the owner policy](../../../docs/songsmith-execution-policy.md). Board order decides selection regardless of issue/prompt location or prefix, with one task and a 60% weekly usage stop threshold. Use the actual implementation repository's scoped execution path. Automatically approve/merge after current-head required checks; verify beta deployment and downloads, post the final issue comment, then close and finally mark board Done. Major/minor increments are deferred; beta build/patch increments are automatic.
