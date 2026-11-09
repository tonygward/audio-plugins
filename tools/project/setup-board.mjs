// Idempotent GitHub Projects v2 bootstrap for Songsmith.
// Run with: node tools/project/setup-board.mjs
// Requires: gh CLI authenticated with `project` scope.
// Does not run Matrix or imply any job pickup, build or deployment.
import { execFileSync } from 'node:child_process';

const owner = 'tonygward';
const title = 'Songsmith Development';
const repos = ['audio-plugins', 'songsmith-website', 'songsmithstudio'];
const seed = [
  { url: 'https://github.com/tonygward/songsmithstudio/issues/2', stage: 'Ready', priority: 'High', product: 'Suite', execution: 'Run Now', release: '0.5.0' },
  { url: 'https://github.com/tonygward/songsmithstudio/issues/3', stage: 'Ready', priority: 'High', product: 'Suite UI', execution: 'Run Now', release: '0.5.0' },
  { url: 'https://github.com/tonygward/songsmith-website/issues/19', stage: 'Backlog', priority: 'High', product: 'Beta Website', execution: 'Separate', release: '' },
  { url: 'https://github.com/tonygward/audio-plugins/issues/155', stage: 'Backlog', priority: 'Medium', product: 'Matrix', execution: 'Deferred', release: '' },
  { url: 'https://github.com/tonygward/audio-plugins/issues/156', stage: 'Backlog', priority: 'Medium', product: 'Matrix', execution: 'Deferred', release: '' },
  { url: 'https://github.com/tonygward/audio-plugins/issues/157', stage: 'Backlog', priority: 'Medium', product: 'Release', execution: 'Deferred', release: '' },
  { url: 'https://github.com/tonygward/audio-plugins/pull/152', stage: 'Done', priority: 'High', product: 'Suite', execution: 'Completed', release: '0.5.0' },
];

function gh(args) {
  return execFileSync('gh', args, { encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] }).trim();
}
function json(args) {
  return JSON.parse(gh([...args, '--format', 'json']));
}
function warn(message, error) {
  console.error('WARNING:', message, error?.stderr?.toString()?.trim() || error?.message || '');
}
function ensureField(project, existing, name, type, options) {
  if (existing.has(name)) return;
  const args = ['project', 'field-create', String(project), '--owner', owner, '--name', name, '--data-type', type];
  if (options) args.push('--single-select-options', options.join(','));
  gh(args);
  existing.add(name);
  console.log('Created field:', name);
}
function set(project, item, field, value) {
  if (!value) return;
  gh(['project', 'item-edit', String(project), '--owner', owner, '--url', item, '--field', field, '--value', value]);
}

try {
  gh(['auth', 'status']);
} catch (error) {
  console.error('GitHub CLI authentication is required. Run: gh auth login');
  process.exit(1);
}
let projects;
try {
  projects = json(['project', 'list', '--owner', owner, '--limit', '100']).projects || [];
} catch (error) {
  console.error('Cannot list GitHub Projects. Run: gh auth refresh -s project');
  warn('GitHub Projects access', error);
  process.exit(1);
}
let project = projects.find(p => p.title === title);
if (!project) {
  project = json(['project', 'create', '--owner', owner, '--title', title]);
  console.log('Created GitHub Project:', project.url);
} else {
  console.log('Reusing GitHub Project:', project.url);
}
const number = project.number;
if (!number) throw new Error('Project creation/list did not return a project number');

const fieldData = json(['project', 'field-list', String(number), '--owner', owner, '--limit', '100']);
const fields = new Set((fieldData.fields || []).map(f => f.name));
ensureField(number, fields, 'Stage', 'SINGLE_SELECT', ['Backlog', 'Ready', 'Running', 'Done']);
ensureField(number, fields, 'Priority', 'SINGLE_SELECT', ['High', 'Medium', 'Low']);
ensureField(number, fields, 'Product', 'SINGLE_SELECT', ['Suite', 'Suite UI', 'EQ', 'Analog Compressor', 'Maximiser', 'Beta Website', 'Matrix', 'Release', 'Other']);
ensureField(number, fields, 'Execution', 'SINGLE_SELECT', ['Run Now', 'Scheduled', 'Deferred', 'Separate', 'Completed']);
ensureField(number, fields, 'Matrix Job', 'TEXT');
ensureField(number, fields, 'Release Version', 'TEXT');
for (const repo of repos) {
  try {
    gh(['project', 'link', String(number), '--owner', owner, '--repo', owner + '/' + repo]);
    console.log('Linked repository:', repo);
  } catch (error) {
    // A link may already exist; avoid aborting subsequent additions.
    warn('Could not link repository ' + repo + ' (possibly already linked)', error);
  }
}
for (const item of seed) {
  try {
    gh(['project', 'item-add', String(number), '--owner', owner, '--url', item.url]);
    for (const [field, value] of Object.entries({
      Stage: item.stage, Priority: item.priority, Product: item.product,
      Execution: item.execution, 'Release Version': item.release,
    })) set(number, item.url, field, value);
    console.log('Added / updated:', item.url, '=>', item.stage);
  } catch (error) {
    warn('Could not add or update ' + item.url, error);
  }
}
console.log('\nProject: https://github.com/users/' + owner + '/projects/' + number);
console.log('In the GitHub UI select Board layout and group by Stage (Backlog / Ready / Running / Done).');
console.log('Re-run this script safely after adding more seed records. No Matrix job has been started.');
