# WEB-01 — Songsmith Private Beta Developer Portal

**Status:** Planned; not implemented. **Priority:** Early commercial/beta readiness; independent of plugin DSP priorities.
**Recommended model:** Sonnet (implementation, integration and tests). **Relative effort/usage:** Medium. **Escalate to Opus:** Only for unresolved private asset-delivery/authentication architecture or security review; document the decision then return to Sonnet.

## Objective
Build a deliberately simple, functional, invite-only Songsmith beta developer portal for a small set of testers (initially Dave, Neil and others invited by Tony). A tester should need only their approved email inbox; no GitHub account, password or subscription. Prepare for a later public storefront without implementing commerce now.

## Architecture and boundaries
1. **Require a separate website GitHub repository** (private, name to be confirmed), deployed as a **static website on Cloudflare Pages** with Cloudflare Access Zero Trust protecting the beta host (proposed `beta.songsmithplugins.com`; domain is not assumed owned). The plugin repo is `tonygward/audio-plugins`; keep the canonical WEB-01 requirements here and implement website code/workflows in the separate website repo. The website builds/deploys independently of JUCE and Matrix.
2. Use Cloudflare Access **One-time PIN** sign-in restricted by an **Allow policy listing individual tester email addresses**. Default deny everyone else. Never use an unrestricted login-method allow rule or an entire email domain for access. Describe exact operator setup, additions and removals, access/session lifetimes and revocation. Keep allowlist in Cloudflare, not source control.
3. Protect *all* portal routes, API endpoints and download paths; disable direct origin bypass and accidental public preview deployments. Add security response headers, noindex and no sensitive information in client-side bundles/logs.
4. The audio-plugins repository and release assets may become **private**. GitHub private releases are not public-download URLs. Implement a server-side download broker backed by appropriately scoped credentials, or private object storage populated by approved CI releases, with short-lived authorised download links. **Never expose a GitHub token, permanent link, private asset URL, or raw storage credential to the browser.** Validate Cloudflare Access identity server-side for each protected download and fail closed. Avoid GitHub Actions artifacts as a long-term distribution mechanism.
5. Existing release discipline remains: exactly one Windows setup EXE and one macOS PKG per approved suite version, sourced from the existing release pipeline; no additional plugin-specific installers. Do not change existing audio builds, plugin codes, root VERSION or release automation without explicit need.
6. Cloudflare Access should be the security boundary, not a hand-coded password or bespoke identity database. Document that a secure session may persist until expiry and revocation needs testing.

## Automatic release updates across two repositories (required MVP)
- **Use two repositories:** `tonygward/audio-plugins` owns release builds and publication; the **separate website repository** owns the Cloudflare Pages static website and its `release.json` file. Do not assume Cloudflare Pages deploys automatically just because the *other* repository changed.
- **Preferred event-driven path:** after a GitHub Release is independently validated and published in the plugin repository, a trusted GitHub Actions job sends `repository_dispatch` (e.g. `songsmith-release-published`) to the website repository, using a fine-grained token/GitHub App installation with minimum required cross-repository permissions. Avoid sharing broad personal access tokens.
- The website repository has a workflow listening for the event. It fetches the **authoritative release details** from the plugin repository via server-side/CI credentials; does not trust the dispatch payload alone; verifies release is approved and published under the documented beta/stable promotion policy, not a draft/unapproved prerelease.
- Verify the correct root `VERSION`/tag and **exactly two installers** named `Songsmith-Plugins-<version>-win64-setup.exe` and `Songsmith-Plugins-<version>-macOS.pkg`, with supported file sizes and checksums, before changing `release.json`. Write only safe public-facing metadata (version, date, notes, file sizes/checksums, and **portal-relative authenticated download routes**), **never** private GitHub asset URLs or tokens.
- Commit the new manifest to the website repository's deployment branch through a narrowly scoped bot identity (or trigger a secure Cloudflare Pages deploy with the same generated metadata). Its existing Git-connected Cloudflare Pages integration redeploys the **static website**. No frontend runtime GitHub API calls or dynamic application server are required for release metadata.
- **Fallback:** optional daily reconciliation in the **website repository's GitHub Actions**, not a daily Matrix rebuild. If a dispatch is missed, reconcile the latest approved release and update metadata only on change. Make updates idempotent, prevent old events downgrading newer releases, and retain the last known good release if assets/checks fail. Document rollback and revoked-release behaviour.
- After login, testers see **Download for Windows** and **Download for Mac** plus version, publish date, file sizes, SHA-256 and concise release notes. Static metadata updates automatically after Cloudflare deploy; no per-release manual edits.
- **Important distinction: the website remains static, but private installers still need a secure delivery path.** Use a narrowly scoped Cloudflare Worker/Pages Function to verify the Access JWT and stream/proxy private GitHub assets, or use private R2 storage populated by trusted release CI with authenticated short-lived downloads. Never put secrets or permanent unrestricted asset URLs into static site files; avoid publicly reachable origins.
- Test release → cross-repo dispatch → verified `release.json` update → Cloudflare Pages deployment; missed/duplicate/out-of-order events, private repository permissions, incomplete Windows/Mac assets, checksum failures, revoked/reverted releases, unauthorised download attempts and expired sessions.
- Integrate with [AUTO-01](AUTO-01-matrix-autonomous-development.md): Matrix does **not** rebuild the website and cannot dispatch or publish unless the independent GitHub release gate has succeeded. Do not trigger website updates for mere merged PRs or CI artifacts.

