// Moves the prompt that a merged Matrix pull request implemented into its done/ folder.
// Run from a checkout of main, after the merge. Prints the moved path; exits 3 when there is nothing to do.
import fs from 'node:fs';
import path from 'node:path';
import { scan } from '../../tools/matrix/backlog.mjs';
import { archivePath, markDone, promptIdFromBranch } from './automerge.mjs';

const id = promptIdFromBranch(process.argv[2]);
if (!id) { console.error('Not a Matrix branch'); process.exit(2); }

const files = new Map();
const walk = directory => {
  for (const entry of fs.readdirSync(directory, { withFileTypes: true })) {
    const full = path.join(directory, entry.name);
    if (entry.isDirectory()) { if (entry.name !== 'node_modules' && entry.name !== 'build') walk(full); }
    else if (entry.name.endsWith('.md') && /(^|[\\/])prompts[\\/]/.test(full)) files.set(path.relative('.', full).split(path.sep).join('/'), fs.readFileSync(full, 'utf8'));
  }
};
for (const root of ['eq', 'analog-compressor', 'maximiser', 'shared']) if (fs.existsSync(root)) walk(root);

const prompt = scan(files).prompts.find(candidate => candidate.id === id && !candidate.file.includes('/done/'));
const target = prompt && archivePath(prompt.file);
if (!target) { console.log(`Nothing to archive for ${id}`); process.exit(3); }
fs.mkdirSync(path.dirname(target), { recursive: true });
fs.writeFileSync(target, markDone(fs.readFileSync(prompt.file, 'utf8')));
fs.rmSync(prompt.file);
console.log(`${prompt.file} -> ${target}`);
