# Pure contract validation and fixed command planning, shared by client and task.
function Assert-BrokerKeys($Value,[string[]]$Allowed,[string[]]$Required=$Allowed) {
    if($null -eq $Value){throw 'Missing broker object'}
    $keys=@($Value.PSObject.Properties.Name)
    if(@($keys | Where-Object {$_ -notin $Allowed}).Count -or @($Required | Where-Object {$_ -notin $keys}).Count){throw 'Unexpected or missing broker fields'}
}
function Assert-BrokerPath([string]$Path) {
    if(-not [IO.Path]::IsPathFullyQualified($Path) -or $Path -match '["\r\n\x00]' -or $Path.Contains('..')){throw 'Invalid broker path'}
    $full=[IO.Path]::GetFullPath($Path)
    # No ADS, UNC/device paths or caller-controlled path expansion.
    if($full -notmatch '^[A-Za-z]:\\' -or $full.Substring(2).Contains(':')){throw 'Broker requires a local absolute path'}
    $full.TrimEnd('\')
}
# Visual C++ leaves background helpers (telemetry, PDB server) running after a build.
# The controller treats any surviving build descendant as an unconfirmed stop and keeps
# the lease, so the build entry (running as the Build account) stops its own known
# helpers when it finishes. Only these names, only in the caller's own logon session;
# a non-admin account cannot stop another account's processes anyway.
function Stop-BrokerToolHelpers([string[]]$Names=@('vctip','mspdbsrv')) {
    $session=[Diagnostics.Process]::GetCurrentProcess().SessionId
    foreach($name in $Names){
        foreach($p in @(Get-Process -Name $name -ErrorAction SilentlyContinue)){
            if($p.SessionId -ne $session){continue}
            try{$p.Kill($true);$null=$p.WaitForExit(5000)}catch{}
            $p.Dispose()
        }
    }
}
# PowerShell 7 turns ISO-8601 strings in JSON into DateTime objects when it reads
# them back, so a time field is accepted as either, and always compared in UTC.
function ConvertTo-BrokerTime($Value) {
    if($Value -is [DateTime]){return $Value.ToUniversalTime()}
    if($Value -is [string]){return [DateTime]::Parse($Value,[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::RoundtripKind).ToUniversalTime()}
    throw 'Invalid broker time'
}
function Test-BrokerRequest($Request,$Contract,[DateTime]$Now=[DateTime]::UtcNow) {
    Assert-BrokerKeys $Request @('version','id','worktree','plugins','createdAt','expiresAt')
    if($Request.version -isnot [long] -and $Request.version -isnot [int]){throw 'Invalid broker version type'}
    if($Request.version -ne 1 -or $Request.id -isnot [string] -or $Request.id -notmatch '^[a-f0-9]{32}$' -or $Request.worktree -isnot [string] -or ($Request.createdAt -isnot [string] -and $Request.createdAt -isnot [DateTime]) -or ($Request.expiresAt -isnot [string] -and $Request.expiresAt -isnot [DateTime])){throw 'Invalid broker request identity'}
    $worktree=Assert-BrokerPath $Request.worktree
    $parent=Assert-BrokerPath $Contract.worktreeRoot
    if([IO.Directory]::GetParent($worktree).FullName -ne $parent){throw 'Worktree must be one direct child of the approved root'}
    if($Request.plugins -isnot [array] -or @($Request.plugins).Count -lt 1 -or @($Request.plugins).Count -gt 3 -or @($Request.plugins | Select-Object -Unique).Count -ne @($Request.plugins).Count){throw 'Invalid plugin list'}
    foreach($plugin in $Request.plugins){if($plugin -notin @('eq','analog-compressor','maximiser')){throw 'Unknown plugin'}}
    $created=(ConvertTo-BrokerTime $Request.createdAt)
    $expires=(ConvertTo-BrokerTime $Request.expiresAt)
    if($created -gt $Now.AddSeconds(5) -or $created -lt $Now.AddSeconds(-60) -or $expires -le $Now -or $expires -gt $created.AddSeconds($Contract.maximumSeconds)){throw 'Stale or excessive broker deadline'}
    $worktree
}
function Get-BrokerPlan($Request,$Contract) {
    $root=Assert-BrokerPath $Request.worktree
    $juce=Assert-BrokerPath $Contract.juceDirectory
    foreach($plugin in $Request.plugins){
        $source=Join-Path $root $plugin; $build=Join-Path $source 'build'
        $testName=@{eq='EQTests';'analog-compressor'='AnalogCompressorTests';maximiser='MaximiserTests'}[$plugin]
        [pscustomobject]@{name="$plugin.configure";exe=$Contract.cmakeExecutable;arguments=@('-S',('"{0}"' -f $source),'-B',('"{0}"' -f $build),'-G','"Visual Studio 18 2026"','-A','x64',('"-DJUCE_DIR={0}"' -f $juce));seconds=600}
        [pscustomobject]@{name="$plugin.build";exe=$Contract.cmakeExecutable;arguments=@('--build',('"{0}"' -f $build),'--config','Release');seconds=1800}
        [pscustomobject]@{name="$plugin.tests";exe=(Join-Path $build "$($testName)_artefacts/Release/$testName.exe");arguments=@();seconds=600}
    }
}
function Read-BrokerContract([string]$Path,[string]$ExpectedHash) {
    if((Get-FileHash -LiteralPath $Path).Hash -ne $ExpectedHash){throw 'Broker contract hash mismatch'}
    $c=Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
    if($c.version -ne 1 -or $c.agentSid -eq $c.buildSid -or $c.maximumSeconds -lt 60 -or $c.maximumSeconds -gt 10800){throw 'Invalid broker contract'}
    $c
}
function Get-BrokerTask($Contract,[switch]$AllowDisabled) {
    $service=New-Object -ComObject 'Schedule.Service';$service.Connect()
    $task=$service.GetFolder('\').GetTask($Contract.taskName)
    $d=$task.Definition
    $sid=([Security.Principal.NTAccount]::new($d.Principal.UserId)).Translate([Security.Principal.SecurityIdentifier]).Value
    if($sid -ne $Contract.buildSid -or [int]$d.Principal.LogonType -ne 1 -or [int]$d.Principal.RunLevel -ne 0 -or $d.Triggers.Count -ne 0 -or $d.Actions.Count -ne 1 -or [int]$d.Settings.MultipleInstances -ne 2 -or -not $d.Settings.AllowDemandStart){throw 'Build task principal or trigger policy changed'}
    $a=$d.Actions.Item(1)
    if($a.Path -ne $Contract.powershellExecutable -or $a.Arguments -ne $Contract.taskArguments -or $a.WorkingDirectory -ne $Contract.workingDirectory){throw 'Build task fixed action changed'}
    # Exact protected DACL: Owner/Admin/System manage, Agent can read/run/stop only.
    $actual=[Security.AccessControl.RawSecurityDescriptor]::new($task.GetSecurityDescriptor(5))
    $expected=[Security.AccessControl.RawSecurityDescriptor]::new($Contract.taskSddl)
    if($actual.Owner.Value -ne $Contract.ownerSid -or -not ($actual.ControlFlags -band [Security.AccessControl.ControlFlags]::DiscretionaryAclProtected) -or $actual.DiscretionaryAcl.Count -ne $expected.DiscretionaryAcl.Count){throw 'Build task owner or permissions changed'}
    $rules=@($actual.DiscretionaryAcl | ForEach-Object {"$($_.AceType):$($_.SecurityIdentifier.Value):$($_.AccessMask)"}) | Sort-Object
    $wanted=@($expected.DiscretionaryAcl | ForEach-Object {"$($_.AceType):$($_.SecurityIdentifier.Value):$($_.AccessMask)"}) | Sort-Object
    if(($rules -join '|') -ne ($wanted -join '|')){throw 'Build task permission mismatch'}
    if(-not $AllowDisabled -and -not $task.Enabled){throw 'Build task is disabled; activation requires separate approval'}
    $task
}

function Read-BrokerLog([string]$Path,[int]$MaximumBytes=4194304) {
    if(-not ('MatrixBrokerLog' -as [type])){
        Add-Type -TypeDefinition @'
using System;
using System.IO;
using System.Text;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public static class MatrixBrokerLog {
    [StructLayout(LayoutKind.Sequential)] struct FileInfo { public uint Attributes; public System.Runtime.InteropServices.ComTypes.FILETIME Created,Accessed,Written; public uint Volume,SizeHigh,SizeLow,Links,IndexHigh,IndexLow; }
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern SafeFileHandle CreateFileW(string path,uint access,uint share,IntPtr security,uint creation,uint flags,IntPtr template);
    [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetFileInformationByHandle(SafeFileHandle file,out FileInfo info);
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern uint GetFinalPathNameByHandleW(SafeFileHandle file,StringBuilder name,uint size,uint flags);
    public static string Read(string path,int maximum) {
        string expected=Path.GetFullPath(path);
        using(var file=CreateFileW(expected,0x80000000u,1,IntPtr.Zero,3,0x00200000u,IntPtr.Zero)) {
            if(file.IsInvalid) throw new Win32Exception(Marshal.GetLastWin32Error());
            FileInfo info; if(!GetFileInformationByHandle(file,out info)) throw new Win32Exception(Marshal.GetLastWin32Error());
            if((info.Attributes & 0x410u)!=0 || info.Links!=1 || info.SizeHigh!=0 || info.SizeLow>maximum) throw new IOException("Unsafe or oversized diagnostic file");
            var final=new StringBuilder(32768); uint length=GetFinalPathNameByHandleW(file,final,(uint)final.Capacity,0);
            if(length==0 || length>=final.Capacity || !String.Equals(final.ToString(),"\\\\?\\"+expected,StringComparison.OrdinalIgnoreCase)) throw new IOException("Diagnostic path was redirected");
            using(var stream=new FileStream(file,FileAccess.Read)) {
                var bytes=new byte[(int)info.SizeLow];int offset=0;
                while(offset<bytes.Length){int read=stream.Read(bytes,offset,bytes.Length-offset);if(read==0)break;offset+=read;}
                return Encoding.UTF8.GetString(bytes,0,offset);
            }
        }
    }
}
'@
    }
    [MatrixBrokerLog]::Read($Path,$MaximumBytes)
}
