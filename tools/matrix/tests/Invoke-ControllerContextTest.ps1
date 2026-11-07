[CmdletBinding()]
param([Parameter(Mandatory)][string]$Config)
$ErrorActionPreference = 'Stop'
$existing = Get-ScheduledTask -TaskName 'Songsmith Matrix'
$before = Export-ScheduledTask -TaskName $existing.TaskName
$c = Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
if ($c.liveExecution -ne $false -or $c.worker.enabled -ne $false) { throw 'Live settings must remain disabled' }
$id = [Guid]::NewGuid().ToString('N')
$name = 'Songsmith Matrix Validation ' + $id
$outputFile = Join-Path (Split-Path -Parent $c.worker.buildCredentialFile) ('controller-context-' + $id + '.json')
$script = Join-Path $PSScriptRoot 'Test-ControllerContext.ps1'
$action = New-ScheduledTaskAction -Execute (Join-Path $PSHOME 'pwsh.exe') -Argument ('-NoProfile -NonInteractive -File "{0}" -Config "{1}" -Output "{2}"' -f $script, $Config, $outputFile) -WorkingDirectory $PSScriptRoot
$taskSettings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 2)
$task = New-ScheduledTask -Action $action -Principal $existing.Principal -Settings $taskSettings
$created = $false
try {
    $null = Register-ScheduledTask -TaskName $name -InputObject $task
    $created = $true
    Start-ScheduledTask -TaskName $name
    $deadline = [DateTime]::UtcNow.AddSeconds(90)
    while (-not (Test-Path -LiteralPath $outputFile) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Seconds 1 }
    if (-not (Test-Path -LiteralPath $outputFile)) { throw ('No scheduled diagnostic report; task result: ' + (Get-ScheduledTaskInfo -TaskName $name).LastTaskResult) }
    Get-Content -LiteralPath $outputFile -Raw
    Write-Output "Evidence: $outputFile"
    $result = Get-Content -LiteralPath $outputFile -Raw | ConvertFrom-Json
    if ($result.elevated -or -not $result.buildCredentialDecrypts) { throw 'Scheduled controller is not deployment-ready: inspect elevation and credential decryption evidence' }
} finally {
    if ($created) {
        if ((Get-ScheduledTask -TaskName $name).State -eq 'Running') { Stop-ScheduledTask -TaskName $name }
        Unregister-ScheduledTask -TaskName $name -Confirm:$false
    }
    if ((Export-ScheduledTask -TaskName $existing.TaskName) -ne $before) { throw 'Existing scheduled task XML changed' }
    Write-Output 'Existing scheduled task XML unchanged; temporary diagnostic task removed'
}
