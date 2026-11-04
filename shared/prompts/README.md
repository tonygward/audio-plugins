# Suite prompts

Prompts for work that spans **every** Songsmith Studio plugin: Songsmith EQ (`eq/`), Songsmith Analog Compressor (`analog-compressor/`) and Songsmith Maximiser (`maximiser/`), plus any plugin added later. Each file can be handed to an agent as it is. They use the same working rules as the [EQ roadmap prompts](../../eq/prompts/README.md): branch from `main`, keep parameter IDs and plugin codes stable, keep the audio thread real-time safe, build and test every plugin, don't install system-wide, and finish with a **draft PR**. Finished prompts move to `done/` once merged.

| Prompt | What it does |
|---|---|
| ✅ [S1 shared look: fonts and type](done/S1-shared-fonts-and-type.md) | One shared UI library and one copy of the Inter font for all plugins, one type scale, and a written style guide |
| ✅ [S2 bevelled controls everywhere](done/S2-bevelled-controls.md) | The slightly bevelled, raised look from the compressor, applied to buttons, knobs, menus and toggles in every plugin |
| ✅ [S3 plain words everywhere](done/S3-plain-words-everywhere.md) | No abbreviations and plain language in every plugin, enforced by one shared test |
| ✅ [S4 Windows suite installer](done/S4-windows-suite-installer.md) | One Windows installer for all plugins, with a tick box per plugin |
| ✅ [S5 macOS suite installer](done/S5-macos-suite-installer.md) | One macOS `.pkg` for all plugins, with a choice per plugin and format |
| [S7 one version number](S7-one-version-number.md) | One version for every plugin, installer and release, from a root `VERSION` file, starting at 1.1.0 |

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
