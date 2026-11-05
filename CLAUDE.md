# Songsmith Plugins

JUCE + CMake audio plugins (VST3, AU on macOS, Standalone) by **Songsmith Studio**.

| Plugin | Folder | Test executable |
|---|---|---|
| Songsmith EQ | `eq/` | `EQTests` |
| Songsmith Analog Compressor | `analog-compressor/` | `AnalogCompressorTests` |
| Songsmith Maximiser | `maximiser/` | `MaximiserTests` |

Prompts: each plugin keeps its own in `<plugin>/prompts/` (`eq/prompts/` is the EQ roadmap); work across every plugin is in `shared/prompts/`. Prompts still to run sit directly in a `prompts/` folder, with a `README.md` status table; finished ones move to its `done/` subfolder in the PR that completes them. The overnight runner is `shared/prompts/overnight-prompt.md`. The UI style guide is `docs/style-guide.md` (created by suite task S1).

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
- Every editor is vector-drawn, scales as a whole (interface size 75–200 % or more), and stays crisp on Retina and Windows display scaling.
- New plugins link the shared UI library; they don't copy fonts or look-and-feel code.

### 4. Plain words, no abbreviations
- Every word on screen is spelled out: labels, buttons, menus, tooltips, hints, parameter display names, preset names. `SC` is "Side Chain", `GR` is "Gain Reduction", `Out` is "Output".
- Allowed short forms only: units (`Hz`, `kHz`, `dB`, `dB/oct`, `dBFS`, `dBTP`, `LUFS`, `ms`, `s`, `%`, `×`), note names (`A4`), key names (`Ctrl`, `Alt`, `Cmd`, `Shift`), `Q`, and format names (`VST3`, `AU`).
- Simple language: say what a control does. When a technical term is the right name, keep it and explain it in one short tooltip sentence.
- One word for one thing across all plugins. Title case for control names, sentence case for tooltips. British spelling.
- If a word doesn't fit, change the layout, never shorten the word. Tests enforce this; keep them passing.

- Name every factory preset for its practical use case in clear, concise language, such as **Acoustic Guitar Smoothing** or **Lead Vocal Control**. Keep hardware and model names, tutorial titles, and playful names out of the preset title. Record model selection in preset data and tutorial attribution in the preset notes. Users choose their own custom preset names.

### 5. Honest modelling
- Plugins inspired by hardware (1176, L1) cite their sources. Say which behaviour is published and which is a Songsmith design choice, and which test checks each.
- The editors are original designs informed by the hardware's control layout, not copies of a faceplate.

### 6. Tell musicians what matters

- **Every guidance message answers three questions:** what matters, why it matters, and **whether the musician needs to do anything**. Prefer reassuring, actionable, one- or two-sentence advice over engineering jargon, alarms or unnecessary changes.
- Separate **measured facts** (actual levels, gain reduction, file sample rate and bit depth) from **listening suggestions**, estimates and uncertain inferences. Do not claim that a plugin knows the source WAV bit depth merely from host audio buffers, that it can hear 'mud', or that streaming platforms have a universal LUFS pass/fail requirement.
- Offer help **at the point of use**: concise, optional, dismissible and non-disruptive; avoid constant notifications, over-automation and advice on silence or insufficient evidence. Give the next useful action only when warranted, and explain that doing nothing is often fine.
- **Recording guidance:** 44.1 kHz/16-bit is legitimate CD quality, not ruined or unsuitable for streaming. 24-bit capture offers more recording headroom and flexibility, not retroactive recovery of detail. 48 kHz is a useful optional workflow choice, not an automatic audible upgrade. Don't recommend converting 16-bit to 24-bit to improve the original recording.
- Make advice accurate, testable, accessible and consistent throughout every current and future Songsmith plugin and the separate Loudness Analyser. Use British English and the existing plain-language terminology.

### 7. Align releases with roadmap milestones
- The root `VERSION` is the single version for every plugin and installer. Roadmap phases are milestones, **not** software versions; the historical EQ v0.3–v1.0 headings must not be treated as the current suite version.
- Before implementing a planned release, document its agreed feature scope and intended version in `shared/prompts/README.md`. Do not assign versions to exploratory features. Keep existing published versions and tags unchanged, and only increase versions.
- Intermediate feature PRs can merge without changing `VERSION`. Increment `VERSION` **once at a validated release checkpoint**, not automatically per prompt. Update `CHANGELOG.md` and the roadmap together.
- Follow semantic versioning: patch for backwards-compatible corrections, minor for backwards-compatible feature releases, major only after review for meaningful compatibility or major product changes. A bumped version explicitly signals a suite release and triggers the existing release automation.

## Working rules

- Branch from an up-to-date `main`; never push commits straight to `main`, never force-push. Every change goes through a PR.
- **Overnight / unattended runs may merge straight into `main`** through their own PR: once the build has no new warnings, every test passes and CI (including macOS) is green, the agent approves the PR (a comment listing what it checked) and merges it. If the PR bumped the version, the agent then publishes the release straight away from `main`'s CI build (see `shared/prompts/overnight-prompt.md`). Blocked work stays as a draft PR with a "Blocked" section.
- Docs- and prompt-only changes (`*.md`, prompt folders, screenshots) skip CI and can merge without waiting for it.
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

## Out of scope

- CLAP format.
- Code signing and macOS notarisation (unsigned installers are fine; the SmartScreen and Gatekeeper warnings are documented).
