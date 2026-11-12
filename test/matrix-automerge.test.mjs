import test from 'node:test';
import assert from 'node:assert/strict';
import { evaluate, inCoreScope, inScope, promptIdFromBranch, archivePath, markDone, relinkMoved, blockedReason, UI_REVIEW_NOTE, CLEAN_VALIDATION, CI_VALIDATION, CORE_STATEMENT, CORE_RUN, BUILD_RUN } from '../scripts/matrix/automerge.mjs';

const repository = 'tonygward/audio-plugins';
const sha = 'a'.repeat(40);
const bodyWith = (validation, blocked) => `Implements SUI-14 from [canonical prompt](https://github.com/${repository}/blob/${sha}/shared/prompts/SUI-14.md).\n\nBase: ${sha}. Implementation: ${sha}.\n\nValidation: ${validation}\n\nClaude Sonnet; file tools only. Release checkpoint: unassigned; no version bump or publication.` + (blocked ? `\n## Blocked\n\n${blocked}\n` : '');
const good = () => ({
  repository,
  pr: { state: 'OPEN', baseRefName: 'main', headRefName: 'matrix/sui-14-aa16c3e5ab59', headRefOid: sha, headRepository: repository, body: bodyWith(CLEAN_VALIDATION) },
  files: ['eq/Source/FrequencyGuide.h', 'eq/Tests/EQTests.cpp', 'CHANGELOG.md'],
  run: { conclusion: 'success', event: 'pull_request', head_sha: sha }
});

test('a clean Matrix pull request with green hosted builds merges', () => {
  assert.deepEqual(evaluate(good()), { merge: true, reasons: [], lane: 'task' });
});

test('UI-review block is accepted by default and refused when the owner switches it off', () => {
  const facts = good(); facts.pr.body = bodyWith(UI_REVIEW_NOTE, UI_REVIEW_NOTE);
  assert.equal(evaluate(facts).merge, true);
  assert.equal(evaluate({ ...facts, allowUiWithoutReview: false }).merge, false);
});

test('any other blocked reason refuses to merge', () => {
  for (const reason of ['Local build/tests failed or reported warnings; inspect isolated build diagnostics', 'Main advanced during execution; rebase/revalidate manually before review', 'Repair stopped by usage policy']) {
    const facts = good(); facts.pr.body = bodyWith(reason, reason);
    assert.equal(evaluate(facts).merge, false, reason);
  }
});

test('refuses unless the hosted build succeeded for the exact head commit', () => {
  for (const patch of [{ conclusion: 'failure' }, { conclusion: 'cancelled' }, { event: 'push' }, { head_sha: 'b'.repeat(40) }]) {
    const facts = good(); facts.run = { ...facts.run, ...patch };
    assert.equal(evaluate(facts).merge, false, JSON.stringify(patch));
  }
  const missing = good(); missing.run = null;
  assert.equal(evaluate(missing).merge, false);
});

test('refuses forks, other bases, non-Matrix branches and foreign descriptions', () => {
  const cases = [
    pr => { pr.headRepository = 'someone/else'; }, pr => { pr.baseRefName = 'dev'; }, pr => { pr.state = 'CLOSED'; },
    pr => { pr.headRefName = 'feature/anything'; }, pr => { pr.headRefName = 'matrix/sui-14-xyz'; }, pr => { pr.body = 'Looks fine, please merge'; }
  ];
  for (const change of cases) { const facts = good(); change(facts.pr); assert.equal(evaluate(facts).merge, false); }
});

