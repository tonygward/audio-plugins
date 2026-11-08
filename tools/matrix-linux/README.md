# Matrix Linux agent container (AUTO-05, phase 1)

An isolated, unprivileged container for the Claude developer agent, for Docker Engine running inside
WSL 2. It replaces the Windows `SongsmithAgent` / `SongsmithBuild` account machinery in later phases.
Native Windows and macOS builds stay on GitHub-hosted runners (`build.yml`); the container never builds
plugins and never holds publishing credentials.

**Status: definition only.** It has not been built or run yet (Docker is not installed on the
development machine), and the Linux worker is not written (phase 2). The current Windows worker is untouched.

## What it enforces

| Control | Where |
|---|---|
| Non-root user 10001, no sudo, no set-id binaries, no Docker CLI | `Dockerfile` |
| Read-only root file system, all capabilities dropped, `no-new-privileges`, private PID/IPC, memory/CPU/PID limits | `compose.yaml` |
| No Docker socket, no host network, no published ports, no host bind mounts | `compose.yaml` |
| Only a disposable named volume (`workspace`) and small tmpfs mounts are writable | `compose.yaml` |
| Agent is on an `internal` network; the only way out is an allow-list proxy (Anthropic and GitHub over HTTPS; private/LAN addresses refused) | `compose.yaml`, `squid/squid.conf` |
| Secrets are read-only files outside the repository; only the Claude key and a fine-grained, repository-scoped GitHub token are given. Release, Cloudflare, R2 and website tokens are never mounted | `compose.yaml`, `verify-isolation.sh` |
| Trusted code is baked into the image rather than mounted from the host | `Dockerfile` |

## Run it (in the WSL Ubuntu shell)

One-time, by the owner, with your approval (this installs software, so Matrix does not do it):

1. Install Docker Engine inside the Ubuntu WSL distribution (not Docker Desktop, so it stays separate from other workloads).
2. `mkdir -m 700 ~/.matrix-secrets` and put `anthropic_api_key` and `agent_github_token` in it (one value per file, `chmod 600`).
   Create the GitHub token as a fine-grained token for `tonygward/audio-plugins` only: Contents and Pull requests read/write, nothing else.

Then:

```bash
cd /mnt/c/dev/audio-plugins/tools/matrix-linux
export MATRIX_SECRETS_DIR=$HOME/.matrix-secrets
export CLAUDE_CODE_VERSION=<exact version you have vetted>
docker compose build
docker compose run --rm agent verify      # isolation self-test; must print ALL CHECKS PASSED
docker compose run --rm agent claude-check
```

Using a clone under the Linux file system (`~/`) rather than `/mnt/c` is faster and avoids Windows permission mapping.

## Self-test (`verify`)

Checks, from inside the container: non-root, no Docker socket or CLI, no sudo, read-only root, writable workspace,
no capabilities, no-new-privileges, no publishing credentials in the environment or `/run/secrets`,
no direct internet, no home-network access, and that the proxy refuses a host not on the allow-list.
Passing proves the container is configured as designed; it does not prove the WSL host is safe, and it does not replace a review.

## Rollback

`docker compose down -v` removes the containers, network and workspace volume. Nothing else on the host is touched.

## Next phases (not done)

2. Linux worker: reuse `backlog.mjs`, `usage.mjs`, `state.mjs`, `policy.mjs` and the validation rules; replace the local Windows
   build with "push draft PR, wait for hosted Build on the exact head SHA, bounded repair".
   (`worker.mjs` currently requires a build launcher; this needs a CI mode.)
3. Supervised pilot with one low-risk prompt, then retire the Windows accounts, build broker and approval renewals.
4. Replace the owner's GitHub login with a dedicated bot identity or GitHub App.
