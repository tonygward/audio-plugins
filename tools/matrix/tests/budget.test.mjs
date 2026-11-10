import test from 'node:test';
import assert from 'node:assert/strict';
import { budgetStop, runMinutes, DEFAULT_RUN_MINUTES } from '../budget.mjs';

const start = Date.parse('2026-11-10T10:00:00Z');
const task = { runStartedAt: new Date(start).toISOString() };

test('the run budget is 120 minutes by default and configurable', () => {
  assert.equal(DEFAULT_RUN_MINUTES, 120);
  assert.equal(runMinutes({}), 120);
  assert.equal(runMinutes({ maximumRunMinutes: 45 }), 45);
});

test('the run stops at the limit, not before, and says why', () => {
  assert.equal(budgetStop(task, {}, start + 119 * 60000 + 59000), null);
  assert.match(budgetStop(task, {}, start + 120 * 60000), /Run time limit of 120 minutes/);
  assert.match(budgetStop(task, { maximumRunMinutes: 30 }, start + 31 * 60000), /30 minutes/);
});

test('no recorded start means no stop (nothing to count from), and bad dates never stop a run by accident', () => {
  assert.equal(budgetStop({}, {}, start + 9e9), null);
  assert.equal(budgetStop({ runStartedAt: 'not a date' }, {}, start + 9e9), null);
});
