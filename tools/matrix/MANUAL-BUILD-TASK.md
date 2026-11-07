# AUTO-01: manual-only Build task

Canonical prompt: [AUTO-01](../../shared/prompts/AUTO-01-matrix-autonomous-development.md).
Implementation: [draft PR #113](https://github.com/tonygward/audio-plugins/pull/113).
Release checkpoint: unassigned; no version bump or merge.

The approved implementation stages `Songsmith Matrix Build` **disabled**, with
Password logon as `MATRIX\SongsmithBuild`, Limited run level, no triggers and no
automatic restart. Staging never starts it, creates a test worktree or leases one.
The existing scanner, worker task and runtime configuration are not modified.

## Fixed execution and permissions

The sole action runs an owner-protected `Build-Broker.ps1` using PowerShell 7 with
`-NoProfile -NonInteractive`. It accepts no runtime parameters and does not use
`$(Arg0)` substitutions. A protected contract fixes account SIDs, task action,
allowed worktree root, JUCE/tool paths, deadlines and isolation targets.

Task owner, Administrators and SYSTEM retain management rights. Agent receives
read/execute access to this task only, intended for query/run/stop. It receives no
task-definition, principal, credential, deletion or DACL write permission. Both
accounts remain non-admin; no debug/security privilege is granted. If Build lacks
`SeBatchLogonRight`, granting that one right is a separate activation prerequisite,
not a staging action. Registration uses the existing owner-protected credential;
no password is reset, prompted for, written to config or passed on a command line.

The broker package is owner-write only. Agent can read it. Build can read/execute
only the entry/common/config files it needs; its initial CWD is Windows, so package
directory enumeration is unnecessary. Agent's exchange directory permits Build to
read request/heartbeat data only; the controller lock is not accessible to Build.
It contains no credentials. The existing DPAPI credentials remain unchanged.

## One build request

1. The client verifies dependency/contract hashes, fixed task definition/DACL,
   actual non-admin Agent SID and idle/enabled task state. A disabled task is
   rejected **before** a lock, request, lease or task invocation.
2. It takes an exclusive controller lock, validates a fresh bounded request and
   grants the existing one-worktree lease: source read/execute, writes only in
   affected plugin build directories. Git/hidden paths remain private.
3. Original DACLs and broker request/instance identifiers are saved in the private
   journal. The read-only exchange request and worktree lease marker must match.
4. Task Scheduler starts Build's primary scheduled logon. The fixed entry checks
   the actual Build SID, non-admin token and effective protected-path denials,
   sanitises its environment, and derives Release configure/build/test commands.
5. Agent checks the scheduler instance and exit code. Build-controlled status/logs
   never choose executable commands or establish identity. Diagnostic reads are
   bounded and use native handles to reject hard links, reparse points, redirects
   and concurrently open writers before reading as Agent.
6. Normal cancellation signals the client, which asks Task Scheduler to stop.
   Recorded process descendants must be gone before lease revocation. Uncertain
   RPC/start/stop/process observations retain the journal and block further work.
   A heartbeat/watchdog and fixed task execution limit bound controller loss;
   forced-kill recovery remains an explicit operator action.

The generated client's hash pins its lease/common code and exact contract; that
contract pins the fixed task entry. Existing worker owner approval still binds the
client script/config. A proposed worker config is staged separately with
`buildMode: scheduled`; all four live/merge/publication switches stay false.
Deploying it and the matching controller cancellation update is a later approval.

## Disabled validation

`Test-BrokerContract.ps1` imports the lease API and tests request types, command/path
injection rejection, deadlines, fixed plans, disabled/elevated/modified task
definitions and hard-linked/junction diagnostic files. It mocks Task Scheduler;
it never registers or starts a task. It is included in the Windows Node test suite.
The Node suite also tests cooperative cancellation without Windows task activation.

After staging, run only the following read-only commands against the printed package:

```powershell
& '<package>\Invoke-MatrixScheduledBuild.ps1' -Inspect
& '<package>\Test-ManualBuild.ps1' -PlanOnly
Get-ScheduledTask -TaskName 'Songsmith Matrix Build'
Get-ScheduledTaskInfo -TaskName 'Songsmith Matrix Build'
```

Expected: disabled, no triggers, no instances, unchanged fixed action/protected
DACL, and last result `267011` (task has not run). No effective scheduled Build
identity, cross-account stop or real broker build is claimed from these checks.

## Separate approval required next

1. **Activation:** inspect account batch-logon rights/denies; approve granting only
   `SeBatchLogonRight` to Build if absent. Approve enabling **only** the manual Build
   task. Keep the Agent worker disabled and all autonomous/merge/publication flags
   false. This approval has not been given by staging approval.
2. **First supervised test:** approve running the staged `Test-ManualBuild.ps1`
   under an actual non-admin Agent Password-logon scheduled token. A temporary
   fixed diagnostic task may require its own explicit registration/start approval
   and existing Agent password via Windows UI. The harness compiles only a small
   controlled fixture; its executable verifies the real Build SID, non-admin token
   and denied source-write access. No Claude turn, EQ-38 or installer is involved.
3. **Boundary/recovery tests:** approve cancellation/descendant checks, denied
   post-revocation access, stale/duplicate requests and interrupted journal recovery.
   Do not broaden task rights if query/run/stop fails; report the failure.
4. **Real EQ:** approve a warning-free EQ/JUCE Release build/test under the same
   scheduled identities, with retained evidence. The obsolete JUCE splash macro
   correction remains subject to this real scheduled-build verification.
5. **EQ-38 pilot:** only after those gates and green CI, obtain owner review/merge
   and matched trusted deployment, verify Agent allowance/paid-extra-usage settings
   and file/network confinement, prepare screenshot validation, issue a fresh
   hash/config/identity-bound owner approval, and separately approve one EQ-38 run.
   Keep auto-merge/publishing off. No worker schedule is needed for that pilot.

## Rollback

Staging adds a new package/exchange and a disabled task; it does not replace existing
tasks or configs. To withdraw it, obtain approval to unregister only
`Songsmith Matrix Build`, retain staging evidence and remove only the exact recorded
new directories after checking resolved paths. No password reset, account removal,
global task-folder ACL change or scanner rollback is required. Existing lease
journals must be investigated, not deleted to bypass recovery.
