[CmdletBinding()]
param([Parameter(Mandatory)][string]$Config)
$ErrorActionPreference = 'Stop'
$c = Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
if ($c.liveExecution -ne $false -or $c.worker.enabled -ne $false) { throw 'Live settings must remain disabled' }
$w = $c.worker
$journal = Join-Path (Split-Path -Parent $w.buildCredentialFile) 'active-build-lease.json'
if (Test-Path -LiteralPath $journal) { throw 'A build lease is already active' }
$credential = Import-Clixml -LiteralPath $w.buildCredentialFile
$buildSid = ([Security.Principal.NTAccount]::new($credential.UserName)).Translate([Security.Principal.SecurityIdentifier]).Value
$launcher = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../Invoke-MatrixBuild.ps1'))
$ast = [Management.Automation.Language.Parser]::ParseFile($launcher, [ref]$null, [ref]$null)
foreach ($function in $ast.FindAll({ param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] }, $false)) { . ([scriptblock]::Create($function.Extent.Text)) }
$report = [ordered]@{start=[DateTime]::UtcNow.ToString('o'); controller=[Security.Principal.WindowsIdentity]::GetCurrent().Name; cases=@()}
$private = Split-Path -Parent $w.buildCredentialFile
$reportFile = Join-Path $private ('interruption-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '.json')
function Wait-File([string]$Path, [int]$Seconds) {
    $deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    while (-not (Test-Path -LiteralPath $Path) -and [DateTime]::UtcNow -lt $deadline) {
        if ($outer.HasExited) { throw 'Validation launcher exited before its PID marker was written' }
        Start-Sleep -Milliseconds 250
    }
    if (-not (Test-Path -LiteralPath $Path)) { throw "Timed out waiting for validation marker: $Path" }
}
function Assert-Stopped([int[]]$ProcessIds) {
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    do {
        $remaining = @($ProcessIds | Where-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue })
        if (-not $remaining.Count) { return }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $deadline)
    throw ('Validation processes survived cancellation: ' + ($remaining -join ','))
}
try {
    foreach ($mode in @('build-child', 'controller-tree')) {
        $outer = $null; $pids = $null
        $root = Join-Path $w.worktreeRoot ('INTERRUPT-' + $mode + '-' + [Guid]::NewGuid().ToString('N'))
        $null = New-Item -ItemType Directory -Path (Join-Path $root 'eq'), (Join-Path $root 'build/matrix') -Force
        Set-Content -LiteralPath (Join-Path $root '.git') -Value 'gitdir: private-validation-pointer'
        Set-Content -LiteralPath (Join-Path $root 'eq/main.cpp') -Value @'
#include <windows.h>
#include <fstream>
int main() {
    STARTUPINFOW si{}; si.cb = sizeof(si); PROCESS_INFORMATION pi{};
    wchar_t command[] = L"\"C:\\Program Files\\PowerShell\\7\\pwsh.exe\" -NoProfile -NonInteractive -Command Start-Sleep -Seconds 300";
    if (!CreateProcessW(nullptr, command, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return 2;
    std::ofstream out(MATRIX_PID_FILE);
    if (!out) return 3;
    out << "{\"test\":" << GetCurrentProcessId() << ",\"descendant\":" << pi.dwProcessId << "}";
    out.close(); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    Sleep(300000); return 0;
}
'@
        Set-Content -LiteralPath (Join-Path $root 'eq/CMakeLists.txt') -Value @'
cmake_minimum_required(VERSION 3.22)
project(MatrixInterruption LANGUAGES CXX)
if(NOT IS_DIRECTORY "${JUCE_DIR}")
  message(FATAL_ERROR "Missing JUCE source")
endif()
add_executable(EQTests main.cpp)
target_compile_definitions(EQTests PRIVATE MATRIX_PID_FILE="${CMAKE_CURRENT_BINARY_DIR}/pids.json")
set_target_properties(EQTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/EQTests_artefacts")
'@
        $targets = @(
            @{path=(Join-Path $c.repository '.git'); directory=$true},
            @{path=(Join-Path $c.repository '.git/HEAD'); directory=$false},
            @{path=(Join-Path $c.repository '.git/config'); directory=$false},
            @{path=$c.stateDirectory; directory=$true},
            @{path=$w.worktreeRoot; directory=$true},
            @{path=$w.buildCredentialFile; directory=$false}
        )
        $request = Join-Path $root 'build/matrix/request.json'
        @{worktree=$root; juceDirectory=$w.juceDirectory; plugins=@('eq'); isolationTargets=$targets} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $request
        $arguments = '-NoProfile -NonInteractive -File "{0}" -RequestFile "{1}" -CredentialFile "{2}" -WorktreeRoot "{3}"' -f $launcher,$request,$w.buildCredentialFile,$w.worktreeRoot
        $outer = Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -ArgumentList $arguments -WorkingDirectory $root -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $private "$mode.stdout.log") -RedirectStandardError (Join-Path $private "$mode.stderr.log")
        Wait-File (Join-Path $root 'eq/build/pids.json') 180
        $pids = Get-Content -LiteralPath (Join-Path $root 'eq/build/pids.json') -Raw | ConvertFrom-Json
        $child = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($outer.Id)" | Where-Object { $_.Name -eq 'pwsh.exe' })
        if ($child.Count -ne 1) { throw 'Cannot uniquely identify the credentialed build child' }
        if ($mode -eq 'build-child') {
            Set-Content -LiteralPath (Join-Path $root 'build/matrix/CANCEL_BUILD') -Value 'operator test cancellation'
            if (-not $outer.WaitForExit(30000)) { throw 'Launcher did not finish after child termination' }
            Assert-Stopped @($child[0].ProcessId, $pids.test, $pids.descendant)
            if (Test-Path -LiteralPath $journal) { throw 'Normal failure did not revoke lease' }
        } else {
            & "$env:SystemRoot/System32/taskkill.exe" /PID $outer.Id /T /F
            if ($LASTEXITCODE) { throw 'Cannot terminate validation controller tree' }
            Assert-Stopped @($outer.Id, $child[0].ProcessId, $pids.test, $pids.descendant)
            if (-not (Test-Path -LiteralPath $journal)) { throw 'Forced termination did not retain recovery journal' }
            $rejected = $false
            try { Grant-MatrixBuildLease $root $w.worktreeRoot $buildSid $journal @('eq') }
            catch { if ($_.Exception.Message -like '*Outstanding build lease*') { $rejected = $true } else { throw } }
            if (-not $rejected) { throw 'Outstanding lease was not rejected' }
            & $launcher -RecoverLease $journal
            if (Test-Path -LiteralPath $journal) { throw 'Operator recovery did not clear journal' }
        }
        # Check a known path directly under the real build token after recovery.
        $source = (Join-Path $root 'eq/main.cpp').Replace("'", "''")
        $probe = "try{[IO.File]::OpenRead('$source').Dispose();exit 9}catch [UnauthorizedAccessException]{exit 0}"
        $arguments='-NoProfile -NonInteractive -Command "{0}"' -f $probe
        $check = Invoke-MatrixCredentialProbe $credential $arguments $env:SystemRoot (Join-Path $private "$mode.probe.stdout.log") (Join-Path $private "$mode.probe.stderr.log")
        if ($check.ExitCode -ne 0) { throw "Post-recovery read probe failed with exit $($check.ExitCode); inspect $mode.probe.stderr.log" }
        $report.cases += @{mode=$mode; worktree=$root; descendantsStopped=$true; leaseRecovered=$true; sourceReadDenied=$true}
        Write-Output "PASS: $mode; all recorded descendants stopped; lease recovered; source access denied"
    }
} catch {
    $report.error = $_.Exception.Message
    if ($outer -and -not $outer.HasExited) { & "$env:SystemRoot/System32/taskkill.exe" /PID $outer.Id /T /F }
    if ($pids) { Assert-Stopped @($pids.test, $pids.descendant) }
    if (Test-Path -LiteralPath $journal) {
        $lease = Get-Content -LiteralPath $journal -Raw | ConvertFrom-Json
        if ($lease.root -eq $root) { & $launcher -RecoverLease $journal }
    }
    throw
}
finally { $report.finish = [DateTime]::UtcNow.ToString('o'); $report | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $reportFile; Write-Output "Evidence: $reportFile" }
