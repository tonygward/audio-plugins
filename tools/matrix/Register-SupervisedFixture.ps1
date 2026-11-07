[CmdletBinding(SupportsShouldProcess)]
param([Parameter(Mandatory)][PSCredential]$Credential)
$ErrorActionPreference='Stop'
$directory='C:\Scheduled\Songsmith-Matrix\supervised-fixture'
$m=Get-Content (Join-Path $directory 'staging.json') -Raw | ConvertFrom-Json
$agent=(Get-LocalUser SongsmithAgent).SID.Value
if($agent -in @(Get-LocalGroupMember -SID S-1-5-32-544 | ForEach-Object {$_.SID.Value})){throw 'Agent must remain non-admin'}
if($m.task -ne 'Songsmith Matrix Supervised Fixture' -or $m.agentSid -ne $agent -or ([Security.Principal.NTAccount]::new($Credential.UserName)).Translate([Security.Principal.SecurityIdentifier]).Value -ne $agent){throw 'Unexpected task or credential identity'}
if($m.entry -ne (Join-Path $directory 'Supervised-Fixture.ps1') -or (Get-FileHash $m.entry).Hash -ne $m.sha256){throw 'Staged entry integrity failed'}
if(Get-ScheduledTask -TaskName $m.task -ErrorAction SilentlyContinue){throw 'Refusing to replace an existing task'}
if(-not $PSCmdlet.ShouldProcess($m.task,'Register DISABLED fixed fixture task only; no start or enable')){return}
$action=New-ScheduledTaskAction -Execute 'C:\Program Files\PowerShell\7\pwsh.exe' -Argument ('-NoProfile -NonInteractive -File "{0}"' -f $m.entry) -WorkingDirectory $env:SystemRoot
$principal=New-ScheduledTaskPrincipal -UserId 'MATRIX\SongsmithAgent' -LogonType Password -RunLevel Limited
$settings=New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit (New-TimeSpan -Minutes 55)
$settings.Enabled=$false
$definition=New-ScheduledTask -Action $action -Principal $principal -Settings $settings
$null=Register-ScheduledTask -TaskName $m.task -InputObject $definition -User $Credential.UserName -Password $Credential.GetNetworkCredential().Password
$s=New-Object -ComObject Schedule.Service;$s.Connect();$t=$s.GetFolder('\').GetTask($m.task);$t.SetSecurityDescriptor($m.sddl,16)
if($t.Enabled -or $t.Definition.Triggers.Count -ne 0 -or $t.GetInstances(0).Count -ne 0){throw 'Disabled registration verification failed'}
$actualSid=([Security.Principal.NTAccount]::new($t.Definition.Principal.UserId)).Translate([Security.Principal.SecurityIdentifier]).Value
if($actualSid -ne $agent -or [int]$t.Definition.Principal.LogonType -ne 1 -or [int]$t.Definition.Principal.RunLevel -ne 0 -or $t.Definition.Actions.Count -ne 1 -or $t.Definition.Actions.Item(1).Arguments -ne $action.Arguments){throw 'Fixed task identity/action mismatch'}
$actual=[Security.AccessControl.RawSecurityDescriptor]::new($t.GetSecurityDescriptor(5))
$expected=[Security.AccessControl.RawSecurityDescriptor]::new($m.sddl)
if($actual.GetSddlForm('Owner,Access') -ne $expected.GetSddlForm('Owner,Access')){throw 'Restricted task DACL mismatch'}
Write-Output 'Registered disabled fixture task; zero triggers; not started.'
