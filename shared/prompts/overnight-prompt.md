# Matrix overnight execution

Follow [the execution and delivery policy](../../docs/songsmith-execution-policy.md), [AGENTS.md](../../AGENTS.md), [CLAUDE.md](../../CLAUDE.md) and the selected canonical prompt.

## Select one task
The Songsmith Development board's Todo column is the manually ordered overnight queue. Select the topmost eligible card in the specific view's visible order. Doing requests immediate work, serialised behind the current job. Never choose by folder, task ID, issue number, priority or repository; never reject because of issue or prompt location/prefix. Resolve the actual implementation repo and its permitted execution path.

AUTO-11 must implement and verify this board reader before claiming board-driven execution. Until then report the existing selector's actual behaviour; do not invent board support. If visible order cannot be read, fail closed with a diagnostic, not a fallback ordering.

Run one task only (batchLimit: 1, maximumPrompts: 1), with no concurrent jobs. Require verified weekly Claude usage below 60%, including scoped meters; unknown/stale usage blocks pickup. Respect dependencies, claims and least-privilege execution permissions.

## Implement, approve and merge
- Branch from current main in an isolated worktree. Comment on the tracking issue with actual pickup, Matrix Job and prompt revision.
- Follow the canonical scope and tests. Keep useful current screenshots, update READMEs and add musician-facing Unreleased notes for visible changes.
- Native Windows/macOS builds and required tests must pass at the exact PR head. Automatically record approval and merge through the trusted workflow when eligible; no routine manual approval for authorised beta work. Do not bypass CI.
- Comment on progress, CI/PR outcomes, failures and blockers. Archive the prompt after merge and repair index/issue links; archiving prevents repeat implementation and does not mean the issue is delivered.

## Publish, verify and finish
Trusted workflows automatically increment the beta third-component build/patch counter on each qualifying successful development build/merge on main. Major/minor increments are deferred. Avoid bumps/releases for PR-only builds, retries and documentation-only changes.

Publish exactly two suite installers (Windows EXE and macOS PKG), verify private R2 bytes, then deploy the matching beta.songsmith.uk version/changelog/download links. Verify both authenticated downloads match the release.

Keep the issue open and board Doing/release-pending until delivery passes. Post the final completion comment with PR, SHA, build/version, release, deployment and download evidence; only then close the issue as completed and finally mark the board Done. A publication or comment failure blocks Done and must not trigger duplicate implementation. The coding agent never holds release/R2/website credentials.
