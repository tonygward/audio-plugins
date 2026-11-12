# WEB-03 — Add plugin features section to private beta portal

- Issue: https://github.com/tonygward/songsmith-website/issues/19
- Status: ready for separately assigned website implementation
- Execution: RUN NOW requested; route to the scoped website execution path. Never reject because the issue or prompt is in another repository.
- Model: Claude Sonnet
- Implementation repository: `tonygward/songsmith-website`

## Objective and user value
Help private beta testers understand what each downloadable plugin does, in plain English, before installation.

## Current and desired behaviour
The site currently presents latest release, What's New, then Getting Started. Add a concise, responsive **Features** / **Meet the Plugins** section directly between What's New and Getting Started, without changing release-note rendering, authentication or downloads.

## Implementation guidance
1. Check live website source, shipped plugin list, documentation and latest **verified** release manifest. Do not treat roadmap entries as delivered.
2. Describe only actually distributed beta plugins in short, approachable cards (1–3 sentences each):
   - Songsmith EQ: shape tonal balance and adjust selected frequency ranges; mention advanced features only when shipping.
   - Songsmith Analog Compressor: manage dynamics and add analogue-style compression character.
   - Songsmith Maximiser: control peaks and raise perceived loudness.
3. Use existing CSS/typography and semantic heading hierarchy; responsive mobile and desktop layout, accessible contrast and readable labels.
4. Prefer no new packages, images, external requests or build infrastructure. Preserve Cloudflare Access, allow-list, private R2 download routes and release-data integrity.
5. Keep descriptions accurate if available plugin count changes; never advertise planned Leveller, Channel Strip or Loudness Analyser as downloadable.

## Acceptance criteria
- Order is exactly **What's New → Features → Getting Started**.
- Every currently downloadable plugin has accurate concise text, no unsupported claims.
- Desktop and mobile visual layouts are readable and accessible.
- Existing release notes, manifests, installer links and Access protection stay intact.
- Website checks/build pass and screenshots are included.
- Successful Cloudflare Pages deployment and Access-protected live page are verified before marking deployed.

## Tests, dependencies, risks and release
- Add/extend page order, card content and release-list consistency tests as appropriate; run `npm test` and `npm run build`.
- Check regression around authenticated release/download flows; do not weaken protection for testing.
- Dependency: factual shipped-feature list; use the current owner-approved exact-head CI and trusted automatic merge policy; do not introduce routine manual review gating for authorised website changes.
- PR and merge are distinct from deploy. Record actual deployed commit and access test; no invented release or Matrix status.

**Current execution and delivery:** Follow [the owner policy](../../docs/songsmith-execution-policy.md). Board order decides selection regardless of issue/prompt location or prefix, with one task and a 60% weekly usage stop threshold. Use the actual implementation repository's scoped execution path. Automatically approve/merge after current-head required checks; verify beta deployment and downloads, post the final issue comment, then close and finally mark board Done. Major/minor increments are deferred; beta build/patch increments are automatic.


## Lifecycle update — 10 November 2026 (authoritative)

This section supersedes older scheduling/status/completion wording in this prompt. Follow [the current execution policy](https://github.com/tonygward/audio-plugins/blob/main/docs/songsmith-execution-policy.md).

- [AUTO-11 #175](https://github.com/tonygward/audio-plugins/issues/175): board statuses, verified visible manual order, atomic claim and handoff. Todo is ready. Doing is a run-now request, picked up ahead of Todo unless the card's board Execution is Deferred; once picked up it is claimed with a start comment. Blocked is excluded from automatic pickup.
- [AUTO-12 #180](https://github.com/tonygward/audio-plugins/issues/180): standard bounded Claude task loop. Defaults 120 minutes elapsed/run, three total repair iterations and an explicit finite configured Claude turn/token budget. One task/run, one active job; verified weekly/scoped usage below 60%, unknown/stale blocks new agent work.
- On limit/blocker, save branch/SHA/PR/checks/delivery stage, comment with progress, actual reason, remaining work and resume action, leave open and set Blocked. No automatic next-poll retry. Explicit Todo/run-now resumes saved work without duplicate coding or publication.
- [AUTO-10 #157](https://github.com/tonygward/audio-plugins/issues/157): trusted delivery and finalisation. Exact-head required checks and relevant native CI precede automatic merge. Plugin changes require release, both suite installers, private R2 and matching authenticated beta downloads. Website-only tasks verify live deployment; infrastructure-only tasks verify deployed runtime/workflows; no unrelated plugin release. Parent epics/historical trackers are not additional jobs.
- Final task-appropriate acceptance evidence comment must succeed before issue close and board Done. A failed board update after close retries finalisation only. Keep merged work intact while delivery is pending. Prompt archives alone are not delivery evidence.
- Qualifying plugin main builds automatically increment only the third beta VERSION component. Major/minor increments deferred; documentation-only changes do not bump or publish.

This specification update does not activate Matrix or demonstrate operational board integration.
