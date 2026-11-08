#!/bin/sh
# Container entry point. Secrets are read-only files under /run/secrets and are never printed.
# The worker process itself never has the GitHub token in its environment: it reads the file only when it
# calls git or gh, and Claude (the agent) never receives it at all.
set -eu
umask 077
CONFIG=/opt/matrix/worker.config.json
SOURCE=/work/source
STATE=/work/state

case "${1:-verify}" in
  verify)
    exec /opt/matrix/verify-isolation.sh
    ;;
  login)
    # One-time, interactive: subscription sign-in. The usage gate requires a claude.ai subscription login
    # (it refuses API keys and alternate providers), so no API key is used anywhere in this container.
    exec claude auth login
    ;;
  claude-check)
    exec claude auth status --json
    ;;
  worker)
    [ -r /run/secrets/agent_github_token ] || { echo "Missing /run/secrets/agent_github_token (mount a secrets folder)." >&2; exit 2; }
    mkdir -p /work/state /work/worktrees
    # A fresh clone is the canonical checkout; the worker only ever fetches and creates worktrees from it.
    if [ ! -d "$SOURCE/.git" ]; then
      ( GH_TOKEN=$(cat /run/secrets/agent_github_token) \
        GIT_CONFIG_GLOBAL=/dev/null GIT_CONFIG_NOSYSTEM=1 \
        git -c credential.helper= -c 'credential.helper=!/usr/bin/gh auth git-credential' \
        clone --no-tags https://github.com/tonygward/audio-plugins.git "$SOURCE" )
    fi
    # This script is the only launcher, so a leftover lock can only be from a killed container.
    rm -f "$STATE/worker.lock"
    echo "Matrix Linux worker started $(date -u +%FT%TZ). Pause: touch $STATE/PAUSED"
    while :; do
      node /opt/matrix/tools/linux-worker.mjs "$CONFIG" timer || true
      sleep 300
    done
    ;;
  *)
    echo "Unknown mode: $1 (use verify, login, claude-check or worker)" >&2
    exit 64
    ;;
esac
