---
id: AUTO-10
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


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the prompt is deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This specification update does not activate Matrix or demonstrate operational board integration.
