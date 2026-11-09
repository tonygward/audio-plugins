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
- Dependency: factual shipped-feature list; current private website process requires normal PR review for design/content work.
- PR and merge are distinct from deploy. Record actual deployed commit and access test; no invented release or Matrix status.

**Current execution and delivery:** Follow [the owner policy](../../docs/songsmith-execution-policy.md). Board order decides selection regardless of issue/prompt location or prefix, with one task and a 60% weekly usage stop threshold. Use the actual implementation repository's scoped execution path. Automatically approve/merge after current-head required checks; verify beta deployment and downloads, post the final issue comment, then close and finally mark board Done. Major/minor increments are deferred; beta build/patch increments are automatic.
