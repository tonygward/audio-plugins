import test from 'node:test';
import assert from 'node:assert/strict';
import { evaluate, inScope, promptIdFromBranch, archivePath, markDone, blockedReason, UI_REVIEW_NOTE, CLEAN_VALIDATION, CI_VALIDATION } from '../scripts/matrix/automerge.mjs';

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
  assert.deepEqual(evaluate(good()), { merge: true, reasons: [] });
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
  assert.equal(archivePath('eq/prompts/done/41-x.md'), null);
  assert.equal(archivePath('docs/x.md'), null);
});

test('markDone changes only the status line and keeps line endings', () => {
  assert.equal(markDone('---\nid: A-1\nstatus: ready\nexecution: build-now\n---\n\n# Title\n'), '---\nid: A-1\nstatus: done\nexecution: build-now\n---\n\n# Title\n');
  assert.equal(markDone('---\r\nid: A-1\r\nstatus: ready\r\n---\r\nbody\r\n'), '---\r\nid: A-1\r\nstatus: done\r\n---\r\nbody\r\n');
  assert.equal(markDone('---\nid: A-1\n---\nbody\n'), '---\nstatus: done\nid: A-1\n---\nbody\n');
  assert.equal(markDone('# No metadata\n'), '# No metadata\n');
});

test('the Linux worker hosted-build validation line is accepted, and an unknown one is not', () => {
  const facts = good(); facts.pr.body = bodyWith(CI_VALIDATION);
  assert.equal(evaluate(facts).merge, true);
  facts.pr.body = bodyWith('Looks fine to me.');
  assert.equal(evaluate(facts).merge, false);
});
