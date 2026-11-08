[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Build', 'Verify', 'Login', 'Run', 'Logs', 'Pause', 'Resume', 'Stop', 'Down')][string]$Action,
    # Exact Claude Code version you have vetted (required for Build).
    [string]$ClaudeCodeVersion,
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
        if (-not $ClaudeCodeVersion) { throw 'Pass -ClaudeCodeVersion <exact version>' }
        Invoke-Wslc build --build-arg "CLAUDE_CODE_VERSION=$ClaudeCodeVersion" -f (Join-Path $here 'Dockerfile') -t $image $root
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
    'Logs' { & wslc logs $agent }
    'Pause' { Invoke-Wslc exec $agent touch /work/state/PAUSED; Write-Output 'Paused: no new work starts; work in progress finishes.' }
    'Resume' { Invoke-Wslc exec $agent rm -f /work/state/PAUSED; Write-Output 'Resumed.' }
    'Stop' { if (Test-Wslc inspect $agent) { Invoke-Wslc remove --force $agent } }
    'Down' {
        foreach ($name in $agent, $proxy) { if (Test-Wslc inspect $name) { Invoke-Wslc remove --force $name } }
        if (Test-Wslc network inspect $network) { Invoke-Wslc network remove $network }
    }
}
