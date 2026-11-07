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
    Set-Acl -LiteralPath $Item.FullName -AclObject $acl
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
            Set-Acl -LiteralPath $item.FullName -AclObject $acl
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
    try {
        $process = Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -ArgumentList $arguments -Credential $credential -LoadUserProfile -WorkingDirectory $worktreePath -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    } finally { Restore-MatrixBuildLease $journal }
    Get-Content -LiteralPath $stdoutPath -ErrorAction SilentlyContinue
    Get-Content -LiteralPath $stderrPath -ErrorAction SilentlyContinue
    exit $process.ExitCode
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
# The build child joins a non-breakaway job before executing CMake/tests. Its
# last job handle closes on exit (including termination), killing descendants
# before the waiting controller restores ACLs. Keep the handle alive statically.
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
public static class MatrixBuildJob {
    [StructLayout(LayoutKind.Sequential)] struct Basic {
        public long ProcessTime, JobTime; public uint Flags;
        public UIntPtr MinWorking, MaxWorking; public uint Active;
        public UIntPtr Affinity; public uint Priority, Scheduling;
    }
    [StructLayout(LayoutKind.Sequential)] struct Io {
        public ulong ReadOps, WriteOps, OtherOps, ReadBytes, WriteBytes, OtherBytes;
    }
    [StructLayout(LayoutKind.Sequential)] struct Extended {
        public Basic Basic; public Io Io;
        public UIntPtr ProcessMemory, JobMemory, PeakProcessMemory, PeakJobMemory;
    }
    [DllImport("kernel32.dll", SetLastError=true)] static extern IntPtr CreateJobObject(IntPtr a, string n);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool SetInformationJobObject(IntPtr j, int c, ref Extended i, uint s);
    [DllImport("kernel32.dll", SetLastError=true)] static extern bool AssignProcessToJobObject(IntPtr j, IntPtr p);
    [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
    static IntPtr handle;
    public static void Enter() {
        handle = CreateJobObject(IntPtr.Zero, null);
        var info = new Extended(); info.Basic.Flags = 0x2000;
        if (handle == IntPtr.Zero || !SetInformationJobObject(handle, 9, ref info, (uint)Marshal.SizeOf<Extended>()) || !AssignProcessToJobObject(handle, GetCurrentProcess()))
            throw new Win32Exception(Marshal.GetLastWin32Error());
    }
}
'@
[MatrixBuildJob]::Enter()
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
