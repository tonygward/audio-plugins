[CmdletBinding(SupportsShouldProcess)]
param([string]$Account = 'MATRIX\SongsmithAgent', [switch]$Remove)
$ErrorActionPreference = 'Stop'
$sid = ([Security.Principal.NTAccount]::new($Account)).Translate([Security.Principal.SecurityIdentifier])
if (-not $PSCmdlet.ShouldProcess($Account, "$(if ($Remove) {'Remove'} else {'Grant'}) only SeBatchLogonRight; do not change groups or other rights")) { return }
Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Security.Principal;
public static class MatrixBatchRight {
    [StructLayout(LayoutKind.Sequential)] struct Attributes { public int Length; public IntPtr Root,Name; public uint Flags; public IntPtr Descriptor,Qos; }
    [StructLayout(LayoutKind.Sequential)] struct Unicode { public ushort Length,Maximum; public IntPtr Buffer; }
    [DllImport("advapi32.dll")] static extern uint LsaOpenPolicy(IntPtr system, ref Attributes a, uint access, out IntPtr handle);
    [DllImport("advapi32.dll")] static extern uint LsaAddAccountRights(IntPtr handle, IntPtr sid, Unicode[] rights, uint count);
    [DllImport("advapi32.dll")] static extern uint LsaRemoveAccountRights(IntPtr handle, IntPtr sid, [MarshalAs(UnmanagedType.U1)] bool all, Unicode[] rights, uint count);
    [DllImport("advapi32.dll")] static extern uint LsaNtStatusToWinError(uint status);
    [DllImport("advapi32.dll")] static extern uint LsaClose(IntPtr handle);
    public static void Set(string sidText, bool remove) {
        var sid = new SecurityIdentifier(sidText); var bytes = new byte[sid.BinaryLength]; sid.GetBinaryForm(bytes,0);
        IntPtr sidPtr = Marshal.AllocHGlobal(bytes.Length), text = Marshal.StringToHGlobalUni("SeBatchLogonRight"), policy=IntPtr.Zero;
        try {
            Marshal.Copy(bytes,0,sidPtr,bytes.Length);
            var attrs = new Attributes { Length=Marshal.SizeOf<Attributes>() };
            uint status=LsaOpenPolicy(IntPtr.Zero,ref attrs,0x810,out policy);
            if(status!=0) throw new Win32Exception((int)LsaNtStatusToWinError(status));
            var rights = new[]{new Unicode {Length=(ushort)("SeBatchLogonRight".Length*2),Maximum=(ushort)(("SeBatchLogonRight".Length+1)*2),Buffer=text}};
            status=remove ? LsaRemoveAccountRights(policy,sidPtr,false,rights,1) : LsaAddAccountRights(policy,sidPtr,rights,1);
            if(status!=0) throw new Win32Exception((int)LsaNtStatusToWinError(status));
        } finally { if(policy!=IntPtr.Zero) LsaClose(policy); Marshal.FreeHGlobal(text); Marshal.FreeHGlobal(sidPtr); }
    }
}
'@
[MatrixBatchRight]::Set($sid.Value,[bool]$Remove)
Write-Output "$(if ($Remove) {'Removed'} else {'Granted'}) SeBatchLogonRight for $Account only"
