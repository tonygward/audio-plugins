// GitHub-backed io for pickup.mjs and delivery.mjs. Every call goes through an injected `gh(args)`
// that returns stdout, so tests need no network and the caller decides which token it holds.
// Private storage, website and download checks come from verifiers.mjs. Any that is not supplied
// throws, and delivery verification fails closed (the issue stays open, the card stays in Doing).
// Card moves use the GraphQL mutation: `gh project item-edit` needs the read:org scope, which the board token does not have.
const MOVE = 'mutation($p:ID!,$i:ID!,$f:ID!,$o:String!){updateProjectV2ItemFieldValue(input:{projectId:$p,itemId:$i,fieldId:$f,value:{singleSelectOptionId:$o}}){projectV2Item{id}}}';

export function githubIo(gh, { projectId, statusFieldId, doneOptionId, doingOptionId, blockedOptionId, maxReleases = 30, verifiers = {} } = {}) {
  const json = async args => JSON.parse(await gh(args));
  const move = (itemId, optionId) => gh(['api', 'graphql', '-f', `query=${MOVE}`, '-f', `p=${projectId}`, '-f', `i=${itemId}`, '-f', `f=${statusFieldId}`, '-f', `o=${optionId}`]);
  const notConfigured = what => async () => { throw new Error(`${what} verifier is not configured`); };
  return {
    pull: async (repository, number) => {
      const pull = await json(['pr', 'view', String(number), '--repo', repository, '--json', 'state,mergeCommit,url']);
      return { state: pull.state, mergeSha: pull.mergeCommit?.oid ?? null, url: pull.url };
    },
    // Earliest release that contains the merge commit. The beta release is built from the
    // automatic version-bump commit that follows the merge, so containment is the test.
    release: async (mergeSha, repository, onlyTag = null) => {
      const list = await json(['release', 'list', '--repo', repository, '--limit', String(maxReleases), '--json', 'tagName,publishedAt']);
      for (const { tagName } of [...list].filter(r => !onlyTag || r.tagName === onlyTag).sort((a, b) => Date.parse(a.publishedAt) - Date.parse(b.publishedAt))) {
        const release = await json(['release', 'view', tagName, '--repo', repository, '--json', 'tagName,url,targetCommitish,assets']);
        const status = (await gh(['api', `repos/${repository}/compare/${mergeSha}...${release.targetCommitish}`, '--jq', '.status'])).trim();
        if (['ahead', 'identical'].includes(status)) {
          return { tag: release.tagName, url: release.url, sourceSha: release.targetCommitish, includesMerge: true,
            version: release.tagName.replace(/^v/, ''), assets: release.assets.map(a => ({ name: a.name, size: a.size })) };
        }
      }
      return null;
    },
    // Head commit and check roll-up of an open pull request, for recovery after a restart.
    prChecks: async (repository, number) => {
      const pull = await json(['pr', 'view', String(number), '--repo', repository, '--json', 'headRefOid,statusCheckRollup']);
      return { headSha: pull.headRefOid, checks: pull.statusCheckRollup ?? [] };
    },
    issueBody: async (repository, issue) => (await json(['issue', 'view', String(issue), '--repo', repository, '--json', 'body'])).body ?? '',
    hasComment: async (repository, issue, marker) => {
      const data = await json(['issue', 'view', String(issue), '--repo', repository, '--json', 'comments']);
      return data.comments.some(comment => comment.body.includes(marker));
    },
    comment: async (repository, issue, body) => { await gh(['issue', 'comment', String(issue), '--repo', repository, '--body', body]); },
    closeIssue: async (repository, issue) => { await gh(['issue', 'close', String(issue), '--repo', repository, '--reason', 'completed']); },
    moveToDoing: async itemId => {
      if (!projectId || !statusFieldId || !doingOptionId) throw new Error('Board Doing field is not configured');
      await move(itemId, doingOptionId);
    },
    moveToBlocked: async itemId => {
      if (!projectId || !statusFieldId || !blockedOptionId) throw new Error('Board Blocked field is not configured');
      await move(itemId, blockedOptionId);
    },
    setDone: async itemId => {
      if (!projectId || !statusFieldId || !doneOptionId) throw new Error('Board Done field is not configured');
      await move(itemId, doneOptionId);
    },
    r2: verifiers.r2 ?? notConfigured('Private storage'),
    site: verifiers.site ?? notConfigured('Beta website'),
    downloads: verifiers.downloads ?? notConfigured('Authenticated download')
  };
}
