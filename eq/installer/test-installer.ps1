# End-to-end test of the Songsmith EQ installer. CI runs it after the unit tests.
#
#   .\installer\test-installer.ps1                    # build the installer, then test it
#   .\installer\test-installer.ps1 -Installer <exe>   # test an existing installer
#
# Everything happens inside a temporary folder. The installer runs per-user (/CURRENTUSER)
# with /DIR= and the test-only /VST3DIR=, so it needs no admin rights and never writes to the
# real Common Files\VST3 folder or to %APPDATA%. (Per-user mode still registers the test
# install under HKCU and adds a Start-menu shortcut; the uninstall step at the end removes both.)
param(
    [string]$Installer
)

$ErrorActionPreference = "Stop"

if (-not $Installer) {
    $Installer = (& (Join-Path $PSScriptRoot "build-installer.ps1") | Select-Object -Last 1)
}
$Installer = (Resolve-Path $Installer).Path

$root    = Join-Path ([IO.Path]::GetTempPath()) ("SongsmithEQ-installer-test-" + [guid]::NewGuid().ToString("N").Substring(0, 8))
$appDir  = Join-Path $root "Program Files\Songsmith Studio\Songsmith EQ"
$vst3Dir = Join-Path $root "Common Files\VST3"
$presets = Join-Path $root "AppData\Songsmith Studio\Songsmith EQ\Presets"
$bundle  = Join-Path $vst3Dir "Songsmith EQ.vst3"
$dll     = Join-Path $bundle "Contents\x86_64-win\Songsmith EQ.vst3"
$exe     = Join-Path $appDir "Songsmith EQ.exe"

$script:failures = 0
function Check([bool]$ok, [string]$what) {
    if ($ok) { Write-Host "  ok    $what" }
    else     { Write-Host "  FAIL  $what" -ForegroundColor Red; $script:failures++ }
}

function Run-Setup([string]$log, [string[]]$extra = @()) {
    $args = @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CURRENTUSER",
              "/DIR=`"$appDir`"", "/VST3DIR=`"$vst3Dir`"", "/LOG=`"$(Join-Path $root $log)`"") + $extra
    $p = Start-Process -FilePath $Installer -ArgumentList $args -Wait -PassThru
    return $p.ExitCode
}

function Snapshot {
    # Every file under the test VST3 and app folders, relative, sorted.
    Get-ChildItem -Recurse -File -Path $vst3Dir, $appDir -ErrorAction SilentlyContinue |
        ForEach-Object { $_.FullName.Substring($root.Length) } | Sort-Object
}

$realVst3 = Join-Path ${env:CommonProgramFiles} "VST3"
function RealVst3State {
    # Names and timestamps in the real VST3 folder: must be identical before and after.
    Get-ChildItem -Path $realVst3 -ErrorAction SilentlyContinue | ForEach-Object { "$($_.Name)|$($_.LastWriteTimeUtc.Ticks)" }
}
$realBefore = RealVst3State

