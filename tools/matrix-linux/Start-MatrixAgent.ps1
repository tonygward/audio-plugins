[CmdletBinding()]
param(
    [Parameter(Mandatory)][ValidateSet('Build', 'Up', 'Verify', 'Down')][string]$Action,
    # Exact Claude Code version you have vetted (required for Build).
    [string]$ClaudeCodeVersion,
    # Folder OUTSIDE the repository holding anthropic_api_key and agent_github_token (one value per file).
    [string]$SecretsDirectory
)
# Drives the Matrix agent container with wslc (WSL containers). Compose is not available in wslc, so this
# script does what compose.yaml did: an internal-only network, an allow-list proxy, and an unprivileged agent.
# It only touches objects named songsmith-matrix-* / matrix-egress / agent-internal. Nothing else is started or removed.
$ErrorActionPreference = 'Stop'
$image = 'songsmith-matrix-agent:dev'
$network = 'agent-internal'
$proxy = 'matrix-egress'
$squidImage = 'ubuntu/squid:6.6-24.04_edge'
$here = $PSScriptRoot

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

function Get-AgentArguments {
    $arguments = @('run', '--rm', '--user', '10001:10001', '--memory', '4G', '--cpus', '2', '--tmpfs', '/tmp',
        '--network', $network, '-e', "HTTPS_PROXY=http://${proxy}:3128", '-e', "HTTP_PROXY=http://${proxy}:3128", '-e', "NO_PROXY=$proxy")
    if ($SecretsDirectory) { $arguments += @('-v', "${SecretsDirectory}:/run/secrets:ro") }
    $arguments
}

switch ($Action) {
    'Build' {
        if (-not $ClaudeCodeVersion) { throw 'Pass -ClaudeCodeVersion <exact version>' }
        Invoke-Wslc build --build-arg "CLAUDE_CODE_VERSION=$ClaudeCodeVersion" -t $image $here
    }
    'Up' { Start-Egress; Write-Output 'Egress proxy is running on the internal network.' }
    'Verify' {
        Start-Egress
        $arguments = Get-AgentArguments
        & wslc @arguments -e EGRESS_CHECK=1 $image verify
        exit $LASTEXITCODE
    }
    'Down' {
        if (Test-Wslc inspect $proxy) { Invoke-Wslc remove --force $proxy }
        if (Test-Wslc network inspect $network) { Invoke-Wslc network remove $network }
    }
}
