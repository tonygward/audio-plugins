[CmdletBinding()]
param([Parameter(Mandatory)][string]$Config, [string]$Revision = 'HEAD')
$ErrorActionPreference = 'Stop'
$c = Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
if ($c.liveExecution -ne $false -or $c.worker.enabled -ne $false) { throw 'Live settings must remain disabled' }
$root = Join-Path $c.worker.worktreeRoot ('EQ-VALIDATION-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
$revision = (& $c.worker.gitExecutable -C $c.repository rev-parse --verify $Revision).Trim()
if ($LASTEXITCODE) { throw 'Cannot pin revision' }
& $c.worker.gitExecutable -C $c.repository worktree add --detach $root $revision
if ($LASTEXITCODE) { throw 'Cannot create validation worktree' }
$exchange = Join-Path $root 'build/matrix'
$null = New-Item -ItemType Directory -Path $exchange -Force
$targets = @(
    @{path=(Join-Path $c.repository '.git'); directory=$true},
    @{path=(Join-Path $c.repository '.git/HEAD'); directory=$false},
    @{path=(Join-Path $c.repository '.git/config'); directory=$false},
    @{path=$c.stateDirectory; directory=$true},
    @{path=$c.worker.worktreeRoot; directory=$true},
    @{path=$c.worker.buildCredentialFile; directory=$false}
)
$request = Join-Path $exchange 'request.json'
@{worktree=$root; juceDirectory=$c.worker.juceDirectory; plugins=@('eq'); isolationTargets=$targets} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $request
$report = [ordered]@{revision=$revision; worktree=$root; start=[DateTime]::UtcNow.ToString('o'); controller=[Security.Principal.WindowsIdentity]::GetCurrent().Name}
$launcher = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Invoke-MatrixBuild.ps1'))
Write-Output "Pinned EQ build: $revision at $root"
try {
    & $launcher -RequestFile $request -CredentialFile $c.worker.buildCredentialFile -WorktreeRoot $c.worker.worktreeRoot
    $report.exitCode = $LASTEXITCODE
} catch { $report.error = $_.Exception.Message; $report.exitCode = 1 }
$report.finish = [DateTime]::UtcNow.ToString('o')
$report.activeLease = Test-Path -LiteralPath (Join-Path (Split-Path -Parent $c.worker.buildCredentialFile) 'active-build-lease.json')
$report.warningLines = @(Select-String -LiteralPath (Join-Path $exchange 'build.stdout.log'),(Join-Path $exchange 'build.stderr.log') -Pattern '\bwarning\s*(?:[A-Z]+\d+|:)' -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Line)
$report | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $exchange 'validation-report.json')
$report | ConvertTo-Json
exit $report.exitCode
