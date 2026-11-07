import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

test('Windows broker request, fixed-task and hostile-log policy (no task starts)', { skip: process.platform !== 'win32' }, () => {
  const output = execFileSync('pwsh.exe', ['-NoProfile', '-NonInteractive', '-File', fileURLToPath(new URL('./Test-BrokerContract.ps1', import.meta.url))], { encoding: 'utf8', timeout: 30000, windowsHide: true });
  assert.match(output, /PASS: \d+ broker contract\/log\/task-policy checks; no task was registered or started/);
});
