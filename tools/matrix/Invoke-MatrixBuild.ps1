[CmdletBinding()]
param(
    [string]$RequestFile,
    [string]$CredentialFile,
    [string]$WorktreeRoot,
    [string]$RecoverLease,
    [string]$ControllerSid,
    [switch]$Child
)
$ErrorActionPreference = 'Stop'

# These functions live in the approval-hashed launcher, not in the checkout.
function Assert-MatrixPlainPath([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if ($item.LinkType -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Linked build path: $Path" }
    $parent = $item.DirectoryName
    if ($item.PSIsContainer) { $parent = $item.Parent.FullName }
    if ($parent) { Assert-MatrixPlainPath $parent }
}

function Get-MatrixLeaseItems([string]$Root) {
    # Enumerate one level at a time: reject junctions before descending into them.
    Assert-MatrixPlainPath $Root
    Get-Item -LiteralPath $Root -Force
    foreach ($item in Get-ChildItem -LiteralPath $Root -Force) {
        if ($item.LinkType -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw "Linked lease item: $($item.FullName)" }
        if ($item.PSIsContainer) { Get-MatrixLeaseItems $item.FullName } else { $item }
    }
}

function Set-MatrixDacl([string]$Path, $Acl) {
    if (-not ('MatrixDacl' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class MatrixDacl {
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool ConvertStringSecurityDescriptorToSecurityDescriptorW(string text,uint revision,out IntPtr descriptor,out uint size);
    [DllImport("advapi32.dll", SetLastError=true)] static extern bool GetSecurityDescriptorDacl(IntPtr descriptor,out bool present,out IntPtr dacl,out bool defaulted);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode)] static extern uint SetNamedSecurityInfoW(string name,int type,uint flags,IntPtr owner,IntPtr group,IntPtr dacl,IntPtr sacl);
    [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr memory);
    public static void Set(string path,string sddl,bool protect) {
        IntPtr descriptor; uint size;
        if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl,1,out descriptor,out size)) throw new Win32Exception(Marshal.GetLastWin32Error());
        try {
            bool present,defaulted; IntPtr dacl;
            if(!GetSecurityDescriptorDacl(descriptor,out present,out dacl,out defaulted)) throw new Win32Exception(Marshal.GetLastWin32Error());
            if(!present || dacl==IntPtr.Zero) throw new InvalidOperationException("A non-null DACL is required");
            // DACL plus its inheritance flag only: never request SACL, owner or
            // group writes, which require privileges the controller must not have.
            uint error=SetNamedSecurityInfoW(path,1,4u | (protect ? 0x80000000u : 0x20000000u),IntPtr.Zero,IntPtr.Zero,dacl,IntPtr.Zero);
            if(error!=0) throw new Win32Exception((int)error);
        } finally { LocalFree(descriptor); }
    }
}
'@
    }
    [MatrixDacl]::Set($Path,$Acl.GetSecurityDescriptorSddlForm('Access'),$Acl.AreAccessRulesProtected)
}

function Set-MatrixLeaseAcl($Item, [string]$ControllerSid, [string]$BuildSid, [string]$Rights) {
    $acl = if ($Item.PSIsContainer) { [Security.AccessControl.DirectorySecurity]::new() } else { [Security.AccessControl.FileSecurity]::new() }
    $acl.SetAccessRuleProtection($true, $false)
    $inheritance = if ($Item.PSIsContainer) { [Security.AccessControl.InheritanceFlags]'ContainerInherit,ObjectInherit' } else { [Security.AccessControl.InheritanceFlags]::None }
    # No broad Users/Authenticated Users or CREATOR OWNER grants. OWNER RIGHTS
    # suppresses the owner's implicit WRITE_DAC on build-created output files.
    foreach ($sid in @($ControllerSid, 'S-1-5-18', 'S-1-5-32-544')) {
        $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($sid), 'FullControl', $inheritance, 'None', 'Allow'))
    }
    $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new('S-1-3-4'), 'ReadPermissions', $inheritance, 'None', 'Allow'))
    if ($Rights) {
        $acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new([Security.Principal.SecurityIdentifier]::new($BuildSid), $Rights, $inheritance, 'None', 'Allow'))
    }
    Set-MatrixDacl $Item.FullName $acl
}

