[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$GitDirectory = 'C:\dev\audio-plugins\.git',
    [string]$BuildAccount = 'MATRIX\SongsmithBuild',
    [string]$BackupFile = 'C:\Scheduled\Songsmith-Matrix\private\git-before-build-deny.acl'
)
$ErrorActionPreference = 'Stop'
$gitPath = (Resolve-Path -LiteralPath $GitDirectory).Path
if ((Split-Path -Leaf $gitPath) -ne '.git' -or -not (Get-Item -LiteralPath $gitPath -Force).PSIsContainer) { throw 'An explicit Git common directory is required' }
foreach ($item in @((Get-Item -LiteralPath $gitPath -Force)) + @(Get-ChildItem -LiteralPath $gitPath -Force -Recurse)) {
    if ($item.LinkType -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Linked Git metadata requires manual review' }
}
$sid = ([Security.Principal.NTAccount]::new($BuildAccount)).Translate([Security.Principal.SecurityIdentifier]).Value
if ($sid -eq [Security.Principal.WindowsIdentity]::GetCurrent().User.Value) { throw 'Cannot deny the controller identity' }
if (Test-Path -LiteralPath $BackupFile) { throw 'Backup already exists; do not overwrite rollback evidence' }
if ($PSCmdlet.ShouldProcess($gitPath, "Back up DACLs and deny $BuildAccount access to Git common metadata only")) {
    Push-Location (Split-Path -Parent $gitPath)
    try {
        & "$env:SystemRoot/System32/icacls.exe" '.git' /save $BackupFile /T /Q
        if ($LASTEXITCODE -ne 0) { throw 'ACL backup failed; no deny applied' }
        if (-not (Test-Path -LiteralPath $BackupFile) -or (Get-Item -LiteralPath $BackupFile).Length -lt 16) { throw 'ACL backup is missing or empty; no deny applied' }
        & "$env:SystemRoot/System32/icacls.exe" '.git' /deny ("*$($sid):(OI)(CI)(F)") /T /Q
        if ($LASTEXITCODE -ne 0) { throw "Deny was incomplete; inspect and restore ACLs using $BackupFile" }
    } finally { Pop-Location }
}
