# Suite prompts

## Prompt identity and traceability

Every task has a **stable, unique, scope-qualified ID**: `EQ-32` (file `eq/prompts/32-...`), `ACOMP-06` (`analog-compressor/prompts/06-...`), `MAX-07` (`maximiser/prompts/07-...`), `SUI-11` (shared suite), `OPT-01`/`CLN-01` (specific classic plugins) or `ADV-01` (future candidate). The ID **never changes** when a prompt moves to `done/`; historical numbers, names and GitHub PR numbers are not task IDs.

For each completed prompt, keep a single canonical Markdown file in its own folder's `done/`, link it from the local prompt README (or this shared README), and record the delivery PR and release checkpoint when verified. **Done means merged**, not merely code written or a plan approved. If a prompt is archived retrospectively, label it as such and do not invent PR or test evidence. Update the release changelog with references to the IDs delivered at each checkpoint. Links must be relative to their source Markdown file. Do not mark an unmerged task done.

| Scope prefix | Canonical location | Completed location |
|---|---|---|
| `EQ-` | `eq/prompts/` | `eq/prompts/done/` |
| `ACOMP-` | `analog-compressor/prompts/` | `analog-compressor/prompts/done/` |
| `MAX-` | `maximiser/prompts/` | `maximiser/prompts/done/` |
| `SUI-` | `shared/prompts/` | `shared/prompts/done/` |
| `OPT-` / `PEQ-` / `BUS-` / `CLN-` | `shared/prompts/classics/` | `shared/prompts/classics/done/` when delivered |
| `ADV-` / `REC-` | `shared/prompts/future/` | `shared/prompts/future/done/` only after implemented |

## Backlog at a glance

