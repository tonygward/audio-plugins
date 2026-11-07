[CmdletBinding()]
param([Parameter(Mandatory)][string]$Config)
$ErrorActionPreference = 'Stop'
$c = Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
if ($c.liveExecution -ne $false -or $c.worker.enabled -ne $false) { throw 'Live settings must remain disabled' }
$w = $c.worker
$journal = Join-Path (Split-Path -Parent $w.buildCredentialFile) 'active-build-lease.json'
if (Test-Path -LiteralPath $journal) { throw 'Outstanding lease requires recovery' }
$credential = Import-Clixml -LiteralPath $w.buildCredentialFile
$sid = ([Security.Principal.NTAccount]::new($credential.UserName)).Translate([Security.Principal.SecurityIdentifier]).Value
$launcher = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Invoke-MatrixBuild.ps1'))
$ast = [Management.Automation.Language.Parser]::ParseFile($launcher, [ref]$null, [ref]$null)
foreach ($function in $ast.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] }, $false)) { . ([scriptblock]::Create($function.Extent.Text)) }
$root = Join-Path $w.worktreeRoot ('LINK-VALIDATION-' + [Guid]::NewGuid().ToString('N'))
$target = Join-Path $w.worktreeRoot ('LINK-UNRELATED-' + [Guid]::NewGuid().ToString('N'))
$null = New-Item -ItemType Directory -Path (Join-Path $root 'eq/Source'), $target -Force
$source = Join-Path $root 'eq/Source/original.txt'
$sentinel = Join-Path $target 'sentinel.txt'
Set-Content -LiteralPath $source -Value 'source'
Set-Content -LiteralPath $sentinel -Value 'unrelated target'
$targetAcl = (Get-Acl -LiteralPath $target).GetSecurityDescriptorSddlForm('Access')
$sentinelAcl = (Get-Acl -LiteralPath $sentinel).GetSecurityDescriptorSddlForm('Access')
$sentinelHash = (Get-FileHash -LiteralPath $sentinel).Hash
$report = [ordered]@{root=$root; target=$target; hardLinkRejected=$false; outputJunctionCreatedByBuildAccount=$false; recoveryRejectedJunction=$false; journalRetained=$false; operatorRecovery=$false; unrelatedUntouched=$false}
$reportFile = Join-Path (Split-Path -Parent $w.buildCredentialFile) ('link-validation-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '.json')
$link = Join-Path $root 'eq/Source/hardlink.txt'
$null = New-Item -ItemType HardLink -Path $link -Target $source
try {
    try { Grant-MatrixBuildLease $root $w.worktreeRoot $sid $journal @('eq') }
    catch { if ($_.Exception.Message -like '*Linked lease item*') { $report.hardLinkRejected = $true } else { throw } }
    if (-not $report.hardLinkRejected -or (Test-Path -LiteralPath $journal)) { throw 'Linked source was not rejected before granting access' }
    Remove-Item -LiteralPath $link
    $link = Join-Path $root 'eq/build/output-junction'
    $helper=Join-Path $root 'create-junction.ps1'
    Set-Content -LiteralPath $helper -Value ('& "$env:SystemRoot\System32\cmd.exe" /d /c mklink /J "{0}" "{1}"; exit $LASTEXITCODE' -f $link,$target)
    Grant-MatrixBuildLease $root $w.worktreeRoot $sid $journal @('eq')
    # Junction creation is performed as the untrusted build identity. Targets are
    # disposable fixtures; never link to personal, credential or production data.
    $arguments = '-NoProfile -NonInteractive -File "{0}"' -f $helper
    $child = Invoke-MatrixCredentialProbe $credential $arguments $env:SystemRoot (Join-Path (Split-Path -Parent $journal) 'junction.stdout.log') (Join-Path (Split-Path -Parent $journal) 'junction.stderr.log')
    if ($child.ExitCode -ne 0 -or (Get-Item -LiteralPath $link -Force).LinkType -ne 'Junction') { throw 'Build account could not create the test junction' }
    $report.outputJunctionCreatedByBuildAccount = $true
    try { Restore-MatrixBuildLease $journal }
    catch { if ($_.Exception.Message -like '*Linked lease item*') { $report.recoveryRejectedJunction = $true } else { throw } }
    $report.journalRetained = Test-Path -LiteralPath $journal
    if (-not $report.recoveryRejectedJunction -or -not $report.journalRetained) { throw 'Recovery followed a junction or lost its journal' }
    $report.unrelatedUntouched = (Get-Acl -LiteralPath $target).GetSecurityDescriptorSddlForm('Access') -eq $targetAcl -and (Get-Acl -LiteralPath $sentinel).GetSecurityDescriptorSddlForm('Access') -eq $sentinelAcl -and (Get-FileHash -LiteralPath $sentinel).Hash -eq $sentinelHash
    if (-not $report.unrelatedUntouched) { throw 'Unrelated target was changed' }
    Write-Output 'PASS: hard-linked source rejected; hostile output junction not followed; unrelated target unchanged; recovery journal retained'
} catch { $report.error = $_.Exception.Message; throw }
finally {
    # Delete only the validated junction itself, with no traversal or recursion.
    if ((Test-Path -LiteralPath $link) -and (Get-Item -LiteralPath $link -Force).LinkType -eq 'Junction') {
        if (-not $link.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Cleanup escaped validation worktree' }
        [IO.Directory]::Delete($link)
    }
    if (Test-Path -LiteralPath $journal) {
        $lease = Get-Content -LiteralPath $journal -Raw | ConvertFrom-Json
        if ($lease.root -eq $root) { Restore-MatrixBuildLease $journal; $report.operatorRecovery = $true }
    }
    $report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportFile
    Write-Output "Evidence: $reportFile"
}
if (-not $report.operatorRecovery) { throw 'Operator recovery did not run' }
