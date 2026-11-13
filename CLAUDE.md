# Songsmith Plugins

JUCE + CMake audio plugins (VST3, AU on macOS, Standalone) by **Songsmith Studio**.

| Plugin | Folder | Test executable |
|---|---|---|
| Songsmith EQ | `eq/` | `EQTests` |
| Songsmith Analog Compressor | `analog-compressor/` | `AnalogCompressorTests` |
| Songsmith Maximiser | `maximiser/` | `MaximiserTests` |

**How work runs:** the [execution policy](docs/songsmith-execution-policy.md) is the one place the development and delivery rules are written; this file only summarises them. In short: cards in **Todo** or **Doing** run (Doing first, Todo one a day) and cards in **Backlog** or **Blocked** wait; one task at a time, no new work at 60% weekly Claude usage; Sonnet by default; after the beta release Matrix comments with the download link, closes the issue and moves the card to Done.

**Backlog:** every task has a GitHub issue titled with a stable, scope-qualified ID (`EQ-32`, `ACOMP-06`, `MAX-07`, `SUI-11`, `COMP-01`). Search for an existing issue or prompt first. A prompt file is optional until Done: Matrix builds one from a detailed issue, and every finished task has one in `done/`. Use the [issue–prompt contract](docs/backlog-issue-prompt-contract.md) for the template. Never confuse a prompt number with a PR number.

Prompts: each plugin keeps its own in `<plugin>/prompts/` (`eq/prompts/` is the EQ roadmap); a planned plugin gets its folder first (`compressor/prompts/` for the Songsmith Compressor, `COMP-##`); work across every plugin is in `shared/prompts/`. Prompts still to run sit directly in a `prompts/` folder, with a `README.md` status table; merged implementation prompts move to its `done/` subfolder automatically after merge to prevent repeat coding (an archived prompt means merged, not delivered). Preserve relative links and original IDs, and do not mark unmerged work done. The UI style guide is `docs/style-guide.md` (created by suite task SUI-01).

## Design principles

### 1. Never break users' sessions
- Never rename or remove a parameter **ID**, and never change `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE`, bundle IDs, product names or installer `AppId`s / package identifiers. Display names may change; list every change in the PR.
- Adding parameters bumps `stateVersion`, with a migration and a test that older sessions still load.
- Install, upgrade and uninstall never delete presets or user data.

### 2. Real-time safe audio
- Nothing in `processBlock` or any engine's process code allocates, locks, logs or touches files.
- Parameter changes are smoothed (no clicks). Latency is reported to the host whenever it changes.

