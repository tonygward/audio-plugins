[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Config,
    [string]$BuildAccount = 'MATRIX\SongsmithBuild',
    [switch]$AuditKnownGitExposure
)
$ErrorActionPreference = 'Stop'
# Explicit operator-only integration test. This script changes ACLs and must never
# be invoked by the scanner or ordinary Node tests. Run as the controller identity.
$settings = Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
if ($settings.liveExecution -ne $false -or $settings.worker.enabled -ne $false) { throw 'Disable live execution and worker before ACL integration testing' }
$w = $settings.worker
$credential = Import-Clixml -LiteralPath $w.buildCredentialFile
if ($credential -isnot [PSCredential] -or $credential.UserName -ne $BuildAccount) { throw 'Unexpected build credential identity' }
$buildSid = ([Security.Principal.NTAccount]::new($BuildAccount)).Translate([Security.Principal.SecurityIdentifier]).Value
$launcher = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Invoke-MatrixBuild.ps1'))
$ast = [Management.Automation.Language.Parser]::ParseFile($launcher, [ref]$null, [ref]$null)
foreach ($function in $ast.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] }, $false)) {
    . ([scriptblock]::Create($function.Extent.Text))
}
$root = Join-Path $w.worktreeRoot ('ACL-PILOT-' + [Guid]::NewGuid().ToString('N'))
$sibling = Join-Path $w.worktreeRoot ('ACL-UNRELATED-' + [Guid]::NewGuid().ToString('N'))
$journal = Join-Path (Split-Path -Parent $w.buildCredentialFile) 'active-build-lease.json'
if (Test-Path -LiteralPath $journal) { throw 'Outstanding lease: recover it before testing' }
$null = New-Item -ItemType Directory -Path (Join-Path $root 'eq'), $sibling -Force
Set-Content -LiteralPath (Join-Path $sibling 'sentinel.txt') -Value 'unrelated'
Set-Content -LiteralPath (Join-Path $root '.git') -Value 'gitdir: protected-test-pointer'
Set-Content -LiteralPath (Join-Path $root 'eq/main.cpp') -Value 'int main() { return 0; }'
Set-Content -LiteralPath (Join-Path $root 'eq/CMakeLists.txt') -Value @'
cmake_minimum_required(VERSION 3.22)
project(MatrixAclProbe LANGUAGES CXX)
if(NOT IS_DIRECTORY "${JUCE_DIR}")
  message(FATAL_ERROR "JUCE source must be readable")
