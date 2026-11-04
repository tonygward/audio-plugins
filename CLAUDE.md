# Songsmith Studio plugins

JUCE + CMake audio plugins (VST3, AU on macOS, Standalone) by **Songsmith Studio**.

| Plugin | Folder | Test executable |
|---|---|---|
| Songsmith EQ | `eq/` | `EQTests` |
| Songsmith Analog Compressor | `analog-compressor/` | `AnalogCompressorTests` |
| Songsmith Maximiser | `maximiser/` | `MaximiserTests` |

Prompts: each plugin keeps its own in `<plugin>/prompts/` (`eq/prompts/` is the EQ roadmap); work across every plugin is in `prompts/`. Prompts still to run sit directly in a `prompts/` folder, with a `README.md` status table; finished ones move to its `done/` subfolder in the PR that completes them. The overnight runner is `prompts/overnight-prompt.md`. The UI style guide is `docs/style-guide.md` (created by suite task S1).

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

### 5. Honest modelling
- Plugins inspired by hardware (1176, L1) cite their sources. Say which behaviour is published and which is a Songsmith design choice, and which test checks each.
- The editors are original designs informed by the hardware's control layout, not copies of a faceplate.

## Working rules

- Branch from an up-to-date `main`; never push commits straight to `main`, never force-push. Every change goes through a PR.
- **Overnight / unattended runs may merge straight into `main`** through their own PR: once the build has no new warnings, every test passes and CI (including macOS) is green, the agent approves the PR (a comment listing what it checked) and merges it. Blocked work stays as a draft PR with a "Blocked" section.
- Docs- and prompt-only changes (`*.md`, prompt folders, screenshots) skip CI and can merge without waiting for it.
- Each plugin builds on its own from its own folder:
  ```powershell
  cmake -S eq -B eq/build -G "Visual Studio 18 2026" -A x64 -DJUCE_DIR=C:/dev/JUCE
  cmake --build eq/build --config Release
  .\eq\build\EQTests_artefacts\Release\EQTests.exe
  ```
  The same goes for `analog-compressor` (`AnalogCompressorTests`) and `maximiser` (`MaximiserTests`). Test executables can render editor screenshots (`--screenshots <folder>` or `--screenshot <file>`, `--scale 2`).
- Never run `install.ps1`, installers or system-wide installs on the dev machines. Installer tests run in temp folders on Windows, and on CI only for macOS.
- PRs cover what changed, choices made differently from the prompt (and why), test results, known limitations, and "Not yet verified in a DAW" when that's true. UI changes include before and after screenshots: commit them on the PR branch, link them in the PR by commit SHA (`.../blob/<sha>/...png?raw=true`), and delete them again before merging, so `main` doesn't collect old screenshots.
- Keep README files up to date with user-facing changes, in the same plain language.

## Out of scope

- CLAP format.
- Code signing and macOS notarisation (unsigned installers are fine; the SmartScreen and Gatekeeper warnings are documented).
