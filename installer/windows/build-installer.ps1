# Builds the Songsmith Studio Windows installer (Inno Setup 6): every plugin in one Setup, each
# one ticked by default. Uses existing Release builds of the three plugins.
#
#   .\installer\windows\build-installer.ps1                  # -> build\suite-installer\Songsmith-Studio-<version>-win64-setup.exe
#   .\installer\windows\build-installer.ps1 -EqDir <folder>  # a plugin's Release artefacts from somewhere else (CI)
#   .\installer\windows\build-installer.ps1 -SuiteVersion <older version> -OutDir <folder>   # tests: an "older" suite
#
# Every plugin and the suite share the one version in VERSION at the repository root.
# Build the plugins first (cmake --build <plugin>/build --config Release for eq, analog-compressor, maximiser).
# Writes the path of the installer to the output.
param(
    [string]$EqDir,
    [string]$FetDir,
    [string]$MaxDir,
    [string]$SuiteVersion,
    [string]$OutDir,
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

$versionFile = Join-Path $repo "VERSION"
if (-not (Test-Path $versionFile)) { throw "Missing $versionFile (the one-line x.y.z version file)" }
$sharedVersion = (Get-Content $versionFile -Raw).Trim()
if ($sharedVersion -notmatch '^\d+\.\d+\.\d+$') { throw "VERSION must hold one version as x.y.z (found '$sharedVersion')" }

# Tests may build an "older" suite with -SuiteVersion; the plugins inside always carry VERSION.
if (-not $SuiteVersion) { $SuiteVersion = $sharedVersion }
if ($SuiteVersion -notmatch '^\d+\.\d+\.\d+$') { throw "Suite version '$SuiteVersion' isn't x.y.z" }

$plugins = @(
    @{ Key = "Eq";  Folder = "eq";                Target = "SongsmithEQ";               Name = "Songsmith EQ";                Dir = $EqDir },
    @{ Key = "Fet"; Folder = "analog-compressor"; Target = "SongsmithAnalogCompressor"; Name = "Songsmith Analog Compressor"; Dir = $FetDir },
    @{ Key = "Max"; Folder = "maximiser";         Target = "SongsmithMaximiser";        Name = "Songsmith Maximiser";         Dir = $MaxDir }
)

$defines = @("/DSuiteVersion=$SuiteVersion")
foreach ($p in $plugins) {
    $dir = $p.Dir
    if (-not $dir) { $dir = Join-Path $repo "$($p.Folder)\build\$($p.Target)_artefacts\$Config" }
    $dir = (Resolve-Path $dir -ErrorAction SilentlyContinue).Path
    $name = $p.Name
    foreach ($required in @("VST3\$name.vst3\Contents\x86_64-win\$name.vst3", "Standalone\$name.exe")) {
        if (-not $dir -or -not (Test-Path (Join-Path $dir $required))) {
            throw "Missing '$required' for $name in '$dir'. Build first: cmake --build $($p.Folder)/build --config $Config"
        }
    }
    $defines += "/D$($p.Key)Dir=$dir"
    $defines += "/D$($p.Key)Version=$sharedVersion"
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

if (-not $OutDir) { $OutDir = Join-Path $repo "build\suite-installer" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

& $iscc /Q @defines "/O$OutDir" (Join-Path $PSScriptRoot "SongsmithStudio.iss")
if ($LASTEXITCODE -ne 0) { throw "iscc failed with exit code $LASTEXITCODE" }

$setup = Join-Path $OutDir "Songsmith-Studio-$SuiteVersion-win64-setup.exe"
if (-not (Test-Path $setup)) { throw "iscc didn't produce $setup" }

Write-Host "Installer: $setup"
$setup
