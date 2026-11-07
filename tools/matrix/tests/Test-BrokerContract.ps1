$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../Build-BrokerCommon.ps1')
. (Join-Path $PSScriptRoot '../Build-Lease.ps1')
$script:checks=0
function Check([bool]$Value,[string]$Label){if(-not $Value){throw $Label};$script:checks++}
function Reject([scriptblock]$Action,[string]$Label){$rejected=$false;try{$null=& $Action}catch{$rejected=$true};Check $rejected $Label}
$root=Join-Path $env:TEMP ('matrix-broker-contract-'+[Guid]::NewGuid().ToString('N'))
$null=New-Item -ItemType Directory -Path $root
try{
    foreach($function in @('Assert-MatrixPlainPath','Get-MatrixLeaseItems','Set-MatrixDacl','Set-MatrixLeaseAcl','Restore-MatrixBuildLease','Grant-MatrixBuildLease')){Check ([bool](Get-Command $function -CommandType Function -ErrorAction SilentlyContinue)) "Lease API missing: $function"}
    $now=[DateTime]::UtcNow
    $contract=[pscustomobject]@{worktreeRoot=$root;juceDirectory='C:\JUCE';maximumSeconds=10800;cmakeExecutable='C:\Tools\cmake.exe'}
    $request=[pscustomobject]@{version=1;id=('a'*32);worktree=(Join-Path $root 'EQ-38');plugins=@('eq');createdAt=$now.ToString('o');expiresAt=$now.AddMinutes(30).ToString('o')}
    Check ((Test-BrokerRequest $request $contract $now) -eq $request.worktree) 'Valid bounded worktree rejected'
    function Clone-Request {$request | ConvertTo-Json -Depth 4 | ConvertFrom-Json}
    foreach($path in @('C:\outside','C:\Windows\..\escape','\\server\share\x','C:\work:stream')){$r=Clone-Request;$r.worktree=$path;Reject {Test-BrokerRequest $r $contract $now} "Unsafe path accepted: $path"}
    $r=Clone-Request;$r | Add-Member command 'powershell evil';Reject {Test-BrokerRequest $r $contract $now} 'Command field accepted'
    foreach($plugins in @(@('eq','eq'),@('eq;evil'),@('not-a-plugin'))){$r=Clone-Request;$r.plugins=$plugins;Reject {Test-BrokerRequest $r $contract $now} 'Unsafe plugin list accepted'}
    $r=Clone-Request;$r.plugins='eq';Reject {Test-BrokerRequest $r $contract $now} 'Scalar plugin list accepted'
    $r=Clone-Request;$r.id=@('a'*32);Reject {Test-BrokerRequest $r $contract $now} 'Array request ID accepted'
    $r=Clone-Request;$r.createdAt=$now.AddMinutes(-2).ToString('o');Reject {Test-BrokerRequest $r $contract $now} 'Stale request accepted'
    $r=Clone-Request;$r.expiresAt=$now.AddDays(1).ToString('o');Reject {Test-BrokerRequest $r $contract $now} 'Excessive deadline accepted'
    $plan=@(Get-BrokerPlan $request $contract)
    Check ($plan.Count -eq 3 -and $plan[0].exe -eq $contract.cmakeExecutable -and $plan[2].exe.EndsWith('EQTests.exe')) 'Fixed build plan changed'
    Check (($plan[1].arguments -join ' ') -like '*--config Release') 'Release configuration not fixed'
    $file=Join-Path $root 'diagnostic.log';[IO.File]::WriteAllText($file,'safe diagnostic')
    Check ((Read-BrokerLog $file) -eq 'safe diagnostic') 'Normal diagnostic read failed'
    Reject {Read-BrokerLog $file 2} 'Oversized diagnostic accepted'
    $link=Join-Path $root 'hardlink.log';$null=New-Item -ItemType HardLink -Path $link -Target $file
    Reject {Read-BrokerLog $link} 'Hard-linked diagnostic read'
    Remove-Item -LiteralPath $link
    $target=Join-Path $root 'target';$null=New-Item -ItemType Directory -Path $target
    [IO.File]::WriteAllText((Join-Path $target 'test.log'),'not through a junction')
    $junction=Join-Path $root 'redirect';$null=New-Item -ItemType Junction -Path $junction -Target $target
    Reject {Read-BrokerLog (Join-Path $junction 'test.log')} 'Junction diagnostic read'
    [IO.Directory]::Delete($junction)
    # Mock Task Scheduler: inspect contracts without starting or registering tasks.
    $buildUser=[Security.Principal.WindowsIdentity]::GetCurrent()
    $ownerSid='S-1-5-21-10-20-30-1001';$agentSid='S-1-5-21-10-20-30-1009'
    $sddl="O:$ownerSid`D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;$ownerSid)(A;;FRFX;;;$agentSid)"
    $contract | Add-Member -NotePropertyMembers @{taskName='fixture';ownerSid=$ownerSid;buildSid=$buildUser.User.Value;powershellExecutable='C:\Tools\pwsh.exe';taskArguments='-NoProfile -NonInteractive -File "C:\trusted\broker.ps1"';workingDirectory='C:\Windows';taskSddl=$sddl}
    $action=[pscustomobject]@{Path=$contract.powershellExecutable;Arguments=$contract.taskArguments;WorkingDirectory=$contract.workingDirectory}
    $actions=[pscustomobject]@{Count=1;Action=$action};$actions | Add-Member ScriptMethod Item {param($index) $this.Action}
    $definition=[pscustomobject]@{Principal=[pscustomobject]@{UserId=$buildUser.Name;LogonType=1;RunLevel=0};Triggers=[pscustomobject]@{Count=0};Actions=$actions;Settings=[pscustomobject]@{MultipleInstances=2;AllowDemandStart=$true}}
    $script:fakeTask=[pscustomobject]@{Enabled=$false;Definition=$definition;Sddl=$sddl};$fakeTask | Add-Member ScriptMethod GetSecurityDescriptor {param($flags) $this.Sddl}
    $folder=[pscustomobject]@{};$folder | Add-Member ScriptMethod GetTask {param($name) $script:fakeTask}
    $script:service=[pscustomobject]@{Folder=$folder};$service | Add-Member ScriptMethod Connect {}; $service | Add-Member ScriptMethod GetFolder {param($path) $this.Folder}
    function New-Object {param($ComObject) if($ComObject -ne 'Schedule.Service'){throw 'Unexpected COM object'};$script:service}
    Check (-not (Get-BrokerTask $contract -AllowDisabled).Enabled) 'Disabled inspection failed'
    Reject {Get-BrokerTask $contract} 'Disabled task accepted for execution'
    $definition.Triggers.Count=1;Reject {Get-BrokerTask $contract -AllowDisabled} 'Triggered task accepted';$definition.Triggers.Count=0
    $definition.Principal.RunLevel=1;Reject {Get-BrokerTask $contract -AllowDisabled} 'Elevated task accepted';$definition.Principal.RunLevel=0
    $action.Arguments+=' $(Arg0)';Reject {Get-BrokerTask $contract -AllowDisabled} 'Variable command-line action accepted';$action.Arguments=$contract.taskArguments
    $fakeTask.Sddl=$sddl.Replace('FRFX','FA');Reject {Get-BrokerTask $contract -AllowDisabled} 'Agent definition-write access accepted'
    Write-Output "PASS: $script:checks broker contract/log/task-policy checks; no task was registered or started"
}finally{
    $full=[IO.Path]::GetFullPath($root)
    if(-not $full.StartsWith([IO.Path]::GetFullPath($env:TEMP).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Fixture cleanup escaped temp'}
    if($junction -and (Test-Path -LiteralPath $junction) -and (Get-Item -LiteralPath $junction).LinkType -eq 'Junction'){[IO.Directory]::Delete($junction)}
    Remove-Item -LiteralPath $root -Recurse -Force
}