test('refuses any changed file outside the worker scope', () => {
  for (const file of ['.github/workflows/build.yml', 'tools/matrix/worker.mjs', 'scripts/release/prepare.mjs', 'VERSION', 'installer/setup.iss', 'eq/Source/../x.cpp', 'eq/Source/run.ps1', 'docs/CHANGELOG.md', 'shared/prompts/SUI-09.md', 'eq/prompts/41-x.md']) {
    const facts = good(); facts.files = [...facts.files, file];
    assert.equal(evaluate(facts).merge, false, file);
  }
  const none = good(); none.files = []; assert.equal(evaluate(none).merge, false);
  assert.equal(inScope('maximiser/Source/Engine.cpp'), true);
  assert.equal(inScope('shared/Resources/font.ttf'), true);
});

test('clean validation line is required when nothing is blocked', () => {
  const facts = good(); facts.pr.body = bodyWith('Looks fine');
  assert.equal(evaluate(facts).merge, false);
});

test('blockedReason reads only the Blocked section', () => {
  assert.equal(blockedReason(bodyWith(CLEAN_VALIDATION)), null);
  assert.equal(blockedReason(bodyWith('x', 'Because\r\n')), 'Because');
});

test('branch names map to prompt ids and prompts to the done folder', () => {
  assert.equal(promptIdFromBranch('matrix/sui-14-aa16c3e5ab59'), 'SUI-14');
  assert.equal(promptIdFromBranch('matrix/acomp-06-0123456789ab'), 'ACOMP-06');
  assert.equal(promptIdFromBranch('matrix/eq-41-xyz'), null);
  assert.equal(promptIdFromBranch('main'), null);
  assert.equal(archivePath('shared/prompts/SUI-14-x.md'), 'shared/prompts/done/SUI-14-x.md');
  assert.equal(archivePath('eq/prompts/41-x.md'), 'eq/prompts/done/41-x.md');
  assert.equal(archivePath('compressor/prompts/01-x.md'), 'compressor/prompts/done/01-x.md');
  assert.equal(archivePath('eq/prompts/done/41-x.md'), null);
  assert.equal(archivePath('docs/x.md'), null);
});

test('markDone removes status and execution (the board owns them), keeps the rest and the line endings', () => {
  assert.equal(markDone('---\nid: A-1\nstatus: ready\nexecution: build-now\n---\n\n# Title\n'), '---\nid: A-1\n---\n\n# Title\n');
  assert.equal(markDone('---\r\nid: A-1\r\nstatus: ready\r\n---\r\nbody\r\n'), '---\r\nid: A-1\r\n---\r\nbody\r\n');
  assert.equal(markDone('---\nid: A-1\n---\nbody\n'), '---\nid: A-1\n---\nbody\n');
  assert.equal(markDone('# No metadata\n'), '# No metadata\n');
  // The file moves one folder deeper, so links that climb out of the prompts folder gain a ../ and sibling links are untouched.
  assert.equal(markDone('---\nid: A-1\n---\nSee [policy](../../docs/p.md) and [sibling](x.md).\n'), '---\nid: A-1\n---\nSee [policy](../../../docs/p.md) and [sibling](x.md).\n');
  assert.equal(markDone('See [policy](../../docs/p.md).\n'), 'See [policy](../../../docs/p.md).\n');
});

test('the Linux worker hosted-build validation line is accepted, and an unknown one is not', () => {
  const facts = good(); facts.pr.body = bodyWith(CI_VALIDATION);
  assert.equal(evaluate(facts).merge, true);
  facts.pr.body = bodyWith('Looks fine to me.');
  assert.equal(evaluate(facts).merge, false);
});

const core = () => ({
  repository,
  pr: { state: 'OPEN', baseRefName: 'main', headRefName: 'matrix/core/board-moves', headRefOid: sha, headRepository: repository, body: `Fixes board moves.\n\n${CORE_STATEMENT}` },
  files: ['tools/matrix/board.mjs', 'tools/matrix/tests/board.test.mjs', 'tools/matrix-linux/worker.config.json', 'docs/songsmith-execution-policy.md'],
  run: { name: CORE_RUN, conclusion: 'success', event: 'pull_request', head_sha: sha }
});