try {
    New-Item -ItemType Directory -Force -Path $vst3Dir, $presets | Out-Null
    Write-Host "Installer: $Installer"
    Write-Host "Test root: $root"

    Write-Host "`nPer-user mode without /VST3DIR"
    $p = Start-Process -FilePath $Installer -Wait -PassThru -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
                       "/CURRENTUSER", "/DIR=`"$appDir`"", "/LOG=`"$(Join-Path $root 'install-peruser.log')`""
    Check ($p.ExitCode -ne 0) "refused (exit code $($p.ExitCode)): a real install always goes to Common Files\VST3"
    Check (-not (Test-Path $appDir)) "nothing installed"

    # Leftovers an upgrade must clean up: older bundles with the same plugin ID, and a
    # stale file inside a previous Songsmith EQ bundle.
    foreach ($legacy in @("TW Pro EQ.vst3", "3-Band EQ.vst3")) {
        $d = Join-Path $vst3Dir "$legacy\Contents\x86_64-win"
        New-Item -ItemType Directory -Force -Path $d | Out-Null
        Set-Content (Join-Path $d $legacy) "old plugin"
    }
    New-Item -ItemType Directory -Force -Path $bundle | Out-Null
    Set-Content (Join-Path $bundle "stale-from-old-version.txt") "stale"
    Set-Content (Join-Path $presets "My vocal.sspreset") "<PARAMETERS/>"

    Write-Host "`nFirst install"
    Check ((Run-Setup "install1.log") -eq 0) "installer exits with 0"
    Check (Test-Path $dll) "VST3 DLL installed: $dll"
    Check (Test-Path (Join-Path $bundle "Contents\Resources\moduleinfo.json")) "VST3 moduleinfo.json installed"
    Check (Test-Path $exe) "standalone app installed"
    Check (Test-Path (Join-Path $appDir "unins000.exe")) "uninstaller present"
    Check (-not (Test-Path (Join-Path $vst3Dir "TW Pro EQ.vst3"))) "legacy 'TW Pro EQ.vst3' removed"
    Check (-not (Test-Path (Join-Path $vst3Dir "3-Band EQ.vst3"))) "legacy '3-Band EQ.vst3' removed"
    Check (-not (Test-Path (Join-Path $bundle "stale-from-old-version.txt"))) "stale file from an older bundle removed"
    $first = Snapshot

    Write-Host "`nInstall again over itself (upgrade)"
    Check ((Run-Setup "install2.log") -eq 0) "installer exits with 0"
    $second = Snapshot
    Check ($null -eq (Compare-Object $first $second)) "same files as after the first install (no duplicates, nothing stale)"
    Check (-not (Test-Path (Join-Path $appDir "unins001.exe"))) "still one uninstaller (upgraded in place)"

    Write-Host "`nPlugin in use"
    $lock = [IO.File]::Open($dll, 'Open', 'ReadWrite', 'None')   # what a host loading the DLL does
    try {
        $before = (Get-Item $dll).LastWriteTimeUtc
        $code = Run-Setup "install-locked.log"
        Check ($code -ne 0) "installer refuses while the plugin is loaded (exit code $code)"
        Check ((Test-Path $dll) -and (Get-Item $dll).LastWriteTimeUtc -eq $before) "installed plugin left untouched"
        $log = Get-Content (Join-Path $root "install-locked.log") -Raw
        Check ($log -match "in use") "the log has the 'in use' message"
    } finally {
        $lock.Close()
    }

    Write-Host "`nUninstall"
    $uninstaller = Join-Path $appDir "unins000.exe"
    Start-Process -FilePath $uninstaller -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART" -Wait | Out-Null
    # The uninstaller re-launches itself from a temp copy: wait until it has finished.
    $deadline = (Get-Date).AddSeconds(60)
    while ((Test-Path $uninstaller) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    Check (-not (Test-Path $uninstaller)) "uninstaller finished and removed itself"
    Check (-not (Test-Path $bundle)) "VST3 bundle removed"
    Check (-not (Test-Path $exe)) "standalone app removed"
    Check (Test-Path $vst3Dir) "shared VST3 folder kept"
    Check (Test-Path (Join-Path $presets "My vocal.sspreset")) "user presets kept"

    # The uninstall must never reach user presets, whatever folder they're in.
    $iss = Get-Content (Join-Path $PSScriptRoot "SongsmithEQ.iss") -Raw
    Check ($iss -notmatch '(?im)^\s*Type:.*\{(userappdata|commonappdata|localappdata|appdata)\}') "no delete entries for any app-data folder"

    $key = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{2F8C1884-56C1-45D8-A78D-CF117EC69B44}_is1"
    Check (-not (Test-Path $key)) "test install's 'Installed apps' entry removed"
    Check ($null -eq (Compare-Object @(RealVst3State) @($realBefore))) "real VST3 folder ($realVst3) untouched"
}
finally {
    Remove-Item -Recurse -Force $root -ErrorAction SilentlyContinue
}

if ($script:failures -gt 0) {
    Write-Host "`n$($script:failures) installer check(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host "`nAll installer checks passed"
exit 0
