---
id: SUI-09
status: done
execution: build-now
priority: 2
model: sonnet
effort: medium
dependencies: ["SUI-14"]
conflicts: []
---

# SUI-09 · Curated factory presets and custom preset saving

**Recommended model:** Sonnet.  
**Estimated effort / usage:** Medium (relative, not a token estimate).  
**Model escalation:** Opus only if shared preset-state migration requires a new architecture. Never switch to Opus silently: record why, scope its use to the difficult design/validation step, then return to Sonnet.  

Follow `CLAUDE.md`. Audit EQ, Analog Compressor and Maximiser before changing them. Reuse the EQ preset implementation where its format and behaviour fit. Apply this acceptance standard to every future plugin.

- Name every factory preset for its practical use case in clear, concise language, such as **Acoustic Guitar Smoothing** or **Lead Vocal Control**. Keep hardware and model names, tutorial titles, and playful names out of the preset title. Record model selection in preset data and tutorial attribution in the preset notes. Users choose their own custom preset names.
- Give each plugin a short, curated set of factory starting points with plain names describing the source or job. Audit existing presets before replacing them. Ground each factory preset in an actual Joe Gilder tutorial. Use `tonygward/music-recording/techniques/joe-gilder.md`, `eq/docs/starting-points.md` and the original tutorials. Record the source link, timestamp where available, lesson, settings and any Songsmith adaptation. Use Tony's mixes to audition the settings where available. Flag evidence gaps and leave unsupported presets out of the curated release set.
- Provide clear controls to save a named custom preset, reload it in another instance or after restart, and overwrite deliberately. Keep factory presets read-only. Store user presets in per-plugin writable locations and preserve existing preset formats and locations or provide tested migration. Keep file access outside audio processing.
- Document which settings a preset recalls. Preserve host session recall and established automation IDs. Verify save, reload, duplicate names, invalid files, factory preset protection and upgrades. Test on both platforms and audition factory presets on relevant material with level matching where practical.