test('core lane: Matrix code with green orchestration tests on the exact head merges', () => {
  assert.deepEqual(evaluate(core()), { merge: true, reasons: [], lane: 'core' });
});

test('core lane: needs the statement, the orchestration tests, the exact head and a same-repository PR on main', () => {
  const no = (change, text) => { const f = core(); change(f); const r = evaluate(f); assert.equal(r.merge, false, text); assert.equal(r.lane, 'core'); };
  no(f => { f.pr.body = 'Fixes board moves.'; }, 'statement');
  no(f => { f.run.name = BUILD_RUN; }, 'wrong workflow');
  no(f => { delete f.run.name; }, 'unnamed workflow');
  no(f => { f.run.conclusion = 'failure'; }, 'failed run');
  no(f => { f.run.head_sha = 'b'.repeat(40); }, 'older commit');
  no(f => { f.pr.headRepository = 'someone/else'; }, 'fork');
  no(f => { f.pr.baseRefName = 'dev'; }, 'base');
  no(f => { f.files = []; }, 'no files');
});

test('core lane: workflows, the gate, scripts, the Dockerfile, the allow-list and plugin code stay manual', () => {
  for (const file of ['.github/workflows/matrix-auto-merge.yml', 'scripts/matrix/automerge.mjs', 'tools/matrix/Notify-Matrix.ps1', 'tools/matrix-linux/entrypoint.sh',
    'tools/matrix-linux/Dockerfile', 'tools/matrix-linux/squid/squid.conf', 'tools/matrix/../../CLAUDE.md', 'tools/matrix/.hidden.mjs', 'eq/Source/Plugin.cpp', 'CHANGELOG.md', 'VERSION', 'tools/matrix/x/y.mjs']) {
    assert.equal(inCoreScope(file), false, file);
    const f = core(); f.files = [file]; assert.equal(evaluate(f).merge, false, file);
  }
  for (const file of ['tools/matrix/board.mjs', 'tools/matrix/README.md', 'tools/matrix/tests/board.test.mjs', 'tools/matrix-linux/worker.config.json', 'docs/songsmith-execution-policy.md']) assert.equal(inCoreScope(file), true, file);
});

test('task lane: only the Build workflow counts, and the core branch pattern is exact', () => {
  const f = good(); f.run.name = CORE_RUN; assert.equal(evaluate(f).merge, false);
  const g = good(); g.run.name = BUILD_RUN; assert.equal(evaluate(g).merge, true);
  for (const branch of ['matrix/core', 'matrix/core/', 'matrix/core/Bad_Name', 'matrix/core/a/b', 'matrix/corex/a']) {
    const c = core(); c.pr.headRefName = branch; const r = evaluate(c); assert.equal(r.merge, false, branch); assert.equal(r.lane, 'task', branch);
  }
});

