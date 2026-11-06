[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$Config,
    [Parameter(Mandatory)][string]$TrustedScript,
    [string]$TaskName = 'Songsmith Matrix Dry Run',
    [switch]$Remove
)
$ErrorActionPreference = 'Stop'
if ($Remove) {
    if ($PSCmdlet.ShouldProcess($TaskName, 'Unregister scheduled task (preserve state and logs)')) {
        Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
    }
    return
}
$configPath = (Resolve-Path -LiteralPath $Config).Path
$scriptPath = (Resolve-Path -LiteralPath $TrustedScript).Path
if ($configPath.Contains('"') -or $scriptPath.Contains('"')) { throw 'Quotes are not allowed in task paths' }
$settingsData = Get-Content -LiteralPath $configPath -Raw | ConvertFrom-Json
if ($settingsData.liveExecution -ne $false -or $settingsData.autoMerge -ne $false -or $settingsData.releasePublication -ne $false) {
    throw 'Only dry-run task installation is supported'
}
$node = (Get-Command node.exe -ErrorAction Stop).Source
# StartWhenAvailable plus repetition catches missed notifications and daily scans.
# S4U needs no stored password or open terminal. Provision a dedicated non-admin account.
# S4U has no network credentials: use a public HTTPS repository or a separately verified
# non-interactive read-only GitHub credential provider under this account.
$action = New-ScheduledTaskAction -Execute $node -Argument ('"{0}" timer "{1}"' -f $scriptPath, $configPath) -WorkingDirectory (Split-Path $scriptPath)
$triggers = @(
    (New-ScheduledTaskTrigger -AtStartup),
    (New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(1) -RepetitionInterval (New-TimeSpan -Minutes 5))
)
$settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -StartWhenAvailable -RestartCount 2 -RestartInterval (New-TimeSpan -Minutes 5) -ExecutionTimeLimit (New-TimeSpan -Minutes 20)
$principal = New-ScheduledTaskPrincipal -UserId ([System.Security.Principal.WindowsIdentity]::GetCurrent().Name) -LogonType S4U -RunLevel Limited
if ($PSCmdlet.ShouldProcess($TaskName, 'Register disabled dry-run task; owner enables after account/network verification')) {
    $task = New-ScheduledTask -Action $action -Trigger $triggers -Settings $settings -Principal $principal
    $task.Settings.Enabled = $false
    Register-ScheduledTask -TaskName $TaskName -InputObject $task -Force | Out-Null
}