function Restore-MatrixBuildLease([string]$Journal) {
    Assert-MatrixPlainPath $Journal
    $lease = Get-Content -LiteralPath $Journal -Raw | ConvertFrom-Json
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    if ($lease.controllerSid -ne $identity -or $lease.version -ne 1) { throw 'Lease recovery identity/version mismatch' }
    $root = [IO.Path]::GetFullPath($lease.root)
    if ([IO.Directory]::GetParent($root).FullName -ne $lease.parent) { throw 'Invalid lease root' }
    $items = @(Get-MatrixLeaseItems $root)
    $original = @{}
    foreach ($entry in $lease.entries) {
        $full = [IO.Path]::GetFullPath($entry.path)
        if ($full -ne $root -and -not $full.StartsWith($root + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Lease journal escapes worktree' }
        $original[$full] = $entry.sddl
    }
    # Remove all temporary grants first, including grants inherited by new outputs.
    foreach ($item in $items) { Set-MatrixLeaseAcl $item $identity $lease.buildSid '' }
    # Restore children before parents so the original inheritance is reinstated last.
    foreach ($item in ($items | Sort-Object { $_.FullName.Length } -Descending)) {
        if ($original.ContainsKey($item.FullName)) {
            $acl = Get-Acl -LiteralPath $item.FullName
            $acl.SetSecurityDescriptorSddlForm($original[$item.FullName], 'Access')
            Set-MatrixDacl $item.FullName $acl
        }
    }
    Remove-Item -LiteralPath $Journal
}

function Grant-MatrixBuildLease([string]$Root, [string]$Parent, [string]$BuildSid, [string]$Journal, [string[]]$Plugins) {
    $rootPath = [IO.Path]::GetFullPath($Root).TrimEnd([IO.Path]::DirectorySeparatorChar)
    $parentPath = [IO.Path]::GetFullPath($Parent).TrimEnd([IO.Path]::DirectorySeparatorChar)
    if ([IO.Directory]::GetParent($rootPath).FullName -ne $parentPath) { throw 'Only a direct child of the approved worktree root may be leased' }
    Assert-MatrixPlainPath $rootPath
    Assert-MatrixPlainPath ([IO.Path]::GetDirectoryName($Journal))
    if (Test-Path -LiteralPath $Journal) { throw 'Outstanding build lease requires recovery before another build' }
    if ($Journal.StartsWith($parentPath + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Lease journal must be outside worktrees' }
    # All directories are inspected before New-Item can touch a build path.
    $null = @(Get-MatrixLeaseItems $rootPath)
    foreach ($plugin in $Plugins) {
        if ($plugin -notin @('eq', 'analog-compressor', 'maximiser')) { throw 'Invalid lease plugin' }
        $null = New-Item -ItemType Directory -Path (Join-Path $rootPath "$plugin/build") -Force
    }
    $items = @(Get-MatrixLeaseItems $rootPath)
    $controller = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    if ($BuildSid -eq $controller) { throw 'Build and controller SID must differ' }
    $entries = @($items | ForEach-Object { @{ path = $_.FullName; sddl = (Get-Acl -LiteralPath $_.FullName).GetSecurityDescriptorSddlForm('Access') } })
    # CreateNew and Flush(true): recovery record exists durably before any grant.
    $bytes = [Text.Encoding]::UTF8.GetBytes((@{ version = 1; root = $rootPath; parent = $parentPath; controllerSid = $controller; buildSid = $BuildSid; entries = $entries } | ConvertTo-Json -Depth 6))
    $stream = [IO.File]::Open($Journal, 'CreateNew', 'Write', 'None')
    try { $stream.Write($bytes); $stream.Flush($true) } finally { $stream.Dispose() }
    try {
        # Children first: .git/hidden files are sealed before root access is granted.
        foreach ($item in ($items | Sort-Object { $_.FullName.Length } -Descending)) {
            $relative = [IO.Path]::GetRelativePath($rootPath, $item.FullName)
            $rights = 'ReadAndExecute'
            if ($relative.Split([IO.Path]::DirectorySeparatorChar) | Where-Object { $_.StartsWith('.') -and $_ -ne '.' }) { $rights = '' }
            foreach ($plugin in $Plugins) {
                $build = Join-Path $rootPath "$plugin/build"
                if ($item.FullName -eq $build -or $item.FullName.StartsWith($build + '\', [StringComparison]::OrdinalIgnoreCase)) { $rights = 'Modify' }
            }
            Set-MatrixLeaseAcl $item $controller $BuildSid $rights
        }
    } catch { Restore-MatrixBuildLease $Journal; throw }
}

function Start-MatrixCredentialProcess($Credential, [string]$Arguments, [string]$Directory, [string]$Stdout, [string]$Stderr) {
    if (-not ('MatrixIsolatedProcess' -as [type])) {
        Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.ComponentModel;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Threading;
using Microsoft.Win32.SafeHandles;
public sealed class MatrixIsolatedProcess : IDisposable {
    [StructLayout(LayoutKind.Sequential)] struct Security { public int Length; public IntPtr Descriptor; [MarshalAs(UnmanagedType.Bool)] public bool Inherit; }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] struct Startup {
        public uint Size; public IntPtr Reserved; public string Desktop; public IntPtr Title;
        public uint X,Y,Width,Height,XChars,YChars,Fill,Flags; public ushort Show,ReservedSize;
        public IntPtr ReservedBytes,Input,Output,Error;
    }
    [StructLayout(LayoutKind.Sequential)] struct ProcessInfo { public IntPtr Process,Thread; public uint Id,ThreadId; }
    [StructLayout(LayoutKind.Sequential)] struct Basic { public long ProcessTime,JobTime; public uint Flags; public UIntPtr MinWorking,MaxWorking; public uint Active; public UIntPtr Affinity; public uint Priority,Scheduling; }
    [StructLayout(LayoutKind.Sequential)] struct Io { public ulong ReadOps,WriteOps,OtherOps,ReadBytes,WriteBytes,OtherBytes; }
    [StructLayout(LayoutKind.Sequential)] struct Extended { public Basic Basic; public Io Io; public UIntPtr ProcessMemory,JobMemory,PeakProcessMemory,PeakJobMemory; }
    [StructLayout(LayoutKind.Sequential)] struct Accounting { public long User,Kernel,PeriodUser,PeriodKernel; public uint Faults,Total,Active,Terminated; }
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool ConvertStringSecurityDescriptorToSecurityDescriptorW(string text,uint revision,out IntPtr descriptor,out uint size);
    [DllImport("advapi32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern bool CreateProcessWithLogonW(string user,string domain,string password,uint logon,string app,StringBuilder command,uint flags,IntPtr env,string cwd,ref Startup startup,out ProcessInfo info);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr CreateWindowStationW(string name,uint flags,uint access,ref Security security);
    [DllImport("user32.dll", SetLastError=true)] static extern IntPtr GetProcessWindowStation();
    [DllImport("user32.dll", SetLastError=true)] static extern bool SetProcessWindowStation(IntPtr station);
    [DllImport("user32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern IntPtr CreateDesktopW(string name,IntPtr device,IntPtr mode,uint flags,uint access,ref Security security);
    [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr desktop);
    [DllImport("user32.dll")] static extern bool CloseWindowStation(IntPtr station);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] static extern SafeFileHandle CreateFileW(string path,uint access,uint share,ref Security security,uint creation,uint flags,IntPtr template);
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr CreateJobObjectW(IntPtr attributes,IntPtr name);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetInformationJobObject(IntPtr job,int kind,ref Extended info,uint size);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool QueryInformationJobObject(IntPtr job,int kind,out Accounting info,uint size,IntPtr returned);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr job,IntPtr process);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool TerminateJobObject(IntPtr job,uint code);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool TerminateProcess(IntPtr process,uint code);
    [DllImport("kernel32.dll", SetLastError=true)] static extern uint ResumeThread(IntPtr thread);
    [DllImport("kernel32.dll", SetLastError=true)] static extern uint WaitForSingleObject(IntPtr handle,uint timeout);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetExitCodeProcess(IntPtr process,out uint code);
    [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr handle);
    [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr memory);
    IntPtr station,desktop,job,process;
    public int Id { get; private set; }
    public int ExitCode { get { uint code; if(!GetExitCodeProcess(process,out code)) Fail(); return unchecked((int)code); } }
    static void Fail() { throw new Win32Exception(Marshal.GetLastWin32Error()); }
    public MatrixIsolatedProcess(string user,string domain,string password,string exe,string arguments,string cwd,string stdout,string stderr) {
        IntPtr descriptor=IntPtr.Zero,thread=IntPtr.Zero;
        SafeFileHandle output=null,error=null,input=null;
        try {
            string owner=WindowsIdentity.GetCurrent().User.Value;
            string build=new NTAccount(domain,user).Translate(typeof(SecurityIdentifier)).Value;
            string sddl="D:P(A;;GA;;;"+owner+")(A;;GA;;;"+build+")(A;;GA;;;SY)(A;;GA;;;BA)";
            uint size; if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl,1,out descriptor,out size)) Fail();
            var security=new Security {Length=Marshal.SizeOf<Security>(),Descriptor=descriptor,Inherit=false};
            string name="SongsmithBuild-"+Guid.NewGuid().ToString("N");
            station=CreateWindowStationW(name,0,0xF037Fu,ref security); if(station==IntPtr.Zero) Fail();
            IntPtr original=GetProcessWindowStation(); if(original==IntPtr.Zero) Fail();
            if(!SetProcessWindowStation(station)) Fail();
            try { desktop=CreateDesktopW("Default",IntPtr.Zero,IntPtr.Zero,0,0xF01FFu,ref security); if(desktop==IntPtr.Zero) Fail(); }
            finally { if(!SetProcessWindowStation(original)) Fail(); }
            // Anonymous, non-inheritable job belongs to Agent. Build never receives
            // a handle and cannot retain it to defeat kill-on-last-handle-close.
            job=CreateJobObjectW(IntPtr.Zero,IntPtr.Zero); if(job==IntPtr.Zero) Fail();
            var limits=new Extended(); limits.Basic.Flags=0x2000;
            if(!SetInformationJobObject(job,9,ref limits,(uint)Marshal.SizeOf<Extended>())) Fail();
            var fileSecurity=new Security {Length=Marshal.SizeOf<Security>(),Inherit=true};
            output=CreateFileW(stdout,0x40000000u,3,ref fileSecurity,2,0x80,IntPtr.Zero);
            error=CreateFileW(stderr,0x40000000u,3,ref fileSecurity,2,0x80,IntPtr.Zero);
            input=CreateFileW("NUL",0x80000000u,3,ref fileSecurity,3,0x80,IntPtr.Zero);
            if(output.IsInvalid || error.IsInvalid || input.IsInvalid) Fail();
            var startup=new Startup {Size=(uint)Marshal.SizeOf<Startup>(),Desktop=name+"\\Default",Flags=0x101,Show=0,Input=input.DangerousGetHandle(),Output=output.DangerousGetHandle(),Error=error.DangerousGetHandle()};
            var command=new StringBuilder("\""+exe+"\" "+arguments);
            if(command.Length>=1024) throw new InvalidOperationException("Credentialed command exceeds Windows bound");
            ProcessInfo info;
            if(!CreateProcessWithLogonW(user,domain,password,1,exe,command,0x08000004u,IntPtr.Zero,cwd,ref startup,out info)) Fail();
            process=info.Process;thread=info.Thread;Id=(int)info.Id;
            // No untrusted code runs before containment: primary thread is suspended.
            if(!AssignProcessToJobObject(job,process)) Fail();
            if(ResumeThread(thread)==0xFFFFFFFFu) Fail();
        } catch { if(process!=IntPtr.Zero) TerminateProcess(process,1); Dispose(); throw; }
        finally { if(thread!=IntPtr.Zero) CloseHandle(thread); if(descriptor!=IntPtr.Zero) LocalFree(descriptor); output?.Dispose();error?.Dispose();input?.Dispose(); }
    }
    public bool Wait(int milliseconds) { uint result=WaitForSingleObject(process,(uint)milliseconds); if(result==0) return true; if(result==258) return false; Fail();return false; }
    public void Cancel() { if(job!=IntPtr.Zero && !TerminateJobObject(job,137)) Fail(); }
    public void Dispose() {
        try {
            if(job!=IntPtr.Zero) {
                if(!TerminateJobObject(job,137)) Fail();
                var deadline=DateTime.UtcNow.AddSeconds(10);
                for(;;) {
                    Accounting info;
                    if(!QueryInformationJobObject(job,1,out info,(uint)Marshal.SizeOf<Accounting>(),IntPtr.Zero)) Fail();
                    if(info.Active==0) break;
                    if(DateTime.UtcNow>=deadline) throw new InvalidOperationException("Build descendants did not terminate");
                    Thread.Sleep(20);
                }
            }
        } finally {
            if(job!=IntPtr.Zero){CloseHandle(job);job=IntPtr.Zero;}
            if(process!=IntPtr.Zero){CloseHandle(process);process=IntPtr.Zero;}
            if(desktop!=IntPtr.Zero){CloseDesktop(desktop);desktop=IntPtr.Zero;}
            if(station!=IntPtr.Zero){CloseWindowStation(station);station=IntPtr.Zero;}
        }
    }
}
'@
    }
    $parts=$Credential.UserName.Split('\')
    if($parts.Count -ne 2){throw 'Qualified build identity required'}
    [MatrixIsolatedProcess]::new($parts[1],$parts[0],$Credential.GetNetworkCredential().Password,(Join-Path $PSHOME 'pwsh.exe'),$Arguments,$Directory,$Stdout,$Stderr)
}

function Invoke-MatrixCredentialProbe($Credential, [string]$Arguments, [string]$Directory, [string]$Stdout, [string]$Stderr) {
    $process=Start-MatrixCredentialProcess $Credential $Arguments $Directory $Stdout $Stderr
    try {
        if(-not $process.Wait(30000)){throw 'Credentialed probe timed out'}
        [pscustomobject]@{Id=$process.Id;ExitCode=$process.ExitCode}
    } finally {$process.Dispose()}
}

if ($RecoverLease) { Restore-MatrixBuildLease $RecoverLease; return }
if (-not $RequestFile) { throw 'RequestFile is required' }
$requestPath = (Resolve-Path -LiteralPath $RequestFile).Path
$request = Get-Content -LiteralPath $requestPath -Raw | ConvertFrom-Json
$worktreePath = (Resolve-Path -LiteralPath $request.worktree).Path
$jucePath = (Resolve-Path -LiteralPath $request.juceDirectory).Path
foreach ($item in @($requestPath, $worktreePath, $jucePath, $PSCommandPath)) {
    if ($item -match '["\r\n]' -or (Get-Item -LiteralPath $item).LinkType) { throw 'Invalid build path' }
}
$expectedRequest = Join-Path $worktreePath 'build/matrix/request.json'
if ($requestPath -ne [IO.Path]::GetFullPath($expectedRequest)) { throw 'Request must be inside its worktree build directory' }
$names = @{ eq = 'EQTests'; 'analog-compressor' = 'AnalogCompressorTests'; maximiser = 'MaximiserTests' }
if (-not $request.plugins -or @($request.plugins | Select-Object -Unique).Count -ne @($request.plugins).Count) { throw 'Invalid plugin list' }
foreach ($plugin in $request.plugins) { if (-not $names.ContainsKey($plugin)) { throw 'Unknown plugin' } }

if (-not $Child) {
    if (-not $CredentialFile) { throw 'Separate build-account credential file is required' }
    $credential = Import-Clixml -LiteralPath $CredentialFile
    if ($credential -isnot [PSCredential]) { throw 'Invalid build account credential' }
    $currentName = [Security.Principal.WindowsIdentity]::GetCurrent().Name
    if ($credential.UserName.Split('\')[-1] -eq $currentName.Split('\')[-1]) { throw 'Build account must differ from controller account' }
    $stdoutPath = Join-Path $worktreePath 'build/matrix/build.stdout.log'
    $stderrPath = Join-Path $worktreePath 'build/matrix/build.stderr.log'
    $parentSid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    if (-not $WorktreeRoot) { throw 'Approved worktree root is required' }
    if (-not $request.isolationTargets) { throw 'Controller isolation targets are required' }
    $buildSid = ([Security.Principal.NTAccount]::new($credential.UserName)).Translate([Security.Principal.SecurityIdentifier]).Value
    $journal = Join-Path (Split-Path -Parent $CredentialFile) 'active-build-lease.json'
    $arguments = @('-NoProfile', '-NonInteractive', '-File', ('"{0}"' -f $PSCommandPath), '-Child', '-ControllerSid', $parentSid, '-RequestFile', ('"{0}"' -f $requestPath))
    # Credential is decrypted only by the trusted controller; never sent to the child.
    Grant-MatrixBuildLease $worktreePath $WorktreeRoot $buildSid $journal @($request.plugins)
    $process=$null
    try {
        $process = Start-MatrixCredentialProcess $credential ($arguments -join ' ') $worktreePath $stdoutPath $stderrPath
        while(-not $process.Wait(250)) {
            if(Test-Path -LiteralPath (Join-Path $worktreePath 'build/matrix/CANCEL_BUILD')){$process.Cancel()}
        }
        $exitCode=$process.ExitCode
    } finally { if($process){$process.Dispose()}; Restore-MatrixBuildLease $journal }
    Get-Content -LiteralPath $stdoutPath -ErrorAction SilentlyContinue
    Get-Content -LiteralPath $stderrPath -ErrorAction SilentlyContinue
    exit $exitCode
}

# All executed commands are derived here, never taken from prompt/agent output.
# Provision this account with source/build access but no controller state, .git
# common directory, Claude credentials, GitHub credentials or personal files.
if (-not $ControllerSid -or [Security.Principal.WindowsIdentity]::GetCurrent().User.Value -eq $ControllerSid) { throw 'Build must execute under a distinct Windows identity' }
$principal = [Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())
if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Build account must not run elevated' }
$targets = $request.isolationTargets
if (@($targets).Count -lt 5) { throw 'Incomplete controller isolation targets' }
foreach ($target in $targets) {
    $denied = $false
    try {
        if ($target.directory) { $null = Get-ChildItem -LiteralPath $target.path -Force -ErrorAction Stop }
        else { $stream = [IO.File]::OpenRead($target.path); $stream.Dispose() }
    } catch [UnauthorizedAccessException] { $denied = $true }
    catch [Security.SecurityException] { $denied = $true }
    if (-not $denied) { [Console]::Error.WriteLine('Build isolation check failed: protected controller path is readable'); exit 74 }
}
# The controller assigned this process to its private job before resuming it.
$buildProfile = (Get-ItemProperty -LiteralPath ('Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\' + [Security.Principal.WindowsIdentity]::GetCurrent().User.Value) -Name ProfileImagePath).ProfileImagePath
$buildProfile = [Environment]::ExpandEnvironmentVariables($buildProfile)
# Start-Process -Credential can inherit the controller's profile/temp variables.
# Derive these from the effective SID instead of granting access to that profile.
$env:USERPROFILE = $buildProfile
$env:HOME = $buildProfile
$env:APPDATA = Join-Path $buildProfile 'AppData/Roaming'
$env:LOCALAPPDATA = Join-Path $buildProfile 'AppData/Local'
$env:HOMEDRIVE = [IO.Path]::GetPathRoot($buildProfile).TrimEnd('\')
$env:HOMEPATH = $buildProfile.Substring($env:HOMEDRIVE.Length)
$env:TEMP = Join-Path $env:LOCALAPPDATA ('Temp/MatrixBuild-' + [Guid]::NewGuid().ToString('N'))
$env:TMP = $env:TEMP
$null = New-Item -ItemType Directory -Path $env:TEMP -Force
$env:PATH = [Environment]::GetEnvironmentVariable('Path', 'Machine')
$env:MSBUILDDISABLENODEREUSE = '1'
foreach ($key in @('GH_TOKEN', 'GITHUB_TOKEN', 'ANTHROPIC_API_KEY', 'ANTHROPIC_AUTH_TOKEN', 'CLAUDE_CODE_OAUTH_TOKEN')) {
    [Environment]::SetEnvironmentVariable($key, $null, 'Process')
}
$cmake = (Get-Command cmake.exe -ErrorAction Stop).Source
foreach ($plugin in $request.plugins) {
    $source = Join-Path $worktreePath $plugin
    $build = Join-Path $source 'build'
    & $cmake -S $source -B $build -G 'Visual Studio 18 2026' -A x64 "-DJUCE_DIR=$jucePath"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & $cmake --build $build --config Release
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $test = Join-Path $build ($names[$plugin] + '_artefacts/Release/' + $names[$plugin] + '.exe')
    & $test
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
exit 0