**Release packaging rule:** publish **one Songsmith Plugins installer for Windows** and **one for macOS**, each with one tick box per plugin. The public release must have **exactly two assets**; individual plugin installer/ZIP artifacts stay internal to CI. See [release instructions](../../TECHNICAL.md#releasing).

**Three product goals:** **6–12 standalone plugins**, a **Channel Strip reusing proven processing engines**, and a **separate whole-track Loudness Analyser**. The foundation is Songsmith EQ, Analog Compressor and Maximiser. [Full product direction](../../eq/ROADMAP.md).

| Priority | Area | Next outcome |
|---|---|---|
| 1 | **Suite UI and validation** | Implement [SUI-12 shared footer](done/SUI-12-processing-load-footer.md) (done, #81), [SUI-13 standard Bypass position and red](done/SUI-13-consistent-bypass.md) (done, #82), [EQ-37 draggable band panel and Dynamic visibility](../../eq/prompts/37-draggable-band-panel.md), **[EQ-36 Dynamic disabled tooltip](../../eq/prompts/done/36-dynamic-disabled-tooltip.md)** (done, #78) and **[ACOMP-08 Analog Compressor editor refinements](../../analog-compressor/prompts/08-1176-editor.md) early**, then verify installation, session recall, reported latency, processing load, Studio One and Cakewalk workflows. |
| 2 | **EQ** | Piano button moved to bottom-right ([EQ-35](../../eq/prompts/done/35-piano-footer.md)), filter shape icons, plain-English Linear Phase explanation and **instrument-specific frequency guide** ([EQ 32](../../eq/prompts/32-frequency-guide.md), grounded in Joe Gilder and the recording notes). |
| 3 | **Analog Compressor DSP** | Oversampling default ([ACOMP-06](../../analog-compressor/prompts/06-oversampling-off-by-default.md)) and detector Low Cut ([ACOMP-07](../../analog-compressor/prompts/07-detector-low-cut.md)); editor polish is already in Priority 1. |
| 4 | **Maximiser** | True-peak checks on exports, suitable defaults, LUFS measurement. |
| 5 | **FET models** | Blue Stripe, Anniversary, Lindell-inspired, with clearly named acoustic-guitar presets. |
| 6 | **Compressor workflow** | Optional guided **Set Levels** adjustment. |
| 7 | **Optical Leveller** | LA-2A-inspired standalone plugin. |
| 8 | **Clean Compressor** | Transparent digital compressor. |
| 9 | **More analogue classics** | Pultec-style EQ and SSL-style Bus Compressor. |
| 10 | **Standalone Loudness Analyser** | Whole-file LUFS, true peak, sample rate/bit depth and **streaming guidance**, without false pass/fail claims. |
| 11 | **Channel Strip** | Combine validated processors in musician-ready chains, including the proposed internal Abbey Road-style reverb path. |

**Additional unscheduled plugin candidates:** Reverb, Delay, De-Esser, Saturator, Gate / Expander, Transient Shaper and Guitar Amp / Cabinet. They are not assigned implementation prompts or release dates yet.

**Future, not queued:** [ADV-01 musician-friendly guidance for each plugin](future/ADV-01-musician-guidance.md) and [REC-01 whole-file Recording and Streaming Advisor](future/REC-01-file-recording-advisor.md). The rule for both is **“What matters? Why does it matter? Do I need to do anything?”**

**Release discipline:** shared suite version **1.1.0** is the current documented baseline; future versions are set against agreed, validated roadmap milestones (not individual prompts). Preserve useful screenshots in PRs and READMEs, and distinguish mockups from shipped features. Details and execution order follow below.

Prompts for work that spans **every** Songsmith Studio plugin: Songsmith EQ (`eq/`), Songsmith Analog Compressor (`analog-compressor/`) and Songsmith Maximiser (`maximiser/`), plus any plugin added later. Each file can be handed to an agent as it is. They use the same working rules as the [EQ roadmap prompts](../../eq/prompts/README.md): branch from `main`, keep parameter IDs and plugin codes stable, keep the audio thread real-time safe, build and test every plugin, don't install system-wide, and finish with a **draft PR**. Finished prompts move to `done/` once merged.

| Prompt | What it does |
|---|---|
| ✅ [SUI-01 shared look: fonts and type](done/SUI-01-shared-fonts-and-type.md) | One shared UI library and one copy of the Inter font for all plugins, one type scale, and a written style guide |
| ✅ [SUI-02 bevelled controls everywhere](done/SUI-02-bevelled-controls.md) | The slightly bevelled, raised look from the compressor, applied to buttons, knobs, menus and toggles in every plugin |
| ✅ [SUI-03 plain words everywhere](done/SUI-03-plain-words-everywhere.md) | No abbreviations and plain language in every plugin, enforced by one shared test |
| ✅ [SUI-04 Windows suite installer](done/SUI-04-windows-suite-installer.md) | One Windows installer for all plugins, with a tick box per plugin |
| ✅ [SUI-05 macOS suite installer](done/SUI-05-macos-suite-installer.md) | One macOS `.pkg` for all plugins, with a choice per plugin and format |
| ✅ [SUI-06 one installer entry per plugin](done/SUI-06-one-entry-per-plugin.md) | One tick box per plugin in the Windows installers and the macOS suite installer: no separate standalone app or format choices |
| ✅ [SUI-07 one version number](done/SUI-07-one-version-number.md) | One version for every plugin, installer and release, from a root `VERSION` file, starting at 1.1.0 |
| ✅ [SUI-08 Songsmith Plugins branding](done/SUI-08-songsmith-plugins-brand.md) | Consistent suite naming without changing plugin identities |
| ✅ [SUI-11 latency and processing load](done/SUI-11-latency-and-processing-load.md) | Diagnostics strip in each editor; shipped in 1.1.0 |
| ✅ [SUI-12 left-side Processing Load](done/SUI-12-processing-load-footer.md) | Done in #81: toggle at the far left of every plugin; when on, CPU and latency show immediately to its right; EQ Piano moved via EQ-35. Release checkpoint pending |
| ✅ [SUI-13 standard Bypass position and red](done/SUI-13-consistent-bypass.md) | Done in #82: global Bypass top-right on every plugin, engaged in the shared #C9563C red (`Songsmith::UI::Colours::bypass`). Release checkpoint pending |

## Future candidate — Recording and Mixing Advisor

- **Not queued for unattended execution:** [ADV-01 musician-friendly advice across plugins](future/ADV-01-musician-guidance.md) — clear, optional recommendations grounded in actual measurements and listening guidance (**Sonnet; Opus only if architecture requires it**).
- **Not queued for unattended execution:** [REC-01 whole-file recording and streaming advisor](future/REC-01-file-recording-advisor.md) — standalone file metadata, LUFS and true-peak advice without false pass/fail claims (**Sonnet; Opus for measurement validation if needed**).
- Both follow the standing rule in `CLAUDE.md`; promote to active prompts only after existing work is prioritised.

## Songsmith Plugins backlog

**Model guide:** Start with Sonnet for routine implementation. Where a task says **Opus for design**, use Opus for the design and validation plan, then switch to Sonnet for implementation once those decisions are written down.

Finish the existing suite work and develop the classic standalone plugins on two tracks. **The Channel Strip follows the standalone classics.**

| Track | Sequence |
|---|---|
| Analog emulation | Existing 1176-inspired FET compressor: **[ACOMP-08 editor polish](../../analog-compressor/prompts/08-1176-editor.md) first (Sonnet; Priority 1)**, then [ACOMP-06 oversampling default](../../analog-compressor/prompts/06-oversampling-off-by-default.md) **(Sonnet)** and [ACOMP-07 detector Low Cut](../../analog-compressor/prompts/07-detector-low-cut.md) **(Sonnet)**. Next [OPT-01 Optical Leveller](classics/OPT-01-optical-leveller.md) **(Opus for design, Sonnet for implementation)**, [PEQ-01 Pultec-style EQ](classics/PEQ-01-pultec-eq.md) **(Opus for design, Sonnet for implementation)**, then [BUS-01 SSL-style compressor](classics/BUS-01-ssl.md) **(Opus for design, Sonnet for implementation)**. |
| Digital | Existing EQ: [filter icons](../../eq/prompts/done/33-filter-shape-icons.md) **(Sonnet)**. Existing Maximiser: [defaults and loudness meter](../../maximiser/prompts/README.md). Next [CLN-01 Clean Compressor](classics/CLN-01-clean-compressor.md) **(Opus for design, Sonnet for implementation)**. |

The [SUI-08 suite branding](done/SUI-08-songsmith-plugins-brand.md) is done. Apply [SUI-09 curated and custom presets](SUI-09-preset-library.md) **(Sonnet)** and [SUI-10 resizable editors and control help](SUI-10-resize-and-plain-language.md) **(Sonnet)** across both tracks. Before starting Channel Strip work, release and validate the standalone classics in both tracks, including their presets and host recall.

### Version alignment with delivery milestones

**Current suite baseline: 1.1.0.** The EQ roadmap's earlier v0.3–v1.0 labels describe historical EQ work, not the shared suite version.

Agree and record a **release checkpoint with a named, testable feature scope** before choosing the next semantic version. Intermediate prompt PRs need not bump `VERSION`; increment it once the whole checkpoint passes validation, and update `CHANGELOG.md`. Future research and proposed ideas have no target version until they are scheduled.

| Release checkpoint | Version | Scope / decision |
|---|---|---|
| Current shared suite baseline | 1.1.0 | Existing shipped suite work (see changelog) |
| Next release milestone | To agree | Choose and validate a coherent subset of priorities below |
| Later milestones | Unscheduled | Further classic plugins, Channel Strip, Loudness Analyser and musical guidance |

### Delivery priority

| Priority | Batch | Order and release checks |
|---|---|---|
| 1 | Suite release and **early EQ/compressor UI** | Run SUI-12, **SUI-13 top-right red Bypass**, **EQ-37 draggable band panel and Dynamic visibility**, **EQ-36 disabled Dynamic tooltip**, and **ACOMP-08 editor polish now**, ahead of ACOMP-06/07: enlarge the analogue meter, refine the scalable textured faceplate, improve layout/ratio wording and prepare the future Low Cut position without adding its DSP control yet. Coordinate with SUI-10; preserve screenshots and verify responsiveness, meter behaviour and old session recall. Also validate existing SUI-06–SUI-08/SUI-11 packaging, versioning, reported latency and load, upgrade/scanning and Studio One/Cakewalk recall. |
| 2 | EQ improvements | EQ 33 filter shape icons, EQ 34 Linear Phase tooltip and EQ 32 instrument frequency guide. The guide can ship independently of the smaller fixes. |
| 3 | Blackface DSP and workflow | **ACOMP-06 → ACOMP-07**: oversampling default followed by detector Low Cut. Integrate Low Cut into the space reserved by already-prioritised **ACOMP-08**; do not redo editor polish. Apply SUI-09 preset rules and ensure SUI-10 wording/resize consistency. |
| 4 | Digital polish | Verify the Maximiser true-peak ceiling on exported files, then run Maximiser 06 → 07 for the default and LUFS meter. |
| 5 | FET compressor models | Deliver compressor 10 in checkpoints: model selector and texture switching with Blue Stripe first, then Anniversary Edition (2:1 and Slow Attack), then Lindell and Acoustic Guitar Smoothing once the exact Lindell reference is confirmed. Preserve legacy Blackface recall and validate each model independently. |
| 6 | Set Levels | Deliver compressor 09 against the selected model. Validate on Blackface and each new model as it becomes available. |
| 7 | Optical Leveller | Deliver OPT-01 as the first new standalone analog classic. Validate vocal and bass presets and host recall. |
| 8 | Clean Compressor | Deliver CLN-01 as its own digital release. It can proceed independently of Phase 7. |
| 9 | Complete the classics | PEQ-01 Pultec-style EQ → BUS-01 SSL-style Bus Compressor. Finish SUI-09 and SUI-10 across the suite as each plugin is released. |
| 10 | Standalone Loudness Analyser | Can start once the Maximiser measurement engine is validated and reusable. |
| 11 | Channel Strip | Start after the standalone classics are released and validated in both tracks. Include the internal Abbey Road reverb bus. |

The priorities describe release order. Independent work can proceed alongside the current batch. Use the model recommendations on each prompt.

### ✅ SUI-11 · Latency and Processing Load

**Done — [archived prompt SUI-11](done/SUI-11-latency-and-processing-load.md)**. Implemented behaviour is described in the [style guide](../../docs/style-guide.md#latency-and-processing-load-strip) and included in the [1.1.0 changelog](../../CHANGELOG.md#110). The original requirements have been preserved in the archive, rather than embedded here as an active instruction.

## Overnight runs merge into `main`

When these prompts are run unattended (for example overnight with [`overnight-prompt.md`](overnight-prompt.md)), each task can go **straight into `main`**: the agent opens a PR, waits for CI, approves it and merges it itself once the build, tests and CI all pass. Nothing is ever pushed to `main` without a PR. Blocked tasks stay as draft PRs for Tony. (GitHub doesn't let an account formally approve its own PR, so the agent records its approval as a PR comment listing what it checked, then merges.)

## Run in waves, merging between waves

| Wave | Prompts | Notes |
|---|---|---|
| 1 | SUI-01 shared look · SUI-04 Windows suite installer | Independent: SUI-01 is editor code and CMake, SUI-04 is installer and CI. |
| 2 | SUI-02 bevelled controls · SUI-03 plain words · SUI-05 macOS suite installer | SUI-02 and SUI-03 both build on SUI-01's shared library and both touch every editor; expect small conflicts in the editors and the style guide. SUI-05 reuses SUI-04's CI job and naming. |

## Conflict hotspots

- Each plugin's `CMakeLists.txt` (SUI-01 links the shared library; SUI-04/SUI-05 only read versions from it).
- `.github/workflows/build.yml`: SUI-04 and SUI-05 both add the suite installer job.
- `docs/style-guide.md`: SUI-01 creates it, SUI-02 and SUI-03 add sections.
- The three look-and-feel files (`eq/Source/EQLookAndFeel.h`, `analog-compressor/Source/FETLookAndFeel.h`, `maximiser/Source/MaxLookAndFeel.h`).
