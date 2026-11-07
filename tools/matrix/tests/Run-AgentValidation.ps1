[CmdletBinding()]
param([Parameter(Mandatory)][string]$Config, [Parameter(Mandatory)][string]$Revision)
$ErrorActionPreference = 'Stop'
$c = Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
if ($c.liveExecution -ne $false -or $c.worker.enabled -ne $false -or $c.autoMerge -ne $false -or $c.releasePublication -ne $false) { throw 'All live/merge/publication switches must be false' }
$private = Split-Path -Parent $c.worker.buildCredentialFile
$reportFile = Join-Path $private 'agent-validation.json'
$report = [ordered]@{started=[DateTime]::UtcNow.ToString('o'); identity=[Security.Principal.WindowsIdentity]::GetCurrent().Name; revision=$Revision; checks=@()}
Start-Transcript -LiteralPath (Join-Path $private 'agent-validation.log') -Force | Out-Null
try {
    if ($report.identity -ne 'MATRIX\SongsmithAgent') { throw 'Unexpected controller identity' }
    & (Join-Path $PSScriptRoot 'Test-ControllerContext.ps1') -Config $Config -Output (Join-Path $private 'agent-context.json')
    if ($LASTEXITCODE) { throw 'Scheduled identity or DPAPI check failed' }
    $report.checks += 'non-admin scheduled identity and build-credential decryption'
    # Authenticate/read metadata only: never start a Claude model turn or print tokens.
    $auth = & $c.claudeUsage.executable auth status --json 2>$null
    if ($LASTEXITCODE -eq 0) { $a = $auth | ConvertFrom-Json; $report.claudeLoggedIn = $a.loggedIn; $report.claudeAuthMethod = $a.authMethod }
    else { $report.claudeLoggedIn = $false }
    $env:GIT_TERMINAL_PROMPT = '0'
    $remote = (& $c.worker.gitExecutable -c "safe.directory=$($c.repository)" -C $c.repository remote get-url origin).Trim()
    if ($remote -notin @('https://github.com/tonygward/audio-plugins','https://github.com/tonygward/audio-plugins.git')) { throw 'Unexpected source origin' }
    # The dedicated clone is administrator-provisioned. Trust only this reviewed
    # absolute path in Agent's own Git profile; never use safe.directory=*.
    & $c.worker.gitExecutable config --global --add safe.directory $c.repository
    if ($LASTEXITCODE) { throw 'Cannot configure the dedicated source path' }
    & $c.worker.gitExecutable -C $c.repository fetch --no-tags origin fix/auto-01-worktree-build-lease
    if ($LASTEXITCODE) { throw 'Agent source fetch failed' }
    $pinned = (& $c.worker.gitExecutable -C $c.repository rev-parse --verify $Revision).Trim()
    if ($pinned -ne $Revision) { throw 'Cannot pin validation source' }
    & (Join-Path $PSScriptRoot 'Test-BuildLease.ps1') -Config $Config
    if ($LASTEXITCODE) { throw 'Effective ACL checks failed' }
    $report.checks += 'effective ACL and credential isolation; installed launcher read'
    & (Join-Path $PSScriptRoot 'Test-BuildInterruption.ps1') -Config $Config
    if ($LASTEXITCODE) { throw 'Interruption checks failed' }
    $report.checks += 'build-child/controller-tree interruption and ACL recovery'
    & (Join-Path $PSScriptRoot 'Test-BuildLinkSafety.ps1') -Config $Config
    if ($LASTEXITCODE) { throw 'Link checks failed' }
    $report.checks += 'hard-link rejection and hostile junction containment'
    & (Join-Path $PSScriptRoot 'Test-RealEqBuild.ps1') -Config $Config -Revision $Revision
    if ($LASTEXITCODE) { throw 'Real EQ build/tests failed' }
    $latest = Get-ChildItem -LiteralPath $c.worker.worktreeRoot -Directory -Filter EQ-VALIDATION-* | Sort-Object CreationTime -Descending | Select-Object -First 1
    $build = Get-Content -LiteralPath (Join-Path $latest.FullName 'build/matrix/validation-report.json') -Raw | ConvertFrom-Json
    $report.realEqBuild = $build
    if (@($build.warningLines).Count) { throw 'Real EQ build produced warnings' }
    $report.checks += 'real warning-free EQ/JUCE Release build and tests'
    $report.passed = $true
} catch { $report.passed = $false; $report.error = $_.Exception.Message }
finally {
    $report.finished = [DateTime]::UtcNow.ToString('o')
    $report.activeLease = Test-Path -LiteralPath (Join-Path $private 'active-build-lease.json')
    $report | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $reportFile
    Stop-Transcript | Out-Null
}
if (-not $report.passed) { exit 1 }
