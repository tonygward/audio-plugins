# AUTO-01 temporary build access: implementation and deployment evidence

Canonical prompt: [AUTO-01](../../shared/prompts/AUTO-01-matrix-autonomous-development.md).
Implementation: [draft PR #113](https://github.com/tonygward/audio-plugins/pull/113).
Release checkpoint: unassigned; no version change. Work remains unmerged.

Reviewed and tested on MATRIX, 7 November 2026. No live development or EQ-38 run
was started. The installed trusted launcher and scanner were not replaced.

## Repository changes

- `Invoke-MatrixBuild.ps1` owns an exact-worktree ACL lease. The approved root is
  passed explicitly by the controller; only its direct child can be leased.
- Existing paths are inspected without following directory links. Reparse points
  and hard links are rejected. Source paths get read/execute; affected plugin build
  directories get Modify. Hidden paths, including `.git`, get no build-account grant.
  No permission is granted on the worktrees parent or unrelated worktrees.
- Original DACLs are flushed to the private `active-build-lease.json` before grants.
  Normal success and failure restore them. New output paths are made controller-only.
  OWNER RIGHTS prevents the output creator acquiring implicit permission to change
  DACLs. An outstanding recovery journal blocks the worker before external operations.
- The child tests effective denial of controller Git metadata (including direct HEAD
  and config reads), state, worktrees parent, build credential and approval file.
  Readable protected paths cause exit 74; the worker interrupts without repair or PR.
- The child uses a Windows kill-on-close job for compiler/test descendants. Its
  profile paths come from its effective SID, temporary files use its own profile,
  PATH comes from the machine, and MSBuild node reuse is disabled. Inheriting Tony's
  temp path initially blocked MSBuild; putting temp under the inaccessible worktrees
  parent also blocked FileTracker. Using the build account's own temp fixed both.
- `process.mjs` preserves subprocess exit codes for the isolation gate. Existing
  scanner selection and scheduling behaviour are unchanged.
- `Test-BuildLease.ps1` is an explicitly invoked, approval-required Windows test;
  normal Node tests do not modify ACLs. `Protect-MatrixBuildGit.ps1` is a separate
  operator provisioning script with an ACL backup and an optional `-WhatIf` preview.
- The stale repository test expecting EQ-38's execution mode to be omitted now
  checks the prompt's existing explicit `build-now` mode. No prompt was changed.

## Actual Windows changes and tests

All ACL-changing test/provisioning commands were submitted for approval first.
No Windows accounts, group membership, privileges or task registrations were changed.

The tests found a pre-existing gap: SongsmithBuild could enumerate
`C:\dev\audio-plugins\.git`. After approval, the provisioning script added an
explicit deny for SongsmithBuild to that subtree only. The original DACL backup is
`C:\Scheduled\Songsmith-Matrix\private\git-before-build-deny.acl`
(421,880 bytes when verified). This is the only lasting production ACL change.
The controller/scanner's ACL entries were preserved.

Latest successful retained fixture:
`C:\Scheduled\Songsmith-Matrix\worktrees\ACL-PILOT-811eafcaeedd40ba9835d15a4dce90b9`.
Earlier diagnostic ACL-PILOT/ACL-UNRELATED fixtures are also retained for inspection.
No active lease remained after the successful test. Build-account temporary files
may remain under its own `AppData\Local\Temp\MatrixBuild-*` directories.

Effective tests ran as `MATRIX\SongsmithBuild` and verified:

1. Reading fixture source and JUCE's CMake file succeeds.
2. Writing build output succeeds; modifying source or writing into JUCE is denied.
3. Enumerating the common Git directory and directly reading HEAD/config is denied.
4. Reading controller state, the build credential, Tony's Claude/GitHub CLI
   directories, the fixture Git pointer and an unrelated worktree sentinel is denied.
5. Listing the worktrees parent is denied during and after the lease.
6. An output file's build-account owner cannot change its DACL.
7. The actual launcher runs CMake with Visual Studio 18 2026, compiles with MSVC
   19.51.36260.0, and runs the tiny fixture executable successfully.
8. Existing DACLs are restored exactly, parent/sibling DACLs are unchanged, and
   source plus build-owned output become unreadable to SongsmithBuild after revocation.
9. Before the Git ACL repair, the final isolation gate refused compilation with
   exit 74 and revoked the lease successfully. This was an expected-failure audit,
   not a passing deployment test.

The compiler fixture is intentionally small; it is not the real EQ/JUCE suite.
No installer, Claude turn, branch push, PR creation or DAW test was performed.

## Deployment blockers

**Updated validation:** [PR #113 follow-up](VALIDATION-113.md) supersedes the
initial checklist below. A real EQ/JUCE build and 12,034 checks now pass, interruption
and linked-output recovery have been exercised, and the actual scheduled-principal
test found deployment blockers: enabled administrator membership, unavailable DPAPI
decryption under S4U, and an unreadable installed launcher. Two JUCE warnings also
block the worker's current warning gate. The original deployment remains disabled.

1. Review/merge the implementation through a PR and validate CI. Repository code
   is on `fix/auto-01-worktree-build-lease`; trusted deployment is not updated.
2. Deploy the revised launcher and controller modules together using the existing
   restricted controller identity and owner-protected installation. The launcher
   hash changes, so any future owner approval must be renewed. Do not reuse old approval.
3. Run the effective test under the actual restricted controller/scheduled-task
   logon context, not merely the approved interactive test context used here. The
   test child was the real SongsmithBuild account; production controller-token and
   DPAPI behaviour must still be checked.
4. Run real plugin configure/build/tests. Validate forced cancellation, power-loss
   recovery, descendant termination and malicious link/ACL mutation scenarios.
   Link detection fails closed and retains the journal for operator intervention;
   it does not promise automatic recovery from an adversarially altered output tree.
5. Validate real Claude file confinement, subscription/usage gates, GitHub draft
   creation, and retained UI before/after screenshots and visual review before EQ-38.

Live execution must remain disabled while these checks are outstanding.

## Rollback commands

Run as the deployment owner/controller with the required permissions. Keep live
execution disabled. Confirm compiler/test processes have stopped before recovery.

Recover an interrupted lease, only if its journal exists:

```powershell
& C:\dev\audio-plugins\tools\matrix\Invoke-MatrixBuild.ps1 `
  -RecoverLease C:\Scheduled\Songsmith-Matrix\private\active-build-lease.json
```

Restore the approved Git ACL change from its exact backup:

```powershell
icacls C:\dev\audio-plugins /restore C:\Scheduled\Songsmith-Matrix\private\git-before-build-deny.acl
```

Restoring that backup reinstates the old build-account Git exposure, so do not
run a worker with the restored permissions. Preserve the backup and test evidence.
No trusted launcher rollback is needed yet: the installed copy was not changed.

Repeat the full integration test after future approved deployment changes:

```powershell
& C:\dev\audio-plugins\tools\matrix\tests\Test-BuildLease.ps1 `
  -Config C:\Scheduled\Songsmith-Matrix.config.json
```

Every invocation changes disposable fixture ACLs and requires operator approval.

Implementation references: Windows [file access and traverse checks](https://learn.microsoft.com/en-us/windows/win32/fileio/file-security-and-access-rights),
[job objects](https://learn.microsoft.com/en-us/windows/win32/procthread/job-objects),
and the MSBuild [FileTracker source](https://github.com/dotnet/msbuild/blob/main/src/Utilities/TrackedDependencies/FileTracker.cs).
