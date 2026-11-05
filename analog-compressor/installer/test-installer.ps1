# End-to-end test of the Songsmith Analog Compressor installer. CI runs it after AnalogCompressorTests.
#
#   .\installer\test-installer.ps1                    # build the installer, then test it
#   .\installer\test-installer.ps1 -Installer <exe>   # test an existing installer
#
# Isolated like eq\installer\test-installer.ps1: everything happens in a temporary folder, the
# installer runs per-user (/CURRENTUSER) with /DIR= and the test-only /VST3DIR=, so it needs no
# admin rights and never writes to the real Common Files\VST3 folder or to %APPDATA%.
#
# On top of the EQ test, it seeds a Songsmith EQ install and Songsmith EQ presets next to this
# plugin's own user presets, and checks that install, upgrade and uninstall leave all of them
# byte-for-byte untouched.
param(
    [string]$Installer
)

$ErrorActionPreference = "Stop"

if (-not $Installer) {
    $Installer = (& (Join-Path $PSScriptRoot "build-installer.ps1") | Select-Object -Last 1)
}
$Installer = (Resolve-Path $Installer).Path

$name    = "Songsmith Analog Compressor"
$root    = Join-Path ([IO.Path]::GetTempPath()) ("SongsmithFET-installer-test-" + [guid]::NewGuid().ToString("N").Substring(0, 8))
$appDir  = Join-Path $root "Program Files\Songsmith Studio\$name"
$vst3Dir = Join-Path $root "Common Files\VST3"
$presets = Join-Path $root "AppData\Songsmith Studio\$name\Presets"
$bundle  = Join-Path $vst3Dir "$name.vst3"
$dll     = Join-Path $bundle "Contents\x86_64-win\$name.vst3"
$exe     = Join-Path $appDir "$name.exe"

# Songsmith EQ, installed alongside: must never be touched.
$eqBundle  = Join-Path $vst3Dir "Songsmith EQ.vst3"
$eqDll     = Join-Path $eqBundle "Contents\x86_64-win\Songsmith EQ.vst3"
$eqApp     = Join-Path $root "Program Files\Songsmith Studio\Songsmith EQ\Songsmith EQ.exe"
$eqPresets = Join-Path $root "AppData\Songsmith Studio\Songsmith EQ\Presets"

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
    Get-ChildItem -Recurse -File -Path $bundle, $appDir -ErrorAction SilentlyContinue |
        ForEach-Object { $_.FullName.Substring($root.Length) } | Sort-Object
}

# Hash of everything that isn't ours: Songsmith EQ's files and every preset.
function Others {
    Get-ChildItem -Recurse -File -Path $eqBundle, (Split-Path $eqApp), $eqPresets, $presets -ErrorAction SilentlyContinue |
        ForEach-Object { "$($_.FullName.Substring($root.Length))|$((Get-FileHash $_.FullName).Hash)" } | Sort-Object
}

$realVst3 = Join-Path ${env:CommonProgramFiles} "VST3"
function RealVst3State {
    Get-ChildItem -Path $realVst3 -ErrorAction SilentlyContinue | ForEach-Object { "$($_.Name)|$($_.LastWriteTimeUtc.Ticks)" }
}
$realBefore = RealVst3State
$realPresets = Join-Path $env:APPDATA "Songsmith Studio"
function RealPresetsState {
    Get-ChildItem -Recurse -File -Path $realPresets -ErrorAction SilentlyContinue | ForEach-Object { "$($_.FullName)|$($_.LastWriteTimeUtc.Ticks)" }
}
$realPresetsBefore = RealPresetsState

