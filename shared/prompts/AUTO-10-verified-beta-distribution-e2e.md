---
id: AUTO-10
status: planned
execution: deferred
priority: medium
model: sonnet
effort: medium
dependencies: ["AUTO-09"]
conflicts: []
---

**Current policy:** [Board ordering, one task, 60% usage gate, automatic beta builds and verified issue completion](../../docs/songsmith-execution-policy.md). This policy supersedes historical batches, folder/ID ordering and merge-only completion in older design notes. Automatic approval/merge requires exact-head green CI; issue Done requires beta deployment/download verification and a final issue comment.

**GitHub tracking issue:** [#157](https://github.com/tonygward/audio-plugins/issues/157).

**Execution policy:** Follow [current selection and delivery rules](../../docs/songsmith-execution-policy.md). Board order decides selection regardless of prefix or issue/prompt location. This deferred task needs its appropriate scoped infrastructure execution path; board placement does not grant new privileges. One task only; 60% weekly usage stop threshold. Documentation does not activate a worker.


# AUTO-10 — Verify automatic beta release and private portal distribution end to end

**Split from:** [AUTO-01](AUTO-01-matrix-autonomous-development.md). **Status:** Open: implementation workflows exist; production evidence required.

## Goal
Prove a successful *real development merge* from the Linux-worker path automatically produces exactly **one Windows EXE suite installer and one macOS PKG suite installer**, advances a correct beta patch version and updates the authenticated Songsmith website after private R2 integrity checks — without a separate manual release approval.

## Work
1. Verify `auto-beta-version.yml`, `auto-publish-release.yml`, `distribute-beta-r2.yml` and private `songsmith-website` deployment integration with the current repo. Review permission boundaries: Linux agent has zero release, R2 or website credentials.
2. Use a real successful qualifying development commit; record the version/tag and exact green hosted Windows/macOS build SHAs. Require fresh full validation, release policy checks and only two installer release assets with SHA-256 provenance.
3. Confirm the installers reach the private R2 bucket, match release checksums and are retrievable only via the authenticated beta portal. Confirm site's displayed version/changelog matches the newly published release.
4. Test repeat events/idempotence, partially missing assets, CI failure and R2 failure. No premature website version update, double bump, duplicate release or public bucket exposure.
5. Record GitHub Actions run URLs, release URL, checked R2 object versions, website manifest/Cloudflare deployment reference and rollback steps (without leaking tokens or private download URLs).

## Acceptance
One reproducible successful **automatic** build → beta version → GitHub Release → verified R2 → beta website path, plus documented failure/retry behaviour. Code/config existing is not sufficient to close this delivery verification milestone. Normal test and security gates apply; no manual release approval gate should be introduced.

**The trusted GitHub workflows publish; the Linux agent never does.**
