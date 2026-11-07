[CmdletBinding()]
param()
$ErrorActionPreference='Stop'
function Assert-FixtureClean([string]$Directory,[hashtable]$Hashes,[string]$Journal){
    foreach($name in $Hashes.Keys){if((Get-FileHash -LiteralPath (Join-Path $Directory $name)).Hash -ne $Hashes[$name]){throw "Fixture source integrity failed: $name"}}
    if(Test-Path -LiteralPath $Journal){throw 'Fixture lease was not cleaned up'}
}
function Assert-FixtureReadOnly([string[]]$Paths){
    foreach($path in $Paths){
        $denied=$false
        try{$handle=[IO.File]::Open($path,'Open','Write','Read');$handle.Dispose()}catch [UnauthorizedAccessException]{$denied=$true}
        if(-not $denied){throw 'Agent can write a trusted entry point'}
    }
}
$package='__BROKER__'
$agentSid='__AGENT__'
$expected=@{'__HASHES__'=''}
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
if($identity.User.Value -ne $agentSid -or ([Security.Principal.WindowsPrincipal]::new($identity)).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)){throw 'Fixture requires actual non-admin Agent token'}
foreach($name in $expected.Keys){if((Get-FileHash -LiteralPath (Join-Path $package $name)).Hash -ne $expected[$name]){throw "Fixture dependency changed: $name"}}
$contract=Get-Content (Join-Path $package 'broker.json') -Raw | ConvertFrom-Json
$reportFile=Join-Path (Split-Path -Parent $contract.journalFile) 'supervised-fixture.json'
$report=[ordered]@{identity=$identity.Name;sid=$identity.User.Value;started=[DateTime]::UtcNow.ToString('o');passed=$false;modelTurns=0}
try{
    if(Test-Path -LiteralPath $contract.journalFile){throw 'Outstanding lease blocks fixture'}
    # Open existing trusted scripts for write without writing/truncating anything.
    Assert-FixtureReadOnly @($PSCommandPath,(Join-Path $package 'Test-ManualBuild.ps1'),(Join-Path $package 'Build-Broker.ps1'))
    & (Join-Path $package 'Invoke-MatrixScheduledBuild.ps1') -Inspect
    & (Join-Path $package 'Test-ManualBuild.ps1')
    if($LASTEXITCODE -ne 0){throw 'Fixture failed'}
    Assert-FixtureClean $package $expected $contract.journalFile
    $report.passed=$true;$report.trustedSourceUnchanged=$true;$report.leaseClosed=$true
}catch{$report.error=$_.Exception.Message}
finally{$report.finished=[DateTime]::UtcNow.ToString('o');$report.activeLease=Test-Path -LiteralPath $contract.journalFile;$report | ConvertTo-Json | Set-Content -LiteralPath $reportFile}
if(-not $report.passed){throw $report.error}

