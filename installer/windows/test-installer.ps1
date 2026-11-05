# End-to-end test of the Songsmith Studio suite installer. CI runs it after building the plugins.
#
#   .\installer\windows\test-installer.ps1                                   # build both installers, then test
#   .\installer\windows\test-installer.ps1 -Installer <exe> -EqInstaller <exe> -OlderInstaller <exe>
#
# -EqInstaller is Songsmith EQ's own installer (eq/installer), used to check that the suite
# replaces a per-plugin install. -OlderInstaller is a suite installer with a lower version, used
# to check upgrades; if it's missing, it's built from the same plugins with an older version (see $olderVersion).
# -EqDir, -FetDir and -MaxDir are the plugins' Release artefacts, as for build-installer.ps1
# (CI passes the folders it downloaded; locally the plugins' own build folders are used).
#
# Everything happens inside a temporary folder. The installers run per-user (/CURRENTUSER) with
# /DIR= and the test-only /VST3DIR=, so they need no admin rights and never write to the real
# Common Files\VST3 folder or to %APPDATA%. (Per-user mode still registers each test install
# under HKCU and adds Start-menu shortcuts; the uninstall steps remove both, and the cleanup at
# the end uninstalls anything a failed run left behind.)
param(
    [string]$Installer,
    [string]$EqInstaller,
    [string]$OlderInstaller,
    [string]$EqDir,
    [string]$FetDir,
    [string]$MaxDir
)

$ErrorActionPreference = "Stop"
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

$root     = Join-Path ([IO.Path]::GetTempPath()) ("SongsmithStudio-installer-test-" + [guid]::NewGuid().ToString("N").Substring(0, 8))
$suiteDir = Join-Path $root "Program Files\Songsmith Studio"
$vst3Dir  = Join-Path $root "Common Files\VST3"
$appData  = Join-Path $root "AppData\Songsmith Studio"

$builds = @{}
if ($EqDir)  { $builds.EqDir  = $EqDir }
if ($FetDir) { $builds.FetDir = $FetDir }
if ($MaxDir) { $builds.MaxDir = $MaxDir }
if (-not $Installer) {
    $Installer = (& (Join-Path $PSScriptRoot "build-installer.ps1") @builds | Select-Object -Last 1)
}
# An "older" suite version derived from VERSION: 0.<major>.<minor> is always lower (major is at least 1).
$sharedVersion = (Get-Content (Join-Path $repo "VERSION") -Raw).Trim()
$olderVersion  = "0.$($sharedVersion.Split('.')[0]).$($sharedVersion.Split('.')[1])"
if (-not $OlderInstaller) {
    $OlderInstaller = (& (Join-Path $PSScriptRoot "build-installer.ps1") @builds -SuiteVersion $olderVersion -OutDir (Join-Path $root "older") | Select-Object -Last 1)
}
if (-not $EqInstaller) {
    $EqInstaller = (& (Join-Path $repo "eq\installer\build-installer.ps1") | Select-Object -Last 1)
}
$Installer      = (Resolve-Path $Installer).Path
$OlderInstaller = (Resolve-Path $OlderInstaller).Path
$EqInstaller    = (Resolve-Path $EqInstaller).Path

$plugins = [ordered]@{
    eq  = "Songsmith EQ"
    fet = "Songsmith Analog Compressor"
    max = "Songsmith Maximiser"
}
function Bundle([string]$key) { Join-Path $vst3Dir "$($plugins[$key]).vst3" }
function Dll([string]$key)    { Join-Path (Bundle $key) "Contents\x86_64-win\$($plugins[$key]).vst3" }
function Exe([string]$key)    { Join-Path $suiteDir "$($plugins[$key])\$($plugins[$key]).exe" }

$suiteKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{9AC9C845-D81C-4667-BBD4-C412BC291211}_is1"
$eqKey    = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\{2F8C1884-56C1-45D8-A78D-CF117EC69B44}_is1"
$suiteUninstaller = Join-Path $suiteDir "unins000.exe"

$script:failures = 0
function Check([bool]$ok, [string]$what) {
    if ($ok) { Write-Host "  ok    $what" }
    else     { Write-Host "  FAIL  $what" -ForegroundColor Red; $script:failures++ }
}

function Run-Suite([string]$exe, [string]$log, [string]$components) {
    $args = @("/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CURRENTUSER",
              "/DIR=`"$suiteDir`"", "/VST3DIR=`"$vst3Dir`"", "/LOG=`"$(Join-Path $root $log)`"")
    if ($components) { $args += "/COMPONENTS=`"$components`"" }
    return (Start-Process -FilePath $exe -ArgumentList $args -Wait -PassThru).ExitCode
}

function Wait-Gone([string]$path) {
    # Inno Setup uninstallers re-launch themselves from a temp copy: wait until they've finished.
    $deadline = (Get-Date).AddSeconds(60)
    while ((Test-Path $path) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
}

function Uninstall([string]$uninstaller) {
    if (-not (Test-Path $uninstaller)) { return }
    Start-Process -FilePath $uninstaller -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART" -Wait | Out-Null
    Wait-Gone $uninstaller
}

function Expect-Installed([string[]]$keys, [string]$context) {
    foreach ($k in $plugins.Keys) {
        $want = $keys -contains $k
        Check ((Test-Path (Dll $k)) -eq $want) "$context`: $($plugins[$k]) VST3 $(if ($want) { 'installed' } else { 'not installed' })"
        Check ((Test-Path (Exe $k)) -eq $want) "$context`: $($plugins[$k]) standalone app $(if ($want) { 'installed' } else { 'not installed' })"
    }
}

