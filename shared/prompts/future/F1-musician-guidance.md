# F1 — Musician-friendly guidance across Songsmith plugins

**Status:** Future candidate, not queued for automatic execution. **Model:** Sonnet for implementation; Opus only if a cross-plugin architecture decision requires it.

## Goal

Every Songsmith product should answer three questions in plain English: **What matters? Why does it matter? Do I need to do anything?**

Follow the standing design rule in `CLAUDE.md`, existing UI style guide, and source notes in `tonygward/music-recording`. Optional guidance must be calm, concise, dismissible, non-disruptive and truthful about what can actually be measured. Never automatically change settings. Preserve real-time safety and all saved sessions.

## Per-plugin advice

| Product | Example wording | Grounding and limits |
|---|---|---|
| Songsmith EQ | “This part of an acoustic guitar can add warmth. If it's boomy, try a small cut while listening to the whole mix.” | Use `eq/docs/starting-points.md`, Joe Gilder notes and the separate `eq/prompts/32-frequency-guide.md`. Do not claim a spectrum alone proves muddiness. |
| Analog Compressor | “This is controlling the louder peaks. If the guitar loses its punch, try slowing the attack.” | Measured gain reduction plus actual ratio/attack/release/model; compare processed and dry at equal loudness. Do not prescribe universal gain-reduction targets. |
| Maximiser | “This will make your track louder, but too much limiting may reduce punch. Try comparing at the same volume.” | Actual reduction, ceiling and validated peak/loudness readings when available. Never imply that a streaming service requires one LUFS number. |
| Future Optical Leveller / Clean Compressor | “This gently evens out your vocal. You might not need more compression.” | Actual available level and compression controls, not guessed source quality. |
| Future EQ, Reverb, Delay, Saturator, Gate or Guitar Amp | Explain the most useful musical control, what to listen for and when leaving it alone is fine. | Refer to implemented controls only; never diagnose room quality, instruments or problems from insufficient evidence. |
| Future Channel Strip | “Start with this vocal chain. Try changing one control at a time, only if you hear something that needs fixing.” | Explain processing order, enabled modules, and internal parallel effects; don't imply the strip creates a shared DAW send. |

## Implementation sequence

1. Design a small reusable help presentation that fits all plugins: “What it does / What to listen for / Try this (optional).” Let the user turn it off. No repeated notifications.
2. Implement EQ, Analog Compressor and Maximiser advice using **existing reliable measurements and actual controls**, not speculative DSP 'intelligence'. Keep the EQ overlay task separate.
3. Base source-specific language on `music-recording/techniques/joe-gilder.md`, `mixing/acoustic-moods.md` and saved FX chains. Distinguish Joe's advice, the owner's recorded settings and new Songsmith wording.
4. Add automated tests for trigger conditions, silence, insufficient playback, text clarity, saved preferences, parameter recall and non-interference with audio/automation. Validate visually in Studio One/Cakewalk and on Mac.
5. Supply before/after screenshots in the PR **and retain useful current screenshots in product READMEs**.

**Boundary:** Live plugins generally cannot know the original WAV file's sample depth from a DAW float buffer. Recording-format explanations belong in the standalone whole-file adviser (F2).

Open a draft PR once implemented; do not release merely because this prompt was written.
