**Current owner policy (9 November 2026):** [Board order, one task, 60% usage, automatic beta build numbers and verified delivery](../../docs/songsmith-execution-policy.md). Historical setup examples below do not override it. AUTO-11 must implement the board reader; never infer live board selection from documentation. A merged prompt archive does not close its issue: beta deployment/download verification and the final issue comment precede issue close and board Done.

# Matrix Linux agent container (AUTO-05, phase 1)

An isolated, unprivileged container for the Claude developer agent, for **wslc (WSL containers)**. It replaces the Windows `SongsmithAgent` / `SongsmithBuild` account machinery in later phases.
Native Windows and macOS builds stay on GitHub-hosted runners (`build.yml`); the container never builds
plugins and never holds publishing credentials.

**Status:** the image, isolation and the Linux worker are built and tested (all 16 container self-test checks and the worker's unit tests pass). It has **not** been run live with real credentials; that is the supervised pilot below, started only by the owner.

## What it enforces

| Control | Where |
|---|---|
| Non-root user 10001, no sudo, no set-id binaries, no Docker CLI | `Dockerfile` |
| Memory and CPU limits, private tmpfs, unprivileged user (wslc has no read-only-root, capability-drop, no-new-privileges or PID-limit flags; the image has no set-id programs and the agent user cannot write outside its workspace) | `Start-MatrixAgent.ps1` |
| No Docker socket or CLI, no published ports, no host mounts except read-only secrets | `Start-MatrixAgent.ps1` |
| Agent is on an `internal` network; the only way out is an allow-list proxy (Anthropic and GitHub over HTTPS; private/LAN addresses refused) | `Start-MatrixAgent.ps1`, `squid/squid.conf` |
| Secrets are read-only files outside the repository; only the Claude key and a fine-grained, repository-scoped GitHub token are given. Release, Cloudflare, R2 and website tokens are never mounted | `Start-MatrixAgent.ps1`, `verify-isolation.sh` |
| Trusted code is baked into the image rather than mounted from the host | `Dockerfile` |

## Run it (PowerShell)

Use the **same elevation** every time. wslc keeps separate images and containers for an elevated and a non-elevated session, so a container
built in an administrator window is invisible from a normal one. These notes were written and tested in an elevated PowerShell.

wslc is part of WSL (`wsl --update`). Compose is not available in wslc, so `Start-MatrixAgent.ps1` does the same job.

```powershell
cd tools\matrix-linux
./Start-MatrixAgent.ps1 -Action Build -ClaudeCodeVersion <exact version you have vetted>
./Start-MatrixAgent.ps1 -Action Verify     # isolation self-test; must print ALL CHECKS PASSED
./Start-MatrixAgent.ps1 -Action Login      # one time: sign in with the Claude subscription (opens a URL; paste the code back)
./Start-MatrixAgent.ps1 -Action Run -SecretsDirectory C:\MatrixSecrets   # the owner's go-ahead: starts the worker (any folder outside the repo works)
./Start-MatrixAgent.ps1 -Action Ensure -SecretsDirectory C:\MatrixSecrets   # start the proxy and worker only if not already running
./Start-MatrixAgent.ps1 -Action Logs       # follow it
./Start-MatrixAgent.ps1 -Action Pause      # no new work starts (Resume to allow it again)
./Start-MatrixAgent.ps1 -Action Stop       # stop the worker;  -Action Down also removes the proxy and network
```

The secrets folder holds one file, `agent_github_token`: a fine-grained GitHub token for `tonygward/audio-plugins` only (Contents and
Pull requests: read and write; nothing else). It is mounted read-only. **No Anthropic API key is used**: the usage gate requires a Claude
subscription login and refuses API keys, and the login is kept in the `matrix-home` volume. Do not put release, Cloudflare, R2 or website tokens anywhere near it.

## Start at logon

wslc has no restart policy, so a scheduled task runs `-Action Ensure` one minute after the owner logs on. It must be set to **run with
highest privileges** (to match the elevated wslc session above), as the owner's own account, and it only starts containers. Recreate it with:

```powershell
$action = New-ScheduledTaskAction -Execute 'C:\Program Files\PowerShell\7\pwsh.exe' -Argument '-NoProfile -NonInteractive -WindowStyle Hidden -File "C:\dev\audio-plugins\tools\matrix-linux\Start-MatrixAgent.ps1" -Action Ensure -SecretsDirectory C:\MatrixSecrets'
$trigger = New-ScheduledTaskTrigger -AtLogOn -User "$env:USERDOMAIN\$env:USERNAME"; $trigger.Delay = 'PT1M'
Register-ScheduledTask -TaskName 'Songsmith Matrix Agent (start containers)' -Action $action -Trigger $trigger `
  -Principal (New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" -LogonType Interactive -RunLevel Highest)
```

It starts only when the owner logs in; nothing starts after a reboot with nobody logged in unless Windows auto-logon is configured.
Remove it with `Unregister-ScheduledTask 'Songsmith Matrix Agent (start containers)'`.

## What the Linux worker does

`tools/matrix/linux-worker.mjs` reuses the Windows worker's backlog selection, 60 % usage gate (unknown fails closed), task states, turn/cost bounds and
change-scope rules. It differs in validation: there is no local build.

1. Pick up `build-now` prompts within about five minutes; `scheduled` prompts at the daily run after 02:00 London time.
2. Claude edits a worktree with file tools only. The GitHub token is never in Claude's environment.
3. Commit, merge `main` in if it moved (a changelog-only conflict is resolved; anything else stops with a Blocked note), push, open a **draft** PR.
4. Wait for the hosted Windows and macOS **Build** on the exact head commit. Stale runs never count. On failure, up to two repairs: the failed-job log is saved to a file
   the agent may read as untrusted data; it is never pasted into the instructions.
5. When the build is green, `matrix-auto-merge.yml` (the existing workflow) merges it; the existing release pipeline publishes the beta. The worker never merges or releases.
6. If it cannot get a green build, the PR stays a draft with a **Blocked** section.

Before every run the worker checks its own isolation (not root, no Docker socket, no publishing or GitHub credentials in its environment, proxy configured,
no capabilities) and refuses to start if any check fails. There is no 24-hour approval file: starting the container is the owner's approval, and
`-Action Pause` / `-Action Stop` are the controls.

## Self-test (`verify`)

Checks, from inside the container: non-root, no Docker socket or CLI, no sudo, read-only root, writable workspace,
no capabilities, no-new-privileges, no publishing credentials in the environment or `/run/secrets`,
no direct internet, no home-network access, and that the proxy refuses a host not on the allow-list.
Passing proves the container is configured as designed; it does not prove the WSL host is safe, and it does not replace a review.

## Rollback

`./Start-MatrixAgent.ps1 -Action Down` removes the proxy container and the internal network. The image can be removed with `wslc rmi songsmith-matrix-agent:dev`. Nothing else is touched.

## Next steps (owner)

1. `-Action Login` and create the fine-grained token (above).
2. Supervised pilot: run with one low-risk `build-now` prompt and watch `-Action Logs`.
3. Only after a clean pilot: retire the Windows accounts, build broker and approval renewals (they stay untouched until then; run only one worker at a time, so stop the Windows worker tasks first).
4. Replace the owner's GitHub login with a dedicated bot identity or GitHub App.

Limits: hosted builds do not detect compiler warnings, so the worker's old "no new warnings" check is not applied here; DAW and screenshot review still need a person.
