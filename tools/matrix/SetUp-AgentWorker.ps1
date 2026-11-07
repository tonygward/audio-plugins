[CmdletBinding(SupportsShouldProcess)]
param(
    [string]$ScannerConfig = 'C:\Scheduled\Songsmith-Matrix.config.json',
    [string]$WorkerConfig = 'C:\Scheduled\Songsmith-Matrix.worker.config.json',
    [string]$Root = 'C:\Scheduled\Songsmith-Matrix',
    [string]$TaskName = 'Songsmith Matrix Worker',
    [switch]$Resume,
    [switch]$RunDiagnostics
)
$ErrorActionPreference = 'Stop'
$account = 'MATRIX\SongsmithAgent'
$scannerBefore = Export-ScheduledTask -TaskName 'Songsmith Matrix'
$registered = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
if ($registered -and -not $Resume) { throw 'Worker task already exists; never replace it implicitly' }
if ($Resume -and -not $registered) { throw 'No disabled worker to resume' }
$baseline = Get-Content -LiteralPath $ScannerConfig -Raw | ConvertFrom-Json
if ($baseline.liveExecution -ne $false -or $baseline.autoMerge -ne $false -or $baseline.releasePublication -ne $false) { throw 'Scanner must remain in dry-run mode' }
$existingWorker = Get-Content -LiteralPath $WorkerConfig -Raw | ConvertFrom-Json
if ($existingWorker.liveExecution -ne $false -or $existingWorker.worker.enabled -ne $false) { throw 'Worker must be disabled' }
$agentSid = ([Security.Principal.NTAccount]::new($account)).Translate([Security.Principal.SecurityIdentifier]).Value
$buildSid = ([Security.Principal.NTAccount]::new('MATRIX\SongsmithBuild')).Translate([Security.Principal.SecurityIdentifier]).Value
$adminMembers = @(Get-LocalGroupMember -SID 'S-1-5-32-544' | ForEach-Object { $_.SID.Value })
if ($agentSid -in $adminMembers -or $buildSid -in $adminMembers) { throw 'Agent/build accounts must not be administrators' }
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$revision = (& $baseline.worker.gitExecutable -C $repo rev-parse HEAD).Trim()
if ($LASTEXITCODE -or $revision -notmatch '^[a-f0-9]{40}$') { throw 'Cannot pin trusted source revision' }
if ($Resume) {
    $revision = Split-Path -Leaf $registered.Actions[0].WorkingDirectory
    $registeredSid = ([Security.Principal.NTAccount]::new($registered.Principal.UserId)).Translate([Security.Principal.SecurityIdentifier]).Value
    if ($registered.Settings.Enabled -or $registeredSid -ne $agentSid -or [int]$registered.Principal.LogonType -ne 1 -or [int]$registered.Principal.RunLevel -ne 0 -or $revision -notmatch '^[a-f0-9]{40}$') { throw 'Existing worker does not match the disabled restricted deployment' }
}
$trusted = Join-Path $Root ('agent-trusted/' + $revision)
$private = Join-Path $Root 'agent-private'
$state = Join-Path $Root 'agent-state'
$worktrees = Join-Path $Root 'agent-worktrees'
if (-not $Resume -and ((Test-Path -LiteralPath $trusted) -or (Test-Path -LiteralPath $private))) { throw 'Agent deployment already exists; inspect before retry' }
if ($Resume -and ($existingWorker.worker.buildScript -ne (Join-Path $trusted 'Invoke-MatrixBuild.ps1') -or $existingWorker.worker.buildCredentialFile -ne (Join-Path $private 'build-account.xml'))) { throw 'Resume configuration paths do not match' }
$operation = if ($Resume) { 'Prompt for existing password and finish per-user DPAPI export; keep the existing worker disabled/unchanged; optionally start a separate fixed diagnostic task' } else { 'Prompt for existing password, install isolated worker bundle/config/ACLs, export per-user DPAPI credential and register a DISABLED password-logon task; optionally run a fixed diagnostic task' }
if (-not $PSCmdlet.ShouldProcess($account, $operation)) { return }

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Security;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class MatrixAgentLogon {
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct Info {
        public int Size; public IntPtr Parent; public string Message, Caption; public IntPtr Banner;
    }
    [DllImport("credui.dll", CharSet=CharSet.Unicode)] static extern int CredUIPromptForCredentialsW(ref Info i, string target, IntPtr reserved, int error, StringBuilder user, int userMax, StringBuilder password, int passMax, ref bool save, int flags);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool LogonUserW(string user, string domain, string password, int type, int provider, out SafeAccessTokenHandle token);
    public static SecureString Prompt(string account) {
        var info = new Info { Size=Marshal.SizeOf<Info>(), Message="Enter the EXISTING password for " + account + ". No password reset. Used for a disabled worker and fixed validation task only.", Caption="Songsmith restricted worker setup" };
        var user = new StringBuilder(account, 514); var password = new StringBuilder(256); bool save=false;
        int error = CredUIPromptForCredentialsW(ref info, "Songsmith Matrix Worker", IntPtr.Zero, 0, user, 514, password, 256, ref save, 0x14008A);
        try {
            if (error != 0) throw new Win32Exception(error);
            if (!String.Equals(account, user.ToString(), StringComparison.OrdinalIgnoreCase)) throw new InvalidOperationException("Unexpected credential identity");
            var result = new SecureString(); for(int n=0;n<password.Length;n++) result.AppendChar(password[n]); result.MakeReadOnly(); return result;
        } finally { for(int n=0;n<password.Length;n++) password[n]='\0'; password.Clear(); }
    }
    public static SafeAccessTokenHandle Batch(string user, string password) {
        SafeAccessTokenHandle token;
        if (!LogonUserW(user, "MATRIX", password, 4, 0, out token)) throw new Win32Exception(Marshal.GetLastWin32Error());
        return token;
    }
    public static void Validate(string user, string password) {
        SafeAccessTokenHandle token;
        if (!LogonUserW(user, "MATRIX", password, 2, 0, out token)) throw new Win32Exception(Marshal.GetLastWin32Error());
        token.Dispose();
    }
}
'@
Write-Output 'Waiting for the native Windows credential dialog; do not enter a password into chat.'
$secure = [MatrixAgentLogon]::Prompt($account)
$agentCredential = [PSCredential]::new($account, $secure)
$password = $agentCredential.GetNetworkCredential().Password
[MatrixAgentLogon]::Validate('SongsmithAgent',$password)
$ownerSid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
function Set-PrivateDirectory([string]$Path, [string]$AgentRights) {
    $null = New-Item -ItemType Directory -Path $Path -Force
    $acl = [Security.AccessControl.DirectorySecurity]::new(); $acl.SetAccessRuleProtection($true,$false)
    foreach ($sid in @($ownerSid,'S-1-5-18','S-1-5-32-544')) { $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($sid),'FullControl','ContainerInherit,ObjectInherit','None','Allow')) }
    $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($agentSid),$AgentRights,'ContainerInherit,ObjectInherit','None','Allow'))
    Set-Acl -LiteralPath $Path -AclObject $acl
}
$diagnosticTask = $null
try {
    if (-not $Resume) {
    Set-PrivateDirectory $private 'Modify'
    Set-PrivateDirectory $state 'Modify'
    Set-PrivateDirectory $worktrees 'Modify'
    Set-PrivateDirectory $trusted 'ReadAndExecute'
    foreach ($file in Get-ChildItem -LiteralPath $PSScriptRoot -Filter '*.mjs') { Copy-Item -LiteralPath $file.FullName -Destination $trusted }
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'Invoke-MatrixBuild.ps1') -Destination $trusted
    $tests = Join-Path $trusted 'tests'; $null = New-Item -ItemType Directory -Path $tests
    foreach ($name in @('Test-ControllerContext.ps1','Test-BuildLease.ps1','Test-BuildInterruption.ps1','Test-BuildLinkSafety.ps1','Test-RealEqBuild.ps1','Run-AgentValidation.ps1')) { Copy-Item -LiteralPath (Join-Path $PSScriptRoot "tests/$name") -Destination $tests }
    # Grant Build only the single installed launcher file, never the bundle/root.
    $launcher = Join-Path $trusted 'Invoke-MatrixBuild.ps1'
    $acl = Get-Acl -LiteralPath $launcher
    $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($buildSid),'ReadAndExecute','Allow'))
    Set-Acl -LiteralPath $launcher -AclObject $acl
    Copy-Item -LiteralPath $WorkerConfig -Destination (Join-Path $private 'worker-config.before.json')
    $c = $existingWorker
    $c.stateDirectory = $state; $c.worker.worktreeRoot = $worktrees
    $c.worker.buildCredentialFile = Join-Path $private 'build-account.xml'
    $c.worker.approvalFile = Join-Path $private 'worker-approval.json'
    $c.worker.buildScript = $launcher
    $c.liveExecution = $false; $c.worker.enabled = $false; $c.autoMerge = $false; $c.releasePublication = $false
    $c | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $WorkerConfig
    $action = New-ScheduledTaskAction -Execute (Get-Command node.exe).Source -Argument ('"{0}" work "{1}" timer' -f (Join-Path $trusted 'matrix.mjs'),$WorkerConfig) -WorkingDirectory $trusted
    $principal = New-ScheduledTaskPrincipal -UserId $account -LogonType Password -RunLevel Limited
    $settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -StartWhenAvailable -ExecutionTimeLimit (New-TimeSpan -Hours 4 -Minutes 15)
    $settings.Enabled = $false
    $triggers = @((New-ScheduledTaskTrigger -AtStartup),(New-ScheduledTaskTrigger -Once -At (Get-Date).AddDays(1) -RepetitionInterval (New-TimeSpan -Minutes 5)))
    $task = New-ScheduledTask -Action $action -Principal $principal -Settings $settings -Trigger $triggers
    $null = Register-ScheduledTask -TaskName $TaskName -InputObject $task -User $account -Password $password
    } else {
        $c = $existingWorker
        $tests = Join-Path $trusted 'tests'
    }
    # Registering Password logon establishes batch-logon rights. Re-encrypt in the
    # verified batch identity, with no plaintext credential file or password argument.
    $buildCredential = Import-Clixml -LiteralPath $baseline.worker.buildCredentialFile
    $token = [MatrixAgentLogon]::Batch('SongsmithAgent',$password)
    try {
        [Security.Principal.WindowsIdentity]::RunImpersonated($token,[Action]{
            if ([Security.Principal.WindowsIdentity]::GetCurrent().User.Value -ne $agentSid) { throw 'Impersonation failed' }
            $buildCredential | Export-Clixml -LiteralPath $c.worker.buildCredentialFile
        })
    } finally { $token.Dispose() }
    if ($RunDiagnostics) {
        $principal = New-ScheduledTaskPrincipal -UserId $account -LogonType Password -RunLevel Limited
        $diagnosticTask = 'Songsmith Matrix Agent Validation ' + [Guid]::NewGuid().ToString('N')
        $a = New-ScheduledTaskAction -Execute (Join-Path $PSHOME 'pwsh.exe') -Argument ('-NoProfile -NonInteractive -File "{0}" -Config "{1}" -Revision {2}' -f (Join-Path $tests 'Run-AgentValidation.ps1'),$WorkerConfig,$revision) -WorkingDirectory $trusted
        $s = New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 30)
        $t = New-ScheduledTask -Action $a -Principal $principal -Settings $s
        $null = Register-ScheduledTask -TaskName $diagnosticTask -InputObject $t -User $account -Password $password
        Start-ScheduledTask -TaskName $diagnosticTask
    }
    $result = @{revision=$revision; trusted=$trusted; workerTask=$TaskName; workerDisabled=((Get-ScheduledTask -TaskName $TaskName).Settings.Enabled -eq $false); diagnosticTask=$diagnosticTask; config=$WorkerConfig; liveExecution=$c.liveExecution; workerEnabled=$c.worker.enabled; autoMerge=$c.autoMerge; releasePublication=$c.releasePublication}
    $result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $private 'deployment.json')
    $result | ConvertTo-Json
} finally {
    $password=$null; $buildCredential=$null; $agentCredential=$null; $secure.Dispose()
    if ((Export-ScheduledTask -TaskName 'Songsmith Matrix') -ne $scannerBefore) { throw 'Tony scanner XML changed' }
}