// ---- Pipeline names: every workflow says whether it is CI, CD, a gate or housekeeping, and nothing that matches on a name can drift ----
test('every workflow is named by kind, and every name something depends on exists exactly', async () => {
  const fs = await import('node:fs'), path = await import('node:path');
  const dir = new URL('../.github/workflows/', import.meta.url);
  const workflows = fs.readdirSync(dir).filter(f => f.endsWith('.yml')).map(f => {
    const text = fs.readFileSync(new URL(f, dir), 'utf8');
    return { file: f, text, name: /^name:\s*(.+?)\s*$/m.exec(text)?.[1] };
  });
  const names = workflows.map(w => w.name);
  for (const w of workflows) assert.match(w.name, /^(CI|CD|Gate|Housekeeping) - \S/, `${w.file} must start with CI, CD, Gate or Housekeeping`);
  assert.equal(new Set(names).size, names.length, 'workflow names are unique');
  for (const w of workflows) for (const m of w.text.matchAll(/^\s*workflows:\s*\[(.+?)\]\s*$/gm)) for (const ref of m[1].split(',').map(s => s.trim().replace(/^["']|["']$/g, ''))) assert.ok(names.includes(ref), `${w.file} listens for a workflow that does not exist: ${ref}`);
  assert.ok(names.includes(BUILD_RUN) && names.includes(CORE_RUN), 'the names the merge gate matches on exist');
  assert.equal(workflows.find(w => w.file === 'build.yml').name, BUILD_RUN);
  assert.equal(workflows.find(w => w.file === 'matrix-tests.yml').name, CORE_RUN);
});

test('relinkMoved repairs relative and blob links to a moved prompt, and nothing else', () => {
  const old = 'maximiser/prompts/09-x.md', next = 'maximiser/prompts/done/09-x.md';
  assert.equal(relinkMoved('| [09](09-x.md) | Todo |', 'maximiser/prompts/README.md', old, next), '| [09](done/09-x.md) | Todo |');
  assert.equal(relinkMoved('[p](../../maximiser/prompts/09-x.md#why)', 'shared/prompts/README.md', old, next), '[p](../../maximiser/prompts/done/09-x.md#why)');
  assert.equal(relinkMoved('[p](maximiser/prompts/09-x.md)', 'README.md', old, next), '[p](maximiser/prompts/done/09-x.md)');
  assert.equal(relinkMoved('[i](https://github.com/o/r/blob/main/maximiser/prompts/09-x.md)', 'x.md', old, next), '[i](https://github.com/o/r/blob/main/maximiser/prompts/done/09-x.md)');
  const untouched = '[other](10-y.md) [web](https://example.com/09-x.md) [anchor](#top) [same](done/09-x.md)';
  assert.equal(relinkMoved(untouched, 'maximiser/prompts/README.md', old, next), untouched);
});

test('the archive step moves the prompt and leaves no broken link in any page', async () => {
  const fs = await import('node:fs'), os = await import('node:os'), path = await import('node:path'), { execFileSync } = await import('node:child_process');
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'archive-'));
  try {
    const put = (file, text) => { fs.mkdirSync(path.dirname(path.join(dir, file)), { recursive: true }); fs.writeFileSync(path.join(dir, file), text); };
    put('maximiser/prompts/09-x.md', '---\nid: MAX-09\nstatus: ready\n---\n# MAX-09 - X\nSee [policy](../../docs/p.md).\n');
    put('maximiser/prompts/README.md', '| [09](09-x.md) | Todo |\n');
    put('shared/prompts/README.md', '| MAX-09 | [x](../../maximiser/prompts/09-x.md) |\n');
    put('docs/p.md', 'policy\n'); put('docs/plan.md', 'see [x](../maximiser/prompts/09-x.md)\n');
    const cli = new URL('../scripts/matrix/archive-cli.mjs', import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, '$1');
    const out = execFileSync(process.execPath, [cli, 'matrix/max-09-aaaaaaaaaaaa'], { cwd: dir, encoding: 'utf8' });
    assert.match(out, /09-x\.md -> maximiser\/prompts\/done\/09-x\.md/); assert.match(out, /Links updated in:/);
    assert.ok(!fs.existsSync(path.join(dir, 'maximiser/prompts/09-x.md')));
    const read = f => fs.readFileSync(path.join(dir, f), 'utf8');
    assert.match(read('maximiser/prompts/README.md'), /\(done\/09-x\.md\)/);
    assert.match(read('shared/prompts/README.md'), /\.\.\/\.\.\/maximiser\/prompts\/done\/09-x\.md/);
    assert.match(read('docs/plan.md'), /\.\.\/maximiser\/prompts\/done\/09-x\.md/);
    assert.match(read('maximiser/prompts/done/09-x.md'), /\.\.\/\.\.\/\.\.\/docs\/p\.md/);
    assert.doesNotMatch(read('maximiser/prompts/done/09-x.md'), /status:/);
  } finally { fs.rmSync(dir, { recursive: true, force: true }); }
});
