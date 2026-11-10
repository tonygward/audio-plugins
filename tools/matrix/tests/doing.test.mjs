import test from 'node:test';
import assert from 'node:assert/strict';
import { reconcileDoing } from '../doing.mjs';

const prompt = (id, extra = {}) => ({ id, complete: false, ...extra });
const card = (n, taskId, extra = {}) => ({ position: n, status: 'Doing', issue: n, state: 'OPEN', taskId, ...extra });
const snap = (...cards) => ({ ok: true, cards });
const pr = (number, state, headRefName, title = 'x') => ({ number, state, headRefName, title });
const kinds = result => result.findings.map(f => [f.taskId, f.kind]);

test('a Doing card with no claim is not pickup evidence: return it to Todo, and enabling waits', () => {
  const r = reconcileDoing(snap(card(1, 'A-01')), [prompt('A-01')], [], {});
  assert.deepEqual(kinds(r), [['A-01', 'unclaimed']]);
  assert.match(r.findings[0].detail, /return the card to Todo/);
  assert.equal(r.enableAllowed, false);
});

test('merged Matrix work stays release-pending and is never recoded', () => {
  const r = reconcileDoing(snap(card(1, 'A-01')), [prompt('A-01', { complete: true })], [pr(7, 'MERGED', 'matrix/a-01-abc123')], {});
  assert.deepEqual(kinds(r), [['A-01', 'release-pending']]);
  assert.match(r.findings[0].detail, /#7/);
  assert.equal(r.enableAllowed, true);
});

test('prompt-only pull requests do not count as merged implementation', () => {
  const r = reconcileDoing(snap(card(1, 'A-01')), [prompt('A-01', { complete: true })], [pr(6, 'MERGED', 'a-01-prompt', 'A-01: prompt')], {});
  assert.deepEqual(kinds(r), [['A-01', 'inconsistent']]);
  assert.equal(r.enableAllowed, false);
});

test('an open pull request or our own claim is recognised, not re-picked', () => {
  const open = reconcileDoing(snap(card(1, 'A-01')), [prompt('A-01')], [pr(9, 'OPEN', 'work', 'A-01: partial')], {});
  assert.deepEqual(kinds(open), [['A-01', 'in-review']]);
  const mine = reconcileDoing(snap(card(1, 'A-01')), [prompt('A-01')], [], { 'A-01': { id: 'A-01', state: 'claimed', job: 'job-3' } });
  assert.deepEqual(kinds(mine), [['A-01', 'active']]);
  assert.equal(mine.enableAllowed, true);
});

test('closed issue, missing prompt, duplicates and non-issue cards block enabling', () => {
  const r = reconcileDoing(snap(card(1, 'A-01', { state: 'CLOSED' }), card(2, 'B-01'), card(3, 'C-01'), card(4, 'C-01'), card(5, null, { issue: null }), card(6, null)),
    [prompt('A-01'), prompt('C-01')], [], {});
  assert.deepEqual(kinds(r).map(k => k[1]), ['inconsistent', 'inconsistent', 'unclaimed', 'inconsistent', 'inconsistent', 'inconsistent']);
  assert.equal(r.enableAllowed, false);
});

test('interrupted or dead-lettered tasks go to Needs attention', () => {
  for (const state of ['interrupted', 'dead-letter']) {
    const r = reconcileDoing(snap(card(1, 'A-01')), [prompt('A-01')], [], { 'A-01': { id: 'A-01', state } });
    assert.equal(r.findings[0].kind, 'needs-attention');
    assert.equal(r.enableAllowed, false);
    assert.match(r.findings[0].detail, new RegExp(state));
  }
});

test('state and board must agree: orphan claims and multiple claims block enabling', () => {
  const orphan = reconcileDoing(snap(card(1, 'A-01', { status: 'Backlog' })), [prompt('A-01')], [], { 'A-01': { id: 'A-01', state: 'claimed', job: 'j' } });
  assert.equal(orphan.enableAllowed, false);
  assert.match(orphan.findings[0].detail, /no Doing or Todo card/);
  const two = reconcileDoing(snap(card(1, 'A-01'), card(2, 'B-01')), [prompt('A-01'), prompt('B-01')], [],
    { 'A-01': { id: 'A-01', state: 'claimed', job: 'j1' }, 'B-01': { id: 'B-01', state: 'claimed', job: 'j2' } });
  assert.equal(two.enableAllowed, false);
  assert.ok(two.findings.some(f => /2 tasks are claimed/.test(f.detail)));
});

test('only Doing cards are reconciled; a failed snapshot stops everything', () => {
  const r = reconcileDoing(snap(card(1, 'A-01', { status: 'Todo' }), card(2, 'B-01', { status: 'Done' })), [prompt('A-01'), prompt('B-01')], [], {});
  assert.deepEqual(r.findings, []);
  assert.equal(r.enableAllowed, true);
  const bad = reconcileDoing({ ok: false, reason: 'Partial board snapshot' }, [], [], {});
  assert.deepEqual([bad.ok, bad.reason], [false, 'Partial board snapshot']);
  assert.equal(bad.enableAllowed, undefined);
});

test('does not modify the worker state it inspects', () => {
  const tasks = { 'A-01': { id: 'A-01', state: 'claimed', job: 'j' } };
  const before = JSON.stringify(tasks);
  reconcileDoing(snap(card(1, 'A-01')), [prompt('A-01')], [], tasks);
  assert.equal(JSON.stringify(tasks), before);
});
