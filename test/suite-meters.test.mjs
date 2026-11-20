// SUI-23: every Songsmith plugin has an Output meter at the far right of its editor, and every plugin that
// turns the level down also has a Gain Reduction meter beside it. The list of plugins is found, not typed:
// any folder with a CMakeLists.txt that adds a plugin, so a new plugin cannot be added without a meter.
import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
// Equalisers: Output meter only. Never a made-up Gain Reduction. Everything else is treated as a dynamics plugin.
// The Plugin Analyzer (ANALYZER-01) is a measuring tool that passes the sound through unchanged, so it never has gain reduction.
const equalisers = new Set(['eq', 'pultec-eq', 'plugin-analyzer']);

const plugins = fs.readdirSync(root, { withFileTypes: true })
  .filter(d => d.isDirectory() && fs.existsSync(path.join(root, d.name, 'CMakeLists.txt')))
  .filter(d => /juce_add_plugin/.test(fs.readFileSync(path.join(root, d.name, 'CMakeLists.txt'), 'utf8')))
  .map(d => d.name);

const editorText = dir => ['PluginEditor.h', 'PluginEditor.cpp']
  .map(f => path.join(root, dir, 'Source', f))
  .filter(f => fs.existsSync(f))
  .map(f => fs.readFileSync(f, 'utf8'))
  .join('\n');

test('the plugin list is found (a guard against this test silently checking nothing)', () => {
  assert.ok(plugins.length >= 8, `found only: ${plugins.join(', ')}`);
  for (const p of ['eq', 'compressor', 'analog-compressor', 'maximiser', 'optical-leveller', 'pultec-eq', 'bus-compressor', 'plugin-analyzer'])
    assert.ok(plugins.includes(p), `${p} should be found`);
});

for (const plugin of plugins) {
  test(`${plugin}: Output meter at the far right`, () => {
    const text = editorText(plugin);
    assert.match(text, /OutputMeterDisplay|MeterColumn/, `${plugin}'s editor has no shared Output meter (use Songsmith::UI::MeterColumn)`);
  });

  if (!equalisers.has(plugin)) {
    test(`${plugin}: Gain Reduction meter beside the Output meter`, () => {
      const text = editorText(plugin);
      assert.match(text, /MeterColumn\s+\w+\s*\{\s*true|reductionMeter|Kind::reduction/,
        `${plugin} turns the level down, so it needs a Gain Reduction meter (use Songsmith::UI::MeterColumn { true })`);
    });
  } else {
    test(`${plugin}: an equaliser shows Output only, never a made-up Gain Reduction`, () => {
      assert.doesNotMatch(editorText(plugin), /MeterColumn\s+\w+\s*\{\s*true|reductionMeter|Kind::reduction/);
    });
  }
}