### 3. One family, one look
- Every plugin uses the same embedded **Inter** typeface (Regular, Medium, SemiBold) and the same type scale, from the shared UI library. No system fonts; nothing smaller than 11 px (grid labels) or 12 px (other text) at 100 %.
- Hierarchy comes from weight and size, not capitals.
- Controls use the **slightly bevelled** look from the compressor: raised buttons and knob caps lit from the top left, recessed wells for value fields, pushed-in look when on. Subtle, not chrome.
- Shared neutral colours. Each plugin has one accent colour.
- Every editor is vector-drawn, scales as a whole (interface size at least 75–200 %; see the style guide for each plugin's range), and stays crisp on Retina and Windows display scaling.
- New plugins link the shared UI library; they don't copy fonts or look-and-feel code.

### 4. Controls belong beside what they affect
- **Place each UI widget near the feature, graph, value, meter, or region it controls.** Spatial proximity should make its effect obvious without requiring the user to search the interface. Do not put controls into a generic toolbar simply because it has spare space.
- Example: Songsmith EQ's **Piano** toggle belongs at the **bottom-right**, beside the piano-note strip along the bottom of the EQ graph, rather than in the top toolbar. The Processing Load toggle belongs on the left of the shared bottom diagnostics strip, directly beside its CPU/latency readout.
- Group related controls together, preserve this relationship across resize/DPI changes, and prefer a local control or contextual action over a distant global toggle. Document and justify any unavoidable exception in the relevant prompt/PR.
- **Global Bypass is the suite-wide exception with a fixed, predictable position:** top-right of every editor (as on EQ and Maximiser), using the same pressed/on red **#C9563C** currently used by Analog Compressor. Red means processing is bypassed, not enabled. Use the shared bypass colour token; do not substitute individual plugin accent colours. See SUI-13.
- Validate location as well as appearance and function in editor UI tests and before/after screenshots. See `docs/style-guide.md#control-proximity`.

### 5. Plain words, no abbreviations
- Every word on screen is spelled out: labels, buttons, menus, tooltips, hints, parameter display names, preset names. `SC` is "Side Chain", `GR` is "Gain Reduction", `Out` is "Output".
- Allowed short forms only: units (`Hz`, `kHz`, `dB`, `dB/oct`, `dBFS`, `dBTP`, `dBu`, `LUFS`, `ms`, `s`, `%`, `×`), note names (`A4`), key names (`Ctrl`, `Alt`, `Cmd`, `Shift`), `Q`, format names (`VST3`, `AU`), and `CPU` inside the Processing Load readout only. The [style guide](docs/style-guide.md#wording) holds the full list.
- Simple language: say what a control does. When a technical term is the right name, keep it and explain it in one short tooltip sentence.
- One word for one thing across all plugins. Title case for control names, sentence case for tooltips. British spelling.
- If a word doesn't fit, change the layout, never shorten the word. Tests enforce this; keep them passing.

- Name every factory preset for its practical use case in clear, concise language, such as **Acoustic Guitar Smoothing** or **Lead Vocal Control**. Keep hardware and model names, tutorial titles, and playful names out of the preset title. Record model selection in preset data and tutorial attribution in the preset notes. Users choose their own custom preset names.

- Keep instructional research sources and named educators in research documentation, provenance fields and optional credits, not ordinary tooltips, Frequency Guide explanations or other operational help. Songsmith's on-screen guidance must stand on its own in plain language. Do not imply an educator endorses the product. Keep musically useful labels when grounded, while explaining subjective adjectives such as Plastic and Cheap in source-specific, optional listening terms. See SUI-14.

### 6. Honest modelling
- Plugins inspired by hardware (1176, L1) cite their sources. Say which behaviour is published and which is a Songsmith design choice, and which test checks each.
- The editors are original designs informed by the hardware's control layout, not copies of a faceplate.

### 7. Tell musicians what matters

- **Every guidance message answers three questions:** what matters, why it matters, and **whether the musician needs to do anything**. Prefer reassuring, actionable, one- or two-sentence advice over engineering jargon, alarms or unnecessary changes.
- Separate **measured facts** (actual levels, gain reduction, file sample rate and bit depth) from **listening suggestions**, estimates and uncertain inferences. Do not claim that a plugin knows the source WAV bit depth merely from host audio buffers, that it can hear 'mud', or that streaming platforms have a universal LUFS pass/fail requirement.
- Offer help **at the point of use**: concise, optional, dismissible and non-disruptive; avoid constant notifications, over-automation and advice on silence or insufficient evidence. Give the next useful action only when warranted, and explain that doing nothing is often fine.
- **Recording guidance:** 44.1 kHz/16-bit is legitimate CD quality, not ruined or unsuitable for streaming. 24-bit capture offers more recording headroom and flexibility, not retroactive recovery of detail. 48 kHz is a useful optional workflow choice, not an automatic audible upgrade. Don't recommend converting 16-bit to 24-bit to improve the original recording.
- Make advice accurate, testable, accessible and consistent throughout every current and future Songsmith plugin and the separate Loudness Analyser. Use British English and the existing plain-language terminology.

### 8. Automatic beta versions
- One `VERSION` file is shared by every plugin and installer. Every source merge to `main` raises its third part automatically (0.5.2 → 0.5.3); never edit it by hand. Major and minor increments wait for an owner decision.
- **Two builds per release:** the pull request build (changed plugins only) and one packaging build of everything on the version bump commit. See [pipelines](docs/pipelines.md) and [release checkpoints](docs/automatic-release-checkpoints.md).

## Working rules

- Branch from an up-to-date `main`; never push commits straight to `main`, never force-push. Every change goes through a PR. Matrix PRs, and Matrix's own code on a `matrix/core/<name>` branch, merge automatically once their checks pass; the [execution policy](docs/songsmith-execution-policy.md) says which files need a person.
- Docs- and prompt-only changes (`*.md`, prompt folders, screenshots) build nothing: their pull request gets a quick CI run that skips every build job, which is what lets Matrix and the merge gate handle them. They never trigger a release.
- Each plugin builds on its own from its own folder:
  ```powershell
  cmake -S eq -B eq/build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE
  cmake --build eq/build --config Release
  .\eq\build\EQTests_artefacts\Release\EQTests.exe
  ```
  The same goes for `analog-compressor` (`AnalogCompressorTests`) and `maximiser` (`MaximiserTests`). Test executables can render editor screenshots (`--screenshots <folder>` or `--screenshot <file>`, `--scale 2`).
- Never run `install.ps1`, installers or system-wide installs on the dev machines. Installer tests run in temp folders on Windows, and on CI only for macOS.
- PRs cover what changed, choices made differently from the prompt (and why), test results, known limitations, and "Not yet verified in a DAW" when that's true. **Keep useful screenshots**: UI changes include before-and-after screenshots committed to the PR, embedded or linked in the PR description. Retain representative, current screenshots in the appropriate plugin README or user guide, with relative Markdown image links to version-controlled assets. Do **not** delete useful screenshots before merging. Distinguish clearly between concept mockups and real screenshots; refresh screenshots when the interface changes, and avoid redundant or obsolete images.
- Keep README files up to date with user-facing changes, in the same plain language.
- **Pipelines are named by kind** (`CI`, `Gate`, `CD`, `Housekeeping`); see [docs/pipelines.md](docs/pipelines.md). When renaming a workflow, change the trigger lists and the names in `scripts/matrix/automerge.mjs` in the same pull request; a test enforces it.
- **Release notes are for musicians.** Any PR with a change users will notice adds plain-language bullets (what they see or hear, and whether they need to do anything) under `## Unreleased` at the top of `CHANGELOG.md`. The automatic beta bump moves them into the new version and the beta website shows them. No commit hashes, PR titles or prompt IDs. See `docs/automatic-release-checkpoints.md`.
- **Public GitHub releases contain exactly two files:** one `Songsmith-Plugins-X.Y.Z-win64-setup.exe` and one `Songsmith-Plugins-X.Y.Z-macOS.pkg`. Each suite installer offers one selectable tick box per plugin; do not publish separate plugin installers, ZIPs, individual plugin bundles or screenshots as GitHub release assets. Internal GitHub Actions build/test artifacts and screenshots in PRs/READMEs can remain. The publish workflow enforces this.

## Out of scope

- CLAP format.
- Code signing and macOS notarisation (unsigned installers are fine; the SmartScreen and Gatekeeper warnings are documented).
