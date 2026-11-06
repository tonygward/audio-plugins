# WEB-01 — Songsmith Private Beta Developer Portal

**Status:** Planned; not implemented. **Priority:** Early commercial/beta readiness; independent of plugin DSP priorities.
**Recommended model:** Sonnet (implementation, integration and tests). **Relative effort/usage:** Medium. **Escalate to Opus:** Only for unresolved private asset-delivery/authentication architecture or security review; document the decision then return to Sonnet.

## Objective
Build a deliberately simple, functional, invite-only Songsmith beta developer portal for a small set of testers (initially Dave, Neil and others invited by Tony). A tester should need only their approved email inbox; no GitHub account, password or subscription. Prepare for a later public storefront without implementing commerce now.

## Architecture and boundaries
1. Prefer a separate **private** portal repository, Cloudflare Pages hosting and Cloudflare Access Zero Trust protecting the entire beta host (proposed `beta.songsmithplugins.com`; domain is a proposal, not assumed owned). Maintain the canonical work item and links in `tonygward/audio-plugins`. Avoid coupling portal deployments to JUCE builds.
2. Use Cloudflare Access **One-time PIN** sign-in restricted by an **Allow policy listing individual tester email addresses**. Default deny everyone else. Never use an unrestricted login-method allow rule or an entire email domain for access. Describe exact operator setup, additions and removals, access/session lifetimes and revocation. Keep allowlist in Cloudflare, not source control.
3. Protect *all* portal routes, API endpoints and download paths; disable direct origin bypass and accidental public preview deployments. Add security response headers, noindex and no sensitive information in client-side bundles/logs.
4. The audio-plugins repository and release assets may become **private**. GitHub private releases are not public-download URLs. Implement a server-side download broker backed by appropriately scoped credentials, or private object storage populated by approved CI releases, with short-lived authorised download links. **Never expose a GitHub token, permanent link, private asset URL, or raw storage credential to the browser.** Validate Cloudflare Access identity server-side for each protected download and fail closed. Avoid GitHub Actions artifacts as a long-term distribution mechanism.
5. Existing release discipline remains: exactly one Windows setup EXE and one macOS PKG per approved suite version, sourced from the existing release pipeline; no additional plugin-specific installers. Do not change existing audio builds, plugin codes, root VERSION or release automation without explicit need.
6. Cloudflare Access should be the security boundary, not a hand-coded password or bespoke identity database. Document that a secure session may persist until expiry and revocation needs testing.

## Automatic approved-release discovery and installer links (required MVP)
- **No manual website edits per release.** Automatically pick up the latest **approved, successfully published, non-draft/non-prerelease Songsmith GitHub Release** (or explicitly approved beta release, according to a documented promotion policy) through a server-side integration. Do not assume the numerically greatest tag, latest commit, CI artifact or unapproved prerelease is safe to offer.
- On release publication, preferably trigger a signed/authenticated webhook or existing trusted CI step to refresh a server-side release manifest; add periodic reconciliation/polling as a fallback. Updates must be idempotent, tolerate delays and support rollback to the prior approved release if an asset is withdrawn.
- From the selected release, verify **both** expected asset names `Songsmith-Plugins-<version>-win64-setup.exe` and `Songsmith-Plugins-<version>-macOS.pkg`, expected release tag/version and integrity information. Never promote an incomplete release; retain last known good release when checks fail and log/report the problem.
- Display **Download for Windows** and **Download for Mac** buttons linked through the portal's authenticated server-side download endpoints, plus displayed version, publication date, file sizes, SHA-256 checksums and concise release notes. The testers must not need GitHub accounts and must never see private credentials or permanent public asset URLs.
- After a tester signs in with an allowlisted email, downloads must continue to work even after `tonygward/audio-plugins` is switched to **private**. Use a least-privilege server-to-server GitHub integration or private storage mirrored by the release pipeline. Validate Cloudflare Access identity for each download request; short-lived signed URLs must not be shareable indefinitely.
- Test new-release propagation without portal redeployment, Windows-only/Mac-only malformed releases, inaccessible assets, duplicate notification, removed or rolled-back release, unauthorised direct link, session expiry and private-repository access.
- Integrate with [AUTO-01](AUTO-01-matrix-autonomous-development.md): a Matrix-driven release becomes visible **only after** the independent CI/approved release checkpoint successfully publishes it. Never let an agent-written PR bypass release approval.

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
2. Scaffold the minimal accessible mobile-friendly site in the separate private repository, with sensible tests, environment separation, lint/build, and a simple deploy workflow.
3. Implement authenticated download proxy/short-lived link service and feedback integration without secrets in frontend or CI output.
4. Write a step-by-step operator runbook to configure DNS, Pages, Access app, One-time PIN, exact-address allowlist, service credentials, Pages secrets, authorised release publication and emergency revocation. Cloudflare setup requiring Tony's dashboard or account approval must be clearly marked as manual; do not claim it is configured automatically.
5. Test with an approved address, unapproved address, logged-out visitor, direct API/download requests, revoked user, expired session, and inaccessible/missing installer. Include regression tests and relevant screenshots. No production switch or public link until auth and asset protection are verified.

## Acceptance criteria
- Only explicitly allowlisted identities can access the portal or installer downloads; tester sign-in works through emailed one-time codes without GitHub accounts.
- Both Windows and macOS approved suite installers are delivered securely from **private** backing assets without public links or client-side credentials.
- Testers can read release notes/guides and submit feedback that creates a traceable private GitHub issue or moderation queue item.
- Documentation clearly separates implemented functionality from manual Cloudflare/domain setup, and explains costs/limits and revocation.
- Existing audio builds remain intact; no accidental new release or VERSION bump.
- Follow `AGENTS.md`: work on branch, open PR, include checks/screenshots. Update backlog links and final delivery traceability; move this prompt to `shared/prompts/done/` only after merged implementation, not when merely planned.

## Not part of WEB-01
Commercial payments, trials or software licence enforcement; public marketing website; automatic release promotion without approval; building a DAW; broadly public release distribution.
