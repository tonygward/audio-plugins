# Songsmith Matrix developer agent - isolated Linux container (AUTO-05).
# Runs under wslc (WSL containers); plain OCI, so it also builds with Docker. It holds Claude Code and git/gh only: no compilers for the
# native plugins (GitHub-hosted Windows and macOS runners build those), no Docker CLI, no sudo.
# Build:  ./Start-MatrixAgent.ps1 -Action Build -ClaudeCodeVersion <exact version>
ARG NODE_IMAGE=node:22-bookworm-slim
FROM ${NODE_IMAGE}

ARG CLAUDE_CODE_VERSION
RUN test -n "$CLAUDE_CODE_VERSION" || (echo "Pass --build-arg CLAUDE_CODE_VERSION=<exact version>" >&2; exit 1)

RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl git gnupg jq \
 && install -d -m 0755 /etc/apt/keyrings \
 && curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg -o /etc/apt/keyrings/githubcli.gpg \
 && echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/githubcli.gpg] https://cli.github.com/packages stable main" > /etc/apt/sources.list.d/github-cli.list \
 && apt-get update && apt-get install -y --no-install-recommends gh \
 && npm install -g "@anthropic-ai/claude-code@${CLAUDE_CODE_VERSION}" \
 && apt-get purge -y curl gnupg && apt-get autoremove -y && rm -rf /var/lib/apt/lists/* /root/.npm \
 # No set-id binaries: nothing in this image can gain privilege.
 && find / -xdev -perm /6000 -type f -exec chmod a-s {} + 2>/dev/null || true

# Trusted worker code is baked in (read-only at run time) instead of mounted from the host.
COPY --chown=root:root entrypoint.sh verify-isolation.sh /opt/matrix/
RUN chmod 0555 /opt/matrix/entrypoint.sh /opt/matrix/verify-isolation.sh

# Unprivileged, fixed identity with no login shell and no password.
RUN groupadd --gid 10001 agent && useradd --uid 10001 --gid 10001 --no-create-home --home-dir /home/agent --shell /usr/sbin/nologin agent
USER 10001:10001
WORKDIR /work
ENV HOME=/home/agent \
    NODE_ENV=production \
    CLAUDE_CONFIG_DIR=/home/agent/.claude \
    GIT_TERMINAL_PROMPT=0 \
    DISABLE_AUTOUPDATER=1
ENTRYPOINT ["/opt/matrix/entrypoint.sh"]
CMD ["verify"]
