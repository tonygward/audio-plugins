# Builds the Songsmith Studio Windows installer (Inno Setup 6): every plugin in one Setup, each
# one ticked by default. Uses existing Release builds of the three plugins.
#
#   .\installer\windows\build-installer.ps1                  # -> build\suite-installer\Songsmith-Studio-<version>-win64-setup.exe
#   .\installer\windows\build-installer.ps1 -EqDir <folder>  # a plugin's Release artefacts from somewhere else (CI)
#   .\installer\windows\build-installer.ps1 -SuiteVersion 0.9.0 -OutDir <folder>   # tests: an "older" suite
#
# The suite version is in installer/version.txt; each plugin's version comes from
# project(... VERSION x.y.z) in its own CMakeLists.txt, so every version is defined in one place.
# Build the plugins first (cmake --build <plugin>/build --config Release for eq, fet, maximizer).
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

function Get-ProjectVersion([string]$folder) {
    $cmake = Get-Content (Join-Path $repo "$folder\CMakeLists.txt") -Raw
    if ($cmake -notmatch 'project\(\s*\w+\s+VERSION\s+(\d+\.\d+\.\d+)') {
        throw "Couldn't find 'project(... VERSION x.y.z)' in $folder/CMakeLists.txt"
    }
    return $Matches[1]
}

if (-not $SuiteVersion) {
    $SuiteVersion = (Get-Content (Join-Path $repo "installer\version.txt") -Raw).Trim()
}
if ($SuiteVersion -notmatch '^\d+\.\d+\.\d+$') { throw "Suite version '$SuiteVersion' isn't x.y.z (installer/version.txt)" }

$plugins = @(
    @{ Key = "Eq";  Folder = "eq";        Target = "SongsmithEQ";               Name = "Songsmith EQ";                Dir = $EqDir },
    @{ Key = "Fet"; Folder = "fet";       Target = "SongsmithAnalogCompressor"; Name = "Songsmith Analog Compressor"; Dir = $FetDir },
    @{ Key = "Max"; Folder = "maximizer"; Target = "SongsmithMaximiser";        Name = "Songsmith Maximiser";         Dir = $MaxDir }
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
    $defines += "/D$($p.Key)Version=$(Get-ProjectVersion $p.Folder)"
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
