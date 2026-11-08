$ErrorActionPreference='Stop'
# Exercises Remember-Processes from the controller with real processes. The function is read from
# the shipped script, so this tests the code that runs, not a copy.
$script=Join-Path $PSScriptRoot '../Invoke-MatrixScheduledBuild.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile((Resolve-Path $script),[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Controller does not parse'}
$function=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -eq 'Remember-Processes'},$true)
if(-not $function){throw 'Remember-Processes not found'}
. ([scriptblock]::Create($function.Extent.Text))
$script:checks=0
function Check([bool]$Value,[string]$Label){if(-not $Value){throw $Label};$script:checks++}
$root=$null;$other=$null
try{
    # A real tree: cmd (root) -> ping (child).
    $root=Start-Process -FilePath cmd.exe -ArgumentList '/c','ping -n 30 127.0.0.1 >nul' -WindowStyle Hidden -PassThru
    Start-Sleep -Milliseconds 800
    $rootInfo=Get-CimInstance Win32_Process -Filter "ProcessId=$($root.Id)"
    $known=[Collections.Generic.Dictionary[int,long]]::new()
    $known[[int]$root.Id]=$rootInfo.CreationDate.Ticks
    $found=@(Remember-Processes)
    Check ($found.Name -contains 'cmd.exe' -and $found.Name -contains 'PING.EXE') 'Real descendant was not recorded'

    # PID reuse: the recorded ID now belongs to a different live process (different start time).
    # Its children are not build descendants.
    $other=Start-Process -FilePath cmd.exe -ArgumentList '/c','ping -n 30 127.0.0.1 >nul' -WindowStyle Hidden -PassThru
    Start-Sleep -Milliseconds 800
    $known=[Collections.Generic.Dictionary[int,long]]::new()
    $known[[int]$other.Id]=1   # recorded start time does not match the live process
    $found=@(Remember-Processes)
    Check ($found.Count -eq 0) 'A recycled process ID was treated as a build process'

    Write-Output "PASS: $script:checks process-tracking checks"
}finally{
    foreach($p in @($root,$other)){ if($p -and -not $p.HasExited){ & taskkill.exe /PID $p.Id /T /F | Out-Null } }
}
