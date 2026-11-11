[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Build', 'Update', 'Rollback', 'Verify', 'Login', 'Run', 'Ensure', 'Logs', 'Pause', 'Resume', 'Stop', 'Down')][string]$Action,
    # Exact Claude Code version to build. Optional: Build defaults to the version recorded by the last build or automatic update.
    [string]$ClaudeCodeVersion,
    # Update only: a Claude Code release must be at least this many days old before the agent takes it, and a new major version needs -AllowMajor.
    [int]$MinimumAgeDays = 3,
    [switch]$AllowMajor,
    # Folder OUTSIDE the repository holding agent_github_token (a fine-grained token, one value in one file).
    [string]$SecretsDirectory
)
# Drives the Matrix agent container with wslc (WSL containers). Compose is not available in wslc, so this
# script does what compose.yaml did: an internal-only network, an allow-list proxy and an unprivileged agent.
# It only touches objects named matrix-* / agent-internal / songsmith-matrix-agent. Nothing else is started or removed.
#   Build   build the image            Login   one-time Claude subscription sign-in (interactive)
#   Verify  isolation self-test        Run     start the worker (detached; this is the owner's go-ahead)
#   Logs    show worker output         Pause / Resume   stop or allow new work without stopping the container
#   Stop    stop the worker            Down    remove the worker, proxy and network (volumes are kept)
$ErrorActionPreference = 'Stop'
$image = 'songsmith-matrix-agent:dev'
$network = 'agent-internal'
$proxy = 'matrix-egress'
$agent = 'matrix-agent'
$squidImage = 'ubuntu/squid:6.6-24.04_edge'
$here = $PSScriptRoot
$root = (Resolve-Path (Join-Path $here '..\..')).Path

function Invoke-Wslc { & wslc @args; if ($LASTEXITCODE -ne 0) { throw "wslc $($args -join ' ') failed ($LASTEXITCODE)" } }
function Test-Wslc { & wslc @args *> $null; $LASTEXITCODE -eq 0 }

# Claude Code version bookkeeping (automatic update). Kept on the host, outside the repository.
#   claude-version.json: current (what the live image carries), previous (what -Action Rollback returns to), rejected (versions that failed a trial)
$imageName = 'songsmith-matrix-agent'
$dataDir = Join-Path $env:LOCALAPPDATA 'MatrixAgent'
$versionFile = Join-Path $dataDir 'claude-version.json'
function Read-VersionState {
    if (Test-Path $versionFile) { $s = Get-Content $versionFile -Raw | ConvertFrom-Json; return @{ current = $s.current; previous = $s.previous; rejected = @($s.rejected) } }
    @{ current = $null; previous = $null; rejected = @() }
}
function Save-VersionState($state) { New-Item -ItemType Directory -Force $dataDir | Out-Null; $state | ConvertTo-Json -Depth 4 | Set-Content $versionFile -Encoding UTF8 }
function Write-UpdateLog($message) { New-Item -ItemType Directory -Force $dataDir | Out-Null; Add-Content (Join-Path $dataDir 'update.log') "$(Get-Date -Format o) $message"; Write-Output $message }
function Test-AgentRunning { @(& wslc list 2>$null | Select-String -SimpleMatch $agent).Count -gt 0 }
function Get-RunningClaudeVersion {
    if (-not (Test-AgentRunning)) { return $null }
    $out = & wslc exec $agent claude --version 2>$null
    if ($LASTEXITCODE -eq 0 -and "$out" -match '^(\d+\.\d+\.\d+)') { $Matches[1] }
}
# Busy means a job is claimed. An update never interrupts work; it waits for the next run.
function Test-AgentBusy {
    if (-not (Test-AgentRunning)) { return $false }
    $count = & wslc exec $agent node -e "const s=require('/work/state/state.json');process.stdout.write(String(Object.values(s.tasks||{}).filter(t=>t.state==='claimed').length))" 2>$null
    if ($LASTEXITCODE -ne 0) { return $true } # cannot tell: assume busy
    [int]$count -gt 0
}
function Build-AgentImage([string]$tag, [string]$version) {
    Invoke-Wslc build --build-arg "CLAUDE_CODE_VERSION=$version" -f (Join-Path $here 'Dockerfile') -t "${imageName}:$tag" $root
}

function Start-Egress {
    if (-not (Test-Wslc network inspect $network)) { Invoke-Wslc network create --internal $network | Out-Null }
    if (-not (Test-Wslc inspect $proxy)) {
        Invoke-Wslc run -d --name $proxy --network $network -v "$here\squid\squid.conf:/etc/squid/squid.conf:ro" $squidImage | Out-Null
        # The proxy is the only container with a route out; the agent network itself stays internal.
        Invoke-Wslc network connect bridge $proxy
    } else { & wslc start $proxy *> $null }
}

