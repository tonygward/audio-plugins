#!/bin/sh
# Container entry point. Reads credentials from /run/secrets (read-only files) and never prints them.
set -eu
umask 077

load_secret() { # name -> exported variable, only when the file exists
  file="/run/secrets/$1"
  if [ -r "$file" ]; then
    value=$(cat "$file")
    export "$2=$value"
  fi
}

case "${1:-verify}" in
  verify)
    exec /opt/matrix/verify-isolation.sh
    ;;
  worker)
    # The Linux worker (CI-based validation instead of local Windows builds) is the next AUTO-05
    # phase. Refuse to start rather than run the Windows-only worker by accident.
    echo "The Linux worker is not implemented yet; see tools/matrix-linux/README.md (phase 2)." >&2
    exit 2
    ;;
  claude-check)
    load_secret anthropic_api_key ANTHROPIC_API_KEY
    exec claude --version
    ;;
  *)
    echo "Unknown mode: $1 (use verify, claude-check or worker)" >&2
    exit 64
    ;;
esac
