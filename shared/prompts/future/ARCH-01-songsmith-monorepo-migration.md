---
id: ARCH-01
status: deferred
execution: deferred
priority: low
model: sonnet
effort: high
dependencies: ["AUTO-11", "AUTO-12", "AUTO-10"]
conflicts: []
---

# ARCH-01 — Consolidate Songsmith into one monorepo

**Tracking issue:** [ARCH-01 #194](https://github.com/tonygward/audio-plugins/issues/194)  
**Target:** `tonygward/songsmith-plugins` (proposed, NOT yet created or renamed)  
**Status:** Future architecture epic / deferred, **not an executable Matrix job**.  
**Owner approval:** Required before any repository rename, history migration, permissions changes, Cloudflare deployment or cutover.  
**Policy:** [Songsmith execution and delivery](../../../docs/songsmith-execution-policy.md).

## Objective and user value

Simplify Songsmith development to a single repository for JUCE audio plugins, Matrix, Claude prompts and two separate websites. Preserve the existing functional delivery pipeline while removing unnecessary cross-repository coordination and maintaining strict trusted publisher/agent permissions.

## Existing and desired behaviour

Today plugin source and Matrix are in `tonygward/audio-plugins`; the beta portal source is in `tonygward/songsmith-website`. `tonygward/songsmithstudio` also contains historical tracking items; inventory its actual purpose and contents rather than assuming it should be merged. The private beta site exists at `beta.songsmith.uk`. The public commercial shop window `songsmith.uk` is **future**, not a finished deployment.

Desired one-repository layout (subject to inventory):

```text
songsmith-plugins/
  plugins/              # EQ, Analog Compressor, Maximiser, planned products only when implemented
  matrix/               # Trusted controller, worker, tests and operational docs
  prompts/              # Canonical Claude implementation prompts (including archive)
  websites/
    beta/               # Existing private tester portal
    public/             # Future public marketing/shop window
  shared/               # Shared branding/docs as appropriate
  .github/workflows/    # Scoped native CI, beta publishing and site deployments
```

Do not adopt this directory layout without measuring disruption to JUCE builds, current prompt resolver, source paths, git history, Windows/macOS runners and GitHub permissions. An incremental layout retaining stable existing paths is acceptable.

## Implementation plan (do not execute from this epic)

1. **Preflight proof:** establish a complete real Todo → claimed Doing → Claude coding → exact-head Windows/macOS CI → trusted merge → incremented beta release → verified private R2 and authenticated site downloads → completion comment → closed issue → Done run. Document deficiencies first.
2. **Inventory:** read all actual repositories, project board fields, issues, prompt indexes, redirects, workflows, branch protection/rulesets, Actions secrets/environment protections, releases, Cloudflare Pages and R2 deployment boundaries; map owner/consumer of every cross-repo webhook or dispatch.
3. **Decision and migration design:** choose whether to rename audio-plugins or create a new repository; document Git history-preserving website import strategy, issue/PR retention, canonical links, release/tag continuity and GitHub redirects. Compare reversible approaches and submit for explicit owner approval.
4. **Split into small child issues** for monorepo foundation; Matrix resolver/orchestration; native build/release workflows; beta portal migration; public website scaffold; cutover and legacy retirement. Each implementation child must have its own issue + canonical Claude prompt, scope, acceptance tests, dependencies and evidence. Avoid duplicate infrastructure tickets.
5. **Migrate safely:** prepare a staging branch/cutover plan, preserve git history and exact commit mapping; protect agent scopes, GitHub rulesets, trusted auto-merge and release/Cloudflare secrets; support staged validation and rollback. Do not publish using Claude-held credentials.
6. **Separate the websites:** `websites/beta` deploys to **beta.songsmith.uk** via Cloudflare Access, with private R2 and tester releases. `websites/public` independently deploys to **songsmith.uk** as a future public product catalogue / sales front. No automatic propagation of beta secrets/assets to public. Commercial fulfilment, licensing, payments and customer accounts require separate product/security decisions.
7. **Path-based CI:** plugin changes trigger only relevant JUCE/native builds and beta VERSION/release rules; beta content deploys only beta; public content deploys only public; Matrix/core changes invoke orchestration tests and independently gated deploy where appropriate. CI must still catch shared cross-component changes.
8. **Cutover:** prove board ordering, Doing run-now, one-task limit, claims, comments, resume/Blocked, exact-head checks, trusted merges, Windows EXE and macOS PKG installers, R2 integrity and authenticated beta downloads. Verify both domains and rollback before retiring old workflows.

## Acceptance criteria

- A documented approved architecture, inventory, migration plan, owners, dependency graph and rollback playbook exist.
- No website or git history is lost; canonical issues/prompts are resolvable, archived tasks are not recoded, links and redirects are checked.
- Github Project board remains single source of ordering across any relevant issue repo. One active Matrix task globally. Todo = one overnight pickup per London day; Doing = immediate unclaimed request or claimed work; claim/comment distinguishes them.
- The current 60% usage gate, bounded Claude loop, Blocked/resume handling and task-specific verified Done are preserved with automated tests.
- Windows/macOS CI and suites build from exact SHAs; beta third-component VERSION increments only for qualifying plugin changes, with major/minor deferred; no duplicate bump on retries.
- `beta.songsmith.uk` stays private and independently deployable; R2/Cloudflare integrity and authenticated download route tested, no public beta exposure.
- `songsmith.uk` gets a separate independent public deployment path, without claiming that a shop, payments or licensing service is live.
- Secrets/publisher credentials remain outside coding-agent access. Protected workflows and environment approvals are reviewed and least-privilege rules tested.
- Cutover and rollback are demonstrably safe; live release provenance, downloads and site content verified. Each child gets evidence comments; this epic is completed only once all accepted child outcomes are verified.

## Automated testing and risks

Test prompt-path resolution, issue/board links, project status and selection, concurrent claims, path filters, native builds, double version-bump prevention, cross-site deploy isolation, Access auth, R2 checksum, rollback and idempotent delivery. Risks: broken GitHub redirects/issues, release history, secrets exposure, accidental public beta distribution, unexpected CI coupling, and current incomplete AUTO-12 lifecycle. Mitigate with staging and reversible stages.

## Dependencies and release expectations

Depends on **verified** AUTO-11, AUTO-12 and AUTO-10 end-to-end operation, not merely merged code or closed issues. Infrastructure-only migration changes must not fabricate plugin version bumps or releases. Website-only deployments should not trigger plugin releases. Do not change existing repos, website routing, project status or Matrix execution from this future-specification prompt.

**Model:** Sonnet for inventory/specification and straightforward implementation; Opus only for difficult architecture/security decisions with a recorded rationale. Any execution has bounded budgets and owner-approved scheduling.
