[CmdletBinding()]
param(
    # Run once, as the machine owner, to record the deployment you have verified and attest to.
    [switch]$Attest,
    [string]$WorkerConfig = 'C:\Scheduled\Songsmith-Matrix.worker.config.json',
    [string]$OwnerDirectory = 'C:\Scheduled\Songsmith-Matrix\owner',
    [string]$TrustedDirectory = 'C:\Scheduled\Songsmith-Matrix\agent-trusted\998a5cfa3e32951df1e53b76f37c4b2df01088e7',
    [string]$AgentUser = 'SongsmithAgent',
    [string]$BuildUser = 'SongsmithBuild',
    [string]$NodeExecutable = 'C:\Program Files\nodejs\node.exe',
    # Renew when less than this many hours remain (the worker caps an approval at 24 hours).
    [int]$RenewWhenHoursLeft = 12
)
# Owner-run renewal of the worker's 24-hour deployment approval. Install under an owner-only folder
# and run as SYSTEM from a scheduled task. It renews ONLY while the deployment still matches what
# the owner attested with -Attest; any drift stops renewal and the approval lapses by itself.
# It never starts the worker, never touches the live switches and never widens any permission.
$ErrorActionPreference = 'Stop'
$log = Join-Path $OwnerDirectory 'renewal.log'
$state = Join-Path $OwnerDirectory 'attested.json'
function Write-Log([string]$Message) { Add-Content -LiteralPath $log -Value ('{0} {1}' -f [DateTime]::UtcNow.ToString('o'), $Message) }
function Get-Sha([string]$Path) { (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash }

$config = Get-Content -LiteralPath $WorkerConfig -Raw | ConvertFrom-Json
$agent = (Get-LocalUser -Name $AgentUser).SID.Value
$build = (Get-LocalUser -Name $BuildUser).SID.Value
$admins = @(Get-LocalGroupMember -SID S-1-5-32-544 | ForEach-Object { $_.SID.Value })
$facts = [ordered]@{
    configSha256      = Get-Sha $WorkerConfig
    buildScriptSha256 = Get-Sha $config.worker.buildScript
    agentSid          = $agent
    buildSid          = $build
}

if ($Attest) {
    if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Attest as an administrator' }
    $null = New-Item -ItemType Directory -Path $OwnerDirectory -Force
    ($facts + @{ attestedAt = [DateTime]::UtcNow.ToString('o') }) | ConvertTo-Json | Set-Content -LiteralPath $state
    Write-Log 'Owner attested the current deployment'
    Write-Output 'Attested. Renewal will continue only while these files and accounts are unchanged.'
    return
}

try {
    if (-not (Test-Path -LiteralPath $state)) { throw 'No owner attestation recorded' }
    $attested = Get-Content -LiteralPath $state -Raw | ConvertFrom-Json
    foreach ($name in 'configSha256', 'buildScriptSha256', 'agentSid', 'buildSid') {
        if ($attested.$name -ne $facts[$name]) { throw "Deployment changed since attestation: $name" }
    }
    if ($agent -eq $build -or $agent -in $admins -or $build -in $admins) { throw 'Agent and Build accounts must stay distinct and non-administrator' }
    if ($config.autoMerge -ne $false -or $config.releasePublication -ne $false) { throw 'Worker auto-merge/release switches must stay false' }
    $task = Get-ScheduledTask -TaskName 'Songsmith Matrix Worker'
    if ((([Security.Principal.NTAccount]$task.Principal.UserId).Translate([Security.Principal.SecurityIdentifier])).Value -ne $agent) { throw 'Worker task no longer runs as the Agent account' }

    $approvalFile = $config.worker.approvalFile
    if (Test-Path -LiteralPath $approvalFile) {
        $current = Get-Content -LiteralPath $approvalFile -Raw | ConvertFrom-Json
        $left = ([DateTime]::Parse($current.expiresAt, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime() - [DateTime]::UtcNow).TotalHours
        if ($left -gt $RenewWhenHoursLeft) { Write-Log ('Approval still valid for {0:N1} hours; no renewal needed' -f $left); return }
    }
    $generator = @'
import fs from 'node:fs'; import os from 'node:os'; import { pathToFileURL } from 'node:url'; import path from 'node:path';
const [,, trusted, cfgPath, out, user] = process.argv;
const w = await import(pathToFileURL(path.join(trusted,'worker.mjs')).href);
const b = await import(pathToFileURL(path.join(trusted,'backlog.mjs')).href);
const config = JSON.parse(fs.readFileSync(cfgPath,'utf8'));
const approval = { authorised:true, user, host:os.hostname(), isolatedAccount:true, networkRestricted:true, separateBuildAccount:true, noReleaseCredentials:true,
  expiresAt:new Date(Date.now()+23*3600000).toISOString(), configHash:w.approvalDigest(config), buildScriptHash:b.digest(fs.readFileSync(config.worker.buildScript)) };
const temp = out + '.tmp'; fs.writeFileSync(temp, JSON.stringify(approval,null,2)); fs.renameSync(temp, out);
const r = w.approve({...config, worker:{...config.worker, approvalFile:out}}, Date.now(), { user, host: os.hostname() });
console.log('VERIFIED ' + r.expiresAt);
'@
    $script = Join-Path $OwnerDirectory 'make-approval.mjs'
    Set-Content -LiteralPath $script -Value $generator
    $result = & $NodeExecutable $script $TrustedDirectory $WorkerConfig $approvalFile $AgentUser 2>&1
    if ($LASTEXITCODE -ne 0) { throw "Approval generator failed: $result" }
    # Owner/admin/system manage the file; the Agent may only read it.
    & icacls.exe $approvalFile /inheritance:r /grant:r 'SYSTEM:(F)' 'BUILTIN\Administrators:(F)' "$($env:COMPUTERNAME)\$AgentUser`:(R)" | Out-Null
    Write-Log "Renewed approval: $result"
} catch {
    Write-Log "NOT renewed: $($_.Exception.Message)"
    throw
}
