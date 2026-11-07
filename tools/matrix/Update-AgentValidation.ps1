[CmdletBinding(SupportsShouldProcess)]
param(
    [Parameter(Mandatory)][string]$DiagnosticTask,
    [string]$Config='C:\Scheduled\Songsmith-Matrix.worker.config.json',
    [switch]$RecoverLease,
    [switch]$UpdateDisabledWorker
)
$ErrorActionPreference='Stop'
$scannerBefore=Export-ScheduledTask -TaskName 'Songsmith Matrix'
$workerBefore=Export-ScheduledTask -TaskName 'Songsmith Matrix Worker'
$worker=Get-ScheduledTask -TaskName 'Songsmith Matrix Worker'
if($worker.Settings.Enabled){throw 'Worker must remain disabled'}
$c=Get-Content -LiteralPath $Config -Raw | ConvertFrom-Json
if($c.liveExecution -ne $false -or $c.worker.enabled -ne $false -or $c.autoMerge -ne $false -or $c.releasePublication -ne $false){throw 'All live switches must remain false'}
$task=Get-ScheduledTask -TaskName $DiagnosticTask
if($task.State -eq 'Running' -or $DiagnosticTask -notlike 'Songsmith Matrix Agent Validation *' -or $task.Principal.UserId -notmatch '(^|\\)SongsmithAgent$' -or [int]$task.Principal.LogonType -ne 1){throw 'Unexpected diagnostic task or task is running'}
if($RecoverLease){
    $active=@(Get-CimInstance Win32_Process | Where-Object { $_.Name -in @('EQTests.exe','MSBuild.exe','cl.exe','link.exe') -or ($_.Name -eq 'pwsh.exe' -and $_.CommandLine -match '\s-Child\s' -and $_.CommandLine.Contains($c.worker.worktreeRoot)) })
    if($active.Count){throw 'Cannot recover while compiler/test/build-child processes exist'}
}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$revision=(& $c.worker.gitExecutable -C $repo rev-parse HEAD).Trim()
if($LASTEXITCODE -or $revision -notmatch '^[a-f0-9]{40}$'){throw 'Invalid trusted revision'}
$root=Split-Path -Parent $c.stateDirectory
$trusted=Join-Path $root ('agent-trusted/'+$revision)
if(-not $PSCmdlet.ShouldProcess($DiagnosticTask,'Install pinned owner-protected bundle, point only the fixed diagnostic task at it and run it; optionally update the worker action while keeping it disabled')){return}
try{
    if(-not(Test-Path -LiteralPath $trusted)){
        $null=New-Item -ItemType Directory -Path $trusted
        $acl=[Security.AccessControl.DirectorySecurity]::new();$acl.SetAccessRuleProtection($true,$false)
        foreach($sid in @([Security.Principal.WindowsIdentity]::GetCurrent().User.Value,'S-1-5-18','S-1-5-32-544')){$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($sid),'FullControl','ContainerInherit,ObjectInherit','None','Allow'))}
        $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.NTAccount]::new('MATRIX\SongsmithAgent'),'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow'))
        Set-Acl -LiteralPath $trusted -AclObject $acl
        foreach($file in Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.mjs'){Copy-Item -LiteralPath $file.FullName -Destination $trusted}
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Invoke-MatrixBuild.ps1') -Destination $trusted
        $tests=Join-Path $trusted 'tests';$null=New-Item -ItemType Directory -Path $tests
        foreach($name in @('Test-ControllerContext.ps1','Test-BuildLease.ps1','Test-BuildInterruption.ps1','Test-BuildLinkSafety.ps1','Test-RealEqBuild.ps1','Run-AgentValidation.ps1')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot "tests/$name") -Destination $tests}
        $launcher=Join-Path $trusted 'Invoke-MatrixBuild.ps1';$acl=Get-Acl -LiteralPath $launcher
        $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.NTAccount]::new('MATRIX\SongsmithBuild'),'ReadAndExecute','Allow'))
        Set-Acl -LiteralPath $launcher -AclObject $acl
        @(Get-ChildItem -LiteralPath $trusted -File -Recurse | ForEach-Object {@{file=[IO.Path]::GetRelativePath($trusted,$_.FullName);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}}) | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $trusted 'manifest.json')
    }
    $c.worker.buildScript=Join-Path $trusted 'Invoke-MatrixBuild.ps1'
    $c | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Config
    $arguments='-NoProfile -NonInteractive -File "{0}" -Config "{1}" -Revision {2}' -f (Join-Path $trusted 'tests/Run-AgentValidation.ps1'),$Config,$revision
    if($RecoverLease){$arguments+=' -RecoverLease'}
    $action=New-ScheduledTaskAction -Execute (Join-Path $PSHOME 'pwsh.exe') -Argument $arguments -WorkingDirectory $trusted
    $null=Set-ScheduledTask -TaskName $DiagnosticTask -Action $action
    if($UpdateDisabledWorker){
        $action=New-ScheduledTaskAction -Execute (Get-Command node.exe).Source -Argument ('"{0}" work "{1}" timer' -f (Join-Path $trusted 'matrix.mjs'),$Config) -WorkingDirectory $trusted
        $null=Set-ScheduledTask -TaskName 'Songsmith Matrix Worker' -Action $action
        if((Get-ScheduledTask -TaskName 'Songsmith Matrix Worker').Settings.Enabled){throw 'Worker unexpectedly enabled'}
    }
    Start-ScheduledTask -TaskName $DiagnosticTask
    @{revision=$revision;trusted=$trusted;diagnosticTask=$DiagnosticTask;workerDisabled=$true} | ConvertTo-Json
}finally{
    if((Export-ScheduledTask -TaskName 'Songsmith Matrix') -ne $scannerBefore){throw 'Tony scanner changed'}
    if(-not $UpdateDisabledWorker -and (Export-ScheduledTask -TaskName 'Songsmith Matrix Worker') -ne $workerBefore){throw 'Disabled worker changed'}
}