function Apps-Entries {
    # Songsmith entries in Settings > Apps for this user (the test installs are per-user).
    @(Get-ChildItem "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall" -ErrorAction SilentlyContinue |
        Where-Object { $_.PSChildName -in @("{9AC9C845-D81C-4667-BBD4-C412BC291211}_is1", "{2F8C1884-56C1-45D8-A78D-CF117EC69B44}_is1",
                                            "{FF3BD6A4-58D4-4AFA-BFA3-26B221E29340}_is1", "{A888B7E4-3776-4CCD-9FA2-3B5CC0415C6C}_is1") })
}

function Snapshot {
    Get-ChildItem -Recurse -File -Path $vst3Dir, $suiteDir -ErrorAction SilentlyContinue |
        ForEach-Object { "$($_.FullName.Substring($root.Length))|$($_.LastWriteTimeUtc.Ticks)" } | Sort-Object
}

$realVst3 = Join-Path ${env:CommonProgramFiles} "VST3"
function RealVst3State {
    Get-ChildItem -Path $realVst3 -ErrorAction SilentlyContinue | ForEach-Object { "$($_.Name)|$($_.LastWriteTimeUtc.Ticks)" }
}
$realBefore = RealVst3State

if ((Test-Path $suiteKey) -or (Test-Path $eqKey)) {
    throw "A per-user Songsmith Studio or Songsmith EQ install is registered for this user already; this test would replace it. Uninstall it first."
}

