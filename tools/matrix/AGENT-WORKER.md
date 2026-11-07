# AUTO-01: separate SongsmithAgent worker

Canonical prompt: [AUTO-01](../../shared/prompts/AUTO-01-matrix-autonomous-development.md).
Implementation: [PR #113](https://github.com/tonygward/audio-plugins/pull/113).
Release checkpoint: unassigned; no version bump or merge.

`SetUp-AgentWorker.ps1` prepares a separate, disabled `Songsmith Matrix Worker`
under `MATRIX\SongsmithAgent`, using Password logon and Limited run level. Tony's
`Songsmith Matrix` scanner is neither replaced nor started; its exported XML is
checked for changes. Existing tasks are never overwritten by this script.

The deployment uses separate `agent-state`, `agent-worktrees`, `agent-private`
directories and a revision-pinned `agent-trusted/<commit>` bundle. The existing
administrator-provisioned Agent source clone is retained. Only that exact clone is
added to SongsmithAgent's Git safe-directory list; no wildcard trust is added.
Worker settings remain `liveExecution: false`, `worker.enabled: false`,
`autoMerge: false` and `releasePublication: false`. No approval file is generated.

The existing SongsmithAgent password is entered in a native Windows credential
dialog, never in chat, source, config or command arguments. It is used transiently
for Windows task registration and authenticated logon; Task Scheduler stores its
own protected logon credential. No password reset or account/group change is made.
Registration may establish the account's batch-logon right through Task Scheduler.
The build credential is re-exported with user-scoped DPAPI under the verified
SongsmithAgent token, to its own protected private directory. Tony's original
credential file is not changed. The resulting encryption must pass an actual
Password-logon scheduled-token test before the deployment is considered usable.

Trusted code remains owner/SYSTEM/Administrators writable, SongsmithAgent readable.
SongsmithBuild gets read/execute only on the installed launcher file, not the bundle
directory, config, state or credentials. Its temporary worktree access still uses
the existing lease mechanism. The original scanner's trusted bundle is untouched.

## Approval and tests

The setup script changes protected directories/ACLs/configuration and registers a
disabled task. `-RunDiagnostics` additionally registers and explicitly starts a
separate fixed diagnostic task. Both operations require owner approval; approving
the diagnostic task does not enable the worker task or autonomous execution.

```powershell
& tools/matrix/SetUp-AgentWorker.ps1 -WhatIf
# Only after explicit deployment/diagnostic approval:
& tools/matrix/SetUp-AgentWorker.ps1 -RunDiagnostics
```

The fixed diagnostic task checks the actual non-admin identity, DPAPI decryption,
denial of Tony's credential files, effective Build ACLs, interruption recovery,
linked-output containment and a real EQ/JUCE build at the pinned PR revision.
It reads Claude authentication status without starting a model turn and writes
sanitised status fields only. Reports/transcript stay in `agent-private`.
The worker task stays disabled. Remove the uniquely named diagnostic task only
after it has finished and its evidence has been retained.

The two JUCE warnings came from EQ and EQTests compiling the same obsolete
`JUCE_DISPLAY_SPLASH_SCREEN=0` definition. Supported JUCE 9.0.5 no longer has a
splash screen and warns whenever that macro is defined. EQ and Maximiser stop
defining it; Analog Compressor already omits it. No compiler warning suppression
or relaxation of the worker's warning gate is introduced. Full CI and a real
warning-free EQ build must verify the correction.

## Supervised EQ-38 pilot prerequisites

1. Validate actual scheduled SongsmithAgent identity, credential decryption and
   read-only installed launcher access; retain effective negative ACL evidence.
2. Obtain green CI and owner approval to merge/deploy the reviewed infrastructure
   and warning fix. The worker reads canonical main, not arbitrary PR branches.
3. Verify Agent's own first-party Claude subscription and GitHub branch/draft-PR
   credentials, network boundaries and file-tool confinement. Fresh authoritative
   usage must be below 85%, session allowance available and paid extra usage off.
4. Arrange retained before/after EQ screenshots and visual review for all ten
   instruments, opened/collapsed selector, widths and 75/100/200% scaling. Screenshot
   automation is not added by this deployment work.
5. Owner issues the exact config/machine/identity/launcher-hash approval (at most
   24 hours), approves a one-prompt foreground `work ... notify` run, and confirms
   selection is EQ-38. Batch size one alone is not an EQ-38 allowlist.
6. Enable live/worker flags only with that separate approval; keep auto-merge and
   publication off. Run once, produce a draft PR and stop. Do not enable the
   scheduled worker just to perform a foreground supervised pilot.

## Recovery / rollback

Keep the worker task disabled. Stop only explicitly identified diagnostic fixture
processes before recovering a lease with the installed launcher and the
`agent-private/active-build-lease.json` journal. Retain manifests and evidence.

The old worker configuration is backed up as `agent-private/worker-config.before.json`.
Restoring it requires owner review; Tony's scanner config/task/bundle were not changed.
Deleting or enabling tasks, changing logon/account settings or removing protected
deployment directories requires separate approval. No automated rollback resets
passwords or removes accounts.
