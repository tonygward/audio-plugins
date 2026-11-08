// Pure decision logic for merging and archiving Matrix pull requests. No network, no git.
// The workflows fetch the facts with `gh` and call these functions from main (never from PR code).

export const UI_REVIEW_NOTE = 'UI changes require retained before/after screenshots and visual review before marking this draft ready';
export const CLEAN_VALIDATION = 'Local Release configure, build and tests passed without detected warnings.';

// Same scope as the worker's validateChanges (tools/matrix/worker.mjs), plus the root CHANGELOG.md.
export function inScope(file) {
  if (typeof file !== 'string' || !file || /[\\\x00-\x1f]/.test(file)) return false;
  if (file.split('/').some(part => part === '..' || part.startsWith('.'))) return false;
  if (/\.(exe|dll|cmd|bat|ps1|sh|cmake)$/i.test(file)) return false;
  return /^(eq|analog-compressor|maximiser|shared)\/(Source|Tests|Resources|docs)\//.test(file)
    || /^(eq|analog-compressor|maximiser)\/README\.md$/.test(file)
    || file === 'CHANGELOG.md';
}

export function blockedReason(body) {
  const match = /\n## Blocked\n\n([\s\S]*?)\s*$/.exec(String(body).replace(/\r\n/g, '\n'));
  return match ? match[1].trim() : null;
}

// Returns { merge, reasons }. Merge only when every gate holds; reasons explain a refusal.
export function evaluate({ repository, pr, files, run, allowUiWithoutReview = true }) {
  const reasons = [];
  const no = reason => reasons.push(reason);
  if (!pr || pr.state !== 'OPEN') no('Pull request is not open');
  if (pr?.baseRefName !== 'main') no('Base branch is not main');
  if (!/^matrix\/[a-z]+(?:-[a-z]+)*-\d+-[0-9a-f]{12}$/.test(pr?.headRefName ?? '')) no('Branch is not a Matrix branch');
  if (pr?.headRepository !== repository) no('Pull request is from another repository');
  if (!/^Implements [A-Z]+(?:-[A-Z]+)*-\d+ from \[canonical prompt\]/.test(pr?.body ?? '') || !/Release checkpoint: unassigned; no version bump/.test(pr?.body ?? '')) no('Description was not written by the Matrix worker');
  const blocked = blockedReason(pr?.body ?? '');
  if (blocked !== null && !(allowUiWithoutReview && blocked === UI_REVIEW_NOTE)) no(`Worker marked the work blocked: ${blocked.slice(0, 200)}`);
  if (blocked === null && !(pr?.body ?? '').includes(`Validation: ${CLEAN_VALIDATION}`)) no('Worker did not record a clean local build and test run');
  if (!Array.isArray(files) || files.length === 0 || files.length > 100) no('Unexpected number of changed files');
  else for (const file of files) if (!inScope(file)) no(`Changed file outside the permitted scope: ${String(file).slice(0, 120)}`);
  if (!run || run.conclusion !== 'success' || run.event !== 'pull_request') no('Hosted Build workflow has not succeeded for the pull request');
  if (run && pr && run.head_sha !== pr.headRefOid) no('Build result is for an older commit than the pull request head');
  return { merge: reasons.length === 0, reasons };
}

// matrix/sui-14-aa16c3e5ab59 -> SUI-14
export function promptIdFromBranch(branch) {
  const match = /^matrix\/([a-z]+(?:-[a-z]+)*-\d+)-[0-9a-f]{12}$/.exec(branch ?? '');
  return match ? match[1].toUpperCase() : null;
}

// dir/prompts/x.md -> dir/prompts/done/x.md. Refuses anything already archived or outside a prompts folder.
export function archivePath(file) {
  const match = /^((?:eq|analog-compressor|maximiser|shared)\/prompts)\/([^/]+\.md)$/.exec(file ?? '');
  return match ? `${match[1]}/done/${match[2]}` : null;
}

// Mark a prompt done in its front matter; files without front matter are only moved.
export function markDone(text) {
  const eol = text.includes('\r\n') ? '\r\n' : '\n';
  const normal = text.replace(/\r\n/g, '\n');
  if (!normal.startsWith('---\n')) return text;
  const end = normal.indexOf('\n---\n', 4);
  if (end < 0) return text;
  const head = normal.slice(4, end).split('\n');
  const index = head.findIndex(line => /^status:/.test(line));
  if (index >= 0) head[index] = 'status: done'; else head.unshift('status: done');
  return (`---\n${head.join('\n')}` + normal.slice(end)).replace(/\n/g, eol);
}
