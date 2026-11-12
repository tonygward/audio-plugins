import test from 'node:test';
import assert from 'node:assert/strict';
import {releaseVersion,releaseNotes,validateRun,validateInstallerFiles,isSkippedBuild,supersededByLaterVersion} from '../scripts/release/prepare.mjs';
const sha='a'.repeat(40),version='1.4.0';
const run={head_sha:sha,head_branch:'main',event:'push',status:'completed',conclusion:'success',run_attempt:1,repository:{full_name:'tonygward/audio-plugins'},head_repository:{full_name:'tonygward/audio-plugins'},path:'.github/workflows/build.yml@refs/heads/main'};
const jobs=['One version number','Songsmith Plugins installer - Windows','Songsmith Plugins installer - macOS',...['EQ','Analog Compressor','Maximiser'].flatMap(n=>[`${n} - Windows (VS, x64)`,`${n} - macOS (Xcode, universal)`])].map(name=>({name,status:'completed',conclusion:'success'}));
test('canonical version only',()=>{assert.equal(releaseVersion('1.4.0\n'),version);for(const v of ['01.4.0','1.4.0-rc','1.4.0\nX','9007199254740999.0.0'])assert.throws(()=>releaseVersion(v));});
test('exactly one nonempty changelog section',()=>{assert.equal(releaseNotes('## 1.4.0\n- New feature\n\n## 1.3.0\n- Old',version),'- New feature');for(const s of ['## 1.3.0\n- Old','## 1.4.0\n\n## 1.3.0\n- Old','## 1.4.0\n- A\n## 1.4.0\n- B'])assert.throws(()=>releaseNotes(s,version));});
test('successful exact full main Build and first attempt only',()=>{assert.doesNotThrow(()=>validateRun(run,jobs,sha));for(const p of [{event:'pull_request'},{head_branch:'feature'},{conclusion:'failure'},{run_attempt:2},{head_sha:'b'.repeat(40)},{path:'.github/workflows/other.yml@main'}])assert.throws(()=>validateRun({...run,...p},jobs,sha));for(const p of [{conclusion:'failure'},{conclusion:'skipped'},{status:'in_progress'}])assert.throws(()=>validateRun(run,[{...jobs[0],...p},...jobs.slice(1)],sha));assert.throws(()=>validateRun(run,jobs.slice(0,-1),sha));});
test('require exactly two correctly named files',()=>{const files=[`tmp/Songsmith-Plugins-${version}-win64-setup.exe`,`tmp/Songsmith-Plugins-${version}-macOS.pkg`];assert.equal(validateInstallerFiles(files,version),true);assert.throws(()=>validateInstallerFiles(files.slice(0,1),version));assert.throws(()=>validateInstallerFiles([...files,'bad.zip'],version));assert.throws(()=>validateInstallerFiles([files[0],files[0]],version));});

import {isBuildPath,onlyNonBuildChanges,currentMainFor} from '../scripts/release/prepare.mjs';
test('documentation-only commits do not make a build stale',()=>{
  assert.equal(onlyNonBuildChanges(['eq/prompts/README.md','eq/prompts/done/38-x.md','CHANGELOG.md','docs/style-guide.md','eq/docs/a.png']),true);
  for(const p of ['eq/Source/Foo.cpp','shared/Source/Bar.h','VERSION','.github/workflows/build.yml','installer/x.iss'])
    assert.equal(isBuildPath(p),true,p);
  assert.equal(onlyNonBuildChanges(['README.md','eq/Source/Foo.cpp']),false);
});
test('currentMainFor accepts docs-only drift and rejects source drift',async()=>{
  const sha='a'.repeat(40),tip='b'.repeat(40);
  const make=(status,names)=>async path=>path==='/branches/main'?{commit:{sha:tip}}:{status,files:names.map(filename=>({filename}))};
  assert.equal(await currentMainFor(async()=>({commit:{sha}}),sha),sha);
  assert.equal(await currentMainFor(make('ahead',['eq/prompts/README.md']),sha),tip);
  await assert.rejects(currentMainFor(make('ahead',['maximiser/Source/X.cpp']),sha),/no longer current main/);
  await assert.rejects(currentMainFor(make('diverged',['README.md']),sha),/no longer current main/);
});

test('a source merge that deliberately built nothing is recognised, and a real packaging build is not',()=>{
  const win='Songsmith Plugins installer - Windows',mac='Songsmith Plugins installer - macOS';
  assert.equal(isSkippedBuild([{name:'One version number',conclusion:'success'},{name:win,conclusion:'skipped'},{name:mac,conclusion:'skipped'}]),true);
  assert.equal(isSkippedBuild(jobs),false,'a full successful build is not skipped');
  assert.equal(isSkippedBuild([{name:win,conclusion:'skipped'},{name:mac,conclusion:'success'}]),false,'only one installer skipped');
  assert.equal(isSkippedBuild([{name:win,conclusion:'failure'},{name:mac,conclusion:'failure'}]),false,'a failure is never treated as intentional');
  assert.equal(isSkippedBuild([]),false,'no jobs at all');
});
test('a build superseded by a later bump is a quiet no-op; anything else superseding it is still a failure',()=>{
  assert.equal(supersededByLaterVersion('0.5.15','0.5.16\n'),true);
  assert.equal(supersededByLaterVersion('0.5.15','0.6.0'),true);
  assert.equal(supersededByLaterVersion('0.5.15','0.5.15'),false,'same version: a later source commit without a bump');
  assert.equal(supersededByLaterVersion('0.5.15','0.5.14'),false);
  assert.throws(()=>supersededByLaterVersion('0.5.15','garbage'));
});
