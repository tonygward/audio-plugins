# EQ-34 — Plain-English Linear Phase tooltip

**Status:** Done in the EQ-34 PR · **Model:** Sonnet · **Scope:** Songsmith EQ processing-mode selector only.

## Purpose
Explain Linear phase to musicians at the point of use: what it does, when it might help, why it adds delay, and why Zero latency is usually the better choice while recording. This prompt was written retrospectively from the requirement in the [EQ prompt status table](../README.md), so the ID EQ-34 has a standalone file.

## Delivered tooltip
Shown on the Zero latency / Linear phase selector (`EQ::linearPhaseTooltip()` in `eq/Source/EQParams.h`):

> Try Linear phase when blending a filtered track with a copy of the same sound. It avoids shifting the timing of some frequencies more than others, but adds delay (about 43 to 186 ms at 44.1 or 48 kHz, longer for higher quality) and turns off Dynamic EQ and Solo. Leave it on Zero latency for recording and most mixing.

## Acceptance
- The tip answers what matters, why, and whether to do anything (guidance principle 7 in `CLAUDE.md`).
- Plain words, British English, no abbreviations beyond units.
- Test: the copy contains each element and the editor's selector carries it.
- No DSP, parameter ID or state changes.