try {
    New-Item -ItemType Directory -Force -Path $vst3Dir | Out-Null
    Write-Host "Installer:       $Installer"
    Write-Host "Older installer: $OlderInstaller"
    Write-Host "EQ installer:    $EqInstaller"
    Write-Host "Test root:       $root"

    # A preset in each plugin's folder: nothing may ever touch these.
    $presets = @()
    foreach ($name in $plugins.Values) {
        $dir = Join-Path $appData "$name\Presets"
        New-Item -ItemType Directory -Force -Path $dir | Out-Null
        $file = Join-Path $dir "Mine.sspreset"
        Set-Content $file "<PARAMETERS/>"
        $presets += $file
    }

    Write-Host "`n1. Everything ticked"
    Check ((Run-Suite $Installer "1-all.log" "") -eq 0) "installer exits with 0"
    Expect-Installed @("eq", "fet", "max") "everything"
    Check (Test-Path $suiteUninstaller) "one uninstaller"
    $name = (Get-ItemProperty $suiteKey).DisplayName
    Check ($name -match "EQ \d+\.\d+\.\d+" -and $name -match "Analog Compressor \d+\.\d+\.\d+" -and $name -match "Maximiser \d+\.\d+\.\d+") "the Apps entry lists each plugin's version: $name"
    Uninstall $suiteUninstaller
    Expect-Installed @() "after uninstall"

    Write-Host "`n2. Only Songsmith EQ ticked"
    Check ((Run-Suite $Installer "2-eq.log" "eq") -eq 0) "installer exits with 0"
    Expect-Installed @("eq") "only the EQ"

    Write-Host "`n3. Run again and tick the compressor too"
    Check ((Run-Suite $Installer "3-eq-fet.log" "eq,fet") -eq 0) "installer exits with 0"
    Expect-Installed @("eq", "fet") "EQ and compressor"
    Check ((Apps-Entries).Count -eq 1) "still one entry in Settings > Apps"
    Check (-not (Test-Path (Join-Path $suiteDir "unins001.exe"))) "still one uninstaller (upgraded in place)"

    Write-Host "`n4. Run again and untick Songsmith EQ"
    Check ((Run-Suite $Installer "4-fet.log" "fet") -eq 0) "installer exits with 0"
    Expect-Installed @("fet") "EQ removed, compressor kept"
    Check (-not (Test-Path (Join-Path $suiteDir "Songsmith EQ"))) "the EQ's app folder is gone"
    Uninstall $suiteUninstaller

    Write-Host "`n5. Songsmith EQ's own installer first, then the suite"
    $p = Start-Process -FilePath $EqInstaller -Wait -PassThru -ArgumentList "/VERYSILENT", "/SUPPRESSMSGBOXES", "/NORESTART", "/CURRENTUSER",
           "/DIR=`"$(Join-Path $suiteDir 'Songsmith EQ')`"", "/VST3DIR=`"$vst3Dir`"", "/LOG=`"$(Join-Path $root '5-eq-own.log')`""
    Check ($p.ExitCode -eq 0) "Songsmith EQ's own installer exits with 0"
    Check ((Test-Path $eqKey) -and (Test-Path (Dll "eq"))) "Songsmith EQ installed on its own, with its own Apps entry"
    Check ((Run-Suite $Installer "5-suite.log" "eq") -eq 0) "suite installer exits with 0"
    Check (-not (Test-Path $eqKey)) "the separate Songsmith EQ entry is gone"
    Check ((Apps-Entries).Count -eq 1) "one entry in Settings > Apps"
    Expect-Installed @("eq") "the EQ, once"
    Check (-not (Test-Path (Join-Path $suiteDir "Songsmith EQ\unins000.exe"))) "the EQ's own uninstaller is gone"
    Check (@(Get-ChildItem $vst3Dir -Directory | Where-Object Name -like "*EQ*").Count -eq 1) "one EQ bundle in the VST3 folder"
    Uninstall $suiteUninstaller

    Write-Host "`n6. Upgrade from an older suite: the ticks are remembered"
    Check ((Run-Suite $OlderInstaller "6-older.log" "fet,max") -eq 0) "older suite installer exits with 0"
    Check ((Get-ItemProperty $suiteKey).DisplayVersion -eq $olderVersion) "older suite installed ($olderVersion)"
    # Suite 1.x listed each standalone app as its own component, and let a plugin be ticked without
    # its app. Rewrite what Setup remembered to that shape: the compressor ticked without its app,
    # the maximiser with "max\standalone".
    Set-ItemProperty $suiteKey -Name "Inno Setup: Selected Components" -Value "fet,max,max\standalone"
    Check ((Run-Suite $Installer "6-newer.log" "") -eq 0) "newer suite installer exits with 0 (no choices given)"
    Check ((Get-ItemProperty $suiteKey).DisplayVersion -eq $sharedVersion) "upgraded to $((Get-ItemProperty $suiteKey).DisplayVersion)"
    Check ((Test-Path (Dll "fet")) -and (Test-Path (Exe "fet"))) "compressor, ticked without its app before, now has its app"
    Check ((Test-Path (Dll "max")) -and (Test-Path (Exe "max"))) "maximiser and its app still there"
    Check (-not (Test-Path (Dll "eq"))) "Songsmith EQ still not installed"
    Check ((Apps-Entries).Count -eq 1) "one entry in Settings > Apps"

    Write-Host "`n8. A plugin is in use"
    $before = Snapshot
    $lock = [IO.File]::Open((Dll "max"), 'Open', 'ReadWrite', 'None')   # what a host loading the plugin does
    try {
        $code = Run-Suite $Installer "8-locked.log" "eq,fet,max"
        Check ($code -ne 0) "Setup refuses while the maximiser is loaded (exit code $code)"
        Check ($null -eq (Compare-Object @(Snapshot) @($before))) "nothing changed"
        Check ((Get-Content (Join-Path $root "8-locked.log") -Raw) -match "in use") "the log has the 'in use' message"
    } finally {
        $lock.Close()
    }

    Write-Host "`n7. Uninstall"
    Uninstall $suiteUninstaller
    Check (-not (Test-Path $suiteUninstaller)) "uninstaller finished and removed itself"
    Expect-Installed @() "after uninstall"
    Check (Test-Path $vst3Dir) "shared VST3 folder kept"
    foreach ($file in $presets) { Check (Test-Path $file) "preset kept: $file" }
    Check (-not (Test-Path $suiteKey)) "the Apps entry is removed"

    # One tick box per plugin: no separate standalone-app components.
    $iss = Get-Content (Join-Path $PSScriptRoot "SongsmithStudio.iss") -Raw
    Check ($iss -notmatch '(?im)^Name:\s*"\w+\\standalone"') "the Components list has no standalone sub-components"
    $componentLines = [regex]::Matches($iss, '(?im)^Name:\s*"(\w+)";\s+Description:.*Types:')
    Check ($componentLines.Count -eq 3) "exactly three components (found $($componentLines.Count))"

    # The installer must never reach user presets, whatever folder they're in.
    $iss = Get-Content (Join-Path $PSScriptRoot "SongsmithStudio.iss") -Raw
    Check ($iss -notmatch '(?im)^\s*Type:.*\{(userappdata|commonappdata|localappdata|appdata)\}') "no delete entries for any app-data folder"
    Check ($null -eq (Compare-Object @(RealVst3State) @($realBefore))) "real VST3 folder ($realVst3) untouched"
}
finally {
    # A failed run may leave test installs registered for this user: remove them.
    foreach ($key in @($suiteKey, $eqKey)) {
        if (Test-Path $key) {
            $u = (Get-ItemProperty $key).UninstallString -replace '"', ''
            if ($u -and $u.StartsWith($root)) { Uninstall $u }
        }
    }
    Remove-Item -Recurse -Force $root -ErrorAction SilentlyContinue
}

if ($script:failures -gt 0) {
    Write-Host "`n$($script:failures) installer check(s) failed" -ForegroundColor Red
    exit 1
}
Write-Host "`nAll suite installer checks passed"
exit 0
