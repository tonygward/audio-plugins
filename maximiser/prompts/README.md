# Songsmith Maximiser prompts

There is one self-contained prompt per build step. They were run in order by the overnight run of 2026-11-03 with [`done/overnight-prompt.md`](done/overnight-prompt.md): each step got its own PR into the integration branch, which was merged into `main` in #48. Finished prompts are in [`done/`](done/). The code is in [`maximiser/`](../). (The prompts still say `maximizer/` and `MaximizerTests`: the folder and test executable were renamed afterwards.)

| Prompt | Status | PR |
|---|---|---|
| [01 plug-in shell](done/01-maximiser-shell.md) | Done | #35 |
| [02 brickwall limiter](done/02-brickwall-limiter.md) | Done | #38 |
| [03 true peak and auto release](done/03-true-peak-and-auto-release.md) | Done | #39 |
| [04 editor and dither](done/04-editor-and-dither.md) | Done | #44 |
| [05 validation and packaging](done/05-validation-and-packaging.md) | Done | #46 |
| [06 true peak by default](06-true-peak-by-default.md) | To do · **Sonnet** | |
| [07 loudness meter (LUFS)](07-loudness-meter.md) | To do · **Opus for measurement design and validation plan, Sonnet for implementation** | |

Still to do by hand before a release: the host checks in [`docs/host-checks.md`](../docs/host-checks.md).
