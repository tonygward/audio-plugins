# Songsmith Analog Compressor prompts

There is one self-contained prompt per build step. They were run in order by the overnight run of 2026-11-03: each step branched from the integration branch `compressor/overnight-2026-11-03`, got a draft PR, and was merged once CI was green. Finished prompts are in [`done/`](done/). The code is in [`fet/`](../../fet/).

| Prompt | Status | PR |
|---|---|---|
| [01 plugin shell](done/01-plugin-shell.md) | Done | #36 |
| [02 feedback compressor](done/02-feedback-compressor.md) | Done | #41 |
| [03 analogue colour](done/03-analogue-colour.md) | Done | #43 |
| [04 All Buttons and editor](done/04-all-buttons-and-editor.md) | Done | #47 |
| [05 presets, validation and packaging](done/05-presets-validation-packaging.md) | Done | #50 |

Still to do by hand before a release: the Studio One and Logic checks in [`fet/docs/daw-checks.md`](../../fet/docs/daw-checks.md), and the listening checklists in `fet/docs`.
