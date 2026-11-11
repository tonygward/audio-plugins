import test from 'node:test';
import assert from 'node:assert/strict';
import { chooseVersion, compareVersions, parseVersion } from '../../matrix-linux/claude-version.mjs';

const DAY = 86400000, now = Date.parse('2026-11-12T12:00:00Z');
const ago = days => new Date(now - days * DAY).toISOString();
const registry = (entries, latest) => ({
  name: '@anthropic-ai/claude-code',
  'dist-tags': { latest: latest ?? entries.filter(e => parseVersion(e[0])).at(-1)[0] },
  versions: Object.fromEntries(entries.map(([v, , extra]) => [v, extra ?? {}])),
  time: Object.fromEntries(entries.map(([v, days]) => [v, ago(days)]))
});
const choose = (reg, extra = {}) => chooseVersion({ current: '2.1.322', registry: reg, now, ...extra });

test('versions compare numerically and only stable x.y.z counts', () => {
  assert.equal(compareVersions('2.1.10', '2.1.9'), 1); assert.equal(compareVersions('2.0.0', '2.0.0'), 0); assert.equal(compareVersions('1.9.9', '2.0.0'), -1);
  assert.equal(parseVersion('2.1.3-beta.1'), null); assert.equal(parseVersion('latest'), null); assert.equal(parseVersion(undefined), null);
  assert.throws(() => compareVersions('2.1.3-beta', '2.1.2'));
});

test('takes the newest version that is old enough', () => {
  const r = choose(registry([['2.1.322', 30], ['2.1.330', 10], ['2.1.340', 5], ['2.1.350', 1]]));
  assert.equal(r.action, 'update'); assert.equal(r.target, '2.1.340');
});

test('a version younger than the minimum age waits, and says when it becomes eligible', () => {
  const r = choose(registry([['2.1.322', 30], ['2.1.330', 1]]));
  assert.equal(r.action, 'none'); assert.match(r.reason, /2\.1\.330 is newer but younger than 3 days; eligible from 2026-11-14/);
  assert.equal(choose(registry([['2.1.322', 30], ['2.1.330', 1]]), { minAgeDays: 0 }).target, '2.1.330');
});

test('never downgrades, never reinstalls the current version', () => {
  assert.equal(choose(registry([['2.1.300', 40], ['2.1.322', 30]])).action, 'none');
  assert.match(choose(registry([['2.1.322', 30]])).reason, /newest eligible/);
});

test('skips deprecated versions, rejected versions and pre-releases', () => {
  const r = registry([['2.1.322', 30], ['2.1.330', 10], ['2.1.340', 9, { deprecated: 'broken' }], ['2.1.350-beta.1', 8], ['2.1.360', 7]]);
  assert.equal(choose(r, { rejected: ['2.1.360'] }).target, '2.1.330');
  assert.equal(choose(r).target, '2.1.360');
});

test('a new major version needs an explicit flag', () => {
  const r = registry([['2.1.322', 30], ['3.0.0', 20]]);
  assert.equal(choose(r).action, 'none'); assert.equal(choose(r, { allowMajor: true }).target, '3.0.0');
});

test('never newer than the latest tag, never trusts a version with no publish time', () => {
  const r = registry([['2.1.322', 30], ['2.1.330', 10], ['2.1.340', 9]], '2.1.330');
  assert.equal(choose(r).target, '2.1.330');
  const noTime = registry([['2.1.322', 30], ['2.1.330', 10]]); delete noTime.time['2.1.330'];
  assert.equal(choose(noTime).action, 'none');
});

test('a registry with no stable latest, or a bad current version, never updates', () => {
  assert.equal(choose({ 'dist-tags': {}, versions: {}, time: {} }).action, 'none');
  assert.throws(() => choose(registry([['2.1.330', 10]]), { current: 'next' }));
});
