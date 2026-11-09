# Matrix Linux agent container (AUTO-05, phase 1)

An isolated, unprivileged container for the Claude developer agent, for **wslc (WSL containers)**. It replaces the Windows `SongsmithAgent` / `SongsmithBuild` account machinery in later phases.
Native Windows and macOS builds stay on GitHub-hosted runners (`build.yml`); the container never builds
plugins and never holds publishing credentials.

**Status: image and isolation tested with wslc on the development machine (all 16 self-test checks pass). The Linux worker is not written (phase 2). The current Windows worker is untouched.

## What it enforces

| Control | Where |
|---|---|
| Non-root user 10001, no sudo, no set-id binaries, no Docker CLI | `Dockerfile` |
| Memory and CPU limits, private tmpfs, unprivileged user (wslc has no read-only-root, capability-drop, no-new-privileges or PID-limit flags; the image has no set-id programs and the agent user cannot write outside its workspace) | `Start-MatrixAgent.ps1` |
| No Docker socket or CLI, no published ports, no host mounts except read-only secrets | `Start-MatrixAgent.ps1` |
| Agent is on an `internal` network; the only way out is an allow-list proxy (Anthropic and GitHub over HTTPS; private/LAN addresses refused) | `Start-MatrixAgent.ps1`, `squid/squid.conf` |
| Secrets are read-only files outside the repository; only the Claude key and a fine-grained, repository-scoped GitHub token are given. Release, Cloudflare, R2 and website tokens are never mounted | `Start-MatrixAgent.ps1`, `verify-isolation.sh` |
| Trusted code is baked into the image rather than mounted from the host | `Dockerfile` |

## Run it (PowerShell, no administrator needed)

wslc is part of WSL (`wsl --update`). Compose is not available in wslc, so a script does the same job.

```powershell
cd toolsmatrix-linux
./Start-MatrixAgent.ps1 -Action Build -ClaudeCodeVersion <exact version you have vetted>
./Start-MatrixAgent.ps1 -Action Verify    # starts the proxy, runs the self-test; must print ALL CHECKS PASSED
./Start-MatrixAgent.ps1 -Action Down      # removes the proxy container and internal network
```

For real runs pass `-SecretsDirectory <folder outside the repository>` containing `anthropic_api_key` and `agent_github_token`
(fine-grained token for this repository only: Contents and Pull requests read/write, nothing else).
The folder is mounted read-only at `/run/secrets`. Do not put release, Cloudflare, R2 or website tokens there.

## Self-test (`verify`)

Checks, from inside the container: non-root, no Docker socket or CLI, no sudo, read-only root, writable workspace,
no capabilities, no-new-privileges, no publishing credentials in the environment or `/run/secrets`,
no direct internet, no home-network access, and that the proxy refuses a host not on the allow-list.
Passing proves the container is configured as designed; it does not prove the WSL host is safe, and it does not replace a review.

## Rollback

`./Start-MatrixAgent.ps1 -Action Down` removes the proxy container and the internal network. The image can be removed with `wslc rmi songsmith-matrix-agent:dev`. Nothing else is touched.

## Next phases (not done)

2. Linux worker: reuse `backlog.mjs`, `usage.mjs`, `state.mjs`, `policy.mjs` and the validation rules; replace the local Windows
   build with "push draft PR, wait for hosted Build on the exact head SHA, bounded repair".
   (`worker.mjs` currently requires a build launcher; this needs a CI mode.)
3. Supervised pilot with one low-risk prompt, then retire the Windows accounts, build broker and approval renewals.
4. Replace the owner's GitHub login with a dedicated bot identity or GitHub App.
