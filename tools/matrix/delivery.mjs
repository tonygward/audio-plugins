// Delivery lifecycle (AUTO-11 requirements 7-9). A merged pull request is not delivery.
// Order is fixed: verify beta delivery -> final issue comment -> close issue -> board Done.
// Each stage is saved, so a crash resumes at the failed stage. A failed final comment blocks
// Done. Merged work is never recoded while publication is pending. Verification reads only
// through injected functions, so this module holds no credentials and writes nothing itself.
import path from 'node:path';
import { createHash } from 'node:crypto';
import { atomic, lockedAsync, readState, record } from './state.mjs';
import { releaseAssets } from './policy.mjs';

export const STAGES = ['pending', 'verified', 'commented', 'closed', 'done'];
export const finalMarker = task => `<!-- matrix-final:${task} -->`;
export const blockerMarker = (task, hash) => `<!-- matrix-blocker:${task}:${hash} -->`;

/**
 * io.pull(repository, number)       -> { state, mergeSha, url }
 * io.release(mergeSha, repository) -> { tag, url, sourceSha, includesMerge, version, assets: [{ name, size }] } | null  (the release is built from the automatic version-bump commit after the merge, so it must contain the merge commit, not equal it)
 * io.r2(version)                    -> [{ name, size }]            private storage objects
 * io.site()                         -> { version, changelogVersions: [], url }   deployed beta website
 * io.promptArchived(file)           -> true when the task's canonical prompt is in a done/ folder on main
 * Downloads are not checked: the final comment publishes the beta website link instead.
 */
export async function verifyDelivery(delivery, io) {
  const missing = [], evidence = {};
  try {
    const pull = await io.pull(delivery.repository, delivery.pull);
    if (pull?.state !== 'MERGED' || !pull.mergeSha) return { verified: false, missing: ['Pull request is not merged'], evidence };
    evidence.pull = { number: delivery.pull, url: pull.url, mergeSha: pull.mergeSha };
    const release = await io.release(pull.mergeSha, delivery.repository, delivery.releaseTag);
    if (!release) return { verified: false, missing: ['No beta release containing the merge commit yet'], evidence };
    if (release.includesMerge !== true) missing.push('Release does not include the merge commit');
    let names;
    try { names = releaseAssets(release.version, release.assets.map(a => a.name)); }
    catch (error) { names = null; missing.push(`Release assets: ${error.message}`); }
    evidence.release = { tag: release.tag, url: release.url, version: release.version, assets: release.assets.map(a => a.name), firstShipped: delivery.firstShipped ?? null };
    if (names) {
      const sizes = new Map(release.assets.map(a => [a.name, a.size]));
      // Each check runs on its own, so one unavailable verifier cannot hide the others' findings.
      const attempt = async (label, fn) => { try { return await fn(); } catch (error) { missing.push(`${label} could not be verified: ${String(error.message).slice(0, 120)}`); return null; } };
      const stored = await attempt('Private storage', async () => new Map((await io.r2(release.version)).map(o => [o.name, o.size])));
      if (stored) for (const name of names) if (stored.get(name) !== sizes.get(name)) missing.push(`Private storage object missing or different size: ${name}`);
      const site = await attempt('Beta website', () => io.site());
      if (site) {
        if (site.version !== release.version) missing.push(`Beta website shows ${site.version ?? 'no version'}, not ${release.version}`);
        if (!site.changelogVersions?.includes(release.version)) missing.push('Beta website changelog does not list this version');
        if (site.deployed === false) missing.push('Beta website deployment is not confirmed');
      }
      evidence.deployment = { websiteVersion: site?.version ?? null, websiteUrl: site?.url ?? null, privateStorage: names };
    }
    // Every finished task keeps a canonical prompt, including one generated from its issue.
    if (delivery.prompt && typeof io.promptArchived === 'function') {
      try { if (!(await io.promptArchived(delivery.prompt))) missing.push(`Canonical prompt is not archived in done/ yet: ${delivery.prompt}`); }
      catch (error) { missing.push(`Canonical prompt could not be checked: ${String(error.message).slice(0, 120)}`); }
    }
  } catch (error) {
    return { verified: false, missing: [`Verification could not complete: ${String(error.message).slice(0, 160)}`], evidence };
  }
  return { verified: missing.length === 0, missing, evidence };
}

