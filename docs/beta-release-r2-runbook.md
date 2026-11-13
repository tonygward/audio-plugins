# Songsmith beta release distribution — GitHub → private R2

**Current state:** live. Every published release is copied to private storage and the beta website updates on its own (`BETA_DISTRIBUTION_ENABLED=true`).

## Owner's publication policy

**No GitHub environment approval or recorded reviewer event is required.**
Publishing a tagged release in `audio-plugins` is the deliberate trigger, but
the workflow still requires repository variable
`BETA_DISTRIBUTION_ENABLED=true` to start. The workflow does **not** attach
a `beta-distribution` environment. Existing environment configuration is
not used for distribution; the secrets reside at repository scope.

The upstream workflow checks the published `vX.Y.Z` tag, matching tagged
`VERSION` and changelog entry, successful full main CI, expected Windows
EXE and macOS PKG release assets and their actual SHA-256 bytes. It ties
uploads to the exact first-attempt release-triggered workflow run and writes
immutable versioned objects only to the private R2 bucket
`songsmith-beta-releases`, verifying both objects after upload.

After successful upload, the upstream workflow dispatches
`songsmith-approved-release` (a legacy internal event identifier, not an
approval guarantee) to the website. The website independently verifies the
source release, successful upstream run and R2 objects, then publishes
release.json + changelog.md together straight to its `main` branch.
Cloudflare Pages then deploys it. **No manual merge or review step: this is
hands off.** Verification is automatic (source release, CI run, R2 objects and
hashes); the safety stop is setting the activation variables to `false`.

## Required repository settings

**audio-plugins repository secrets:**
`R2_UPLOAD_KEY_ID`, `R2_UPLOAD_SECRET`, `WEBSITE_READ_TOKEN`,
`WEBSITE_DISPATCH_TOKEN`.

**songsmith-website repository secrets:**
`R2_READ_KEY_ID`, `R2_READ_SECRET`, `SOURCE_READ_TOKEN`,
`WEBSITE_UPDATE_TOKEN`.

**Both repositories' variables:**
`R2_ENDPOINT=https://<account-id>.r2.cloudflarestorage.com`,
`R2_BUCKET=songsmith-beta-releases`.

**Activation variables:** Keep `BETA_DISTRIBUTION_ENABLED=false` and
`BETA_WEBSITE_RELEASE_ENABLED=false` until a controlled live test.
These are repository variables, not encrypted credentials.

The website verifier is pinned to immutable commit
`dc9feadc10f69cfbd41f47b84b39addf00b2ea88` and must be reviewed
before changing that pin. Never commit token contents to this repo.

## First controlled end-to-end test

1. Confirm the two sets of GitHub secrets and R2 endpoint authenticate and
   private bucket R2 public domains remain disabled.
2. Check the tagged version matches a successful full main CI run and that
   a genuine published GitHub release contains the exactly named installers
   for both platforms, with matching hash and byte size.
3. Deliberately set both activation variables to `true` **only after**
   validation. Publishing a new release starts distribution automatically.
   Older release events won't automatically rerun just from changing vars.
4. Verify source workflow upload, private R2 HEAD/readback, source release
   identity and website dispatch. Observe website verification and its commit.
5. Check the website commit and CI, verify
   Cloudflare Pages deployment and download both files as a logged-in
   authorised beta tester. Signed-out or unapproved testers must be denied.
6. On any issue, set activation variables to `false`, cancel already-running
   jobs and retain the last known good release. Never enable public R2
   access or bypass Access authentication to debug an upload.

**Security limitation:** This is a deliberate single-owner release policy,
not separation of duties. A repository writer can publish a release, and
there is no second environment approval. Provenance, hashes, private
storage and fail-closed authentication remain.
