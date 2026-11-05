# Builds the Songsmith Analog Compressor Windows installer (Inno Setup 6) from a Release build.
#
#   .\installer\build-installer.ps1          # -> build\installer\Songsmith-Analog-Compressor-<version>-win64-setup.exe
#   .\installer\build-installer.ps1 -Zip     # also build\installer\Songsmith-Analog-Compressor-<version>-win64.zip
#
# Same as eq\installer\build-installer.ps1. The version comes from VERSION in the repository root.
# Build first: cmake --build build --config Release. Writes the path of
# the installer to the output.
param(
    [string]$BuildDir = (Join-Path (Split-Path $PSScriptRoot -Parent) "build"),
    [string]$Config = "Release",
    [switch]$Zip
)

$ErrorActionPreference = "Stop"
$fetDir = Split-Path $PSScriptRoot -Parent
$name = "Songsmith Analog Compressor"

# The one version number for every plugin is in VERSION at the repository root.
$versionFile = Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) "VERSION"
if (-not (Test-Path $versionFile)) { throw "Missing $versionFile (the one-line x.y.z version file)" }
$version = (Get-Content $versionFile -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { throw "VERSION must hold one version as x.y.z (found '$version')" }

$artefacts = Join-Path $BuildDir "SongsmithAnalogCompressor_artefacts\$Config"
foreach ($required in @("VST3\$name.vst3\Contents\x86_64-win\$name.vst3", "Standalone\$name.exe")) {
    if (-not (Test-Path (Join-Path $artefacts $required))) {
        throw "Missing '$required' in $artefacts. Build first: cmake --build build --config $Config"
    }
}

$iscc = (Get-Command iscc.exe -ErrorAction SilentlyContinue).Source
if (-not $iscc) {
    $iscc = @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
              "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
              "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $iscc) {
    throw "Inno Setup 6 not found. Install it with: winget install JRSoftware.InnoSetup"
}

$outDir = Join-Path $BuildDir "installer"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

& $iscc /Q "/DAppVersion=$version" "/DBuildDir=$artefacts" "/O$outDir" (Join-Path $PSScriptRoot "SongsmithAnalogCompressor.iss")
if ($LASTEXITCODE -ne 0) { throw "iscc failed with exit code $LASTEXITCODE" }

$setup = Join-Path $outDir "Songsmith-Analog-Compressor-$version-win64-setup.exe"
if (-not (Test-Path $setup)) { throw "iscc didn't produce $setup" }

if ($Zip) {
    # Manual install: the VST3 bundle and the standalone app at the top level.
    $zipPath = Join-Path $outDir "Songsmith-Analog-Compressor-$version-win64.zip"
    Remove-Item $zipPath -ErrorAction SilentlyContinue
    Compress-Archive -Path (Join-Path $artefacts "VST3\$name.vst3"), (Join-Path $artefacts "Standalone\$name.exe") -DestinationPath $zipPath
    Write-Host "Zip:       $zipPath"
}

Write-Host "Installer: $setup"
$setup