## MVP pages/features
- **Home:** clearly labelled Private Beta, current approved version, short testing instructions, last updated.
- **Downloads:** Windows and macOS suite installers; filenames, version, size, publish date, SHA-256 and platform notes where available. Explicitly distinguish beta-approved builds from latest internal CI artifacts. Friendly empty/error states.
- **What's New:** release notes drawn from an approved changelog/release metadata, not blindly from every push.
- **Getting Started:** install instructions, first DAW scan, Studio One and Cakewalk basics, Windows/macOS unsigned installer warnings accurately described; link plugin-specific guides and current screenshots where appropriate.
- **Feedback:** short form for suggestion or bug, requesting plugin, version, OS, DAW, reproduction steps and optional screenshot. Submit via a server-side GitHub integration with least-privilege credentials, into labelled GitHub issues or an administrator review queue. Do not expose private issue data or submitter details publicly. Rate limit, validate, sanitise and handle failures clearly.
- **Known Issues:** curated list, no sensitive internal comments or credentials.
- No e-commerce, licence activation, public signup, analytics trackers, elaborate visual design or custom admin dashboard in v1.

## Delivery plan
1. Inspect existing GitHub release pipeline, privacy constraints, Cloudflare account/domain prerequisites and download threat model; write a short architecture decision record covering private release distribution and estimated costs/limits.
2. Scaffold the minimal accessible mobile-friendly **static Cloudflare Pages site in the separate website repository**, with sensible tests, environment separation, lint/build and Git-connected deployment. Implement the cross-repo release dispatch receiver, manifest update workflow and optional reconciliation.
3. Implement authenticated download proxy/short-lived link service and feedback integration without secrets in frontend or CI output.
4. Write a step-by-step operator runbook to configure DNS, Pages, Access app, One-time PIN, exact-address allowlist, service credentials, Pages secrets, authorised release publication and emergency revocation. Cloudflare setup requiring Tony's dashboard or account approval must be clearly marked as manual; do not claim it is configured automatically.
5. Test with an approved address, unapproved address, logged-out visitor, direct API/download requests, revoked user, expired session, and inaccessible/missing installer. Include regression tests and relevant screenshots. No production switch or public link until auth and asset protection are verified.

## Acceptance criteria
- Only explicitly allowlisted identities can access the portal or installer downloads; tester sign-in works through emailed one-time codes without GitHub accounts.
- Both Windows and macOS approved suite installers are delivered securely from **private** backing assets without public links or client-side credentials.
- Testers can read release notes/guides and submit feedback that creates a traceable private GitHub issue or moderation queue item.
- Documentation clearly separates implemented functionality from manual Cloudflare/domain setup, and explains costs/limits and revocation.
- A validated plugin-repo GitHub Release updates the **separate website repo** manifest and triggers a Cloudflare Pages static deployment; no daily Matrix website rebuild or per-release manual website change.
- Existing audio builds remain intact; no accidental new release or VERSION bump.
- Follow `AGENTS.md`: work on branch, open PR, include checks/screenshots. Update backlog links and final delivery traceability; move this prompt to `shared/prompts/done/` only after merged implementation, not when merely planned.

## Not part of WEB-01
Commercial payments, trials or software licence enforcement; public marketing website; automatic release promotion without approval; building a DAW; broadly public release distribution.
