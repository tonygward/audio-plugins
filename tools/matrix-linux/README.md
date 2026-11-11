**Policy:** [board order, one task, 60% usage, automatic beta build numbers and verified delivery](../../docs/songsmith-execution-policy.md).

# Matrix Linux agent container (AUTO-05, phase 1)

An isolated, unprivileged container for the Claude developer agent, for **wslc (WSL containers)**. It replaced the earlier Windows `SongsmithAgent` / `SongsmithBuild` account machinery, which has been retired.
Native Windows and macOS builds stay on GitHub-hosted runners (`build.yml`); the container never builds
plugins and never holds publishing credentials.

**Status:** in live use. The image, isolation self-test (16 checks) and the Linux worker are built, tested and running with real credentials; it has delivered real tasks end to end.

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
./Start-MatrixAgent.ps1 -Action Build -ClaudeCodeVersion <exact version>   # first time only; later builds and updates use the recorded version
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

## Start at startup and logon

wslc has no restart policy, so a scheduled task runs `-Action Ensure` one minute after Windows starts and again one minute after the owner logs on. It
runs as the owner's own account with highest privileges (to match the elevated wslc session above), using a logon type with no stored password, and it only
starts containers. Its output is appended to `C:\Users\<you>\AppData\Local\MatrixAgent\start.log`. Recreate it with:

```powershell
$arguments = '-NoProfile -NonInteractive -WindowStyle Hidden -Command "& ''C:\dev\audio-plugins\tools\matrix-linux\Start-MatrixAgent.ps1'' -Action Ensure -SecretsDirectory C:\MatrixSecrets *>> ''' + "$env:LOCALAPPDATA\MatrixAgent\start.log" + '''; exit $LASTEXITCODE"'
$action = New-ScheduledTaskAction -Execute 'C:\Program Files\PowerShell\7\pwsh.exe' -Argument $arguments
$logon = New-ScheduledTaskTrigger -AtLogOn -User "$env:USERDOMAIN\$env:USERNAME"; $logon.Delay = 'PT1M'
$boot = New-ScheduledTaskTrigger -AtStartup; $boot.Delay = 'PT1M'
Register-ScheduledTask -TaskName 'Songsmith Matrix Agent (start containers)' -Action $action -Trigger @($logon, $boot) `
  -Principal (New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" -LogonType S4U -RunLevel Highest)
```

Starting at boot with nobody logged in has not yet been confirmed across a real restart: after one, check `start.log` before logging in.
Remove the task with `Unregister-ScheduledTask 'Songsmith Matrix Agent (start containers)'`.

## Automatic Claude Code updates

The image carries one exact Claude Code version (the container's own auto-updater is off, so a running worker never changes under itself). Keeping it current is a **host** job,
because the container cannot rebuild itself:

```powershell
./Start-MatrixAgent.ps1 -Action Update -SecretsDirectory C:\MatrixSecrets    # normally run daily by a scheduled task
./Start-MatrixAgent.ps1 -Action Rollback -SecretsDirectory C:\MatrixSecrets  # manual: return to the image kept before the last update
```

What `Update` does:

1. **Choose a target** (`claude-version.mjs`, from the npm registry): the newest release that is a stable `x.y.z`, **not deprecated**, **at least 3 days old** (`-MinimumAgeDays`, so a bad release is usually withdrawn before Matrix takes it),
   on the **same major version** (a new major needs `-AllowMajor`), never newer than the registry's `latest`, and never one that already failed a trial. It never downgrades.
2. **Wait for an idle worker.** If a job is claimed it does nothing and tries again at the next scheduled run; an update never interrupts work.
3. **Prove a candidate image** built beside the live one: the 16-check isolation self-test passes, it reports exactly the target version, and the Claude subscription sign-in still works with it.
   A candidate that fails is **rejected** (recorded, never retried) and the live image is untouched.
4. **Swap and watch.** The live image is kept as `songsmith-matrix-agent:previous`, the candidate becomes live, the worker restarts, and it must report started and still be running the new version a minute later.
   If not, it **rolls back automatically** to the previous image and rejects that version.

The versions and decisions are recorded on the host (not in the repository): `%LOCALAPPDATA%\MatrixAgent\claude-version.json` (current, previous, rejected) and `update.log`. `-Action Build` with no version rebuilds the recorded one.

Register the daily task once (same account and elevation as the start task; it only runs this script):

```powershell
$arguments = '-NoProfile -NonInteractive -WindowStyle Hidden -Command "& ''C:\dev\audio-plugins\tools\matrix-linux\Start-MatrixAgent.ps1'' -Action Update -SecretsDirectory C:\MatrixSecrets *>> ''' + "$env:LOCALAPPDATA\MatrixAgent\update-task.log" + '''; exit $LASTEXITCODE"'
Register-ScheduledTask -TaskName 'Songsmith Matrix Agent (update Claude Code)' `
  -Action (New-ScheduledTaskAction -Execute 'C:\Program Files\PowerShell\7\pwsh.exe' -Argument $arguments) `
  -Trigger (New-ScheduledTaskTrigger -Daily -At 4:30am) `
  -Settings (New-ScheduledTaskSettingsSet -StartWhenAvailable -ExecutionTimeLimit (New-TimeSpan -Hours 1)) `
  -Principal (New-ScheduledTaskPrincipal -UserId "$env:USERDOMAIN\$env:USERNAME" -LogonType S4U -RunLevel Highest)
```

Limits: the update trusts the npm registry and Anthropic's package, with the age delay and the self-test as the safeguards; it does not run a paid agent task to prove the new version works, so a behaviour change in Claude Code
would show up on the next real task. Roll back with `-Action Rollback` if that happens.

## What the Linux worker does

`tools/matrix/linux-worker.mjs` shares the backlog parser, 60 % usage gate (unknown fails closed), task states, turn/cost bounds and
change-scope rules. It differs in validation: there is no local build.

1. Every five minutes, read the project board and pick **one** card: a Doing card at once (a run-now request), or one Todo card per London day after 02:00. Nothing in a prompt decides this.
2. Claude edits a worktree with file tools only. The GitHub token is never in Claude's environment.
3. Commit, merge `main` in if it moved (a changelog-only conflict is resolved; anything else stops with a Blocked note), push, open a **draft** PR.
4. Wait for the hosted Windows and macOS **Build** on the exact head commit. Stale runs never count. On failure, up to two repairs: the failed-job log is saved to a file
   the agent may read as untrusted data; it is never pasted into the instructions.
5. When the build is green, `matrix-auto-merge.yml` (the existing workflow) merges it; the existing release pipeline publishes the beta. The worker never merges or releases. Changes to Matrix's own code use the core lane described in `tools/matrix/README.md`; the image is still rebuilt by hand.
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

## Operating notes

1. The worker code is baked into the image. After a change to `tools/matrix/`, run `-Action Build`, then `-Action Stop` and `-Action Ensure`, and do this only while no job is running (check `-Action Logs`).
2. The agent still uses the owner's GitHub login. A dedicated bot identity or GitHub App is a possible later change.

Limits: hosted builds do not detect compiler warnings, so the worker's old "no new warnings" check is not applied here; DAW and screenshot review still need a person.
