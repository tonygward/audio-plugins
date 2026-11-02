# Copies the built TW Pro EQ VST3 bundle into the system VST3 folder and removes
# the bundle from before the rename ("3-Band EQ.vst3"), which has the same plugin ID.
# Usage:  .\install.ps1            (Release build)
#         .\install.ps1 -Config Debug
param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

$bundleName    = "TW Pro EQ.vst3"
$legacyBundles = @("3-Band EQ.vst3")
$source        = Join-Path $PSScriptRoot "build\ThreeBandEQ_artefacts\$Config\VST3\$bundleName"
$destDir       = Join-Path $env:CommonProgramFiles "VST3"
$dest          = Join-Path $destDir $bundleName

if (-not (Test-Path $source)) {
    Write-Error "Plugin not found at '$source'. Build it first: cmake --build build --config $Config"
}

# Writing to Program Files needs admin rights: relaunch elevated if necessary.
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
    Write-Host "Requesting administrator rights to write to $destDir ..."
    $shell = (Get-Process -Id $PID).Path
    $proc = Start-Process $shell -Verb RunAs -Wait -PassThru -ArgumentList @(
        "-NoProfile", "-ExecutionPolicy", "Bypass",
        "-File", "`"$PSCommandPath`"", "-Config", $Config)
    exit $proc.ExitCode
}

New-Item -ItemType Directory -Force -Path $destDir | Out-Null

# Refuse to touch an install if a host (e.g. Studio One) has it loaded,
# rather than deleting half the bundle and then failing.
function Assert-NotInUse([string]$bundle) {
    $dll = Join-Path $destDir "$bundle\Contents\x86_64-win\$bundle"
    if (-not (Test-Path $dll)) { return }
    try {
        $stream = [System.IO.File]::Open($dll, 'Open', 'ReadWrite', 'None')
        $stream.Close()
    } catch {
        $users = (tasklist /m "$bundle" /fo csv 2>$null | ConvertFrom-Csv | ForEach-Object { $_.'Image Name' }) -join ", "
        Write-Error "'$bundle' is in use$(if ($users) { " by: $users" }). Close the host and run the install again."
    }
}

foreach ($b in @($bundleName) + $legacyBundles) { Assert-NotInUse $b }

foreach ($b in $legacyBundles) {
    $old = Join-Path $destDir $b
    if (Test-Path $old) {
        Remove-Item -Recurse -Force $old
        Write-Host "Removed old bundle $old"
    }
}

# Replace any previous install so stale files don't linger.
if (Test-Path $dest) {
    Remove-Item -Recurse -Force $dest
}

Copy-Item -Recurse -Path $source -Destination $dest
Write-Host "Installed $bundleName -> $dest"
