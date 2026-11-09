---
id: SUI-15
status: ready
execution: build-now
priority: 1
model: sonnet
effort: medium
dependencies: []
conflicts: []
---

# SUI-15 — Standardise Songsmith product version to 0.5.0

**Source request:** [VER-01 GitHub issue](https://github.com/tonygward/songsmithstudio/issues/2). **Execute now** as a functional release/versioning task using Matrix's normal safety gates.

## Goal
Make **0.5.0** the requested Songsmith suite product version, consistent across existing plugins, installers, changelogs and release metadata. This is an intentional reset; do not imply 0.5.0 has already been built or distributed.

## Work
1. Inspect `VERSION`, JUCE/CMake definitions, all plugin metadata, shared/version-generated code, installer packaging and workflow references. Establish one authoritative product version.
2. Set intentional release version **0.5.0** without changing stable plugin IDs, parameter IDs, automation or DAW session recall. Document implications of downgrading the semantic version from existing 1.x builds and handle installer upgrade/downgrade rules deliberately.
3. Distinguish the semantic product version from per-build unique CI/run IDs and commit SHAs. Existing instruction to increase a version after every successful build is superseded: ordinary CI builds must not increment **0.5.0**.
4. Update necessary version consistency tests and release notes. Produce the release checkpoint PR under the established gates. Do not claim a published website release until verified installers and manifest exist.
5. Preserve native Windows/macOS builds, two-installer publishing discipline and existing security/approval rules; do not bypass CI or release verification.

## Acceptance criteria
- All plugins and combined installers generated from a validated release checkpoint report product version **0.5.0**.
- Repeated non-release builds keep version 0.5.0, with independently traceable build provenance.
- DAW identifiers and saved sessions remain compatible; release/downgrade handling tested.
- Version tests and full relevant CI pass; no release or R2 publication without verified matching installers.
- Report PR, build result, release status, limitations and any blocker.
