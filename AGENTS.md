# Songsmith Plugins

JUCE + CMake audio plugins (VST3, AU on macOS, Standalone) by **Songsmith Studio**.

| Plugin | Folder | Test executable |
|---|---|---|
| Songsmith EQ | `eq/` | `EQTests` |
| Songsmith Analog Compressor | `analog-compressor/` | `AnalogCompressorTests` |
| Songsmith Maximiser | `maximiser/` | `MaximiserTests` |

**Current execution policy:** Board order decides pickup (a Doing card is a run-now request and goes ahead of Todo unless its prompt is deferred) regardless of issue/prompt repository or ID prefix; one task per run and one active job, 60% weekly Claude stop threshold. [Full selection, release and issue lifecycle](docs/songsmith-execution-policy.md). AUTO-11's board pickup is implemented and running; AUTO-12's resume and recovery work is still partial.

Prompt tracking: use stable, scope-qualified references (for example `EQ-32`, `ACOMP-06`, `MAX-07`, `SUI-11`, `OPT-01`, `CLN-01`, `ADV-01`). The ID stays unchanged when moving a task to `done/`. Record a canonical prompt link, implementation PR when known, and release checkpoint; never confuse a prompt number with a PR number. See `shared/prompts/README.md#prompt-identity-and-traceability`.

Prompts: each plugin keeps its own in `<plugin>/prompts/` (`eq/prompts/` is the EQ roadmap); work across every plugin is in `shared/prompts/`. Prompts still to run sit directly in a `prompts/` folder, with a `README.md` status table; merged implementation prompts move to its `done/` subfolder after merge to prevent repeat coding. This archive is not proof of delivery: keep the issue open until verified beta deployment and its final completion comment, then close it and set the board Done. Update each local status table, related roadmap and root changelog at the relevant release checkpoint; preserve relative links and original IDs. Do not mark unmerged work done. The overnight runner is `shared/prompts/overnight-prompt.md`. The UI style guide is `docs/style-guide.md` (created by suite task SUI-01).

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

### 4. Controls belong beside what they affect
- **Place each UI widget near the feature, graph, value, meter, or region it controls.** Spatial proximity should make its effect obvious without requiring the user to search the interface. Do not put controls into a generic toolbar simply because it has spare space.
- Example: Songsmith EQ's **Piano** toggle belongs at the **bottom-right**, beside the piano-note strip along the bottom of the EQ graph, rather than in the top toolbar. The Processing Load toggle belongs on the left of the shared bottom diagnostics strip, directly beside its CPU/latency readout.
- Group related controls together, preserve this relationship across resize/DPI changes, and prefer a local control or contextual action over a distant global toggle. Document and justify any unavoidable exception in the relevant prompt/PR.
- **Global Bypass is the suite-wide exception with a fixed, predictable position:** top-right of every editor (as on EQ and Maximiser), using the same pressed/on red **#C9563C** currently used by Analog Compressor. Red means processing is bypassed, not enabled. Use the shared bypass colour token; do not substitute individual plugin accent colours. See SUI-13.
- Validate location as well as appearance and function in editor UI tests and before/after screenshots. See `docs/style-guide.md#control-proximity`.

### 5. Plain words, no abbreviations
- Every word on screen is spelled out: labels, buttons, menus, tooltips, hints, parameter display names, preset names. `SC` is "Side Chain", `GR` is "Gain Reduction", `Out` is "Output".
- Allowed short forms only: units (`Hz`, `kHz`, `dB`, `dB/oct`, `dBFS`, `dBTP`, `LUFS`, `ms`, `s`, `%`, `×`), note names (`A4`), key names (`Ctrl`, `Alt`, `Cmd`, `Shift`), `Q`, and format names (`VST3`, `AU`).
- Simple language: say what a control does. When a technical term is the right name, keep it and explain it in one short tooltip sentence.
- One word for one thing across all plugins. Title case for control names, sentence case for tooltips. British spelling.
- If a word doesn't fit, change the layout, never shorten the word. Tests enforce this; keep them passing.

