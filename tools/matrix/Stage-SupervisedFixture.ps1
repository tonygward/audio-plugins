[CmdletBinding(SupportsShouldProcess)]
param(
 [string]$Broker='C:\Scheduled\Songsmith-Matrix\build-broker\769e296f791257f50d73118d1c385c32e9cc3d85',
 [PSCredential]$Credential
)
$ErrorActionPreference='Stop'
$taskName='Songsmith Matrix Supervised Fixture'
$owner=[Security.Principal.WindowsIdentity]::GetCurrent()
if(-not ([Security.Principal.WindowsPrincipal]::new($owner)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Administrator staging required'}
if(Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue){throw 'Fixture task exists; refusing replacement'}
$before=@{};foreach($name in @('Songsmith Matrix','Songsmith Matrix Worker','Songsmith Matrix Build')){$before[$name]=Export-ScheduledTask -TaskName $name}
$diagnostics=@{};Get-ScheduledTask -TaskName 'Songsmith Matrix Agent Validation *' | ForEach-Object {$diagnostics[$_.TaskName]=Export-ScheduledTask -TaskName $_.TaskName}
& (Join-Path $Broker 'Invoke-MatrixScheduledBuild.ps1') -Inspect
$c=Get-Content (Join-Path $Broker 'broker.json') -Raw | ConvertFrom-Json
$agent=(Get-LocalUser SongsmithAgent).SID.Value
if($agent -ne $c.agentSid -or $agent -in @(Get-LocalGroupMember -SID S-1-5-32-544 | ForEach-Object {$_.SID.Value})){throw 'Unexpected or administrative Agent'}
if($Credential -and ([Security.Principal.NTAccount]::new($Credential.UserName)).Translate([Security.Principal.SecurityIdentifier]).Value -ne $agent){throw 'Registration credential must be existing Agent'}
$destination=Join-Path (Split-Path (Split-Path $Broker -Parent) -Parent) 'supervised-fixture'
if(Test-Path -LiteralPath $destination){throw 'Staging directory exists; inspect rather than overwrite'}
if(-not $PSCmdlet.ShouldProcess($taskName,'Stage owner-protected fixed fixture; register disabled only when existing Agent credential supplied')){return}
try{
 $null=New-Item -ItemType Directory -Path $destination
 $acl=[Security.AccessControl.DirectorySecurity]::new();$acl.SetAccessRuleProtection($true,$false)
 foreach($sid in @($owner.User.Value,'S-1-5-18','S-1-5-32-544')){$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($sid),'FullControl','ContainerInherit,ObjectInherit','None','Allow'))}
 $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($agent),'ReadAndExecute','ContainerInherit,ObjectInherit','None','Allow'))
 Set-Acl -LiteralPath $destination -AclObject $acl
 $hashes=@();foreach($name in @('Test-ManualBuild.ps1','Invoke-MatrixScheduledBuild.ps1','Build-Broker.ps1','Build-BrokerCommon.ps1','Build-Lease.ps1','broker.json','worker.config.proposed.json')){$hashes+="'$name'='$((Get-FileHash -LiteralPath (Join-Path $Broker $name)).Hash)'"}
 $entry=Join-Path $destination 'Supervised-Fixture.ps1'
 (Get-Content (Join-Path $PSScriptRoot 'Supervised-Fixture.ps1') -Raw).Replace('__BROKER__',$Broker).Replace('__AGENT__',$agent).Replace("'__HASHES__'=''",($hashes -join ';')) | Set-Content -LiteralPath $entry
 $action=New-ScheduledTaskAction -Execute $c.powershellExecutable -Argument ('-NoProfile -NonInteractive -File "{0}"' -f $entry) -WorkingDirectory $env:SystemRoot
 $principal=New-ScheduledTaskPrincipal -UserId 'MATRIX\SongsmithAgent' -LogonType Password -RunLevel Limited
 $settings=New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 55)
 $settings.Enabled=$false
 $definition=New-ScheduledTask -Action $action -Principal $principal -Settings $settings
 $definition | Export-Clixml -LiteralPath (Join-Path $destination 'definition.clixml')
 $sddl="O:$($owner.User.Value)D:P(A;;FA;;;SY)(A;;FA;;;BA)(A;;FA;;;$($owner.User.Value))(A;;FR;;;$agent)"
 @{task=$taskName;entry=$entry;sha256=(Get-FileHash $entry).Hash;sddl=$sddl;agentSid=$agent;disabled=$true;triggers=0} | ConvertTo-Json | Set-Content (Join-Path $destination 'staging.json')
 Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Register-SupervisedFixture.ps1') -Destination $destination
 if($Credential){
  $null=Register-ScheduledTask -TaskName $taskName -InputObject $definition -User $Credential.UserName -Password $Credential.GetNetworkCredential().Password
  $service=New-Object -ComObject Schedule.Service;$service.Connect();$task=$service.GetFolder('\').GetTask($taskName);$task.SetSecurityDescriptor($sddl,16)
  if($task.Enabled -or $task.Definition.Triggers.Count -ne 0 -or $task.GetInstances(0).Count -ne 0){throw 'Fixture registration violated disabled contract'}
 }
 Get-Content (Join-Path $destination 'staging.json')
}finally{
 foreach($name in $before.Keys){if((Export-ScheduledTask -TaskName $name) -ne $before[$name]){throw 'Existing task changed'}}
 foreach($name in $diagnostics.Keys){if((Export-ScheduledTask -TaskName $name) -ne $diagnostics[$name]){throw 'Diagnostic task changed'}}
}

