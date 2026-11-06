[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$RequestFile,
    [string]$CredentialFile,
    [string]$ControllerSid,
    [switch]$Child
)
$ErrorActionPreference = 'Stop'
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
    $arguments = @('-NoProfile', '-NonInteractive', '-File', ('"{0}"' -f $PSCommandPath), '-Child', '-ControllerSid', $parentSid, '-RequestFile', ('"{0}"' -f $requestPath))
    # Credential is decrypted only by the trusted controller; never sent to the child.
    $process = Start-Process -FilePath (Join-Path $PSHOME 'pwsh.exe') -ArgumentList $arguments -Credential $credential -LoadUserProfile -WorkingDirectory $worktreePath -WindowStyle Hidden -Wait -PassThru -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
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
