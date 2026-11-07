[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Config,
    [Parameter(Mandatory)][string]$Output
)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = [Security.Principal.WindowsPrincipal]::new($identity)
$result = [ordered]@{
    time = [DateTime]::UtcNow.ToString('o')
    identity = $identity.Name
    sid = $identity.User.Value
    elevated = $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    sessionId = (Get-Process -Id $PID).SessionId
    configurationReadable = $false
    buildCredentialDecrypts = $false
    buildCredentialIdentity = $null
    repositoryReadable = $false
    stateReadable = $false
}
try {
    $c = Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
    $result.configurationReadable = $true
    if ($c.liveExecution -ne $false -or $c.worker.enabled -ne $false) { throw 'Live settings must remain disabled' }
    $null = Get-Content -LiteralPath (Join-Path $c.repository '.git/HEAD')
    $result.repositoryReadable = $true
    $null = Get-ChildItem -LiteralPath $c.stateDirectory -ErrorAction Stop
    $result.stateReadable = $true
    try {
        $credential = Import-Clixml -LiteralPath $c.worker.buildCredentialFile
        if ($credential -isnot [PSCredential]) { throw 'Wrong credential type' }
        # Force DPAPI decryption, retaining no password or token in evidence.
        $null = $credential.GetNetworkCredential().Password.Length
        $result.buildCredentialDecrypts = $true
        $result.buildCredentialIdentity = $credential.UserName
    } catch { $result.credentialError = $_.Exception.Message }
} catch { $result.error = $_.Exception.Message }
$result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $Output
if (-not $result.buildCredentialDecrypts -or $result.elevated) { exit 1 }
