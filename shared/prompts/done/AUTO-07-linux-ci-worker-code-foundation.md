---
id: AUTO-07
priority: high
model: sonnet
effort: medium
dependencies: ["AUTO-06"]
conflicts: []
---

# AUTO-07 — Linux worker and CI-handoff code foundation

**Milestone:** Completed **source implementation** only, not deployment or end-to-end operation. **Split from:** [AUTO-05](../AUTO-05-linux-agent-github-actions.md) and [AUTO-01](../AUTO-01-matrix-autonomous-development.md). **Evidence:** [PR #139](https://github.com/tonygward/audio-plugins/pull/139), merged 9 November 2026; earlier CI auto-merge foundation [PR #135](https://github.com/tonygward/audio-plugins/pull/135).

## Delivered scope
- `tools/matrix/linux-worker.mjs` and `tools/matrix/tests/linux-worker.test.mjs` committed together in PR #139.
- Container-specific preflight checks: refuse root, Docker socket, effective process capabilities and unconfigured HTTPS egress proxy; guard against release/website/R2 credentials in the environment.
- Linux agent worktree/Claude/PR flow with bounded use, durable task records, hosted GitHub Windows/macOS CI checks at the exact head revision and limited repairs; worker does not merge or publish itself.
- GitHub Actions `matrix-auto-merge.yml` exists, with independent trusted merge policy.

## Acceptance evidence and boundaries
The shipped source and tests are the acceptance for **implementation presence**, not evidence of a running/isolated Linux container. The exact CI build/repair/merge sequence, shutdown/recovery, credentials and beta distribution remain unverified here. These are separate backlog stories AUTO-08–AUTO-10.

**Archived as a completed, narrowly scoped code milestone.** Parent AUTO-05 stays open until the container is operational and verified.
