# Suite prompts

Prompts for work that spans **every** Songsmith Studio plugin: Songsmith EQ (`eq/`), Songsmith Analog Compressor (`analog-compressor/`) and Songsmith Maximiser (`maximiser/`), plus any plugin added later. Each file can be handed to an agent as it is. They use the same working rules as the [EQ roadmap prompts](../../eq/prompts/README.md): branch from `main`, keep parameter IDs and plugin codes stable, keep the audio thread real-time safe, build and test every plugin, don't install system-wide, and finish with a **draft PR**. Finished prompts move to `done/` once merged.

| Prompt | What it does |
|---|---|
| ✅ [S1 shared look: fonts and type](done/S1-shared-fonts-and-type.md) | One shared UI library and one copy of the Inter font for all plugins, one type scale, and a written style guide |
| ✅ [S2 bevelled controls everywhere](done/S2-bevelled-controls.md) | The slightly bevelled, raised look from the compressor, applied to buttons, knobs, menus and toggles in every plugin |
| ✅ [S3 plain words everywhere](done/S3-plain-words-everywhere.md) | No abbreviations and plain language in every plugin, enforced by one shared test |
| ✅ [S4 Windows suite installer](done/S4-windows-suite-installer.md) | One Windows installer for all plugins, with a tick box per plugin |
| ✅ [S5 macOS suite installer](done/S5-macos-suite-installer.md) | One macOS `.pkg` for all plugins, with a choice per plugin and format |
| [S6 one installer entry per plugin](S6-one-entry-per-plugin.md) | One tick box per plugin in every installer: no separate standalone app or format choices |
| [S7 one version number](S7-one-version-number.md) | One version for every plugin, installer and release, from a root `VERSION` file, starting at 1.1.0 |

## Songsmith Plugins backlog

Finish the existing suite work and develop the classic standalone plugins on two tracks. **The Channel Strip follows the standalone classics.**

| Track | Sequence |
|---|---|
| Analog emulation | Existing 1176-inspired FET compressor: [Low Cut](../../analog-compressor/prompts/07-detector-low-cut.md) then [editor polish](../../analog-compressor/prompts/08-1176-editor.md). Next [A1 Optical Leveller](classics/A1-optical-leveller.md), [A2 Pultec-style EQ](classics/A2-pultec-eq.md), then [A3 SSL-style compressor](classics/A3-ssl.md). |
| Digital | Existing EQ: [filter icons](../../eq/prompts/33-filter-shape-icons.md). Existing Maximiser: [defaults and loudness meter](../../maximiser/prompts/README.md). Next [D1 Clean Compressor](classics/D1-clean-compressor.md). |

Apply [S8 suite branding](S8-songsmith-plugins-brand.md), [S9 curated and custom presets](S9-preset-library.md), and [S10 resizable editors and control help](S10-resize-and-plain-language.md) across both tracks. Before starting Channel Strip work, release and validate the standalone classics in both tracks, including their presets and host recall.

## Overnight runs merge into `main`

When these prompts are run unattended (for example overnight with [`overnight-prompt.md`](overnight-prompt.md)), each task can go **straight into `main`**: the agent opens a PR, waits for CI, approves it and merges it itself once the build, tests and CI all pass. Nothing is ever pushed to `main` without a PR. Blocked tasks stay as draft PRs for Tony. (GitHub doesn't let an account formally approve its own PR, so the agent records its approval as a PR comment listing what it checked, then merges.)

## Run in waves, merging between waves

| Wave | Prompts | Notes |
|---|---|---|
| 1 | S1 shared look · S4 Windows suite installer | Independent: S1 is editor code and CMake, S4 is installer and CI. |
| 2 | S2 bevelled controls · S3 plain words · S5 macOS suite installer | S2 and S3 both build on S1's shared library and both touch every editor; expect small conflicts in the editors and the style guide. S5 reuses S4's CI job and naming. |

## Conflict hotspots

- Each plugin's `CMakeLists.txt` (S1 links the shared library; S4/S5 only read versions from it).
- `.github/workflows/build.yml`: S4 and S5 both add the suite installer job.
- `docs/style-guide.md`: S1 creates it, S2 and S3 add sections.
- The three look-and-feel files (`eq/Source/EQLookAndFeel.h`, `analog-compressor/Source/FETLookAndFeel.h`, `maximiser/Source/MaxLookAndFeel.h`).