endif()
add_executable(EQTests main.cpp)
set_target_properties(EQTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/EQTests_artefacts")
'@
$null = New-Item -ItemType Directory -Path (Join-Path $root 'build/matrix') -Force
$requestFile = Join-Path $root 'build/matrix/request.json'
@{worktree=$root; juceDirectory=$w.juceDirectory; plugins=@('eq')} | ConvertTo-Json | Set-Content -LiteralPath $requestFile
$probe = Join-Path $root 'probe.ps1'
$protected = @(
    (Join-Path $settings.repository '.git'),
    (Join-Path $settings.repository '.git/HEAD'),
    (Join-Path $settings.repository '.git/config'),
    $settings.stateDirectory,
    $w.worktreeRoot,
    $w.buildCredentialFile,
    (Join-Path $sibling 'sentinel.txt'),
    (Join-Path $root '.git')
)
foreach ($candidate in @((Join-Path $env:USERPROFILE '.claude'), (Join-Path $env:APPDATA 'GitHub CLI'), (Join-Path $env:APPDATA 'gh'))) {
    if (Test-Path -LiteralPath $candidate) { $protected += $candidate }
}
$protectedItems = @($protected | ForEach-Object {
    $item = Get-Item -LiteralPath $_ -Force -ErrorAction Stop
    @{path=$item.FullName; directory=[bool]$item.PSIsContainer}
})
@{worktree=$root; juceDirectory=$w.juceDirectory; plugins=@('eq'); isolationTargets=$protectedItems} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $requestFile
@{root=$root; protected=$protectedItems; juce=$w.juceDirectory; sid=$buildSid} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $root 'probe.json')
Set-Content -LiteralPath $probe -Value @'
$ErrorActionPreference = 'Stop'
$p = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'probe.json') -Raw | ConvertFrom-Json
if ([Security.Principal.WindowsIdentity]::GetCurrent().User.Value -ne $p.sid) { throw 'Wrong effective identity' }
$script:violations = @()
$script:case = ''
function Denied([scriptblock]$Action) {
    try { & $Action; $script:violations += $script:case }
    catch [UnauthorizedAccessException] { }
    catch [System.Security.SecurityException] { }
}
$null = Get-Content -LiteralPath (Join-Path $p.root 'eq/main.cpp')
$null = Get-Content -LiteralPath (Join-Path $p.juce 'CMakeLists.txt')
foreach ($target in $p.protected) {
    $script:case = 'read ' + $target.path
    Write-Output ('Checking denied read: ' + $target.path)
    Denied { if ($target.directory) { $null = Get-ChildItem -LiteralPath $target.path -Force -ErrorAction Stop } else { $s = [IO.File]::OpenRead($target.path); $s.Dispose() } }
}
$script:case = 'source write'
Denied { [IO.File]::AppendAllText((Join-Path $p.root 'eq/main.cpp'), 'changed') }
$script:case = 'JUCE write'
Denied { [IO.File]::WriteAllText((Join-Path $p.juce ('matrix-denied-' + [Guid]::NewGuid() + '.tmp')), 'denied') }
$output = Join-Path $p.root 'eq/build/owner-probe.txt'
[IO.File]::WriteAllText($output, 'output works')
$script:case = 'output WRITE_DAC'
Denied { $acl = Get-Acl -LiteralPath $output; Set-Acl -LiteralPath $output -AclObject $acl -ErrorAction Stop }
if ($script:violations.Count) { Write-Output ('VIOLATIONS:' + (ConvertTo-Json -Compress @($script:violations))); exit 74 }
Write-Output 'PASS: source read; build write; source/JUCE write denied; metadata/state/credential/sibling reads denied; output WRITE_DAC denied'
'@
$before = @{}
$parentDacl = (Get-Acl -LiteralPath $w.worktreeRoot).GetSecurityDescriptorSddlForm('Access')
$siblingDacl = (Get-Acl -LiteralPath $sibling).GetSecurityDescriptorSddlForm('Access')
foreach ($item in Get-MatrixLeaseItems $root) { $before[$item.FullName] = (Get-Acl -LiteralPath $item.FullName).GetSecurityDescriptorSddlForm('Access') }
$report = Join-Path (Split-Path -Parent $w.buildCredentialFile) 'build-lease-test.stdout.log'
$errors = Join-Path (Split-Path -Parent $w.buildCredentialFile) 'build-lease-test.stderr.log'
try {
    Grant-MatrixBuildLease $root $w.worktreeRoot $buildSid $journal @('eq')
    try {
        $argsList = @('-NoProfile', '-NonInteractive', '-File', ('"{0}"' -f $probe))
        $process = Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -Credential $credential -LoadUserProfile -ArgumentList $argsList -WorkingDirectory $root -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $report -RedirectStandardError $errors
        if ($process.ExitCode -ne 0) {
            $line = Get-Content -LiteralPath $report | Where-Object { $_.StartsWith('VIOLATIONS:') } | Select-Object -Last 1
            $violations = if ($line) { @($line.Substring(11) | ConvertFrom-Json) } else { @() }
            $violations = @($violations)
            if (-not $AuditKnownGitExposure -or $process.ExitCode -ne 74 -or $violations.Count -ne 1 -or $violations[0] -ne ('read ' + [IO.Path]::GetFullPath((Join-Path $settings.repository '.git')))) {
                Get-Content -LiteralPath $report; throw "Effective ACL probe failed; inspect $errors"
            }
            Write-Warning 'Confirmed existing Git metadata exposure; audit will verify launcher refusal and revocation, not approve deployment'
        }
        Get-Content -LiteralPath $report
    } finally { Restore-MatrixBuildLease $journal }
    foreach ($path in $before.Keys) {
        if ((Get-Acl -LiteralPath $path).GetSecurityDescriptorSddlForm('Access') -ne $before[$path]) { throw "DACL restoration mismatch: $path" }
    }
    # Exercise the actual launcher, job containment and separate-account compiler.
    & $launcher -RequestFile $requestFile -CredentialFile $w.buildCredentialFile -WorktreeRoot $w.worktreeRoot
    if ($AuditKnownGitExposure) {
        if ($LASTEXITCODE -ne 74) { throw 'Launcher did not refuse exposed Git metadata' }
        Write-Output 'PASS: final launcher refused compilation with exit 74 for exposed Git metadata'
    } elseif ($LASTEXITCODE -ne 0) { throw 'Compilation through trusted launcher failed' }
    if (Test-Path -LiteralPath $journal) { throw 'Lease was not revoked' }
    if ((Get-Acl -LiteralPath $w.worktreeRoot).GetSecurityDescriptorSddlForm('Access') -ne $parentDacl -or (Get-Acl -LiteralPath $sibling).GetSecurityDescriptorSddlForm('Access') -ne $siblingDacl) { throw 'Parent or sibling DACL changed' }
    $sourceJson = ConvertTo-Json -Compress @((Join-Path $root 'eq/main.cpp'), (Join-Path $root 'eq/build/owner-probe.txt'))
    $postProbe = @'
$ErrorActionPreference = 'Stop'
$paths = ConvertFrom-Json '__PATHS__'
foreach ($path in $paths) {
    try { $s = [IO.File]::OpenRead($path); $s.Dispose(); throw 'Read succeeded after revocation' }
    catch [UnauthorizedAccessException] { }
}
try { $null = Get-ChildItem -LiteralPath '__PARENT__' -ErrorAction Stop; throw 'Parent listing succeeded' }
catch [UnauthorizedAccessException] { }
Write-Output 'PASS: source and build-owned output inaccessible after revocation; worktrees parent listing still denied'
'@
    $postProbe = $postProbe.Replace('__PATHS__', $sourceJson.Replace("'", "''")).Replace('__PARENT__', $w.worktreeRoot.Replace("'", "''"))
    # CreateProcessWithLogonW has a short command-line bound; avoid base64 expansion.
    $postProbe = ($postProbe -split '\r?\n' | Where-Object { $_.Trim() }) -join '; '
    $postProbe = $postProbe -replace ';\s*catch', ' catch'
    $process = Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -Credential $credential -LoadUserProfile -ArgumentList @('-NoProfile', '-NonInteractive', '-Command', ('"{0}"' -f $postProbe.Replace('"', '\"'))) -WorkingDirectory $env:SystemRoot -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $report -RedirectStandardError $errors
    if ($process.ExitCode -ne 0) { throw "Post-revocation access probe failed; inspect $errors" }
    Get-Content -LiteralPath $report
    if ($AuditKnownGitExposure) { Write-Output "AUDIT COMPLETE; deployment BLOCKED on common Git ACL. Retained fixture: $root" }
    else { Write-Output "PASS: actual separate-account CMake configure/build/test and ACL restoration. Retained fixture: $root" }
} finally {
    if (Test-Path -LiteralPath $journal) { Write-Warning "Lease requires recovery: & '$launcher' -RecoverLease '$journal'" }
}
# Retain fixtures and logs for inspection. No protected production ACL is changed.
