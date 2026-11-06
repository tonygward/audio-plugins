[CmdletBinding()]
param([Parameter(Mandatory)][ValidatePattern('^[a-f0-9]{40,64}$')][string]$Revision)
$ErrorActionPreference = 'Stop'
# This wrapper is copied and ACL-protected by the owner, outside any checkout.
# The runner account can write inbox only; it cannot replace trusted Matrix code.
$inbox = 'C:\Matrix\state\inbox'
New-Item -ItemType Directory -Path $inbox -Force | Out-Null
$file = Join-Path $inbox "$Revision.json"
try {
    $stream = [System.IO.File]::Open($file, [System.IO.FileMode]::CreateNew, [System.IO.FileAccess]::Write, [System.IO.FileShare]::None)
    $stream.Dispose()
} catch [System.IO.IOException] {
    if (-not (Test-Path -LiteralPath $file)) { throw }
}
# Five-minute task polling consumes the signal. Nothing from GitHub executes locally.
