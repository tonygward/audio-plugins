# Fails if the one version number ever drifts. CI runs it, and so can you:
#
#   pwsh installer/check-version.ps1            # the files that must read VERSION, not hold a version
#   pwsh installer/check-version.ps1 -Built     # also: every built plugin reports the same version
#   pwsh installer/check-version.ps1 -Built -Plugin eq   # only that plugin's build (eq, analog-compressor, maximiser, compressor)
#
# Checks:
#  1. VERSION (repository root) holds exactly one x.y.z line.
#  2. Each plugin's CMakeLists.txt reads ../VERSION and has no x.y.z of its own.
#  3. No installer script, installer definition or workflow writes its own x.y.z.
#     (JUCE's own version in the workflow is the one allowed exception.)
#  4. With -Built: JucePlugin_VersionString in each plugin's build folder equals VERSION.
param([switch]$Built, [string]$Plugin)

$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
$failures = 0
function Fail([string]$message) { Write-Host "  FAIL  $message" -ForegroundColor Red; $script:failures++ }
function Ok([string]$message)   { Write-Host "  ok    $message" }

# 1. The version file.
$versionFile = Join-Path $repo "VERSION"
if (-not (Test-Path $versionFile)) { throw "Missing $versionFile" }
$lines = @(Get-Content $versionFile | Where-Object { $_.Trim() -ne "" })
$version = if ($lines.Count -gt 0) { $lines[0].Trim() } else { "" }
if ($lines.Count -eq 1 -and $version -match '^\d+\.\d+\.\d+$') { Ok "VERSION holds $version" }
else { Fail "VERSION must hold exactly one line, x.y.z (found $($lines.Count) line(s), first: '$version')"; exit 1 }

$plugins = @(
    @{ Folder = "eq";                Target = "SongsmithEQ" },
    @{ Folder = "analog-compressor"; Target = "SongsmithAnalogCompressor" },
    @{ Folder = "maximiser";         Target = "SongsmithMaximiser" },
    @{ Folder = "compressor";        Target = "SongsmithCompressor" }
)

# 2. CMake reads the file and holds no version of its own.
foreach ($p in $plugins) {
    $path = Join-Path $repo "$($p.Folder)\CMakeLists.txt"
    $cmake = Get-Content $path -Raw
    if ($cmake -notmatch '\.\./VERSION') { Fail "$($p.Folder)/CMakeLists.txt doesn't read ../VERSION" }
    elseif ($cmake -match '(?m)^\s*project\([^)]*VERSION\s+\d') { Fail "$($p.Folder)/CMakeLists.txt gives project() a literal version" }
    elseif ([regex]::IsMatch($cmake, '(?m)^(?!\s*#)(?!.*cmake_minimum_required).*\b\d+\.\d+\.\d+\b')) { Fail "$($p.Folder)/CMakeLists.txt contains its own x.y.z version" }
    else { Ok "$($p.Folder)/CMakeLists.txt reads VERSION" }
}

# 3. Installers and workflows read the file too.
$scripts = @()
$scripts += Get-ChildItem (Join-Path $repo "installer") -Recurse -File -Include *.ps1, *.sh, *.iss, *.xml
foreach ($p in $plugins) {
    $scripts += Get-ChildItem (Join-Path $repo "$($p.Folder)\installer") -Recurse -File -Include *.ps1, *.sh, *.iss, *.xml -ErrorAction SilentlyContinue
}
$scripts += Get-ChildItem (Join-Path $repo ".github\workflows") -File -Include *.yml, *.yaml
foreach ($file in $scripts) {
    if ($file.Name -eq "check-version.ps1") { continue }
    $relative = $file.FullName.Substring($repo.Length + 1)
    $hits = @()
    $n = 0
    foreach ($line in Get-Content $file.FullName) {
        $n++
        if ($line -match 'JUCE') { continue }                      # JUCE's own version, not ours
        if ($line -match '(?<![\d.])\d+\.\d+\.\d+(?![\d.])') { $hits += "line ${n}: $($line.Trim())" }
    }
    if ($hits.Count -gt 0) { Fail "$relative holds its own x.y.z version instead of reading VERSION"; $hits | ForEach-Object { Write-Host "          $_" } }
}
if ($failures -eq 0) { Ok "$($scripts.Count) installer and workflow files read VERSION" }

# 4. What was built reports the same version.
if ($Built) {
    foreach ($p in $plugins) {
        if ($Plugin -and $p.Folder -ne $Plugin) { continue }
        $defs = @(Get-ChildItem (Join-Path $repo "$($p.Folder)\build\$($p.Target)_artefacts\JuceLibraryCode") -Recurse -Filter Defs.txt -ErrorAction SilentlyContinue)
        if ($defs.Count -eq 0) { Fail "$($p.Folder): no build found to check (build it first)"; continue }
        foreach ($d in $defs) {
            if ((Get-Content $d.FullName -Raw) -match 'JucePlugin_VersionString="([^"]+)"') {
                if ($Matches[1] -eq $version) { Ok "$($p.Folder) ($($d.Directory.Name)) reports $($Matches[1])" }
                else { Fail "$($p.Folder) ($($d.Directory.Name)) reports $($Matches[1]) but VERSION is $version (reconfigure and rebuild)" }
            } else { Fail "$($p.Folder): no JucePlugin_VersionString in $($d.FullName)" }
        }
    }
}

if ($failures -gt 0) { Write-Host "`n$failures version check(s) failed" -ForegroundColor Red; exit 1 }
Write-Host "`nOne version everywhere: $version"
exit 0
