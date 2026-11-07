[CmdletBinding()]
param([Parameter(Mandatory)][string]$Request)
$ErrorActionPreference='Stop'
$r=Get-Content -LiteralPath $Request -Raw | ConvertFrom-Json
$c=Get-Content -LiteralPath $r.config -Raw | ConvertFrom-Json
if($c.liveExecution -ne $false -or $c.worker.enabled -ne $false -or $c.autoMerge -ne $false -or $c.releasePublication -ne $false){throw 'Diagnostic task requires all live switches disabled'}
$root=Split-Path -Parent $c.stateDirectory
$expected=Join-Path $root ('agent-trusted/'+$r.revision)
if($r.revision -notmatch '^[a-f0-9]{40}$' -or $r.trusted -ne $expected -or (Split-Path -Parent $c.worker.buildScript) -ne $expected){throw 'Diagnostic bundle does not match pinned owner request'}
& (Join-Path $expected 'tests/Run-AgentValidation.ps1') -Config $r.config -Revision $r.revision -RecoverLease:([bool]$r.recoverLease)
exit $LASTEXITCODE
