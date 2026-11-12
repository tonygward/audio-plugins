import test from 'node:test';
import assert from 'node:assert/strict';
import {planPush} from '../scripts/release/plan.mjs';

test('a source merge after a published release does not build: the bump follows it',()=>{
  const plan=planPush({bumpEnabled:true,message:'MAX-09: Matrix implementation (#240)',versionTagged:true});
  assert.equal(plan.build,false); assert.match(plan.reason,/bump follows/);
});
test('the version bump commit is the packaging build',()=>{
  assert.equal(planPush({bumpEnabled:true,message:'[beta-version] 0.5.14: Something',versionTagged:false}).build,true);
  assert.equal(planPush({bumpEnabled:true,message:'[beta-version] 0.5.14: Something',versionTagged:true}).build,true);
});
test('a source merge while VERSION is unreleased builds nothing either: its bump supersedes that version',()=>{
  const plan=planPush({bumpEnabled:true,message:'Fix something',versionTagged:false});
  assert.equal(plan.build,false); assert.match(plan.reason,/supersedes/);
});
test('when bumps are off, every source push builds as before',()=>{
  assert.equal(planPush({bumpEnabled:false,message:'Fix something',versionTagged:true}).build,true);
});
test('anything uncertain builds rather than risking a stalled release',()=>{
  assert.equal(planPush({bumpEnabled:true,message:'Fix something',versionTagged:null}).build,true);
  assert.equal(planPush({bumpEnabled:true,message:undefined,versionTagged:undefined}).build,true);
});

test('the bump workflow and the build workflow watch exactly the same push paths',async()=>{
  const fs=await import('node:fs');
  const pushPaths=file=>{
    const text=fs.readFileSync(new URL(`../.github/workflows/${file}`,import.meta.url),'utf8').replace(/\r\n/g,'\n');
    const push=/^  push:\n    branches: \[main\]\n    paths:\n((?:      - .+\n)+)/m.exec(text);
    assert.ok(push,`${file} has a push paths list`);
    return push[1].trim().split('\n').map(l=>l.replace(/^\s*-\s*/,'').replace(/^'|'$/g,''));
  };
  assert.deepEqual(pushPaths('auto-beta-version.yml'),pushPaths('build.yml'));
});
test('build.yml asks the planner on a push, and never builds a plain merge by default',async()=>{
  const fs=await import('node:fs');
  const text=fs.readFileSync(new URL('../.github/workflows/build.yml',import.meta.url),'utf8');
  assert.match(text,/node scripts\/release\/plan\.mjs/);
  assert.match(text,/BUMP_ENABLED: \$\{\{ vars\.AUTO_BETA_VERSION_ENABLED \}\}/);
});
test('every pull request starts a Build run (docs-only ones build nothing), so Matrix and the merge gate always have a result',async()=>{
  const {readFile}=await import('node:fs/promises');
  const text=(await readFile(new URL('../.github/workflows/build.yml',import.meta.url),'utf8')).replace(/\r\n/g,'\n');
  assert.match(text,/^  pull_request:\n(?!    paths:)/m,'pull_request must not have a paths filter');
});
