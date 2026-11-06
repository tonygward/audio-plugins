# Approved beta release distribution to Cloudflare R2

**Status:** Upstream GitHub Action installed; intentionally disabled until external
secrets, permissions and approval gates are configured and tested.

## How it works

A published GitHub release in `audio-plugins` invokes
`.github/workflows/distribute-beta-r2.yml`. It runs **only** with repository
variable `BETA_DISTRIBUTION_ENABLED=true`, behind the GitHub environment
`beta-distribution`. The job validates the tagged version, full successful
`main` build, root changelog and the **exactly two** signed-digest suite
installers. It transfers them under immutable R2 keys
`releases/X.Y.Z/Songsmith-Plugins-X.Y.Z-...` in the private bucket
`songsmith-beta-releases`, reads them back and verifies hashes and metadata.
Only then is the website repo sent `songsmith-approved-release`.

The release verifier is pinned to an immutable reviewed commit in
`tonygward/songsmith-website`, not a moving branch. Coordinate upgrades of
this pin with the website repo's `prompts/WEB-02-release-distribution-hardening.md`
and its `.github/workflows/release.yml`. Matrix AUTO-01 is **not**
responsible for uploading to R2.

## Required manual GitHub / Cloudflare setup

1. Ensure Cloudflare R2 bucket `songsmith-beta-releases` has disabled public
   `r2.dev` and custom domains. Create bucket-scoped S3 API keys with write/
   read permissions for upstream and separate **read-only** keys for website CI.
2. **audio-plugins** Actions repository variables:
   `R2_BUCKET=songsmith-beta-releases` and
   `R2_ENDPOINT=https://<account-id>.r2.cloudflarestorage.com`.
   Keep `BETA_DISTRIBUTION_ENABLED` **unset or false** initially.
3. **audio-plugins** protected environment `beta-distribution`:
   `R2_UPLOAD_KEY_ID`, `R2_UPLOAD_SECRET`, `WEBSITE_READ_TOKEN` (private
   website repo contents read), `WEBSITE_DISPATCH_TOKEN` (website repo
   repository-dispatch permission). Prefer separately scoped GitHub App
   installation credentials. Don't reuse broad personal tokens.
4. **songsmith-website** Actions variables `R2_BUCKET`, `R2_ENDPOINT`
   and protected environment `beta-release` secrets `SOURCE_READ_TOKEN`
   (plugin repo release, actions, contents read), `R2_READ_KEY_ID`,
   `R2_READ_SECRET`, `WEBSITE_UPDATE_TOKEN` (website contents write and
   permission to trigger Pages through the resulting commit).
5. **Important:** GitHub environment required reviewers/approval may not be
   supported for the repo visibility/plan. Verify real enforcement and disable
   self-review. A named environment is not proof of required approval. If
   approvals cannot be enforced, retain the false activation switch and use
   a separately approved manual action or other enforceable promotion gate.
6. Independently verify the website receiver, token permissions, AWS CLI v2
   conditional upload support and GitHub Releases SHA-256 digest availability.
   Do not silently fall back to unverifiable releases.
7. Test an approved release end-to-end and negative cases (partial transfer,
   wrong checksum, extra asset, duplicate/older release, failed CI, failed
   dispatch, expired Access token). Check that no website metadata advances
   on failure and no direct public R2 URL is accessible.
8. Only after verification, explicitly opt into automated distribution by
   setting `BETA_DISTRIBUTION_ENABLED=true`.

## Operations

- **Pause immediately:** change `BETA_DISTRIBUTION_ENABLED` to false.
- **Releases are not created by this workflow.** It responds only to published
  release events, and never changes plugin version or installer build logic.
- Website and installer download remain on last known good release if upload
  or independent website verification fails.
- Do not claim end-to-end functionality merely because this workflow exists.
