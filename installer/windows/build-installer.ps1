# Builds the Songsmith Plugins Windows installer (Inno Setup 6): every plugin in one Setup, each
# one ticked by default, the Songsmith Plugin Analyzer (a VST3 only) and the Songsmith Audio Analyzer app. Uses existing Release builds of the seven plugins and the app.
#
#   .\installer\windows\build-installer.ps1                  # -> build\suite-installer\Songsmith-Plugins-<version>-win64-setup.exe
#   .\installer\windows\build-installer.ps1 -EqDir <folder>  # a plugin's Release artefacts from somewhere else (CI)
#   .\installer\windows\build-installer.ps1 -SuiteVersion <older version> -OutDir <folder>   # tests: an "older" suite
#
# Every plugin and the suite share the one version in VERSION at the repository root.
# Build the plugins first (cmake --build <plugin>/build --config Release for eq, analog-compressor, maximiser, compressor, optical-leveller, pultec-eq, bus-compressor, analog-desk)
# the Plugin Analyzer (plugin-analyzer, VST3 only) and the app (audio-analyzer).
# Writes the path of the installer to the output.
param(
    [string]$EqDir,
    [string]$FetDir,
    [string]$MaxDir,
    [string]$CompDir,
    [string]$OptDir,
    [string]$PeqDir,
    [string]$BusDir,
    [string]$DeskDir,
    [string]$AnaDir,
    [string]$PlaDir,
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
    @{ Key = "Max"; Folder = "maximiser";         Target = "SongsmithMaximiser";        Name = "Songsmith Maximiser";         Dir = $MaxDir },
    @{ Key = "Comp"; Folder = "compressor";       Target = "SongsmithCompressor";       Name = "Songsmith Compressor";        Dir = $CompDir },
    @{ Key = "Opt";  Folder = "optical-leveller"; Target = "SongsmithOpticalLeveller";   Name = "Songsmith Optical Leveller";  Dir = $OptDir },
    @{ Key = "Peq";  Folder = "pultec-eq";        Target = "SongsmithPassiveEQ";        Name = "Songsmith Passive EQ";        Dir = $PeqDir },
    @{ Key = "Bus";  Folder = "bus-compressor";   Target = "SongsmithBusCompressor";   Name = "Songsmith Bus Compressor";    Dir = $BusDir },
    @{ Key = "Desk"; Folder = "analog-desk";      Target = "SongsmithAnalogDesk";      Name = "Songsmith Analog Desk";       Dir = $DeskDir }
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

# The Songsmith Plugin Analyzer (ANALYZER-01) is a plugin only: it has a VST3 but no Standalone app (it needs two copies inside a host).
$pluginAnalyzer = @{ Key = "Pla"; Folder = "plugin-analyzer"; Target = "SongsmithPluginAnalyzer"; Name = "Songsmith Plugin Analyzer"; Dir = $PlaDir }
$plaDir = $pluginAnalyzer.Dir
if (-not $plaDir) { $plaDir = Join-Path $repo "$($pluginAnalyzer.Folder)\build\$($pluginAnalyzer.Target)_artefacts\$Config" }
$plaDir = (Resolve-Path $plaDir -ErrorAction SilentlyContinue).Path
$plaVst3 = "VST3\$($pluginAnalyzer.Name).vst3\Contents\x86_64-win\$($pluginAnalyzer.Name).vst3"
if (-not $plaDir -or -not (Test-Path (Join-Path $plaDir $plaVst3))) {
    throw "Missing '$plaVst3' in '$plaDir'. Build first: cmake --build $($pluginAnalyzer.Folder)/build --config $Config"
}
$defines += "/DPlaDir=$plaDir"
$defines += "/DPlaVersion=$sharedVersion"

# The Songsmith Audio Analyzer (REC-02) is an application, not a plugin: there is no VST3 and no Standalone folder, only its exe.
$analyzer = @{ Key = "Ana"; Folder = "audio-analyzer"; Target = "SongsmithAudioAnalyzer"; Name = "Songsmith Audio Analyzer"; Dir = $AnaDir }
$anaDir = $analyzer.Dir
if (-not $anaDir) { $anaDir = Join-Path $repo "$($analyzer.Folder)\build\$($analyzer.Target)_artefacts\$Config" }
$anaDir = (Resolve-Path $anaDir -ErrorAction SilentlyContinue).Path
if (-not $anaDir -or -not (Test-Path (Join-Path $anaDir "$($analyzer.Name).exe"))) {
    throw "Missing '$($analyzer.Name).exe' in '$anaDir'. Build first: cmake --build $($analyzer.Folder)/build --config $Config"
}
$defines += "/DAnaDir=$anaDir"
$defines += "/DAnaVersion=$sharedVersion"

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

$setup = Join-Path $OutDir "Songsmith-Plugins-$SuiteVersion-win64-setup.exe"
if (-not (Test-Path $setup)) { throw "iscc didn't produce $setup" }

Write-Host "Installer: $setup"
$setup
