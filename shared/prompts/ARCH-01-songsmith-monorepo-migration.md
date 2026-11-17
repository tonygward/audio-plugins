---
id: ARCH-01
priority: low
model: sonnet
effort: high
dependencies: ["AUTO-11", "AUTO-12", "AUTO-10"]
conflicts: []
---

# ARCH-01 — Consolidate Songsmith into one monorepo

**Tracking issue:** [ARCH-01 #194](https://github.com/tonygward/audio-plugins/issues/194)  
**Target:** `tonygward/songsmith-plugins` (proposed, NOT yet created or renamed)  
**Owner approval:** Required before any repository rename, history migration, permissions changes, Cloudflare deployment or cutover.  
**Policy:** [Songsmith execution and delivery](../../docs/songsmith-execution-policy.md).

## Objective and user value

Simplify Songsmith development to a single repository for JUCE audio plugins, Matrix, Claude prompts and two separate websites. Preserve the existing functional delivery pipeline while removing unnecessary cross-repository coordination and maintaining strict trusted publisher/agent permissions.

## Existing and desired behaviour

Today plugin source and the remote development agent are in `tonygward/audio-plugins`; the beta portal source is in `tonygward/songsmith-website`. `tonygward/songsmithstudio` is explicitly **out of migration scope**. Only `audio-plugins` and `songsmith-website` are source repositories for this epic. The private beta site exists at `beta.songsmith.uk`. The public commercial shop window `songsmith.uk` is **future**, not a finished deployment.

Desired one-repository layout (refine only after a dependency audit):

```text
songsmith-plugins/                    # Rename existing audio-plugins repository
  plugins/
    eq/
    analog-compressor/
    maximiser/
    compressor/
    shared/                           # Shared JUCE UI, common source, resources and tests
    ...                               # All other actual plugin products
  agent-runner/                        # Host-neutral remote task pickup/orchestration, not 'matrix'
  prompts/                             # Canonical task instructions and archive
  websites/
    beta/                              # Existing beta portal; existing Cloudflare Pages deploy
    public/                            # Future public site; separate Cloudflare deploy when built
  docs/                                # Architecture, policies and non-plugin documentation
  .github/workflows/                   # Protected scoped CI/CD
```

**Repository scope and rename:** Rename `tonygward/audio-plugins` **in place** to `tonygward/songsmith-plugins`; import `tonygward/songsmith-website` with its history, rather than creating an unrelated empty destination repo. Only these **two** source repositories are in scope. Preserve the original audio repository's issues, PRs, tags, releases and audit history. Audit/update remote URLs, GitHub Project references, submodules, documentation, Actions, webhooks, branch protections, bots and Cloudflare integrations; do not assume GitHub redirects alone fix webhooks, API endpoints or CI workflows.

**Agent name:** Replace the future top-level `matrix/` folder proposal with `agent-runner/`, expressing a **remote autonomous work-pickup/orchestration service independent of the machine hosting it**. Existing `tools/matrix` and `tools/matrix-linux` are implementation paths today; inventory all scripts, imports, CLI names, config, tests, Docker/WSLc entrypoints, service names and docs before changing paths. Preserve current scheduling, Claude execution, task claims, auto-merge and privilege boundaries. Historical references can retain the term Matrix when required for traceability.

**Shared folder:** Confirmed `shared/CMakeLists.txt` builds `songsmith_ui` consumed by plugins; move plugin-specific shared code, Resources, Tests and their relevant docs to `plugins/shared/`, fixing include paths, CMake targets and build/test references. **Do not** move `shared/prompts/` into plugins: move canonical prompts/archive to `prompts/`. Move architecture/operational docs to `docs/` and assess any other shared files individually. Include tests that all products still link shared code.

**Cloudflare non-regression (hard blocker):** The **existing** beta Pages workflow/integration must remain operational after moving website source and changing the repository. Retain `beta.songsmith.uk` and its Access protection, route/configuration, environment bindings and credentials in trusted contexts; explicitly adapt Cloudflare Pages repository binding, root/build/output directories, branch/build hooks, GitHub Actions, path filters and deployment authorization. A subsequent `websites/public/` deployment to `songsmith.uk` is a **future separate integration**: scaffold boundaries but do not claim the public site or integration exists, and never publish beta assets publicly. Both sites must be deployable independently.

**R2 non-regression (hard blocker):** Existing release-to-Cloudflare-R2 pipeline must still publish the actual Windows EXE and macOS PKG installer artifacts, preserve release provenance, versioning, object paths, checksums, bucket credentials, restricted downloads and beta-site links. Audit source-path changes, GitHub release events, Actions dependencies, artifact naming and the publish workflow. Verify an end-to-end release/download and a separate beta content-only Pages update post-migration. Neither website-only changes nor the new public scaffold may accidentally trigger plugin releases.

**Migration timing:** Planning/specification changes only now. Owner expects to consider implementation around the coming weekend; **this is not approval to run**. Keep ARCH-01 in Backlog/Deferred until explicit authorization, and split implementation into reviewable child issues/prompts with staged verification and rollback. Do not rename any repo, move source, touch Cloudflare config or trigger deployments from this planning prompt.

## Implementation plan (do not execute from this epic)

1. **Preflight proof:** establish a complete real Todo → claimed Doing → Claude coding → exact-head Windows/macOS CI → trusted merge → incremented beta release → verified private R2 and authenticated site downloads → completion comment → closed issue → Done run. Document deficiencies first.
2. **Inventory:** read all actual repositories, project board fields, issues, prompt indexes, redirects, workflows, branch protection/rulesets, Actions secrets/environment protections, releases, Cloudflare Pages and R2 deployment boundaries; map owner/consumer of every cross-repo webhook or dispatch.
3. **Decision and migration design:** implement the approved in-place audio-plugins → songsmith-plugins rename (document ordering, history preservation and rollback); document Git history-preserving website import strategy, issue/PR retention, canonical links, release/tag continuity and GitHub redirects. Compare reversible approaches and submit for explicit owner approval.
4. **Split into small child issues** for monorepo foundation; agent-runner resolver/orchestration; native build/release workflows; beta portal migration; public website scaffold; cutover and legacy retirement. Each implementation child must have its own issue + canonical Claude prompt, scope, acceptance tests, dependencies and evidence. Avoid duplicate infrastructure tickets.
5. **Migrate safely:** prepare a staging branch/cutover plan, preserve git history and exact commit mapping; protect agent scopes, GitHub rulesets, trusted auto-merge and release/Cloudflare secrets; support staged validation and rollback. Do not publish using Claude-held credentials.
6. **Separate the websites:** `websites/beta` deploys to **beta.songsmith.uk** via Cloudflare Access, with private R2 and tester releases. `websites/public` independently deploys to **songsmith.uk** as a future public product catalogue / sales front. No automatic propagation of beta secrets/assets to public. Commercial fulfilment, licensing, payments and customer accounts require separate product/security decisions.
7. **Path-based CI:** plugin changes trigger only relevant JUCE/native builds and beta VERSION/release rules; beta content deploys only beta; public content deploys only public; agent-runner/core changes invoke orchestration tests and independently gated deploy where appropriate. CI must still catch shared cross-component changes.
8. **Cutover:** prove board ordering, Doing run-now, one-task limit, claims, comments, resume/Blocked, exact-head checks, trusted merges, Windows EXE and macOS PKG installers, R2 integrity and authenticated beta downloads. Verify both domains and rollback before retiring old workflows.

## Acceptance criteria

- A documented approved architecture, inventory, migration plan, owners, dependency graph and rollback playbook exist.
- No website or git history is lost; canonical issues/prompts are resolvable, archived tasks are not recoded, links and redirects are checked.
- Github Project board remains single source of ordering across any relevant issue repo. One active agent-runner task globally. Todo = one overnight pickup per London day; Doing = immediate unclaimed request or claimed work; claim/comment distinguishes them.
- The current 60% usage gate, bounded Claude loop, Blocked/resume handling and task-specific verified Done are preserved with automated tests.
- Windows/macOS CI and suites build from exact SHAs; beta third-component VERSION increments only for qualifying plugin changes, with major/minor deferred; no duplicate bump on retries.
- `beta.songsmith.uk` stays private and independently deployable; R2/Cloudflare integrity and authenticated download route tested, no public beta exposure.
- `songsmith.uk` gets a separate independent public deployment path, without claiming that a shop, payments or licensing service is live.
- Secrets/publisher credentials remain outside coding-agent access. Protected workflows and environment approvals are reviewed and least-privilege rules tested.
- Cutover and rollback are demonstrably safe; live release provenance, downloads and site content verified. Each child gets evidence comments; this epic is completed only once all accepted child outcomes are verified.

## Automated testing and risks

Test prompt-path resolution, issue/board links, project status and selection, concurrent claims, path filters, native builds, double version-bump prevention, cross-site deploy isolation, Access auth, R2 checksum, rollback and idempotent delivery. Risks: broken GitHub redirects/issues, release history, secrets exposure, accidental public beta distribution, unexpected CI coupling, and current incomplete AUTO-12 lifecycle. Mitigate with staging and reversible stages.

## Dependencies and release expectations

Depends on **verified** AUTO-11, AUTO-12 and AUTO-10 end-to-end operation, not merely merged code or closed issues. Infrastructure-only migration changes must not fabricate plugin version bumps or releases. Website-only deployments should not trigger plugin releases. Do not change existing repos, website routing, project status or agent execution from this future-specification prompt.

**Model:** Sonnet for inventory/specification and straightforward implementation; Opus only for difficult architecture/security decisions with a recorded rationale. Any execution has bounded budgets and owner-approved scheduling.
## Delivery

Follow the [execution policy](../../docs/songsmith-execution-policy.md). The board decides when this runs; Sonnet is the default model.

## Mandatory implementation issue/prompt inheritance
Every ARCH-01 implementation child issue **and its canonical Claude prompt** must restate applicable requirements from this parent: exact two-repository scope, rename-in-place, `agent-runner` naming and host portability, split of `plugins/shared` versus `prompts`/`docs`, retention of existing beta Cloudflare Pages/Access deployment, future independent public Pages integration, unchanged Windows/macOS R2 installer publishing, secrets isolation, path-based CI, verification evidence, staged rollback and explicit owner approval. Link this epic in each child. No migration child is Done without relevant live integration evidence; parent remains open until all acceptance gates pass.
