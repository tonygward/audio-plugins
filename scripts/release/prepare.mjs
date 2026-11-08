import {readFile,writeFile} from 'node:fs/promises';
import {pathToFileURL} from 'node:url';

const repo='tonygward/audio-plugins';
const expectedJobs=['One version number','Songsmith Plugins installer - Windows','Songsmith Plugins installer - macOS',
  ...['EQ','Analog Compressor','Maximiser'].flatMap(n=>[`${n} - Windows (VS, x64)`,`${n} - macOS (Xcode, universal)`])];

export function releaseVersion(contents) {
  const v=contents.trim();
  if(!/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(v)||!v.split('.').every(n=>Number.isSafeInteger(Number(n))))throw Error('VERSION must be canonical X.Y.Z');
  return v;
}
// Mirrors the path filter in .github/workflows/build.yml: a commit that touches
// only documentation, prompts, screenshots or ignore files never builds, so it
// must not make an earlier successful Build look stale.
const buildRoots=['eq/','analog-compressor/','maximiser/','shared/','installer/','.github/workflows/'];
const buildFiles=['VERSION','.gitattributes'];
export function isBuildPath(path) {
  const p=String(path).replaceAll('\\','/');
  if(!(buildFiles.includes(p)||buildRoots.some(r=>p.startsWith(r))))return false;
  const parts=p.split('/'),name=parts.at(-1);
  return !(p.endsWith('.md')||p.endsWith('.png')||name==='.gitignore'||parts.slice(0,-1).some(d=>d==='prompts'||d==='docs'));
}
export function onlyNonBuildChanges(files) {
  return Array.isArray(files)&&files.every(f=>!isBuildPath(f));
}
// Returns the current main SHA when main is the built commit or only
// documentation landed after it; throws when a later source commit exists.
export async function currentMainFor(api,sha) {
  const tip=(await api('/branches/main')).commit?.sha;
  if(tip===sha)return tip;
  const cmp=await api(`/compare/${sha}...${tip}`);
  const files=(cmp.files||[]).map(f=>f.filename);
  if(cmp.status==='ahead'&&files.length<300&&onlyNonBuildChanges(files))return tip;
  throw Error('Build is no longer current main; a later source commit superseded it');
}
export function releaseNotes(changelog,version) {
  const headings=[...changelog.matchAll(/^## (\d+\.\d+\.\d+)\s*$/gm)];
  const matches=headings.filter(m=>m[1]===version);
  if(matches.length!==1)throw Error(`CHANGELOG must have exactly one ## ${version} section`);
  const m=matches[0],next=headings.find(h=>h.index>m.index);
  const notes=changelog.slice(m.index+m[0].length,next?.index??changelog.length).trim();
  if(!/[\p{L}\p{N}]/u.test(notes.replace(/^#+.*$/gm,'')))throw Error('Release notes are empty');
  return notes;
}
export function validateRun(run,jobs,sha) {
  if(!/^[a-f0-9]{40}$/.test(sha)||run?.head_sha!==sha||run?.head_branch!=='main'||
     run?.event!=='push'||run?.status!=='completed'||run?.conclusion!=='success'||
     run?.run_attempt!==1||run?.repository?.full_name!==repo||
     run?.head_repository?.full_name!==repo||
     run?.path?.split('@')[0]!=='.github/workflows/build.yml')
    throw Error('Not a successful first-attempt main push Build workflow at the exact commit');
  for(const name of expectedJobs) {
    const jobsNamed=jobs.filter(job=>job.name===name);
    if(jobsNamed.length!==1||jobsNamed[0].status!=='completed'||jobsNamed[0].conclusion!=='success')
      throw Error(`Full Build job missing, skipped or failed: ${name}`);
  }
}
export function validateInstallerFiles(files,version) {
  const expected=[`Songsmith-Plugins-${version}-win64-setup.exe`,`Songsmith-Plugins-${version}-macOS.pkg`].sort();
  if(files.length!==2||JSON.stringify(files.map(p=>p.split('/').at(-1)).sort())!==JSON.stringify(expected))
    throw Error('Exactly two correctly named suite installers required');
  return true;
}
async function main() {
  const env=process.env,token=env.GITHUB_TOKEN,runId=env.BUILD_RUN_ID;
  if(!token||!/^\d+$/.test(runId||''))throw Error('Authenticated Build workflow run ID required');
  const api=async path=>{
    const response=await fetch(`https://api.github.com/repos/${repo}${path}`,{headers:{
      Authorization:`Bearer ${token}`,Accept:'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'
    }});
    if(!response.ok)throw Error(`GitHub API ${response.status}: ${path.split('?')[0]}`);
    return response.json();
  };
  const run=await api(`/actions/runs/${runId}`),jobs=[];
  for(let page=1;page<=10;page++){
    const batch=(await api(`/actions/runs/${runId}/jobs?filter=latest&per_page=100&page=${page}`)).jobs;
    if(!Array.isArray(batch))throw Error('Missing Build job results');
    jobs.push(...batch);
    if(batch.length<100)break;
    if(page===10)throw Error('Too many Build jobs');
  }
  validateRun(run,jobs,env.BUILD_SHA);
  const version=releaseVersion(await readFile('VERSION','utf8'));
  const notes=releaseNotes(await readFile('CHANGELOG.md','utf8'),version);
  const lookup=await fetch(`https://api.github.com/repos/${repo}/git/ref/tags/v${version}`,{headers:{
    Authorization:`Bearer ${token}`,Accept:'application/vnd.github+json'
  }});
  if(lookup.status===200){
    console.log(`v${version} already tagged; no new release`);
    await writeFile(env.GITHUB_OUTPUT,'publish=false\n',{flag:'a'});
    return;
  }
  if(lookup.status!==404)throw Error(`GitHub tag lookup returned ${lookup.status}`);
  // Checked after the tag lookup so a build superseded by its own version
  // bump commit is a quiet no-op rather than a red failure.
  await currentMainFor(api,env.BUILD_SHA);
  await writeFile('release-notes.md',notes+'\n');
  await writeFile(env.GITHUB_OUTPUT,`publish=true\nversion=${version}\n`,{flag:'a'});
  console.log(`Ready for release v${version} from verified main Build run ${runId}`);
}
if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href)
  main().catch(e=>{console.error(`Release preflight failed: ${e.message}`);process.exitCode=1;});
