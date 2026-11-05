# SONGSMITH PLUGINS — UNATTENDED UI REFINEMENT & RELEASE

## Mission

You are Claude Code, running on **Matrix**, developing the Songsmith Plugins GitHub repository:

`tonygward/audio-plugins`

**Model: Claude Sonnet throughout. Do not switch to Opus.**

Your objective is to implement six lightweight UI refinements, validate them, merge the completed changes into `main`, and automatically publish one new Songsmith Plugins release containing the Windows and macOS suite installers.

**This is an authorised unattended development and release run. Do not stop for routine approvals or confirmations. Begin implementation immediately.**

Follow the existing repository instructions in:

- `CLAUDE.md`
- `shared/prompts/overnight-prompt.md`
- `shared/prompts/README.md`
- `TECHNICAL.md`
- `docs/style-guide.md`
- The individual implementation prompts

The repository's existing development, testing, CI, merge and release safeguards remain mandatory.

---

# 1. Scope — Six UI improvements

Complete the following tasks.

| Order | Reference | Description |
|---|---|---|
| 1 | EQ-36 | Context-sensitive disabled Dynamic EQ tooltip |
| 2 | EQ-34 | Plain-English Linear Phase tooltip |
| 3 | EQ-33 | Graphical filter-shape icons |
| 4 | SUI-12 | Shared Processing Load footer |
| 5 | EQ-35 | Piano toggle bottom-right |
| 6 | SUI-13 | Consistent top-right Bypass button and red active state |

These six tasks form **one release milestone**.

Do not implement EQ-37, EQ-32, ACOMP-08 or later roadmap features in this run.

## 1.1 EQ-36 — Dynamic EQ tooltip

Read:

`eq/prompts/36-dynamic-disabled-tooltip.md`

When the Dynamic button is disabled, its tooltip must explain why.

Dynamic EQ supports:

- Bell
- Low Shelf
- High Shelf

It does not support:

- Low Cut
- High Cut
- Notch

Example disabled tooltip:

"Dynamic EQ isn't available on Low Cut filters. Use Bell, Low Shelf or High Shelf to enable dynamic processing."

Also handle Linear Phase mode and situations where both restrictions apply.

**Critical:** Ensure the tooltip actually appears when the JUCE button is disabled.

Do not change filter processing, eligibility, parameter IDs or existing project compatibility.

## 1.2 EQ-34 — Linear Phase tooltip

Locate the existing requirement in `eq/prompts/README.md`.

Create the missing standalone implementation prompt if necessary, retaining the stable reference **EQ-34**.

Implement a concise, musician-friendly tooltip explaining:

- What Linear Phase does.
- When it might be useful.
- Why it adds latency.
- Why normal EQ is usually preferable during recording.

Suggested wording:

"Try this when blending a filtered track with a copy of the same sound. It avoids shifting the timing of some frequencies more than others, but adds delay. Leave it off for recording and most mixing."

Ensure the tooltip is discoverable and fits the interface.

## 1.3 EQ-33 — Filter-shape icons

Read:

`eq/prompts/33-filter-shape-icons.md`

Add clear graphical representations of the frequency response beside each filter type.

Use scalable vector drawing, consistent with the existing shared Songsmith UI library.

Preserve existing selection and filter behaviour.

Verify the icons at normal and high-DPI scaling.

## 1.4 SUI-12 — Processing Load footer

Read:

`shared/prompts/SUI-12-processing-load-footer.md`

Across every Songsmith plugin, position the **Processing Load** toggle at the far left of the bottom status strip.

When enabled, display the CPU and latency figures directly to its right.

Example:

`Processing Load   CPU 0.35%, Latency 0.0ms (0 samples)`

The values must come from actual measurements, not hardcoded examples.

When Processing Load is disabled, hide the diagnostic text.

Preserve existing latency measurement, performance safeguards and saved diagnostic preferences.

Ensure all three existing plugins use the same shared implementation.

## 1.5 EQ-35 — Piano toggle placement

Read:

`eq/prompts/35-piano-footer.md`

Move the Piano toggle to the **bottom-right** of the EQ editor.

The Piano button controls the note strip along the bottom of the frequency graph. It therefore belongs beside the feature it affects.

Required footer layout:

`[Processing Load] [CPU / Latency when enabled] ........ [Piano]`

The Piano toggle must remain right-aligned regardless of whether Processing Load is active.

Retain existing Piano functionality and state behaviour.

**Implement EQ-35 together with SUI-12 where practical**, since both affect the footer.

## 1.6 SUI-13 — Consistent Bypass

Read:

`shared/prompts/SUI-13-consistent-bypass.md`

Across Songsmith EQ, Analog Compressor and Maximiser:

- Position global Bypass at the **top-right**.
- Use the same active colour: **#C9563C**.
- Red means bypass is engaged.
- Normal processing uses the neutral, unpressed appearance.
- Preserve existing automation and project state.

