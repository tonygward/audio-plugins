[CmdletBinding()]
param([string]$RequestFile,[string]$CredentialFile,[string]$WorktreeRoot,[string]$RecoverLease,[switch]$Inspect)
$ErrorActionPreference='Stop'
$common=Join-Path $PSScriptRoot 'Build-BrokerCommon.ps1'
$leaseCode=Join-Path $PSScriptRoot 'Build-Lease.ps1'
if((Get-FileHash -LiteralPath $common).Hash -ne '__COMMON_HASH__' -or (Get-FileHash -LiteralPath $leaseCode).Hash -ne '__LEASE_HASH__'){throw 'Scheduled build dependency hash mismatch'}
. $common
. $leaseCode
$contract=Read-BrokerContract (Join-Path $PSScriptRoot 'broker.json') '__CONTRACT_HASH__'
if((Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'Build-Broker.ps1')).Hash -ne $contract.entryHash){throw 'Build task entry changed'}
$task=Get-BrokerTask $contract -AllowDisabled:$Inspect
if($Inspect){
    @{task=$contract.taskName;enabled=$task.Enabled;buildSid=$contract.buildSid;agentSid=$contract.agentSid;triggers=$task.Definition.Triggers.Count;fixedActionVerified=$true;taskDaclVerified=$true;dependenciesVerified=$true} | ConvertTo-Json
    return
}
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
if($identity.User.Value -ne $contract.agentSid -or ([Security.Principal.WindowsPrincipal]::new($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Build client requires the restricted Agent identity'}
if($task.GetInstances(0).Count -ne 0){throw 'A build task is already running; operator review required'}
if($RecoverLease){
    if([IO.Path]::GetFullPath($RecoverLease) -ne $contract.journalFile){throw 'Unexpected lease journal'}
    Restore-MatrixBuildLease $RecoverLease
    return
}
# CredentialFile is retained for worker CLI compatibility; it is never opened.
if((Assert-BrokerPath $WorktreeRoot) -ne $contract.worktreeRoot){throw 'Worktree root differs from owner contract'}
if(Test-Path -LiteralPath $contract.journalFile){throw 'Outstanding lease requires operator recovery'}
$inputRequest=Get-Content -LiteralPath $RequestFile -Raw | ConvertFrom-Json
Assert-BrokerKeys $inputRequest @('worktree','juceDirectory','plugins','isolationTargets')
if((Assert-BrokerPath $inputRequest.juceDirectory) -ne $contract.juceDirectory){throw 'JUCE path differs from owner contract'}
$now=[DateTime]::UtcNow
$request=[pscustomobject]@{version=1;id=[Guid]::NewGuid().ToString('N');worktree=$inputRequest.worktree;plugins=@($inputRequest.plugins);createdAt=$now.ToString('o');expiresAt=$now.AddSeconds([Math]::Min($contract.maximumSeconds,3000*@($inputRequest.plugins).Count+30)).ToString('o')}
$root=Test-BrokerRequest $request $contract
if(Test-Path -LiteralPath (Join-Path $root 'build/matrix/CANCEL_BUILD')){throw 'Build was cancelled before startup'}
if([IO.Path]::GetFullPath($RequestFile) -ne (Join-Path $root 'build/matrix/request.json')){throw 'Unexpected controller request location'}
Assert-MatrixPlainPath $root
$lock=[IO.File]::Open((Join-Path $contract.exchangeDirectory 'client.lock'),'Open','ReadWrite','None')
$leased=$false;$mayHaveStarted=$false;$confirmedStopped=$false;$running=$null
# Diagnostic log beside the lease journal (owned by the Agent); never contains secrets.
$controllerLog=Join-Path (Split-Path -Parent $contract.journalFile) 'controller.log'
function Write-ControllerLog([string]$Message){
    try{Add-Content -LiteralPath $controllerLog -Value ('{0} {1}' -f [DateTime]::UtcNow.ToString('o'),$Message)}catch{}
}
# The build entry reads heartbeat.json at the same moment it is replaced, so a brief
# sharing violation is normal and must not abort the controller.
function Write-Heartbeat {
    $path=Join-Path $contract.exchangeDirectory 'heartbeat.json';$temp=$path+'.tmp'
    @{id=$request.id;time=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json | Set-Content -LiteralPath $temp
    for($try=1;;$try++){
        try{[IO.File]::Move($temp,$path,$true);return}
        catch [IO.IOException]{if($try -ge 40){throw};Start-Sleep -Milliseconds 25}
        catch [UnauthorizedAccessException]{if($try -ge 40){throw};Start-Sleep -Milliseconds 25}
    }
}
function Save-BrokerJournal([hashtable]$Fields){
    $value=Get-Content -LiteralPath $contract.journalFile -Raw | ConvertFrom-Json
    foreach($key in $Fields.Keys){$value | Add-Member -NotePropertyName $key -NotePropertyValue $Fields[$key] -Force}
    $temp=$contract.journalFile+'.tmp';$bytes=[Text.Encoding]::UTF8.GetBytes(($value | ConvertTo-Json -Depth 8))
    $stream=[IO.File]::Open($temp,'Create','Write','None')
    try{$stream.Write($bytes);$stream.Flush($true)}finally{$stream.Dispose()}
    [IO.File]::Move($temp,$contract.journalFile,$true)
}
function Remember-Processes {
    $all=@(Get-CimInstance Win32_Process -ErrorAction Stop)
    do{
        $added=$false
        foreach($p in $all){if($known.Contains([int]$p.ParentProcessId) -and $known.Add([int]$p.ProcessId)){$added=$true}}
    }while($added)
    @($all | Where-Object {$known.Contains([int]$_.ProcessId)})
}
try{
    if($task.GetInstances(0).Count -ne 0){throw 'Build became busy before lease'}
    Grant-MatrixBuildLease $root $contract.worktreeRoot $contract.buildSid $contract.journalFile @($request.plugins)
    $leased=$true
    Save-BrokerJournal @{brokerId=$request.id;brokerTask=$contract.taskName;brokerPhase='lease-granted'}
    $mailbox=Join-Path $contract.exchangeDirectory 'request.json'
    $temp=$mailbox+'.tmp';$request | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $temp
    [IO.File]::Move($temp,$mailbox,$true)
    @{id=$request.id;requestHash=(Get-FileHash -LiteralPath $mailbox).Hash} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $root 'build/matrix/BROKER_LEASE.json')
    Write-Heartbeat
    $mayHaveStarted=$true
    Save-BrokerJournal @{brokerPhase='starting-task'}
    $running=$task.Run($null)
    $instance=$running.InstanceGuid
    Save-BrokerJournal @{brokerInstance=$instance;brokerPhase='running'}
    $startupDeadline=[DateTime]::UtcNow.AddSeconds(30)
    do{$running.Refresh();$engine=[int]$running.EnginePID;if($engine -gt 0){break};Start-Sleep -Milliseconds 100}while([DateTime]::UtcNow -lt $startupDeadline)
    if($engine -le 0){throw 'No scheduler engine identity observed; retain lease for review'}
    Write-ControllerLog "Build task started; engine $engine"
    $known=[Collections.Generic.HashSet[int]]::new()
    # Engine PID is supplied by Task Scheduler, never by build output.
    $null=$known.Add($engine)
    $lastBeat=[DateTime]::MinValue;$cancelled=$false
    while($task.GetInstances(0).Count -gt 0){
        if($running.InstanceGuid -ne $instance){throw 'Scheduler instance changed'}
        $null=Remember-Processes
        if([DateTime]::UtcNow -gt (ConvertTo-BrokerTime $request.expiresAt) -or (Test-Path -LiteralPath (Join-Path $root 'build/matrix/CANCEL_BUILD'))){$cancelled=$true;$task.Stop(0)}
        if(([DateTime]::UtcNow-$lastBeat).TotalSeconds -ge 1){Write-Heartbeat;$lastBeat=[DateTime]::UtcNow}
        Start-Sleep -Milliseconds 250
    }
    # A shared task engine may remain alive, but its recorded descendants may not.
    $null=$known.Remove($engine)
    # Children such as the console host can still be exiting when the instance ends; give them a short grace period, still failing closed.
    $grace=[DateTime]::UtcNow.AddSeconds(20)
    do{$remaining=@(Remember-Processes);if(-not $remaining.Count){break};Start-Sleep -Milliseconds 500}while([DateTime]::UtcNow -lt $grace)
    if($remaining.Count){Write-ControllerLog ('Still running: '+(($remaining | ForEach-Object {'{0}({1}) parent {2}' -f $_.Name,$_.ProcessId,$_.ParentProcessId}) -join '; '))}
    if($remaining.Count){throw 'Recorded build descendants remain; retain lease for operator recovery'}
    $confirmedStopped=$true
    Save-BrokerJournal @{brokerPhase='stopped'}
    if($cancelled){throw 'Build cancelled'}
    $exitCode=[int]$task.LastTaskResult
    $logs=Join-Path $root ($request.plugins[0]+'/build/matrix-broker/'+$request.id)
    $stdout='';$stderr=''
    if(Test-Path -LiteralPath $logs){
        Assert-MatrixPlainPath $logs
        $names=@(Get-BrokerPlan $request $contract | ForEach-Object {$_.name+'.stdout.log';$_.name+'.stderr.log'})
        foreach($name in $names){
            $file=Join-Path $logs $name
            if(-not(Test-Path -LiteralPath $file)){continue}
            $text=Read-BrokerLog $file
            if($name -like '*.stderr.log'){$stderr+=$text}else{$stdout+=$text}
            if([Text.Encoding]::UTF8.GetByteCount($stdout+$stderr) -gt 4194304){throw 'Build diagnostic output exceeded limit'}
        }
    }
    Set-Content -LiteralPath (Join-Path $root 'build/matrix/build.stdout.log') -Value $stdout
    Set-Content -LiteralPath (Join-Path $root 'build/matrix/build.stderr.log') -Value $stderr
    [Console]::Out.Write($stdout);[Console]::Error.Write($stderr)
    $resultCode=$exitCode
}catch{
    Write-ControllerLog ('FAILED: '+$_.Exception.GetType().Name+': '+$_.Exception.Message+' at '+$_.InvocationInfo.PositionMessage)
    throw
}finally{
    try{
        if($leased){
            if(-not $mayHaveStarted -or $confirmedStopped){Restore-MatrixBuildLease $contract.journalFile}
            else{Write-Warning 'Task termination uncertain; lease retained for explicit operator recovery'}
        }
    }finally{$lock.Dispose()}
}
exit $resultCode
