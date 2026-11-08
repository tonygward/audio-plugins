import {readFile,writeFile} from 'node:fs/promises';
import {execFileSync} from 'node:child_process';
import {pathToFileURL} from 'node:url';
import {releaseVersion,validateRun,currentMainFor} from './prepare.mjs';

const repository='tonygward/audio-plugins';
const assert=(ok,message)=>{if(!ok)throw Error(message);};
export function nextBetaVersion(current) {
  const [major,minor,patch]=releaseVersion(current).split('.').map(Number);
  assert(patch<Number.MAX_SAFE_INTEGER,'Beta patch counter overflow');
  return [major,minor,patch+1].join('.');
}
export function betaChangelog(changelog,version,sha,message) {
  assert(/^([a-f0-9]{40})$/.test(sha),'Invalid source revision');
  assert(/^\d+\.\d+\.\d+$/.test(version),'Invalid beta version');
  const summary=String(message||'Development updates').split('\n')[0]
    .replace(/[\r\n\t]/g,' ').replace(/^\[beta-version\]\s*/i,'').trim().slice(0,140);
  const note=summary || 'Development updates';
  assert(!new RegExp('^## '+version.replaceAll('.','\\.')+'\\s*$','m').test(changelog),'Changelog version already exists');
  const pos=changelog.indexOf('## ');
  assert(pos!==-1,'Changelog missing version sections');
  return changelog.slice(0,pos)+`## ${version}\n\nPrivate beta development update from \`${sha.slice(0,12)}\`.\n\n- ${note}\n\n`+changelog.slice(pos);
}
export function shouldBump({message,tagFound}) {
  return tagFound && !String(message).startsWith('[beta-version]');
}
const git=(args,options={})=>execFileSync('git',args,{encoding:'utf8',stdio:['pipe','pipe','pipe'],...options}).trim();
async function main() {
  const e=process.env,runId=e.BUILD_RUN_ID,sha=e.BUILD_SHA;
  assert(/^\d+$/.test(runId||'')&&/^[a-f0-9]{40}$/.test(sha||''),'Build provenance required');
  assert(e.GH_TOKEN,'RELEASE_PUBLISH_TOKEN required for beta version bumps');
  const api=async path=>{
    const response=await fetch(`https://api.github.com/repos/${repository}${path}`,{
      headers:{Authorization:`Bearer ${e.GH_TOKEN}`,Accept:'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'}});
    if(!response.ok)throw Error(`GitHub ${response.status}: ${path.split('?')[0]}`);
    return response.json();
  };
  const run=await api(`/actions/runs/${runId}`),jobs=[];
  for(let page=1;page<=10;page++) {
    const part=(await api(`/actions/runs/${runId}/jobs?filter=latest&per_page=100&page=${page}`)).jobs;
    assert(Array.isArray(part),'Build jobs response malformed');
    jobs.push(...part);
    if(part.length<100)break;
    assert(page<10,'Build job pagination limit');
  }
  validateRun(run,jobs,sha);
  const tip=await currentMainFor(api,sha);
  const childEnv={...e,GIT_TERMINAL_PROMPT:'0',
    GIT_CONFIG_COUNT:'2',GIT_CONFIG_KEY_0:'credential.helper',GIT_CONFIG_VALUE_0:'',
    GIT_CONFIG_KEY_1:'credential.helper',GIT_CONFIG_VALUE_1:'!gh auth git-credential'};
  if(tip!==sha) {
    // Only documentation landed after the build: bump on top of it.
    git(['fetch','origin','main'],{env:childEnv});
    assert(git(['rev-parse','FETCH_HEAD'])===tip,'main moved while preparing the bump');
    git(['checkout','--detach',tip]);
  }
  const commit=await api(`/commits/${sha}`);
  const message=commit.commit?.message??'';
  const current=releaseVersion(await readFile('VERSION','utf8'));
  const tagResponse=await fetch(`https://api.github.com/repos/${repository}/git/ref/tags/v${current}`,{
    headers:{Authorization:`Bearer ${e.GH_TOKEN}`,Accept:'application/vnd.github+json'}});
  assert(tagResponse.status===200||tagResponse.status===404,'Unable to establish current release tag');
  if(!shouldBump({message,tagFound:tagResponse.status===200})) {
    console.log('Version already new, or source commit is an auto-version commit; no bump.');
    return;
  }
  const version=nextBetaVersion(current);
  const nextTag=await fetch(`https://api.github.com/repos/${repository}/git/ref/tags/v${version}`,{
    headers:{Authorization:`Bearer ${e.GH_TOKEN}`,Accept:'application/vnd.github+json'}});
  assert(nextTag.status===404,'Next beta tag already exists or lookup failed; refusing version reuse');
  const changelog=await readFile('CHANGELOG.md','utf8');
  await writeFile('VERSION',version+'\n');
  await writeFile('CHANGELOG.md',betaChangelog(changelog,version,sha,message));
  git(['config','user.name','Songsmith beta release bot']);
  git(['config','user.email','release-bot@users.noreply.github.com']);
  git(['add','--','VERSION','CHANGELOG.md']);
  assert(git(['diff','--cached','--name-only']).split('\n').sort().join(',')==='CHANGELOG.md,VERSION','Unexpected staged files');
  git(['commit','-m',`[beta-version] ${version}: ${message.split('\n')[0].slice(0,100)}`]);
  // Protected from force pushing: if main advanced, non-fast-forward push fails.
  git(['push','origin','HEAD:refs/heads/main'],{env:childEnv});
  console.log(`New beta checkpoint ${version} pushed; full Build will run before release publication`);
}
if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href)
  main().catch(e=>{console.error(`Automatic beta version bump stopped: ${e.message}`);process.exitCode=1;});
