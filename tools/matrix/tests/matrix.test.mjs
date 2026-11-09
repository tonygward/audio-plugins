import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { execFileSync } from 'node:child_process';
import { parsePrompt, scan, select } from '../backlog.mjs';
import { atomic, locked, readState, reconcile, claim, finish } from '../state.mjs';
import { usageGate, TELEMETRY_SOURCE, dailyDue, londonDay, claudePlan, releaseAssets } from '../policy.mjs';
import { tick, notify, snapshot, readConfig } from '../matrix.mjs';

const prose = '# EQ-41 — Example\n\n**Recommended model:** Sonnet. **Estimated effort / usage:** Low.\n';
function prompt(mode, extra = '', file = 'eq/prompts/41-example.md') {
  return parsePrompt(file, mode === undefined ? prose + extra : `---\nexecution: ${mode}\n${extra}---\n${prose}`);
}
function temp(t) {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'matrix-test-'));
  t.after(() => fs.rmSync(dir, { recursive: true, force: true })); return dir;
}
function state() { return { schema: 1, tasks: {}, events: [], history: [] }; }
const revision = 'a'.repeat(40);
function configuration(dir) { return { repository: dir, stateDirectory: path.join(dir, 'state'), enabled: true, batchLimit: 6, dailyTime: '02:00', liveExecution: false, autoMerge: false, releasePublication: false }; }
function load(...items) { return () => ({ revision, files: new Map(items.map(([file, text]) => [file, text])) }); }

