import {readFile,writeFile} from 'node:fs/promises';
import {execFileSync} from 'node:child_process';
import {pathToFileURL} from 'node:url';
import {releaseVersion,currentMainFor} from './prepare.mjs';

const repository='tonygward/audio-plugins';
const assert=(ok,message)=>{if(!ok)throw Error(message);};
export function nextBetaVersion(current) {
  const [major,minor,patch]=releaseVersion(current).split('.').map(Number);
  assert(patch<Number.MAX_SAFE_INTEGER,'Beta patch counter overflow');
  return [major,minor,patch+1].join('.');
}
// Release notes are written for musicians: features and fixes in plain words,
// never commit hashes or pull request titles. Authors add them under
// "## Unreleased"; the bump moves that text into the new version's section.
export const QUIET_RELEASE_NOTE='Behind-the-scenes improvements and fixes. There is nothing you need to do.';
export function betaChangelog(changelog,version,sha) {
  assert(/^([a-f0-9]{40})$/.test(sha),'Invalid source revision');
  assert(/^\d+\.\d+\.\d+$/.test(version),'Invalid beta version');
  assert(!new RegExp('^## '+version.replaceAll('.','\\.')+'\\s*$','m').test(changelog),'Changelog version already exists');
  let rest=changelog,notes=QUIET_RELEASE_NOTE;
  const unreleased=/^## Unreleased[ \t]*\r?\n([\s\S]*?)(?=^## |(?![\s\S]))/m.exec(changelog);
  if(unreleased) {
    rest=changelog.slice(0,unreleased.index)+changelog.slice(unreleased.index+unreleased[0].length);
    if(/[\p{L}\p{N}]/u.test(unreleased[1]))notes=unreleased[1].trim();
  }
  const pos=rest.indexOf('## ');
  assert(pos!==-1,'Changelog missing version sections');
  return rest.slice(0,pos)+`## ${version}\n\n${notes}\n\n`+rest.slice(pos);
}
// Two builds per release: the bump now follows the merge directly, so there is no main build to vouch for it. What vouches for the change is the
// pull request build: some merged pull request for this commit must have had a successful pull_request Build run at exactly the head it merged.
// (The packaging build of the bump commit then checks the combined result before anything is published.)
export function pullRequestBuilt({prs,runs}) {
  if(!Array.isArray(prs)||!Array.isArray(runs))return false;
  return prs.some(pr=>pr?.merged_at&&pr?.base?.ref==='main'&&/^[a-f0-9]{40}$/.test(pr?.head?.sha||'')&&
    runs.some(run=>run?.head_sha===pr.head.sha&&run?.event==='pull_request'&&run?.status==='completed'&&run?.conclusion==='success'&&
      run?.path?.split('@')[0]==='.github/workflows/build.yml'&&run?.repository?.full_name===repository&&run?.head_repository?.full_name===repository));
}
// A pull request build that is still queued or running at merge time: the bump waits for it instead of giving up.
export function pullRequestBuilding({prs,runs}) {
  if(!Array.isArray(prs)||!Array.isArray(runs))return false;
  return prs.some(pr=>pr?.merged_at&&pr?.base?.ref==='main'&&/^[a-f0-9]{40}$/.test(pr?.head?.sha||'')&&
    runs.some(run=>run?.head_sha===pr.head.sha&&run?.event==='pull_request'&&run?.status!=='completed'&&
      run?.path?.split('@')[0]==='.github/workflows/build.yml'&&run?.repository?.full_name===repository&&run?.head_repository?.full_name===repository));
}
export function shouldBump({message,tagFound}) {
  return tagFound && !String(message).startsWith('[beta-version]');
}
const git=(args,options={})=>execFileSync('git',args,{encoding:'utf8',stdio:['pipe','pipe','pipe'],...options}).trim();
async function main() {
  const e=process.env,sha=e.BUILD_SHA;
  assert(/^[a-f0-9]{40}$/.test(sha||''),'The merged commit is required');
  assert(e.GH_TOKEN,'RELEASE_PUBLISH_TOKEN required for beta version bumps');
  const api=async path=>{
    const response=await fetch(`https://api.github.com/repos/${repository}${path}`,{
      headers:{Authorization:`Bearer ${e.READ_TOKEN||e.GH_TOKEN}`,Accept:'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'}});
    if(!response.ok)throw Error(`GitHub ${response.status}: ${path.split('?')[0]}`);
    return response.json();
  };
  const prs=await api(`/commits/${sha}/pulls`);
  assert(Array.isArray(prs),'Pull requests response malformed');
  const lookup=async()=>{
    const runs=[];
    for(const pr of prs.filter(p=>p?.merged_at&&/^[a-f0-9]{40}$/.test(p?.head?.sha||''))) {
      const found=await api(`/actions/workflows/build.yml/runs?head_sha=${pr.head.sha}&event=pull_request&per_page=30`);
      assert(Array.isArray(found.workflow_runs),'Build runs response malformed');
      runs.push(...found.workflow_runs);
    }
    return runs;
  };
  // Merged before its pull request build finished (a hand merge): wait for it, up to 30 minutes, rather than failing and needing a re-run.
  let runs=await lookup();
  for(let waited=0;!pullRequestBuilt({prs,runs})&&pullRequestBuilding({prs,runs})&&waited<30;waited++) {
    console.log('The pull request build is still running; waiting a minute before deciding.');
    await new Promise(resolve=>setTimeout(resolve,60000));
    runs=await lookup();
  }
  assert(pullRequestBuilt({prs,runs}),'No successful pull request Build at the exact head of a merged pull request for this commit; not bumping');
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
  console.log(`New beta checkpoint ${version} pushed; its packaging build runs next and the release is published from it`);
}
if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href)
  main().catch(e=>{console.error(`Automatic beta version bump stopped: ${e.message}`);process.exitCode=1;});
