// Two builds per release: 1) the pull request build (changed plugins only), 2) one packaging build of everything.
// A push to main therefore does NOT build by default: the version bump follows the merge immediately (auto-beta-version.yml), and the
// bump commit is the packaging build the release is made from. This decides, for a push to main, whether the push is that packaging build.
import {readFile,appendFile} from 'node:fs/promises';
import {pathToFileURL} from 'node:url';

export const BUMP_PREFIX='[beta-version]';

/**
 * bumpEnabled:   AUTO_BETA_VERSION_ENABLED. When off, nothing bumps, so every source push is a full build (the original behaviour).
 * message:       the pushed commit's message.
 * versionTagged: true when the current VERSION already has its release tag; null when that could not be established.
 * Returns { build, reason }. Anything uncertain builds: an unneeded build costs time, a missing one could stall a release.
 */
export function planPush({bumpEnabled,message,versionTagged}) {
  if(!bumpEnabled)return {build:true,reason:'Automatic version bumps are off, so every source push is a full build'};
  if(String(message??'').startsWith(BUMP_PREFIX))return {build:true,reason:'This is the version bump commit: the packaging build the release is made from'};
  if(versionTagged===null||versionTagged===undefined)return {build:true,reason:'Could not establish whether VERSION is released, so building to be safe'};
  if(versionTagged===false)return {build:true,reason:'The current VERSION is not released yet, so this push is the packaging build for it'};
  return {build:false,reason:'A source merge: the version bump follows it, and the bump commit is the packaging build'};
}

async function main() {
  const e=process.env,version=(await readFile('VERSION','utf8')).trim();
  let versionTagged=null;
  if(/^\d+\.\d+\.\d+$/.test(version)&&e.GITHUB_TOKEN&&e.GITHUB_REPOSITORY) {
    try {
      const response=await fetch(`https://api.github.com/repos/${e.GITHUB_REPOSITORY}/git/ref/tags/v${version}`,{headers:{Authorization:`Bearer ${e.GITHUB_TOKEN}`,Accept:'application/vnd.github+json','X-GitHub-Api-Version':'2022-11-28'}});
      if(response.status===200)versionTagged=true; else if(response.status===404)versionTagged=false;
    } catch { /* leave null: build */ }
  }
  const plan=planPush({bumpEnabled:e.BUMP_ENABLED==='true',message:e.COMMIT_MESSAGE,versionTagged});
  console.log(`Build this push: ${plan.build} (${plan.reason}; VERSION ${version}, tagged: ${versionTagged})`);
  if(e.GITHUB_OUTPUT)await appendFile(e.GITHUB_OUTPUT,`build=${plan.build}\n`);
}
if(process.argv[1]&&import.meta.url===pathToFileURL(process.argv[1]).href)
  main().catch(error=>{console.error(`Build plan failed: ${error.message}`);process.exitCode=1;});