- Name every factory preset for its practical use case in clear, concise language, such as **Acoustic Guitar Smoothing** or **Lead Vocal Control**. Keep hardware and model names, tutorial titles, and playful names out of the preset title. Record model selection in preset data and tutorial attribution in the preset notes. Users choose their own custom preset names.

### 6. Honest modelling
- Plugins inspired by hardware (1176, L1) cite their sources. Say which behaviour is published and which is a Songsmith design choice, and which test checks each.
- The editors are original designs informed by the hardware's control layout, not copies of a faceplate.

### 7. Tell musicians what matters

- **Every guidance message answers three questions:** what matters, why it matters, and **whether the musician needs to do anything**. Prefer reassuring, actionable, one- or two-sentence advice over engineering jargon, alarms or unnecessary changes.
- Separate **measured facts** (actual levels, gain reduction, file sample rate and bit depth) from **listening suggestions**, estimates and uncertain inferences. Do not claim that a plugin knows the source WAV bit depth merely from host audio buffers, that it can hear 'mud', or that streaming platforms have a universal LUFS pass/fail requirement.
- Offer help **at the point of use**: concise, optional, dismissible and non-disruptive; avoid constant notifications, over-automation and advice on silence or insufficient evidence. Give the next useful action only when warranted, and explain that doing nothing is often fine.
- **Recording guidance:** 44.1 kHz/16-bit is legitimate CD quality, not ruined or unsuitable for streaming. 24-bit capture offers more recording headroom and flexibility, not retroactive recovery of detail. 48 kHz is a useful optional workflow choice, not an automatic audible upgrade. Don't recommend converting 16-bit to 24-bit to improve the original recording.
- Make advice accurate, testable, accessible and consistent throughout every current and future Songsmith plugin and the separate Loudness Analyser. Use British English and the existing plain-language terminology.

### 8. Automatic beta build numbers
- Follow [the current execution and delivery policy](docs/songsmith-execution-policy.md). Root VERSION is shared by all plugins and installers.
- Automatically increment the third beta version component for each successful qualifying development build/merge on main, through the trusted beta pipeline (for example 0.5.2 → 0.5.3). Retain CI run ID and commit SHA as provenance; prevent recursive bumps, retries and PR-only/docs-only releases.
- Major and minor increments are deferred until a new owner decision. Do not assign feature milestones new major/minor versions or reset VERSION.
- User-visible changes add musician-facing notes under CHANGELOG.md's Unreleased section; trusted automation moves them into the incremented release.
- Automatic PR approval/merge follows passing current-head required checks. Delivery is complete only after verified beta deployment, installer downloads and the final tracking-issue comment, then issue close and board Done.

## Working rules

- Branch from an up-to-date `main`; never push commits straight to `main`, never force-push. Every change goes through a PR.
- **Matrix's own code merges automatically too:** PRs on a `matrix/core/<name>` branch that touch only Matrix controller code, tests, evidence, container config or the execution policy merge once the hosted Matrix orchestration tests pass (workflows, the merge gate, scripts, the Dockerfile and plugin source stay manual). See the [execution policy](docs/songsmith-execution-policy.md).
- **Automatically approve and merge authorised beta PRs** through the trusted workflow after all current-head required tests and hosted Windows/macOS CI pass; record approval checks in a comment if self-approval is unavailable. Blocked PRs remain pending with issue updates. Trusted workflows increment the beta build/patch number, publish installers and deploy the beta website. Keep the tracking issue open until verified delivery and its final completion comment; finally close it and set the board Done. See [execution policy](docs/songsmith-execution-policy.md).
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
- **Public GitHub releases contain exactly two files:** one `Songsmith-Plugins-X.Y.Z-win64-setup.exe` and one `Songsmith-Plugins-X.Y.Z-macOS.pkg`. Each suite installer offers one selectable tick box per plugin; do not publish separate plugin installers, ZIPs, individual plugin bundles or screenshots as GitHub release assets. Internal GitHub Actions build/test artifacts and screenshots in PRs/READMEs can remain. Always verify the two release filenames and asset count; see `TECHNICAL.md` and `shared/prompts/overnight-prompt.md`.

## Out of scope

- CLAP format.
- Code signing and macOS notarisation (unsigned installers are fine; the SmartScreen and Gatekeeper warnings are documented).
