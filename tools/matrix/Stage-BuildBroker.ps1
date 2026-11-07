[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$WorkerConfig='C:\Scheduled\Songsmith-Matrix.worker.config.json',
    [string]$RegistrationCredentialFile='C:\Scheduled\Songsmith-Matrix\private\build-account.xml',
    [string]$TaskName='Songsmith Matrix Build'
)
$ErrorActionPreference='Stop'
$owner=[Security.Principal.WindowsIdentity]::GetCurrent()
if(-not ([Security.Principal.WindowsPrincipal]::new($owner)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Owner/admin staging context required'}
if(Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue){throw 'Build task already exists; refusing to replace it'}
$scannerBefore=Export-ScheduledTask -TaskName 'Songsmith Matrix'
$workerBefore=Export-ScheduledTask -TaskName 'Songsmith Matrix Worker'
$configBefore=Get-FileHash -LiteralPath $WorkerConfig
$c=Get-Content -LiteralPath $WorkerConfig -Raw | ConvertFrom-Json
if($c.liveExecution -ne $false -or $c.worker.enabled -ne $false -or $c.autoMerge -ne $false -or $c.releasePublication -ne $false){throw 'All live switches must remain false'}
if((Get-ScheduledTask -TaskName 'Songsmith Matrix Worker').Settings.Enabled){throw 'Worker must remain disabled'}
$agent=(Get-LocalUser -Name SongsmithAgent).SID.Value;$build=(Get-LocalUser -Name SongsmithBuild).SID.Value
$admins=@(Get-LocalGroupMember -SID S-1-5-32-544 | ForEach-Object {$_.SID.Value})
if($agent -eq $build -or $agent -in $admins -or $build -in $admins){throw 'Both build/controller identities must remain distinct non-admin accounts'}
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$revision=(& $c.worker.gitExecutable -C $repo rev-parse HEAD).Trim()
if($LASTEXITCODE -or $revision -notmatch '^[a-f0-9]{40}$'){throw 'Cannot pin broker source'}
$root=Split-Path -Parent $c.stateDirectory
$package=Join-Path $root ('build-broker/'+$revision)
$exchange=Join-Path $root 'build-exchange'
if((Test-Path -LiteralPath $package) -or (Test-Path -LiteralPath $exchange)){throw 'Staging paths already exist; inspect before retry'}
if(-not $PSCmdlet.ShouldProcess($TaskName,'Install fixed broker package and restricted exchange; register DISABLED task with no triggers; inspect only, never start')){return}
function Protect-Directory([string]$Path,[string]$AgentRights,[switch]$BuildRead){
    $null=New-Item -ItemType Directory -Path $Path -Force
    $acl=[Security.AccessControl.DirectorySecurity]::new();$acl.SetAccessRuleProtection($true,$false)
    foreach($sid in @($owner.User.Value,'S-1-5-18','S-1-5-32-544')){$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($sid),'FullControl','ContainerInherit,ObjectInherit','None','Allow'))}
    $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($agent),$AgentRights,'ContainerInherit,ObjectInherit','None','Allow'))
    if($BuildRead){$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($build),'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow'))}
    Set-Acl -LiteralPath $Path -AclObject $acl -ErrorAction Stop
}
try{
    Protect-Directory $package 'ReadAndExecute'
    Protect-Directory $exchange 'Modify' -BuildRead
    $lock=Join-Path $exchange 'client.lock';[IO.File]::WriteAllText($lock,'')
    $acl=Get-Acl -LiteralPath $lock;$acl.SetAccessRuleProtection($true,$true)
    foreach($rule in @($acl.Access | Where-Object {$_.IdentityReference.Translate([Security.Principal.SecurityIdentifier]).Value -eq $build})){$null=$acl.RemoveAccessRuleSpecific($rule)}
    Set-Acl -LiteralPath $lock -AclObject $acl -ErrorAction Stop
    foreach($name in @('Build-Lease.ps1','Build-BrokerCommon.ps1')){Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination $package}
    $commonHash=(Get-FileHash -LiteralPath (Join-Path $package 'Build-BrokerCommon.ps1')).Hash
    $leaseHash=(Get-FileHash -LiteralPath (Join-Path $package 'Build-Lease.ps1')).Hash
    $entry=Join-Path $package 'Build-Broker.ps1'
    (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Build-Broker.ps1') -Raw).Replace('__COMMON_HASH__',$commonHash) | Set-Content -LiteralPath $entry
    $sddl="O:$($owner.User.Value)D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;$($owner.User.Value))(A;;FRFX;;;$agent)"
    $targets=@(
        @{path=(Join-Path $c.repository '.git');directory=$true},
        @{path=(Join-Path $c.repository '.git/HEAD');directory=$false},
        @{path=(Join-Path $c.repository '.git/config');directory=$false},
        @{path=$c.stateDirectory;directory=$true},
        @{path=(Split-Path -Parent $c.worker.buildCredentialFile);directory=$true},
        @{path=$c.worker.buildCredentialFile;directory=$false},
        @{path=$WorkerConfig;directory=$false},
        @{path=$c.worker.worktreeRoot;directory=$true}
    )
    $contract=[ordered]@{version=1;taskName=$TaskName;ownerSid=$owner.User.Value;agentSid=$agent;buildSid=$build;packageDirectory=$package;workingDirectory=$env:SystemRoot;exchangeDirectory=$exchange;worktreeRoot=[IO.Path]::GetFullPath($c.worker.worktreeRoot);juceDirectory=[IO.Path]::GetFullPath($c.worker.juceDirectory);journalFile=(Join-Path (Split-Path -Parent $c.worker.buildCredentialFile) 'active-build-lease.json');cmakeExecutable=(Get-Command cmake.exe -ErrorAction Stop).Source;powershellExecutable=$c.worker.powershellExecutable;taskArguments=('-NoProfile -NonInteractive -File "{0}"' -f $entry);taskSddl=$sddl;maximumSeconds=10800;protectedPaths=$targets;entryHash=(Get-FileHash -LiteralPath $entry).Hash}
    # Client checks the exact bytes of this owner-protected contract before leasing.
    $contractPath=Join-Path $package 'broker.json';$contract | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $contractPath
    $contractHash=(Get-FileHash -LiteralPath $contractPath).Hash
    $client=Join-Path $package 'Invoke-MatrixScheduledBuild.ps1'
    (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'Invoke-MatrixScheduledBuild.ps1') -Raw).Replace('__COMMON_HASH__',$commonHash).Replace('__LEASE_HASH__',$leaseHash).Replace('__CONTRACT_HASH__',$contractHash) | Set-Content -LiteralPath $client
    (Get-Content -LiteralPath (Join-Path $PSScriptRoot 'tests/Test-ManualBuild.ps1') -Raw).Replace('__COMMON_HASH__',$commonHash) | Set-Content -LiteralPath (Join-Path $package 'Test-ManualBuild.ps1')
    foreach($file in @($entry,$contractPath,(Join-Path $package 'Build-BrokerCommon.ps1'))){
        $acl=Get-Acl -LiteralPath $file
        $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($build),'ReadAndExecute','Allow'))
        Set-Acl -LiteralPath $file -AclObject $acl -ErrorAction Stop
    }
    # Keep existing runtime configuration untouched. This is a proposal for later
    # reviewed deployment, still with every live/merge/publication switch false.
    $c.worker.buildScript=$client;$c.worker | Add-Member -NotePropertyName buildMode -NotePropertyValue scheduled -Force
    $c | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $package 'worker.config.proposed.json')
    $action=New-ScheduledTaskAction -Execute $contract.powershellExecutable -Argument $contract.taskArguments -WorkingDirectory $contract.workingDirectory
    $principal=New-ScheduledTaskPrincipal -UserId 'MATRIX\SongsmithBuild' -LogonType Password -RunLevel Limited
    $settings=New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Seconds 10830) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
    $settings.Enabled=$false
    $definition=New-ScheduledTask -Action $action -Principal $principal -Settings $settings
    $credential=Import-Clixml -LiteralPath $RegistrationCredentialFile
    if($credential -isnot [PSCredential] -or ([Security.Principal.NTAccount]::new($credential.UserName)).Translate([Security.Principal.SecurityIdentifier]).Value -ne $build){throw 'Registration credential is not the build identity'}
    $null=Register-ScheduledTask -TaskName $TaskName -InputObject $definition -User $credential.UserName -Password $credential.GetNetworkCredential().Password -ErrorAction Stop
    $service=New-Object -ComObject 'Schedule.Service';$service.Connect();$registered=$service.GetFolder('\').GetTask($TaskName)
    $registered.SetSecurityDescriptor($sddl,16)
    $installed=Get-ScheduledTask -TaskName $TaskName
    if($installed.Settings.Enabled -or $installed.Triggers.Count -ne 0 -or $registered.GetInstances(0).Count -ne 0){throw 'Broker was unexpectedly enabled or running'}
    $info=Get-ScheduledTaskInfo -TaskName $TaskName
    if($info.LastTaskResult -ne 267011){throw 'New task does not report never-run state'}
    & $client -Inspect
    & (Join-Path $package 'Test-ManualBuild.ps1') -PlanOnly
    @{revision=$revision;task=$TaskName;enabled=$false;triggers=0;lastTaskResult=$info.LastTaskResult;package=$package;client=$client;contractHash=$contractHash;liveExecution=$false;workerEnabled=$false;autoMerge=$false;releasePublication=$false} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $package 'staging.json')
    Get-Content -LiteralPath (Join-Path $package 'staging.json') -Raw
}finally{
    $credential=$null
    if((Export-ScheduledTask -TaskName 'Songsmith Matrix') -ne $scannerBefore -or (Export-ScheduledTask -TaskName 'Songsmith Matrix Worker') -ne $workerBefore -or (Get-FileHash -LiteralPath $WorkerConfig).Hash -ne $configBefore.Hash){throw 'An existing scanner/worker task or runtime config changed'}
}
