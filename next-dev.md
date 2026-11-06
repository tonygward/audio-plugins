# SONGSMITH PLUGINS — UNATTENDED v1.3.0 MILESTONE

**Repository:** `tonygward/audio-plugins`  
**Execution:** Matrix / Claude Code  
**Model:** Claude Sonnet throughout — do not use Opus.  
**Baseline:** Songsmith Plugins v1.2.0

## Mission

Implement, test, merge and release exactly these three backlog items, in order:

1. **EQ-37 — Draggable EQ band panel**
2. **ACOMP-08 — Analog Compressor editor refinements**
3. **EQ-32 — Instrument Frequency Guide**

These three tasks form the proposed **Songsmith Plugins v1.3.0 — Interface & Frequency Guide** milestone.

Do not implement unrelated backlog tasks.

## Step 1 — EQ-37

Read and follow:

`eq/prompts/37-draggable-band-panel.md`

Implement:
- Click-and-drag repositioning of the floating band controls.
- Automatic repositioning when Dynamic EQ is enabled so every control remains visible.
- Sensible positioning when resizing, switching bands or changing display scaling.
- No accidental EQ parameter changes from dragging the panel.
- Layout regression tests and real before-and-after screenshots.

Preserve existing DSP behaviour, parameter IDs and session compatibility.

## Step 2 — ACOMP-08

Read and follow:

`analog-compressor/prompts/08-1176-editor.md`

Implement the complete existing editor refinement specification, including:
- Larger, more prominent analogue gain-reduction meter.
- Improved 1176-inspired faceplate and scalable textured appearance.
- Better layout, spacing, typography and control grouping.
- Clearer ratio selection and descriptions.
- Responsive rendering at standard and high-DPI scaling.
- Space reserved for the future detector Low Cut control.

Maintain the shared Songsmith appearance, including the top-right red Bypass and bottom Processing Load footer.

Do not implement ACOMP-06, ACOMP-07 or new compressor models in this milestone.

Preserve the existing compressor sound, automation, parameter identity, session recall and preset behaviour.

Include real screenshots in the PR.

## Step 3 — EQ-32

Read and follow:

`eq/prompts/32-frequency-guide.md`

Implement the optional Instrument Frequency Guide for Songsmith EQ.

Requirements:
- Acoustic Guitar first, then the remaining instruments in the order defined by EQ-32.
- Subtle coloured frequency regions behind the EQ curve and analyser.
- Plain-English musical descriptions and helpful tooltips.
- Grounding in Joe Gilder's material and Tony's `music-recording` repository.
- Clear presentation of overlapping listening regions.
- Optional overlay, without altering audio processing.
- Correct editor-state persistence and backward compatibility.
- Responsive layout and high-DPI rendering.

Verify source material rather than inventing references.

Implement all ten defined instrument categories where possible. If a category cannot be completed safely, document it as outstanding.

## GitHub workflow

Follow:
- `CLAUDE.md`
- `shared/prompts/overnight-prompt.md`
- `shared/prompts/README.md`
- `docs/style-guide.md`
- Each individual task specification.

For every task:

1. Update from the latest `main`.
2. Use a separate feature branch and traceable PR.
3. Implement using Sonnet, with focused, efficient development.
4. Run relevant builds, automated tests and CI.
5. Review the diff and capture useful screenshots.
6. Automatically merge successful PRs.
7. Move completed prompts into their `done/` folders.
8. Update roadmap, prompt indexes and traceability references.

Do not directly push implementation commits to `main`.

Do not run system-wide installation scripts.

If a task is blocked, preserve its draft PR, record the blocker and continue with independent tasks where safe.

Do not ask for routine confirmations.

## Release v1.3.0

After all three tasks are merged and validated:

1. Verify that v1.3.0 is available and appropriate.
2. Create a release checkpoint PR updating `VERSION`, `CHANGELOG.md`, roadmap and release documentation.
3. Perform full-suite Release builds and tests.
4. Run `EQTests --bench`.
5. Verify Windows and macOS CI.
6. Merge the release checkpoint PR.
7. Publish Songsmith Plugins v1.3.0 from the validated release commit.

Publish exactly two downloadable assets:
- `Songsmith-Plugins-1.3.0-win64-setup.exe`
- `Songsmith-Plugins-1.3.0-macOS.pkg`

Each must be a combined installer with per-plugin selection checkboxes.

Do not publish individual plugin installers, ZIP archives or development artifacts.

Do not publish an incomplete or failing milestone.

Preserve v1.2.0 as the released baseline if any required task or release validation remains incomplete.

## Final report

Create the standard unattended-run GitHub issue and report:

- Status and PR link for each of EQ-37, ACOMP-08 and EQ-32.
- Frequency Guide instruments delivered.
- Windows/macOS build and CI results.
- Automated tests and EQ benchmark results.
- Screenshots and manual DAW checks remaining.
- Final `main` commit SHA.
- Release URL if v1.3.0 was published.
- Blockers and outstanding work.

**Begin immediately with EQ-37, proceed to ACOMP-08, then EQ-32, and automatically release v1.3.0 only when the complete milestone passes validation.**