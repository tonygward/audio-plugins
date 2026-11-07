# Shared DACL-only worktree leasing. Contains no account logon or process-launch code.

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
