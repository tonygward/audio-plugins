#!/bin/sh
# Negative and positive isolation tests, run inside the container. Exit status is non-zero if any
# check fails. These do not prove the host is safe; they prove this container is configured as designed.
fail=0
pass() { echo "PASS  $1"; }
bad()  { echo "FAIL  $1"; fail=1; }

[ "$(id -u)" != "0" ] && pass "runs as a non-root user ($(id -u))" || bad "running as root"
[ ! -S /var/run/docker.sock ] && [ ! -e /run/docker.sock ] && pass "no Docker socket" || bad "Docker socket is present"
command -v docker >/dev/null 2>&1 && bad "docker CLI present" || pass "no Docker CLI"
command -v sudo >/dev/null 2>&1 && bad "sudo present" || pass "no sudo"

# The agent user must not be able to change the image (wslc has no --read-only flag; file permissions enforce it).
if touch /probe 2>/dev/null; then rm -f /probe; bad "root file system is writable by the agent"; else pass "cannot write to the root file system"; fi
if touch /work/.probe 2>/dev/null; then rm -f /work/.probe; pass "workspace is writable"; else bad "workspace is not writable"; fi

# No Linux capabilities and no privilege gain.
caps=$(grep '^CapEff:' /proc/self/status | awk '{print $2}')
[ "$caps" = "0000000000000000" ] && pass "no effective capabilities" || bad "capabilities present: $caps"
if grep -q '^NoNewPrivs:[[:space:]]*1' /proc/self/status; then pass "no-new-privileges set"
elif [ -z "$(find / -xdev -perm /6000 -type f 2>/dev/null | head -1)" ]; then pass "no set-id programs exist, so there is nothing to gain privilege from (wslc has no no-new-privileges flag)"
else bad "set-id programs exist and no-new-privileges is not set"; fi

# Publishing credentials must never be available to the agent.
for name in RELEASE_PUBLISH_TOKEN CLOUDFLARE_API_TOKEN R2_ACCESS_KEY_ID R2_SECRET_ACCESS_KEY WEBSITE_DISPATCH_TOKEN GH_TOKEN GITHUB_TOKEN; do
  [ -z "$(printenv "$name")" ] || bad "$name is set"
done
pass "no publishing credentials in the environment"
ls /run/secrets 2>/dev/null | grep -Eqi 'publish|release|cloudflare|r2|dispatch' && bad "publishing secret mounted" || pass "no publishing secrets mounted"

# Network: only the egress proxy is reachable; direct internet and the home network are not.
if [ -n "${EGRESS_CHECK:-}" ]; then
  if node -e "require('net').connect({host:'1.1.1.1',port:443,timeout:3000}).on('connect',()=>process.exit(0)).on('error',()=>process.exit(1)).on('timeout',()=>process.exit(1))" 2>/dev/null; then bad "direct internet access works (bypasses the proxy)"; else pass "direct internet access blocked"; fi
  if node -e "require('net').connect({host:'192.168.1.1',port:80,timeout:3000}).on('connect',()=>process.exit(0)).on('error',()=>process.exit(1)).on('timeout',()=>process.exit(1))" 2>/dev/null; then bad "home network reachable"; else pass "home network not reachable"; fi
  curl_status=$(node -e "
const http=require('http');const p=new URL(process.env.HTTPS_PROXY);
const r=http.request({host:p.hostname,port:p.port,method:'CONNECT',path:'example.com:443',timeout:5000});
r.on('connect',res=>{console.log(res.statusCode);process.exit(0)});r.on('error',()=>{console.log('error');process.exit(0)});r.on('timeout',()=>{console.log('timeout');process.exit(0)});r.end();" 2>/dev/null)
  [ "$curl_status" != "200" ] && pass "proxy refuses a host that is not on the allow-list ($curl_status)" || bad "proxy allowed example.com"
else
  echo "SKIP  network checks (run through Start-MatrixAgent.ps1 -Action Verify)"
fi

claude --version >/dev/null 2>&1 && pass "claude CLI runs" || bad "claude CLI missing"
git --version >/dev/null 2>&1 && pass "git present" || bad "git missing"
gh --version >/dev/null 2>&1 && pass "gh present" || bad "gh missing"

[ "$fail" = 0 ] && echo "ALL CHECKS PASSED" || echo "SOME CHECKS FAILED"
exit "$fail"
