[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Package,
    [string]$Source = 'C:\Scheduled\Songsmith-Matrix\source',
    [string]$Revision = 'origin/main',
    [string[]]$Plugins = @('eq')
)
# Supervised test: builds the REAL plugin(s) at a pinned revision through the fixed Build task,
# as the non-admin Agent. No AI turn, no push, no PR. Run it from the Agent window using
# PowerShell 7. It leaves the worktree in place so you can inspect it.
$ErrorActionPreference = 'Stop'
$contract = Get-Content -LiteralPath (Join-Path $Package 'broker.json') -Raw | ConvertFrom-Json
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
if ($identity.User.Value -ne $contract.agentSid -or ([Security.Principal.WindowsPrincipal]::new($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this as the non-admin SongsmithAgent'
}
foreach ($p in $Plugins) { if ($p -notin @('eq', 'analog-compressor', 'maximiser')) { throw "Unknown plugin: $p" } }
$git = 'C:\Program Files\Git\cmd\git.exe'
& $git -C $Source fetch --quiet origin main
if ($LASTEXITCODE) { Write-Warning 'Could not fetch origin; using the local copy of main' }
$sha = (& $git -C $Source rev-parse --verify $Revision).Trim()
if ($LASTEXITCODE -or $sha -notmatch '^[a-f0-9]{40}$') { throw 'Cannot pin revision' }
$root = Join-Path $contract.worktreeRoot ('REAL-BUILD-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss'))
& $git -C $Source worktree add --detach $root $sha
if ($LASTEXITCODE) { throw 'Cannot create worktree' }
$null = New-Item -ItemType Directory -Path (Join-Path $root 'build/matrix') -Force
$request = Join-Path $root 'build/matrix/request.json'
@{worktree = $root; juceDirectory = $contract.juceDirectory; plugins = @($Plugins); isolationTargets = @()} |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $request
Write-Output "Building $($Plugins -join ', ') at $sha"
Write-Output "Worktree: $root"
$watch = [Diagnostics.Stopwatch]::StartNew()
& (Join-Path $Package 'Invoke-MatrixScheduledBuild.ps1') -RequestFile $request -WorktreeRoot $contract.worktreeRoot
$code = $LASTEXITCODE
$watch.Stop()
$changed = @(& $git -C $root status --porcelain --untracked-files=no)
$report = [ordered]@{
    revision      = $sha
    plugins       = @($Plugins)
    exitCode      = $code
    minutes       = [Math]::Round($watch.Elapsed.TotalMinutes, 1)
    leaseClosed   = -not (Test-Path -LiteralPath $contract.journalFile)
    sourceChanged = $changed.Count -gt 0
    worktree      = $root
}
$report | ConvertTo-Json
if ($code -ne 0 -or -not $report.leaseClosed -or $report.sourceChanged) { throw 'REAL BUILD FAILED: see output above and the logs in the worktree build folders' }
Write-Output "PASS: real build and tests succeeded as the non-admin build account in $($report.minutes) minutes. No AI turn."
