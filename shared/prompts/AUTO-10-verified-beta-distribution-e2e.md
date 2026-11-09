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

**GitHub tracking issue:** [#157](https://github.com/tonygward/audio-plugins/issues/157).

**Execution policy (9 November 2026):** Infrastructure/automation maintenance is **excluded from all overnight scheduled and build-now functional runs**. This story remains open for deliberate, separately authorised infrastructure work; the functional Matrix worker must never select it. Do not auto-activate any worker merely because this backlog file changes.


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