# Hardening available in wslc: unprivileged user, memory/CPU limits, internal network, read-only secrets.
# (It has no read-only-root, capability-drop, no-new-privileges or PID-limit flags; see README.)
function Get-AgentArguments([switch]$Secrets) {
    $arguments = @('--user', '10001:10001', '--memory', '4G', '--cpus', '2', '--tmpfs', '/tmp',
        '-v', 'matrix-work:/work', '-v', 'matrix-home:/home/agent',
        '--network', $network, '-e', "HTTPS_PROXY=http://${proxy}:3128", '-e', "HTTP_PROXY=http://${proxy}:3128", '-e', "NO_PROXY=$proxy")
    if ($Secrets) {
        if (-not $SecretsDirectory -or -not (Test-Path (Join-Path $SecretsDirectory 'agent_github_token'))) { throw 'Pass -SecretsDirectory <folder containing agent_github_token>' }
        $arguments += @('-v', "${SecretsDirectory}:/run/secrets:ro")
    }
    $arguments
}

switch ($Action) {
    'Build' {
        $state = Read-VersionState
        $version = if ($ClaudeCodeVersion) { $ClaudeCodeVersion } elseif ($state.current) { $state.current } else { Get-RunningClaudeVersion }
        if (-not $version) { throw 'Pass -ClaudeCodeVersion <exact version> (no version is recorded yet)' }
        Build-AgentImage 'dev' $version
        $state.current = $version; Save-VersionState $state
        Write-Output "Built $image with Claude Code $version."
    }
    'Update' {
        # Automatic Claude Code update. The image is immutable and the container cannot rebuild itself, so this runs on the host (scheduled daily).
        # It chooses a target (claude-version.mjs: stable, not deprecated, old enough, same major), builds a CANDIDATE image beside the live one,
        # proves it (isolation self-test, version, Claude sign-in still valid), and only then swaps it in, while no job is running. The previous
        # image is kept for -Action Rollback, and an automatic rollback happens if the new worker does not come up healthy.
        if (-not $SecretsDirectory) { throw 'Pass -SecretsDirectory <folder containing agent_github_token> (the worker is restarted after the swap)' }
        $state = Read-VersionState
        if (-not $state.current) { $state.current = Get-RunningClaudeVersion }
        if (-not $state.current) { throw 'The current Claude Code version is unknown: run -Action Build -ClaudeCodeVersion <version> once' }
        $nodeArguments = @((Join-Path $here 'claude-version.mjs'), 'decide', '--current', $state.current, '--min-age-days', $MinimumAgeDays)
        if ($state.rejected.Count) { $nodeArguments += @('--rejected', ($state.rejected -join ',')) }
        if ($AllowMajor) { $nodeArguments += '--allow-major' }
        $decision = (& node @nodeArguments | Out-String) | ConvertFrom-Json
        if ($decision.action -eq 'error') { Write-UpdateLog "Update check failed: $($decision.reason)"; exit 1 }
        if ($decision.action -ne 'update') { Write-UpdateLog "No update: $($decision.reason)"; break }
        $target = $decision.target
        if (Test-AgentBusy) { Write-UpdateLog "Claude Code $target is available but a job is running; trying again at the next run."; break }
        Write-UpdateLog "Trying Claude Code $($state.current) -> $target ($($decision.reason))"
        $candidate = "${imageName}:candidate"
        $failure = $null
        try {
            Build-AgentImage 'candidate' $target
            Start-Egress
            $trial = Get-AgentArguments
            & wslc run --rm @trial -e EGRESS_CHECK=1 $candidate verify *> $null
            if ($LASTEXITCODE -ne 0) { throw 'the isolation self-test failed' }
            $reported = (& wslc run --rm @trial $candidate claude-version | Out-String).Trim()
            if ($LASTEXITCODE -ne 0 -or $reported -notmatch "^$([regex]::Escape($target))\b") { throw "the candidate reports version '$reported', not $target" }
            $auth = (& wslc run --rm @trial $candidate claude-check | Out-String) | ConvertFrom-Json
            if (-not $auth.loggedIn -or $auth.authMethod -ne 'claude.ai') { throw 'the Claude subscription sign-in does not work with the candidate' }
        } catch { $failure = "$_" }
        if ($failure) {
            $state.rejected = @($state.rejected) + $target; Save-VersionState $state
            Write-UpdateLog "Claude Code $target REJECTED, live image unchanged: $failure"
            & wslc image remove $candidate *> $null
            exit 1
        }
        if (Test-AgentBusy) { Write-UpdateLog "A job started while $target was being proved; the candidate is kept and the swap waits for the next run."; break }
        # Swap: keep the live image as 'previous', promote the candidate, restart the worker, and watch it come up.
        & wslc image tag $image "${imageName}:previous"
        Invoke-Wslc image tag $candidate $image
        if (Test-Wslc inspect $agent) { Invoke-Wslc remove --force $agent }
        Start-Egress
        Invoke-Wslc run -d --name $agent @(Get-AgentArguments -Secrets) $image worker | Out-Null
        $healthy = $false
        for ($i = 0; $i -lt 18 -and -not $healthy; $i++) {
            Start-Sleep -Seconds 5
            $healthy = (Test-AgentRunning) -and ((& wslc logs $agent 2>$null | Out-String) -match 'Matrix Linux worker started')
        }
        if ($healthy) { Start-Sleep -Seconds 60; $healthy = (Test-AgentRunning) -and ((Get-RunningClaudeVersion) -eq $target) }
        if (-not $healthy) {
            Write-UpdateLog "The worker did not come up healthy on Claude Code $target; rolling back to $($state.current)."
            Invoke-Wslc image tag "${imageName}:previous" $image
            if (Test-Wslc inspect $agent) { Invoke-Wslc remove --force $agent }
            Invoke-Wslc run -d --name $agent @(Get-AgentArguments -Secrets) $image worker | Out-Null
            $state.rejected = @($state.rejected) + $target; Save-VersionState $state
            Write-UpdateLog "Rolled back: Claude Code $($state.current) is live again."
            exit 1
        }
        $state.previous = $state.current; $state.current = $target; Save-VersionState $state
        & wslc image remove $candidate *> $null
        Write-UpdateLog "UPDATED: Claude Code $($state.previous) -> $target. The previous image is kept for -Action Rollback."
    }
    'Rollback' {
        # Return to the image kept before the last automatic update, and stop taking the version that was rolled back from.
        if (-not $SecretsDirectory) { throw 'Pass -SecretsDirectory <folder containing agent_github_token>' }
        $state = Read-VersionState
        if (-not $state.previous -or -not (Test-Wslc image inspect "${imageName}:previous")) { throw 'There is no previous image to roll back to.' }
        if (Test-AgentBusy) { throw 'A job is running; roll back when it has finished (or use -Action Pause first).' }
        Invoke-Wslc image tag "${imageName}:previous" $image
        if (Test-Wslc inspect $agent) { Invoke-Wslc remove --force $agent }
        Start-Egress
        Invoke-Wslc run -d --name $agent @(Get-AgentArguments -Secrets) $image worker | Out-Null
        $state.rejected = @($state.rejected) + $state.current; $state.current = $state.previous; $state.previous = $null; Save-VersionState $state
        Write-UpdateLog "Rolled back to Claude Code $($state.current); the version rolled back from will not be tried again."
    }
    'Verify' {
        Start-Egress
        $arguments = Get-AgentArguments
        & wslc run --rm @arguments -e EGRESS_CHECK=1 $image verify
        exit $LASTEXITCODE
    }
    'Login' {
        Start-Egress
        $arguments = Get-AgentArguments
        & wslc run --rm -it @arguments $image login
        & wslc run --rm @arguments $image claude-check
    }
    'Run' {
        Start-Egress
        if (Test-Wslc inspect $agent) { throw "$agent already exists; use -Action Stop or Down first" }
        $arguments = Get-AgentArguments -Secrets
        Invoke-Wslc run -d --name $agent @arguments $image worker | Out-Null
        Write-Output "Worker started. Follow it with: ./Start-MatrixAgent.ps1 -Action Logs"
    }
    'Ensure' {
        # Safe to run at every logon: start the proxy, and start the worker only if it is not already running.
        # A stopped worker (for example after a reboot) is recreated; its state lives in the matrix-work and matrix-home volumes.
        Start-Egress
        if (@(& wslc list 2>$null | Select-String -SimpleMatch $agent).Count -gt 0) { Write-Output "$agent is already running."; break }
        if (Test-Wslc inspect $agent) { Invoke-Wslc remove --force $agent }
        $arguments = Get-AgentArguments -Secrets
        Invoke-Wslc run -d --name $agent @arguments $image worker | Out-Null
        Write-Output "$agent started."
    }
    'Logs' { & wslc logs $agent }
    'Pause' { Invoke-Wslc exec $agent touch /work/state/PAUSED; Write-Output 'Paused: no new work starts; work in progress finishes.' }
    'Resume' { Invoke-Wslc exec $agent rm -f /work/state/PAUSED; Write-Output 'Resumed.' }
    'Stop' { if (Test-Wslc inspect $agent) { Invoke-Wslc remove --force $agent } }
    'Down' {
        foreach ($name in $agent, $proxy) { if (Test-Wslc inspect $name) { Invoke-Wslc remove --force $name } }
        if (Test-Wslc network inspect $network) { Invoke-Wslc network remove $network }
    }
}