test('Legacy metadata omission defaults to scheduled and never executes on notification', () => {
  const p = prompt(); assert.equal(p.execution, 'scheduled'); assert.equal(p.defaulted, true);
  assert.equal(select([p], {}, 'notify').selected.length, 0);
  assert.equal(select([p], {}, 'daily').selected.length, 1);
});
for (const mode of ['scheduled', 'build-now', 'deferred']) test(`Explicit ${mode} scheduling`, () => {
  const p = prompt(mode);
  assert.equal(select([p], {}, 'notify').selected.length, mode === 'build-now' ? 1 : 0);
  assert.equal(select([p], {}, 'daily').selected.length, mode === 'deferred' ? 0 : 1);
});
for (const value of ['', 'now', 'null', '[build-now]', '&alias', 'BUILD-NOW']) test(`Invalid explicit execution ${value} fails closed`, () => assert.throws(() => prompt(value)));
test('Duplicate metadata and malformed front matter rejected', () => {
  assert.throws(() => prompt('scheduled', 'execution: build-now\n'));
  assert.throws(() => parsePrompt('eq/prompts/41-x.md', '---\nexecution: build-now'));
  assert.throws(() => prompt('scheduled', 'unknown: value\n'));
});
test('Deferred prose overrides missing execution and build-now', () => {
  const p = prompt(undefined, '**Status:** Deferred / planned only — DO NOT IMPLEMENT YET.');
  assert.equal(select([p], {}, 'daily').selected.length, 0);
  const q = parsePrompt('shared/prompts/AUTO-02-example.md', '---\nexecution: build-now\n---\n**Status:** Deferred\n' + prose);
  assert.equal(select([q], {}, 'notify').selected.length, 0);
});
test('Future, completed and blocked prompts excluded', () => {
  for (const file of ['shared/prompts/future/ADV-01-x.md', 'eq/prompts/done/41-x.md']) assert.equal(select([parsePrompt(file, prose)], {}, 'daily').selected.length, 0);
  assert.equal(select([prompt('build-now', 'status: blocked\n')], {}, 'daily').selected.length, 0);
});
test('Identity mismatch and unmerged done status rejected', () => {
  assert.throws(() => prompt('scheduled', 'id: EQ-42\n'));
  assert.throws(() => prompt('scheduled', 'status: done\n'));
});
test('Unknown and circular dependencies block; only canonical merged archive unblocks', () => {
  const p = prompt('build-now', 'dependencies: ["EQ-40"]\n');
  assert.equal(select([p], {}, 'daily').selected.length, 0);
  const done = parsePrompt('eq/prompts/done/40-old.md', prose);
  assert.equal(select([p, done], {}, 'daily').selected.length, 1);
  const cycle = prompt('build-now', 'dependencies: ["EQ-41"]\n');
  assert.equal(select([cycle], {}, 'daily').selected.length, 0);
  assert.throws(() => prompt('build-now', 'dependencies: EQ-40\n'));
});
test('Legacy labelled dependency links are recognised', () => {
  const p = prompt(undefined, '**Depends on:** Shipped [EQ-32](done/32-frequency-guide.md).');
  assert.deepEqual(p.dependencies, ['EQ-32']);
});
test('Duplicate active stable IDs produce visible error', () => {
  const result = scan(new Map([['eq/prompts/41-x.md', prose], ['eq/prompts/41-y.md', prose]]));
  assert.equal(result.errors.length, 1);
});
test('Overnight runs skip infrastructure (AUTO) unless it is build-now', () => {
  const auto = prompt('scheduled', '', 'shared/prompts/AUTO-03-x.md').id;
  assert.equal(auto, 'AUTO-03');
  assert.equal(select([prompt('scheduled', '', 'shared/prompts/AUTO-03-x.md')], {}, 'daily').selected.length, 0);
  assert.equal(select([prompt('build-now', '', 'shared/prompts/AUTO-03-x.md')], {}, 'daily').selected.length, 1);
});
test('Immediate backlog precedes ordinary scheduled work', () => {
  const scheduled = prompt(undefined);
  const immediate = prompt('build-now', '', 'eq/prompts/42-now.md');
  assert.equal(select([scheduled, immediate], {}, 'daily').selected[0].id, 'EQ-42');
});
test('Compatible independent plugin batches and same-plugin/shared conflicts', () => {
  const items = ['eq/prompts/41-x.md', 'analog-compressor/prompts/11-x.md', 'maximiser/prompts/09-x.md'].map(f => prompt('scheduled', '', f));
  assert.equal(select(items, {}, 'daily').selected.length, 3);
  assert.equal(select([...items, prompt('scheduled', '', 'eq/prompts/42-x.md')], {}, 'daily').selected.length, 3);
  const shared = prompt('build-now', '', 'shared/prompts/SUI-14-x.md');
  assert.equal(select([...items, shared], {}, 'daily').selected.length, 1);
});
test('High/unknown effort and non-Sonnet design recommendations require review', () => {
  assert.equal(select([prompt('scheduled', 'effort: high\n')], {}, 'daily').selected.length, 0);
  const p = parsePrompt('eq/prompts/41-x.md', prose.replace('Sonnet.', 'Opus for design, Sonnet for implementation.'));
  assert.equal(select([p], {}, 'daily').selected.length, 0);
  assert.throws(() => claudePlan(p));
});
test('Claims and completion are idempotent, and only merged archive completes', () => {
  const s = state(), p = prompt('build-now'); reconcile(s, [p], revision);
  assert.equal(claim(s, p, revision), true);
  assert.throws(() => claim(s, p, revision));
  finish(s, p.id, 'awaiting-review', { pr: 123 });
  assert.throws(() => finish(s, p.id, 'complete'));
  reconcile(s, [p], revision); assert.equal(s.tasks[p.id].state, 'awaiting-review');
  assert.equal(select([p], s.tasks, 'daily').selected.length, 0);
  reconcile(s, [parsePrompt('eq/prompts/done/41-x.md', prose)], revision);
  assert.equal(s.tasks[p.id].state, 'complete');
});
test('Restart recovery preserves interrupted work without blind retry', () => {
  const s = state(), p = prompt('build-now'); reconcile(s, [p], revision); claim(s, p, revision);
  reconcile(s, [p], revision); assert.equal(s.tasks[p.id].state, 'interrupted');
  assert.equal(select([p], s.tasks, 'daily').selected.length, 0);
});
test('Bounded attempts enter dead letter and changed revision cannot be claimed', () => {
  const s = state(), p = prompt('build-now'); reconcile(s, [p], revision);
  assert.throws(() => claim(s, p, 'b'.repeat(40)));
  s.tasks[p.id].attempts = 2; assert.equal(claim(s, p, revision), false);
  assert.equal(s.tasks[p.id].state, 'dead-letter');
});
test('Deleted and reclassified pending prompts cannot run from stale queue', () => {
  const s = state(), p = prompt('build-now'); reconcile(s, [p], revision); reconcile(s, [], revision);
  assert.equal(s.tasks[p.id].state, 'withdrawn');
  reconcile(s, [prompt('deferred')], revision); assert.equal(s.tasks[p.id].state, 'withdrawn');
});
test('Atomic state persistence and corrupt state fails closed', t => {
  const dir = temp(t); atomic(path.join(dir, 'state.json'), state()); assert.deepEqual(readState(dir), state());
  fs.writeFileSync(path.join(dir, 'state.json'), '{'); assert.throws(() => readState(dir));
});
test('One worker; stale lock not stolen; lock released after errors', t => {
  const dir = temp(t);
  locked(dir, () => assert.throws(() => locked(dir, () => {}), { code: 'EEXIST' }));
  assert.throws(() => locked(dir, () => { throw Error('failure'); }));
  assert.equal(fs.existsSync(path.join(dir, 'worker.lock')), false);
  fs.writeFileSync(path.join(dir, 'worker.lock'), '{"pid":999999}');
  assert.throws(() => locked(dir, () => {}));
});
test('Cross-process concurrency lock rejects a second worker', t => {
  const dir = temp(t);
  const module = new URL('../state.mjs', import.meta.url).href;
  locked(dir, () => assert.throws(() => execFileSync(process.execPath, ['--input-type=module', '-e', `import {locked} from ${JSON.stringify(module)}; locked(${JSON.stringify(dir)},()=>{});`], { stdio: 'pipe' })));
});
const now = Date.parse('2026-11-06T10:00:00Z');
function usage(percentage) { return { source: TELEMETRY_SOURCE, percentage, observedAt: new Date(now).toISOString(), resetsAt: new Date(now + 3600000).toISOString() }; }
for (const [value, allowed] of [[69.9, true], [70, false], [70.1, false], [0, true], [100, false]]) test(`Weekly usage boundary ${value}`, () => assert.equal(usageGate(usage(value), now).allowed, allowed));
test('Usage missing, stale, future, bad source, expired or contradictory fails closed', () => {
  for (const snapshot of [null, {}, usage('84'), usage(NaN), usage(-1), { ...usage(10), source: 'estimate' }, { ...usage(10), observedAt: new Date(now - 61000).toISOString() }, { ...usage(10), observedAt: new Date(now + 1000).toISOString() }, { ...usage(10), resetsAt: new Date(now).toISOString() }]) assert.equal(usageGate(snapshot, now).allowed, false);
  assert.equal(usageGate(usage(69.9), now).allowed, true);
  assert.equal(usageGate(usage(70), now).allowed, false); // Recheck between prompts.
});
test('London scheduler catches missed run, handles spring/fall DST once per day', () => {
  assert.equal(dailyDue(null, '02:00', new Date('2026-03-29T01:01:00Z')), true);
  assert.equal(dailyDue('2026-03-29', '02:00', new Date('2026-03-29T04:00:00Z')), false);
  assert.equal(dailyDue(null, '01:30', new Date('2026-10-25T00:40:00Z')), true);
  assert.equal(dailyDue('2026-10-25', '01:30', new Date('2026-10-25T01:40:00Z')), false);
  assert.equal(dailyDue('2026-11-01', '02:00', new Date('2026-11-06T10:00:00Z')), true);
  assert.equal(londonDay(new Date('2026-06-01T23:30:00Z')), '2026-06-02');
});
test('Notification duplicate, offline/missed and busy recovery use canonical daily scan', t => {
  const dir = temp(t), config = configuration(dir);
  const files = load(['eq/prompts/41-x.md', '---\nexecution: build-now\n---\n' + prose]);
  notify(config.stateDirectory, revision); notify(config.stateDirectory, revision);
  assert.equal(fs.readdirSync(path.join(config.stateDirectory, 'inbox')).length, 1);
  let result = tick(config, { trigger: 'notify', loadSnapshot: files }); assert.equal(result.candidates.length, 1);
  const saved = readState(config.stateDirectory); assert.equal(saved.tasks['EQ-41'].attempts, 0); assert.equal(saved.events.length, 1);
  locked(config.stateDirectory, () => assert.throws(() => tick(config, { trigger: 'notify', loadSnapshot: files })));
  result = tick(config, { trigger: 'daily', loadSnapshot: files }); assert.equal(result.candidates.length, 1);
  // No inbox at all: missed event still recovered.
  const second = configuration(temp(t)); result = tick(second, { trigger: 'daily', loadSnapshot: files }); assert.equal(result.candidates.length, 1);
});
test('Pause and disabled config prevent even snapshot fetch', t => {
  const dir = temp(t), config = configuration(dir);
  const fail = () => { throw Error('must not fetch'); };
  assert.equal(tick({ ...config, enabled: false }, { loadSnapshot: fail }).paused, true);
  fs.writeFileSync(path.join(config.stateDirectory, 'PAUSED'), '');
  assert.equal(tick(config, { loadSnapshot: fail }).paused, true);
});
test('Failed fetch retains pending event and does not mark day completed', t => {
  const config = configuration(temp(t)); notify(config.stateDirectory, revision);
  assert.throws(() => tick(config, { loadSnapshot: () => { throw Error('offline'); } }));
  assert.equal(readState(config.stateDirectory).lastDaily, undefined);
  assert.equal(fs.readdirSync(path.join(config.stateDirectory, 'inbox')).length, 1);
});
test('Malformed active metadata blocks full batch, dry run never claims', t => {
  const config = configuration(temp(t));
  const report = tick(config, { loadSnapshot: load(['eq/prompts/41-x.md', prose], ['eq/prompts/42-x.md', '---\nexecution: invalid\n---\n' + prose]) });
  assert.equal(report.errors.length, 1); assert.equal(report.candidates.length, 0); assert.equal(report.liveExecution, false);
  assert.equal(readState(config.stateDirectory).tasks['EQ-41'].state, 'pending');
});
test('Live config cannot activate execution, merge or release', t => {
  const dir = temp(t), file = path.join(dir, 'config.json');
  for (const key of ['liveExecution', 'autoMerge', 'releasePublication']) { atomic(file, { ...configuration(dir), [key]: true }); assert.throws(() => readConfig(file)); }
  atomic(file, configuration(dir)); assert.equal(readConfig(file).liveExecution, false);
});
test('Notifications reject shell injection and do not execute supplied content', t => {
  const dir = temp(t); for (const value of ['../foo', 'a;whoami', '', 'x'.repeat(40)]) assert.throws(() => notify(dir, value));
});
test('Immutable Git snapshot uses only fixed commands and never checkout or prompt shell', () => {
  const calls = [];
  const result = snapshot('/repo', { run: (exe, args) => { calls.push([exe, args]); if (args[0] === 'rev-parse') return revision; if (args[0] === 'ls-tree') return 'eq/prompts/41-x.md\nmalicious.sh'; if (args[0] === 'show') return prose; return ''; } });
  assert.equal(result.files.size, 1); assert.equal(calls.some(([, args]) => args[0] === 'checkout'), false);
});
test('Release contract accepts exactly two matching installers', () => {
  const good = ['Songsmith-Plugins-1.4.0-win64-setup.exe', 'Songsmith-Plugins-1.4.0-macOS.pkg'];
  assert.equal(releaseAssets('1.4.0', good).length, 2);
  for (const names of [[], good.slice(0, 1), [...good, 'screenshot.png'], [good[0], good[0]], good.map(n => n.replace('1.4.0', '1.3.0'))]) assert.throws(() => releaseAssets('1.4.0', names));
});
test('Actual repository backlog parses legacy files and preserves AUTO-02 deferred policy', () => {
  const repo = fileURLToPath(new URL('../../../', import.meta.url));
  const result = scan(snapshot(repo, { local: true }).files);
  assert.deepEqual(result.errors, []);
  assert.equal(result.prompts.find(p => p.id === 'AUTO-02').deferred, true);
  assert.equal(result.prompts.find(p => p.id === 'EQ-38').execution, 'build-now');
  assert.equal(result.prompts.find(p => p.id === 'EQ-38').defaulted, false);
  assert.equal(result.prompts.find(p => p.id === 'ACOMP-07').dependencies.includes('ACOMP-06'), true);
});
test('Timer acknowledges notification only after reconciliation and does one daily scan per London date', t => {
  const config = configuration(temp(t));
  const files = load(['eq/prompts/41-x.md', '---\nexecution: build-now\n---\n' + prose]);
  const morning = new Date('2026-11-06T10:00:00Z');
  const first = tick(config, { trigger: 'timer', loadSnapshot: files, now: morning });
  assert.equal(first.trigger, 'daily');
  assert.equal(tick(config, { trigger: 'timer', loadSnapshot: files, now: morning }).idle, true);
  notify(config.stateDirectory, revision);
  assert.equal(tick(config, { trigger: 'timer', loadSnapshot: files, now: morning }).trigger, 'notify');
  assert.equal(tick(config, { trigger: 'timer', loadSnapshot: files, now: morning }).idle, true);
});
test('Local dry run cannot complete tasks or acknowledge canonical queue', t => {
  const config = configuration(temp(t)); notify(config.stateDirectory, revision);
  tick(config, { local: true, loadSnapshot: load(['eq/prompts/done/41-x.md', prose]) });
  assert.deepEqual(readState(config.stateDirectory), state());
});