export function finalComment({ taskId, job, evidence }) {
  const { pull, release, deployment } = evidence;
  return [finalMarker(taskId), `**${taskId} is delivered to the beta.**`, '',
    `- Pull request: ${pull.url ?? `#${pull.number}`} (merge commit \`${pull.mergeSha.slice(0, 12)}\`)`,
    `- Matrix job: \`${job}\``,
    `- Beta version: ${release.version} (${release.url ?? release.tag})${release.firstShipped ? `; this change first shipped in ${release.firstShipped} and is included in the live beta` : ''}`,
    `- Installers: ${release.assets.join(' and ')}`,
    `- Private storage matches the release; beta website shows ${deployment.websiteVersion} with this version in its changelog`,
    `- Download: ${deployment.websiteUrl ?? 'https://beta.songsmith.uk'}`, '',
    'Closing this issue and moving the card to Done.'].join('\n');
}

/**
 * Move one merged task through delivery. Safe to call repeatedly; each call resumes.
 * io adds: hasComment(repo, issue, marker), comment(repo, issue, body), closeIssue(repo, issue), setDone(itemId)
 */
export async function advanceDelivery(stateDirectory, taskId, io, now = () => new Date().toISOString()) {
  return lockedAsync(stateDirectory, async () => {
    const state = readState(stateDirectory);
    const delivery = state.deliveries?.[taskId];
    if (!delivery) return { stage: null, blocked: 'No delivery recorded for this task' };
    const save = () => atomic(path.join(stateDirectory, 'state.json'), state);
    const slug = delivery.issueRepository;
    const stop = (reason, event) => { record(state, { event, task: taskId, reason: reason.slice(0, 200) }); save(); return { stage: delivery.stage, blocked: reason }; };

    if (delivery.stage === 'pending') {
      const result = await verifyDelivery(delivery, io);
      if (!result.verified) {
        // One blocker comment per distinct set of reasons; no heartbeat spam.
        const hash = createHash('sha256').update(result.missing.join('|')).digest('hex').slice(0, 12);
        if (delivery.blockerHash !== hash) {
          try {
            if (!(await io.hasComment(slug, delivery.issue, blockerMarker(taskId, hash)))) {
              await io.comment(slug, delivery.issue, [blockerMarker(taskId, hash), `**${taskId}: merged, waiting for beta delivery.**`, '', ...result.missing.map(m => `- ${m}`), '',
                'The implementation is merged and will not be coded again. The issue stays open and the card stays in Doing until delivery is verified.'].join('\n'));
            }
            delivery.blockerHash = hash;
          } catch { /* comment retried next run; never blocks recording the blocker */ }
        }
        return stop(result.missing.join('; '), 'delivery-blocked');
      }
      Object.assign(delivery, { stage: 'verified', evidence: result.evidence, verifiedAt: now() });
      record(state, { event: 'delivery-verified', task: taskId }); save();
    }
    if (delivery.stage === 'verified') {
      try {
        if (!(await io.hasComment(slug, delivery.issue, finalMarker(taskId)))) await io.comment(slug, delivery.issue, finalComment({ taskId, job: delivery.job, evidence: delivery.evidence }));
      } catch (error) { return stop(`Final comment failed: ${error.message}`, 'final-comment-failed'); }
      Object.assign(delivery, { stage: 'commented', commentedAt: now() }); record(state, { event: 'final-comment', task: taskId }); save();
    }
    if (delivery.stage === 'commented') {
      try { await io.closeIssue(slug, delivery.issue); } catch (error) { return stop(`Closing the issue failed: ${error.message}`, 'close-failed'); }
      Object.assign(delivery, { stage: 'closed', closedAt: now() }); record(state, { event: 'issue-closed', task: taskId }); save();
    }
    if (delivery.stage === 'closed') {
      try { await io.setDone(delivery.itemId); } catch (error) { return stop(`Setting the card to Done failed: ${error.message}`, 'done-failed'); }
      Object.assign(delivery, { stage: 'done', doneAt: now() });
      // Delivered work leaves the active states, so a finished task can never hold the single job slot.
      const task = state.tasks[taskId]; if (task && ['claimed', 'awaiting-review'].includes(task.state)) task.state = 'complete';
      record(state, { event: 'card-done', task: taskId }); save();
    }
    return { stage: delivery.stage, blocked: null };
  });
}

// Called when a pull request for a claimed task merges. Idempotent.
export function recordMerge(state, taskId, { repository, pull, releaseTag = null, firstShipped = null }) {
  const task = state.tasks[taskId];
  if (!task?.job || !task.issue || !task.itemId) throw new Error('Task has no pickup record');
  state.deliveries ??= {};
  state.deliveries[taskId] ??= { stage: 'pending', repository, pull, issue: task.issue, issueRepository: task.issueRepository ?? task.repository, itemId: task.itemId, job: task.job, prompt: task.file ?? null, releaseTag, firstShipped, mergedRecordedAt: new Date().toISOString() };
  return state.deliveries[taskId];
}
