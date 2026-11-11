# ADR: AUTO-05 — isolated Linux container for the Matrix developer agent

> **Update (November 2026):** the Linux container is live and the Windows machinery described below (the worker task, approval renewal, build broker, `SongsmithBuild` and `SongsmithAgent`) has been retired from the repository. This record is kept as the decision history.

Status: proposed (pilot running 8 November 2026). Decision owner: Tony. Supersedes the Windows account/ACL approach in AUTO-01 **only after** a clean supervised pilot.

## Context

AUTO-01 runs the Claude developer agent on the Windows host under two local accounts (`SongsmithAgent`, `SongsmithBuild`) with a hash-pinned build broker, NTFS ACL leases, a 24-hour owner approval and a local Visual Studio build. It works (v1.3.2 shipped through it) but cost most of a weekend to stabilise: ACL grants that need an administrator, PID-reuse bugs in the lease controller, a 24-hour approval expiry that needs a renewal task, and every worker change needing the owner to copy a hash-pinned file. The same checks already run on GitHub-hosted Windows and macOS runners (`build.yml`), which are what the release installers come from.

## Options compared

| | A. Linux VM | B. Rootless container (wslc) — **chosen** | C. Windows accounts (AUTO-01) |
|---|---|---|---|
| Isolation from the host | Strongest (own kernel and disk) | Good: WSL containers run in a lightweight VM, unprivileged user, internal-only network | Account/ACL separation on one OS; the owner's data is one ACL mistake away |
| Isolation from other containers (Home Assistant etc.) | Separate machine image | Separate session and network; no Docker socket | Not applicable |
| Operational cost | Highest (patching a full OS, disk, backups) | Low: one image, one script | High (measured) |
| Recovery | Snapshot/restore | `Down`, rebuild; state in two volumes | Operator recovery of ACL leases |
| Local native builds | No (needs hosted runners) | No (hosted runners) | Yes, but fragile |
| Screenshots / DAW checks | Not possible | Not possible | Possible, but still needs a person |

Decision: **B**, with the VM option kept as a stronger fallback if the owner later wants a harder boundary (the same image runs in a VM unchanged).

## Trust boundaries

```
 GitHub (draft PR, hosted Build, existing release workflows hold the publishing secrets)
   ^  HTTPS, repository-scoped token (Contents + Pull requests)
   |
 [ matrix-egress proxy ]  allow-list: anthropic, claude.ai, github   (only container with a route out)
   ^  internal network only
   |
 [ matrix-agent container ]  non-root, no Docker socket, no publishing credentials
     worker (trusted, baked in)  ->  Claude (file tools only, no GitHub token)
```

Nothing inside the agent container can reach the home network, other containers or the internet except through the proxy, and the proxy refuses private addresses.

## Credentials

| Secret | Where it lives | Agent container | Claude process |
|---|---|---|---|
| Claude subscription login | `matrix-home` volume | yes (needed) | yes (needed) |
| Agent GitHub token (this repo; Contents + Pull requests; no Workflows) | read-only file on the host | yes | **no** |
| `RELEASE_PUBLISH_TOKEN`, Cloudflare/R2, website dispatch | GitHub Actions secrets | **no** | no |

Only trusted workflows on `main` hold publishing secrets; the agent can only open PRs. The merge workflow runs from `main`'s copy and never checks out PR code.

## Gap analysis (draft PR → CI → repair → merge → beta)

| Step | Status |
|---|---|
| Backlog selection, usage gate, task states, bounds, change scope | Reused from AUTO-01 unchanged |
| Draft PR from an isolated agent | Built (`linux-worker.mjs`), unit-tested, live pilot in progress |
| Hosted Build on the exact head commit | Existing (`build.yml`); the worker ignores stale runs |
| Bounded repair from CI failures | Built (log in a file, never in instructions) |
| Merge when green | Existing (`matrix-auto-merge.yml`); **needs Pull requests write on `RELEASE_PUBLISH_TOKEN`** |
| Beta bump, publish, R2, website | Existing, proven with v1.3.2 and v1.3.3 |
| Compiler-warning gate | **Gap**: hosted builds do not report new warnings; the old local check is not applied |
| Screenshots, DAW verification | **Gap**: still need a person (unchanged) |
| Conflicts with main (changelog bump) | Built: changelog-only conflicts auto-resolved, otherwise Blocked |

## Limits of the isolation claim

wslc has no flags for a read-only root file system, capability drop, `no-new-privileges` or a PID limit. The image compensates (no set-id programs, agent user cannot write outside its workspace and home) and the self-test says so. The 16-check self-test and the worker's preflight prove the container is configured as designed; they do not prove the WSL host is safe. The worker uses the owner's GitHub identity unless a bot account or GitHub App is created.

## Migration plan

1. Pilot (now): one worker at a time; Windows worker task disabled; one low-risk `build-now` prompt; owner watches logs.
2. If the PR reaches green and merges cleanly: leave the container as the only worker for a week.
3. Then retire, in this order: Windows worker task, approval renewal, build broker and Build task, `SongsmithBuild`, `SongsmithAgent`. Keep the dry-run scanner.
4. Replace the owner's GitHub identity with a bot account or App.

Rollback at any point: `./Start-MatrixAgent.ps1 -Action Down`, then `Enable-ScheduledTask 'Songsmith Matrix Worker'` (and renew the 24-hour approval). No Windows component is removed before step 3.

## Cost

Hosted runners: roughly 10 minutes of Windows and 8 minutes of macOS per Matrix PR attempt (SUI-14: 9m56s and 7m41s), times up to three attempts. Check current GitHub plan limits before raising throughput. Claude usage is a 70 % weekly gate (lowered from 85 % on 9 November 2026 by the owner).
