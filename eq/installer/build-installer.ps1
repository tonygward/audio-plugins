# Builds the Songsmith EQ Windows installer (Inno Setup 6) from an existing Release build.
#
#   .\installer\build-installer.ps1                      # -> build\installer\Songsmith-EQ-<version>-win64-setup.exe
#   .\installer\build-installer.ps1 -Zip                 # also build\installer\Songsmith-EQ-<version>-win64.zip
#   .\installer\build-installer.ps1 -BuildDir other\dir  # a different CMake build folder
#
# The version comes from project(... VERSION x.y.z) in eq/CMakeLists.txt. Build the plugin
# first: cmake --build build --config Release. Writes the path of the installer to the output.
param(
    [string]$BuildDir = (Join-Path (Split-Path $PSScriptRoot -Parent) "build"),
    [string]$Config = "Release",
    [switch]$Zip
)

$ErrorActionPreference = "Stop"
$eqDir = Split-Path $PSScriptRoot -Parent

# Version from CMakeLists.txt, so it's defined in exactly one place.
$cmake = Get-Content (Join-Path $eqDir "CMakeLists.txt") -Raw
if ($cmake -notmatch 'project\(\s*\w+\s+VERSION\s+(\d+\.\d+\.\d+)') {
    throw "Couldn't find 'project(... VERSION x.y.z)' in eq/CMakeLists.txt"
}
$version = $Matches[1]

$artefacts = Join-Path $BuildDir "SongsmithEQ_artefacts\$Config"
foreach ($required in @("VST3\Songsmith EQ.vst3\Contents\x86_64-win\Songsmith EQ.vst3", "Standalone\Songsmith EQ.exe")) {
    if (-not (Test-Path (Join-Path $artefacts $required))) {
        throw "Missing '$required' in $artefacts. Build first: cmake --build build --config $Config"
    }
}

# Inno Setup's compiler: on PATH (CI), or the machine-wide / per-user (winget --scope user) install.
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

& $iscc /Q "/DAppVersion=$version" "/DBuildDir=$artefacts" "/O$outDir" (Join-Path $PSScriptRoot "SongsmithEQ.iss")
if ($LASTEXITCODE -ne 0) { throw "iscc failed with exit code $LASTEXITCODE" }

$setup = Join-Path $outDir "Songsmith-EQ-$version-win64-setup.exe"
if (-not (Test-Path $setup)) { throw "iscc didn't produce $setup" }

if ($Zip) {
    # The manual-install zip, same shape as earlier releases: just the bundle at the top level.
    $zipPath = Join-Path $outDir "Songsmith-EQ-$version-win64.zip"
    Remove-Item $zipPath -ErrorAction SilentlyContinue
    Compress-Archive -Path (Join-Path $artefacts "VST3\Songsmith EQ.vst3") -DestinationPath $zipPath
    Write-Host "Zip:       $zipPath"
}

Write-Host "Installer: $setup"
$setup
