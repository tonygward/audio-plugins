# AUTO-01 PR #113: deployment validation follow-up

Canonical prompt: [AUTO-01](../../shared/prompts/AUTO-01-matrix-autonomous-development.md).
Implementation: [draft PR #113](https://github.com/tonygward/audio-plugins/pull/113).
Release checkpoint: unassigned; no version change or merge.

Validated on MATRIX on 7 November 2026. The real plugin build pins PR revision
`617609b4478fe4022ab711428630f20941c75488`. Follow-up changes add validation
harnesses/evidence only; they do not replace the installed trusted code.
Machine-readable evidence: [sanitised results](evidence/auto-01-pr113-validation.json).

## Results

| Validation | Result | Evidence |
|---|---|---|
| Real EQ/JUCE Release configure/build | Passed as SongsmithBuild | Detached worktree `EQ-VALIDATION-20261107-165737`; exit 0; source unchanged |
| Real EQTests | 12,034 checks passed, 0 failed | `build/matrix/build.stdout.log` in that worktree |
| Warning gate | Blocked | Two JUCE warnings: `JUCE_DISPLAY_SPLASH_SCREEN` is ignored; the current worker rejects detected warnings |
| Source/build ACLs | Passed | Source read and build writes succeed; source/JUCE writes denied; parent/sibling DACLs unchanged |
| Credential isolation | Passed for tested paths | Direct reads of controller config, build credential, Claude `.credentials.json`, GitHub CLI `hosts.yml`, Git HEAD/config and state denied |
| Lease revocation | Passed | Source and build-owned output unreadable afterward; no active journal remains |
| Build child termination | Passed | Killing the credentialed PowerShell stopped the compiled test and its descendant; launcher restored ACLs |
| Controller-tree termination | Passed | Exact fixture tree terminated with `taskkill /T /F`; stale journal rejected another lease; explicit recovery restored access boundaries |
| Linked source | Passed | Hard-linked source rejected before granting access |
| Hostile output junction | Contained; manual recovery required | SongsmithBuild created it; recovery refused to follow it and retained the journal; unrelated target content/DACLs unchanged; operator removed only the junction and recovered |
| Scheduled controller identity | Blocked | Actual principal Tony/S4U/Limited, in a separate diagnostic task, has enabled administrator membership and cannot decrypt the saved build credential |
| Installed trusted launcher | Blocked | Direct read as SongsmithBuild is denied; tests used the reviewed repository launcher |
| Existing scheduled task | Unchanged | Exported XML equal before/after both diagnostic runs; diagnostic tasks removed |
| CI at tested PR revision | Passed | [Windows, Ubuntu and macOS orchestration run](https://github.com/tonygward/audio-plugins/actions/runs/42392792340) |

The scheduled-token `elevated` field records
`WindowsPrincipal.IsInRole(Administrator)`, not a separate UAC elevation query.
It is true despite the configured Limited run level, so this is not a verified
non-admin controller. The S4U test ran in session 0 as `MATRIX\Tony`; config,
repository and state were readable, but `Import-Clixml`/DPAPI reported
`Error occurred during a cryptographic operation.` Interactive Tony could decrypt
the same credential and launch the successful SongsmithBuild tests.

The original task was never started by these tests. A uniquely named temporary
diagnostic task used its exact principal/logon configuration and a fixed read-only
probe, then was removed. No existing task, account, group membership, owner approval,
trusted deployment or live setting was changed. ACL-changing fixtures and the
temporary-task commands were submitted for approval before execution.

## Remaining blockers

1. **Controller identity and S4U credential availability.** Provision an approved
   non-admin controller/logon design that can legitimately decrypt its protected
   build credential. Do not weaken credential storage or treat Limited run level
   as proof of isolation. Changing the existing identity/task requires owner approval;
   no such change was made.
2. **Installed launcher access and deployment.** SongsmithBuild needs narrowly
   scoped read/execute access to the reviewed trusted launcher while owner-only write
   protection remains. Install the controller/launcher together and renew the
   launcher-hash-bound approval only after review. No installation or ACL relaxation
   on the trusted path was performed.
3. **JUCE warnings.** Resolve the ignored splash-screen definition compatibly with
   supported JUCE versions, or agree and test a precise warning policy. The actual
   worker currently blocks these two warnings; the successful direct launcher build
   is not a warning-free end-to-end worker pass. No warning was suppressed here.
4. **Failure coverage limits and live pilot.** Abrupt process-tree termination was
   tested, not a physical power outage/reboot. Hostile linked outputs still need
   deliberate operator repair before another lease. Claude confinement/usage gates,
   real draft PR creation and EQ-38 before/after visual evidence still require a
   separately approved supervised pilot after the deployment blockers are fixed.
   No autonomous execution or EQ-38 work was started. Not yet verified in a DAW.

## Retained evidence and reproduction

The successful real build ran from 16:57:38 to 17:02:06 UTC. Its logs and
`validation-report.json` remain under:
`C:\Scheduled\Songsmith-Matrix\worktrees\EQ-VALIDATION-20261107-165737\build\matrix`.

Protected operator reports remain in `C:\Scheduled\Songsmith-Matrix\private`:

- `controller-context-f3ad0c49c35c4f8d925933ee167620ad.json`: expected deployment failure.
- `lease-validation-20261107-171016.json`: effective permission/credential checks.
- `interruption-20261107-170556.json`: both successful termination/recovery cases.
- `link-validation-20261107-170822.json`: hard-link/junction containment and recovery.

Earlier failed diagnostic fixtures/reports are retained. The initial interruption
harness used a relative PID marker, then had a short-probe syntax error; those
harness errors were corrected, their fixture processes stopped, leases recovered,
and both interruption cases rerun successfully. They are not recorded as product
passes. Final evidence contains the successful reruns and the unresolved deployment
failures. No credentials or raw secret values are included in the repository evidence.

Operator-only commands below require approval for fixture ACL changes or temporary
task registration. Do not run them concurrently: they share the private lease journal.

```powershell
& tools/matrix/tests/Test-RealEqBuild.ps1 -Config C:/Scheduled/Songsmith-Matrix.config.json
& tools/matrix/tests/Test-BuildLease.ps1 -Config C:/Scheduled/Songsmith-Matrix.config.json
& tools/matrix/tests/Test-BuildInterruption.ps1 -Config C:/Scheduled/Songsmith-Matrix.config.json
& tools/matrix/tests/Test-BuildLinkSafety.ps1 -Config C:/Scheduled/Songsmith-Matrix.config.json
& tools/matrix/tests/Invoke-ControllerContextTest.ps1 -Config C:/Scheduled/Songsmith-Matrix.config.json
```

The last command intentionally returns failure while the recorded controller
blockers remain. The lease test's compile uses the repository launcher and separately
records installed-launcher readability, so its success does not certify deployment.

See [lease rollback commands](BUILD-ACCESS.md#rollback-commands). All validation
leases were recovered, no fixture descendants remain from successful tests, and
temporary diagnostic tasks were removed. Final checks show `liveExecution: false`,
`worker.enabled: false`, `autoMerge: false` and no active lease journal. The PR
remains draft; neither merge nor autonomous activation is authorised by these tests.

## Disabled manual Build staging — 7 November 2026

The approved manual-only broker is installed at revision
`769e296f791257f50d73118d1c385c32e9cc3d85`. See
[the design, separate approval gates and rollback](MANUAL-BUILD-TASK.md) and
[retained staging evidence](evidence/auto-01-build-broker-staged.json).

`Songsmith Matrix Build` is disabled, has zero triggers/instances and last result
267011 (never run). Fixed action, protected task DACL, dependency hashes and
file-specific Build read/execute permissions passed inspection. The disabled
client refused a request before creating a lease. Scanner/worker definitions and
existing runtime configuration passed unchanged assertions. Both identities remain
non-admin; Build still lacks the batch-logon right. All four execution/merge/release
switches remain false; no active lease exists.

Local validation: 97 Node tests passed; 31 offline broker policy checks passed;
PowerShell scripts parsed successfully. Windows CI initially failed because the
fixture compared a normalised path with its unnormalised input; the assertion now
normalises the expected path too. CI must confirm that correction. This test-only
change does not alter the installed broker package.

Scheduled-token execution, effective permissions, task run/stop rights, descendant
termination, recovery and a real EQ/JUCE build through this new broker are explicitly
unverified while disabled. Earlier launcher results above do not certify the new
broker. Activation and supervised tests require separate approval; EQ-38 remains
blocked and no merge or publication is authorised.