try {
    New-Item -ItemType Directory -Force -Path $vst3Dir, $presets, $eqPresets, (Split-Path $eqDll), (Split-Path $eqApp) | Out-Null
    Write-Host "Installer: $Installer"
    Write-Host "Test root: $root"

    Write-Host "`nPer-user mode without /VST3DIR"
    $p = Start-Process -FilePath $Installer -Wait -PassThru -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART",
                       "/CURRENTUSER", "/DIR=`"$appDir`"", "/LOG=`"$(Join-Path $root 'install-peruser.log')`""
    Check ($p.ExitCode -ne 0) "refused (exit code $($p.ExitCode)): a real install always goes to Common Files\VST3"
    Check (-not (Test-Path $appDir)) "nothing installed"

    # A Songsmith EQ install and presets for both plugins.
    Set-Content $eqDll "songsmith eq plugin"
    Set-Content $eqApp "songsmith eq app"
    Set-Content (Join-Path $eqPresets "My EQ.sspreset") "<PARAMETERS/>"
    Set-Content (Join-Path $presets "My vocal comp.sspreset") "<SongsmithAnalogCompressorPreset input=`"3`"/>"
    # A stale file inside an older bundle of ours.
    New-Item -ItemType Directory -Force -Path $bundle | Out-Null
    Set-Content (Join-Path $bundle "stale-from-old-version.txt") "stale"
    $othersBefore = Others

    Write-Host "`nFirst install"
    Check ((Run-Setup "install1.log") -eq 0) "installer exits with 0"
    Check (Test-Path $dll) "VST3 DLL installed: $dll"
    Check (Test-Path (Join-Path $bundle "Contents\Resources\moduleinfo.json")) "VST3 moduleinfo.json installed"
    Check (Test-Path $exe) "standalone app installed"
    Check (Test-Path (Join-Path $appDir "unins000.exe")) "uninstaller present"
    Check (-not (Test-Path (Join-Path $bundle "stale-from-old-version.txt"))) "stale file from an older bundle removed"
    Check ($null -eq (Compare-Object @(Others) @($othersBefore))) "Songsmith EQ and all presets untouched"
    $first = Snapshot

    Write-Host "`nInstall again over itself (upgrade)"
    Check ((Run-Setup "install2.log") -eq 0) "installer exits with 0"
    Check ($null -eq (Compare-Object $first (Snapshot))) "same files as after the first install"
    Check (-not (Test-Path (Join-Path $appDir "unins001.exe"))) "still one uninstaller (upgraded in place)"
    Check ($null -eq (Compare-Object @(Others) @($othersBefore))) "Songsmith EQ and all presets untouched"

    Write-Host "`nPlugin in use"
    $lock = [IO.File]::Open($dll, 'Open', 'ReadWrite', 'None')
    try {
        $before = (Get-Item $dll).LastWriteTimeUtc
        $code = Run-Setup "install-locked.log"
        Check ($code -ne 0) "installer refuses while the plugin is loaded (exit code $code)"
        Check ((Test-Path $dll) -and (Get-Item $dll).LastWriteTimeUtc -eq $before) "installed plugin left untouched"
        Check ((Get-Content (Join-Path $root "install-locked.log") -Raw) -match "in use") "the log has the 'in use' message"
    } finally {
        $lock.Close()
    }

    Write-Host "`nSongsmith EQ in use doesn't block this installer"
    $eqLock = [IO.File]::Open($eqDll, 'Open', 'ReadWrite', 'None')
    try {
        Check ((Run-Setup "install-eq-locked.log") -eq 0) "installer exits with 0 while Songsmith EQ is loaded"
    } finally {
        $eqLock.Close()
    }

    Write-Host "`nUninstall"
    $uninstaller = Join-Path $appDir "unins000.exe"
    Start-Process -FilePath $uninstaller -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART" -Wait | Out-Null
    $deadline = (Get-Date).AddSeconds(60)
    while ((Test-Path $uninstaller) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    Check (-not (Test-Path $uninstaller)) "uninstaller finished and removed itself"
    Check (-not (Test-Path $bundle)) "VST3 bundle removed"
    Check (-not (Test-Path $exe)) "standalone app removed"
    Check (Test-Path $vst3Dir) "shared VST3 folder kept"
    Check ($null -eq (Compare-Object @(Others) @($othersBefore))) "Songsmith EQ and all presets untouched"

    $iss = Get-Content (Join-Path $PSScriptRoot "SongsmithAnalogCompressor.iss") -Raw
    Check ($iss -notmatch '(?im)^\s*Type:.*\{(userappdata|commonappdata|localappdata|appdata)\}') "no delete entries for any app-data folder"
    Check ($iss -notmatch '(?im)^\[Components\]') "no Components page: the VST3 and the standalone app always install together"
    Check ($iss -notmatch '(?im)^\s*Type:.*Songsmith EQ') "no delete entries for Songsmith EQ"
    Check ($iss -notmatch '2F8C1884-56C1-45D8-A78D-CF117EC69B44') "doesn't reuse Songsmith EQ's AppId"

    $key = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{FF3BD6A4-58D4-4AFA-BFA3-26B221E29340}_is1"
    Check (-not (Test-Path $key)) "test install's 'Installed apps' entry removed"
    Check ($null -eq (Compare-Object @(RealVst3State) @($realBefore))) "real VST3 folder ($realVst3) untouched"
    Check ($null -eq (Compare-Object @(RealPresetsState) @($realPresetsBefore))) "real presets ($realPresets) untouched"
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