EQ and Maximiser already provide the preferred placement. Move Analog Compressor's Bypass to match.

Create or reuse a single shared bypass-colour token instead of duplicating colour definitions.

Do not confuse global Bypass with local band toggles or Gain Reduction Off.

---

# 2. Standing UI design principles

These requirements apply to all changes.

**Controls belong near the things they affect.**

Use logical grouping and spatial proximity.

Examples:

- Piano toggle: beside the piano-note strip.
- Processing Load: beside CPU/latency readouts.
- Filter controls: within the relevant EQ section.
- Global Bypass: consistently top-right across the suite.

Use the shared Inter typography, subtle bevelled controls and existing Songsmith colour scheme.

Maintain responsive layout and accessible controls.

Preserve correct rendering at 100%, 150%, 200% and supported Retina/HiDPI scaling.

Do not introduce clipped text or overlapping controls.

Use plain English and British spelling.

---

# 3. Credit-efficient execution

This run should minimise unnecessary Claude usage while maintaining quality.

**Mandatory model: Sonnet.**

Do not switch to Opus for design, implementation, code review or troubleshooting.

Prefer the simplest maintainable implementation.

Reuse existing JUCE controls, drawing functions, layout helpers and shared UI utilities.

Avoid:

- Unrelated refactoring.
- Dependency upgrades.
- DSP redesign.
- New frameworks.
- Unnecessary research.
- Repeated full-suite builds after every minor edit.
- Repeated attempts at purely cosmetic perfection.

Use focused builds and tests during development, followed by full validation at the release checkpoint.

Read only relevant sections of large files where possible.

Keep diagnostic and compiler output concise, but retain enough information to diagnose failures.

Do not sacrifice correctness, accessibility, compatibility or release validation to save credits.

---

# 4. GitHub execution workflow

First inspect the repository state.

1. Verify the active repository and GitHub authentication.
2. Fetch the latest remote state.
3. Check for existing uncommitted changes.
4. Review active branches, PRs and recently merged work.
5. Determine whether any of the six tasks have already been completed or are actively being implemented.
6. Avoid duplicate implementation.
7. Read the applicable prompt and repository instructions.

**Never overwrite or discard existing uncommitted work.**

Do not directly push implementation commits to `main`.

Use small, traceable feature branches and PRs against the latest `main`.

Group SUI-12 and EQ-35 together where appropriate, while retaining both task references.

Other tasks may use separate PRs.

Each PR must include:

- Prompt references.
- Changes implemented.
- Relevant tests.
- Before-and-after screenshots.
- Compatibility considerations.
- Any decisions or limitations.
- DAW verification status.

Screenshots are encouraged and should be preserved where useful.

Do not delete useful screenshots merely to tidy the repository.

## Automatic merging

Follow `shared/prompts/overnight-prompt.md`.

You are authorised to merge successful PRs without requesting approval between tasks, subject to the existing repository rules.

Before merging:

- Build passes without new warnings.
- Relevant unit and integration tests pass.
- UI changes have been reviewed against their requirements.
- Required screenshots are captured.
- GitHub CI is green, including macOS where required.
- There are no unresolved failures or merge conflicts.
- Existing compatibility rules are respected.

Record the checks in the PR comment using the repository's established unattended-run convention.

Merge successful PRs and delete their feature branches.

If a task cannot be completed safely, leave a draft PR explaining the blocker and continue with independent tasks.

Never mark blocked or unmerged work as done.

---

# 5. Testing and validation

Perform focused validation throughout the implementation.

## EQ

Verify:

- Dynamic tooltip for each supported and unsupported filter type.
- Dynamic tooltip behaviour when Linear Phase is selected.
- Disabled tooltip accessibility.
- Linear Phase explanatory tooltip.
- Filter-shape icon accuracy.
- Piano toggle placement and operation.
- Bypass state and active colour.
- Existing parameter and preset recall.

## Analog Compressor

Verify:

- Bypass is top-right.
- Active Bypass is #C9563C.
- Bypass continues to operate correctly.
- No interference with the analogue meter or existing controls.
- Processing Load footer works.
- Existing sessions and presets remain compatible.

Do not perform the larger ACOMP-08 editor redesign.

## Maximiser

Verify:

- Bypass remains top-right.
- Active Bypass is #C9563C.
- Processing Load footer works.
- Existing controls, meters and processing remain unchanged.

## Full suite

After merging the implementation PRs, perform a clean Release build and run the complete available test suites.

Check:

- Windows compilation.
- macOS CI compilation.
- Existing automated tests.
- Parameter identity and state compatibility.
- CPU load measurement.
- Latency reporting.
- UI scaling.
- No new build warnings.
- No unintended processing changes.

Run the EQ benchmark according to the repository instructions.

