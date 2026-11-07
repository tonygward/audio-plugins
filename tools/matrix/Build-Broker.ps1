[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
$common=Join-Path $PSScriptRoot 'Build-BrokerCommon.ps1'
if((Get-FileHash -LiteralPath $common).Hash -ne '__COMMON_HASH__'){throw 'Broker common-code hash mismatch'}
. $common
$contractPath=Join-Path $PSScriptRoot 'broker.json'
# Config is owner-write only. Client additionally pins its exact bytes.
$contract=Get-Content -LiteralPath $contractPath -Raw | ConvertFrom-Json
if((Get-FileHash -LiteralPath $PSCommandPath).Hash -ne $contract.entryHash){throw 'Fixed broker entry hash mismatch'}
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$principal=[Security.Principal.WindowsPrincipal]::new($identity)
if($identity.User.Value -ne $contract.buildSid -or $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Unexpected or elevated build token'}
$request=Get-Content -LiteralPath (Join-Path $contract.exchangeDirectory 'request.json') -Raw | ConvertFrom-Json
$root=Test-BrokerRequest $request $contract
$lease=Get-Content -LiteralPath (Join-Path $root 'build/matrix/BROKER_LEASE.json') -Raw | ConvertFrom-Json
if($lease.id -ne $request.id -or $lease.requestHash -ne (Get-FileHash -LiteralPath (Join-Path $contract.exchangeDirectory 'request.json')).Hash){throw 'Build request has no matching read-only lease marker'}
foreach($target in $contract.protectedPaths){
    $denied=$false
    try{if($target.directory){$null=Get-ChildItem -LiteralPath $target.path -Force -ErrorAction Stop}else{$s=[IO.File]::OpenRead($target.path);$s.Dispose()}}
    catch [UnauthorizedAccessException]{$denied=$true}
    if(-not $denied){[Console]::Error.WriteLine('Build isolation check failed');exit 74}
}
foreach($key in @((Get-ChildItem Env:).Name | Where-Object {$_ -match '^(GH_|GITHUB_|ANTHROPIC_|CLAUDE_)'})){[Environment]::SetEnvironmentVariable($key,$null,'Process')}
$profile=(Get-ItemProperty -LiteralPath ('Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion\ProfileList\'+$identity.User.Value)).ProfileImagePath
$env:USERPROFILE=$profile;$env:HOME=$profile;$env:APPDATA=Join-Path $profile 'AppData/Roaming';$env:LOCALAPPDATA=Join-Path $profile 'AppData/Local'
$env:TEMP=Join-Path $env:LOCALAPPDATA ('Temp/MatrixBuild-'+$request.id);$env:TMP=$env:TEMP
$env:PATH=[Environment]::GetEnvironmentVariable('Path','Machine');$env:MSBUILDDISABLENODEREUSE='1'
$null=New-Item -ItemType Directory -Path $env:TEMP -Force
$logs=Join-Path $root ($request.plugins[0]+'/build/matrix-broker/'+$request.id)
$null=New-Item -ItemType Directory -Path $logs -Force
$exitCode=1;$steps=@()
try{
    foreach($step in Get-BrokerPlan $request $contract){
        $out=Join-Path $logs ($step.name+'.stdout.log');$err=Join-Path $logs ($step.name+'.stderr.log')
        $start=@{FilePath=$step.exe;WorkingDirectory=$root;WindowStyle='Hidden';PassThru=$true;RedirectStandardOutput=$out;RedirectStandardError=$err;ErrorAction='Stop'}
        if($step.arguments.Count){$start.ArgumentList=$step.arguments}
        $p=Start-Process @start
        $deadline=[DateTime]::UtcNow.AddSeconds($step.seconds)
        try{
            while(-not $p.WaitForExit(250)){
                $heartbeat=Get-Content -LiteralPath (Join-Path $contract.exchangeDirectory 'heartbeat.json') -Raw | ConvertFrom-Json
                if($heartbeat.id -ne $request.id -or [DateTime]::Parse($heartbeat.time).ToUniversalTime() -lt [DateTime]::UtcNow.AddSeconds(-15) -or [DateTime]::UtcNow -gt $deadline -or [DateTime]::UtcNow -gt [DateTime]::Parse($request.expiresAt).ToUniversalTime()){throw 'Build controller heartbeat expired or deadline reached'}
            }
            $steps+=@{name=$step.name;exitCode=$p.ExitCode}
            if($p.ExitCode -ne 0){$exitCode=$p.ExitCode;throw 'Fixed build step failed'}
        }finally{if(-not $p.HasExited){$p.Kill($true);$null=$p.WaitForExit(10000)};$p.Dispose()}
    }
    $exitCode=0
}catch{[Console]::Error.WriteLine($_.Exception.Message)}
finally{@{id=$request.id;exitCode=$exitCode;steps=$steps;buildSid=$identity.User.Value} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $logs 'result.json')}
exit $exitCode
