---
id: ACOMP-10
priority: medium
effort: high
dependencies: ["ACOMP-09"]
conflicts: []
---

# ACOMP-10: Switchable FET compressor models

Issue: [#265](https://github.com/tonygward/audio-plugins/issues/265). Follow [CLAUDE.md](../../CLAUDE.md); the rules for running it are in the [execution policy](../../docs/songsmith-execution-policy.md). Matrix runs Sonnet; the Opus suggestion below is advice for a person doing the DSP design by hand.

**Recommended model:** Opus for DSP design and validation planning; Sonnet for implementation. **Estimated effort / usage:** High. **Escalation:** Use Opus only for model-specific DSP design and measurement plans, document decisions, then Sonnet for implementation and tests. Build on the 1176 editor and guided calibration prompts.

Keep all variants in one **Songsmith Analog Compressor** plugin. Add a clearly labelled **Model** selector that switches both the DSP and a distinct, subtle faceplate texture. Keep the existing model for session compatibility and use the named references below for the new models. Confirm Tony's exact Lindell reference before choosing a circuit or voicing. Use original faceplates and cite the references. Document which differences are supported by measurements or published material and which are Songsmith voicing choices.

Define meaningful differences in gain reduction, timing and harmonic colour. Keep shared controls familiar and give each model a plain-language description. Save and recall the model in every factory preset, every custom preset and every host project. Use stable model IDs. Older presets without a model must load the original Rev D Blackface sound. Verify factory preset recall, custom preset round-trips and legacy preset migration. Older sessions must open with the existing model and sound. Retain IDs and verify safe switching during playback, output levels, bypass, latency and calibration for each model. Use repeatable measurements and level-matched listening on real material to verify the variants.

### Model choices and tooltip copy

Show the selected model’s use case beside the selector and make it available as a tooltip. Include it in the choice list where the host UI permits. Use these as starting copy and verify each description by level-matched listening.

| Model | Suggested tooltip |
|---|---|
| Rev A · Blue Stripe | “Try this for a gritty, upfront vocal or to make room mics sound more aggressive.” |
| Rev D · Blackface (Original Songsmith) | “Start here to tame vocal, bass or guitar peaks while keeping the sound fairly clean.” |
| 1176AE · Anniversary Edition | “Try 2:1 for gentler control, or Slow Attack to let more of the first hit through.” |
| Lindell-inspired | Confirm the exact reference first. If it is the 7X-500, start by auditioning it on Tony’s acoustic guitar recordings. Suggested tooltip: “Try this on acoustic guitar to even out strong strums while keeping the pick attack.” Check that the finished model supports this use on strummed and picked parts. |

Support the AE’s 2:1 ratio and Slow Attack as explicit model-specific choices. Verify which ratio and attack settings each reference supports and define safe, predictable mapping when the user switches models. Keep the existing Rev D-inspired sound as the Blackface choice and preserve it for older sessions. Use Rev E as a comparison reference only.

### Reference material

- [Universal Audio 1176 Classic Limiter Collection tips and model notes](https://www.uaudio.com/blogs/ua/1176-collection-tips)
- [Universal Audio 1176LN hardware manual and specifications](https://media.uaudio.com/assetlibrary/1/1/1176ln_manual.pdf)
- [Universal Audio 2-1176 hardware manual and revision history](https://media.uaudio.com/assetlibrary/2/-/2-1176_manual.pdf)
- [Plugin Alliance Lindell 7X-500 product reference](https://www.plugin-alliance.com/products/7x-500)

Use the UA collection notes for model-specific features and the hardware manuals for the specifications of the units they cover. Keep published facts, measured behaviour and Songsmith design choices separate in the implementation notes.

### Required factory preset · Acoustic Guitar Smoothing

Add **Acoustic Guitar Smoothing**, based on Joe Gilder’s Butter Compression tutorial, with the **Lindell-inspired model** explicitly selected in the preset data. Locate the original tutorial and record its link, timestamp and demonstrated settings in the preset source notes. Map the settings to Songsmith controls, documenting any adaptations and setting Input and Output against a representative acoustic guitar recording where needed. Keep the preset pending if the tutorial or its settings cannot be verified. Audition it on Tony’s strummed and picked acoustic guitar recordings. Test that loading the preset switches to Lindell from another model, recalls the intended controls, and retains that model if saved as a custom preset.