Do not claim Studio One, Cakewalk or Logic testing has passed unless it has actually been performed.

---

# 6. Documentation and traceability

Maintain the established stable IDs:

- EQ-33
- EQ-34
- EQ-35
- EQ-36
- SUI-12
- SUI-13

Update:

- `shared/prompts/README.md`
- `eq/prompts/README.md`
- Relevant plugin READMEs
- `docs/style-guide.md` where appropriate
- `CHANGELOG.md` at the release checkpoint

Move implemented prompts to their corresponding `done/` folders as part of the successful PR workflow.

Preserve their original IDs.

Record PR numbers and canonical relative links.

Do not change task IDs to match PR numbers.

Do not confuse completed implementation with a released milestone.

---

# 7. Automatic release milestone

**Once all six tasks have successfully merged and passed the required validation, automatically create one new Songsmith Plugins suite release.**

This release is explicitly authorised.

Do not stop for another confirmation.

## Versioning

Read:

- Root `VERSION`
- Existing release tags
- Existing GitHub releases
- `CHANGELOG.md`
- Versioning instructions in `CLAUDE.md`

Determine the next appropriate semantic version.

Because these are backwards-compatible UI refinements, prefer a **minor version increment**, provided that aligns with the repository's release history.

Do not reuse an existing release version or overwrite existing tags.

Record the agreed scope of this UI milestone in the roadmap and changelog.

Increment `VERSION` **once only**, after all six features are validated.

Create a dedicated release checkpoint PR.

Run the complete required validation and CI checks, then merge it according to the established unattended workflow.

## Release build

After the release checkpoint PR merges:

1. Identify the exact merge commit on `main`.
2. Wait for that commit's GitHub Actions build.
3. Verify all required CI jobs pass.
4. Download the Windows suite-installer artifact.
5. Download the macOS suite-installer artifact.
6. Confirm both packages correspond to the exact release commit and version.

Do not publish packages assembled from unrelated builds.

## Release assets

The public GitHub release must contain **exactly two files**.

**Windows:**

`Songsmith-Plugins-X.Y.Z-win64-setup.exe`

**macOS:**

`Songsmith-Plugins-X.Y.Z-macOS.pkg`

Replace `X.Y.Z` with the validated release version.

Each installer must contain the Songsmith plugin suite with one selectable checkbox per plugin.

Do not publish:

- Individual plugin installers.
- ZIP archives.
- Standalone VST3/AU packages.
- Build logs.
- Screenshots.
- Debug symbols.
- Other CI artifacts.

Keep useful screenshots in PRs and READMEs, not public release assets.

## Release notes

Publish plain-language release notes covering:

- Improved and consistent Bypass controls.
- Simplified Processing Load display.
- Piano toggle placement.
- Clearer Dynamic EQ tooltips.
- Linear Phase guidance.
- Filter-shape icons.
- Compatibility and known limitations.
- Windows/macOS installation guidance.
- Unsigned installer warnings where appropriate.

## Release safety

**Do not publish an incomplete, failed or unverified milestone.**

If one or more of the six required tasks remains blocked:

- Do not bump the suite version.
- Do not create a GitHub release for this milestone.
- Leave completed work merged where appropriate.
- Document blockers and failed validation.
- Report precisely what remains outstanding.

If CI fails, attempt reasonable targeted repairs within the scope of this run.

Never force-merge failing PRs or publish red builds.

---

# 8. Completion report

When finished, create the standard overnight-run GitHub issue as described in `shared/prompts/overnight-prompt.md`.

Include task results, PRs, test results, relevant benchmarks, release information and any manual validation still required.

Then provide a concise final summary:

| Reference | Task | Result | PR |
|---|---|---|---|
| EQ-36 | Dynamic tooltip | | |
| EQ-34 | Linear Phase tooltip | | |
| EQ-33 | Filter icons | | |
| SUI-12 | Processing Load footer | | |
| EQ-35 | Piano toggle | | |
| SUI-13 | Bypass consistency | | |

Also provide:

- Final commit SHA on `main`.
- Suite version.
- Windows build status.
- macOS build status.
- Test results.
- GitHub release URL, if published.
- Any blocked tasks.
- Remaining manual DAW checks.
- Estimated Claude usage if available from reliable usage information.

Do not invent token counts or usage estimates.

---

# 9. Final operating instructions

This is an **unattended implementation, validation, merge and release task**.

Use Sonnet throughout.

Work sequentially where necessary to avoid merge conflicts.

Do not ask for approval between routine development steps.

Do not stop after planning or opening PRs.

Complete all six tasks, subject to validation and available permissions.

Automatically merge successful changes.

Automatically publish the release when the entire milestone passes all required checks.

Follow existing repository safety rules and do not modify unrelated backlog items.

**Start now: inspect the repository, identify outstanding work, and begin implementing EQ-36.**