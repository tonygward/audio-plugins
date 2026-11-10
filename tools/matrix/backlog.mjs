import path from 'node:path';
import { createHash } from 'node:crypto';

const modes = new Set(['scheduled', 'build-now', 'deferred']);
const fields = new Set(['id', 'status', 'execution', 'priority', 'model', 'effort', 'dependencies', 'conflicts']);
const idPattern = /^[A-Z][A-Z0-9]*-\d{2,}$/;
const scopes = { eq: 'EQ', 'analog-compressor': 'ACOMP', maximiser: 'MAX' };
export const digest = text => createHash('sha256').update(text).digest('hex');

// Overnight/notification agent selection is for functional audio-product changes only.
// Infrastructure (AUTO), portal operations (WEB), and any unknown shared scope are
// always routed to a separate explicitly authorised maintenance workflow.
const functionalTopLevel = { eq: 'EQ', 'analog-compressor': 'ACOMP', maximiser: 'MAX' };
const functionalShared = new Set(['SUI', 'OPT', 'PEQ', 'BUS', 'CLN', 'ADV', 'REC']);
export function isFunctionalPrompt(prompt) {
  const parts = prompt.file.split('/');
  if (parts[1] !== 'prompts') return false;
  if (parts[0] === 'shared') return parts.length >= 3 && functionalShared.has(prompt.id.split('-')[0]);
  return functionalTopLevel[parts[0]] === prompt.id.split('-')[0];
}


// Deliberately a small, documented YAML subset; never evaluate YAML tags/aliases.
export function parsePrompt(file, text) {
  const normal = text.replace(/^\uFEFF/, '').replace(/\r\n/g, '\n');
  let body = normal, meta = {};
  if (normal.startsWith('---\n')) {
    const end = normal.indexOf('\n---\n', 4);
    if (end < 0) throw new Error('Unterminated metadata');
    for (const line of normal.slice(4, end).split('\n')) {
      if (!line.trim() || line.trim().startsWith('#')) continue;
      const match = /^([a-z]+):\s*(.*?)\s*$/.exec(line);
      if (!match || !fields.has(match[1]) || Object.hasOwn(meta, match[1])) throw new Error(`Invalid or duplicate metadata: ${line}`);
      const [, key, raw] = match;
      if (['dependencies', 'conflicts'].includes(key)) {
        try { meta[key] = JSON.parse(raw); } catch { throw new Error(`${key} must be an inline JSON array`); }
        if (!Array.isArray(meta[key]) || meta[key].some(x => typeof x !== 'string' || !idPattern.test(x))) throw new Error(`Invalid ${key}`);
      } else {
        let value = raw;
        if (/^".*"$/.test(raw)) { try { value = JSON.parse(raw); } catch { throw new Error(`Invalid ${key}`); } }
        else if (/^'.*'$/.test(raw)) value = raw.slice(1, -1);
        if (typeof value !== 'string' || !value || /[\[\]{}&*!#\n]/.test(value)) throw new Error(`Invalid ${key}`);
        meta[key] = value;
      }
    }
    body = normal.slice(end + 5);
  }
  if (Object.hasOwn(meta, 'execution') && !modes.has(meta.execution)) throw new Error('Unknown execution mode');
  const base = path.posix.basename(file);
  const inferred = /^([A-Z][A-Z0-9]*-\d{2,})-/.exec(base)?.[1]
    ?? (scopes[file.split('/')[0]] && /^\d{2,}-/.test(base) ? `${scopes[file.split('/')[0]]}-${base.split('-')[0]}` : null);
  const id = meta.id ?? inferred;
  if (!id || !idPattern.test(id) || (inferred && id !== inferred)) throw new Error('Missing or inconsistent stable ID');
  const statusLine = [...body.matchAll(/\*\*Status:\*\*([^\n]*)/gi)].map(m => m[1]).join(' ');
  const status = (meta.status ?? statusLine).toLowerCase();
  const excluded = file.split('/').some(p => ['done', 'future', 'archive', 'archived'].includes(p));
  const proseDeferred = /\b(deferred|do not implement yet|not queued|not authorised|not authorized)\b/i.test(statusLine + '\n' + body.slice(0, 1500));
  const blocked = /\b(blocked|in.progress|claimed|experimental|cancelled|done|completed|archived)\b/.test(status);
  const complete = file.split('/').includes('done');
  const invalidStatus = meta.status && !['ready', 'planned', 'blocked', 'in-progress', 'deferred', 'done'].includes(meta.status);
  if (invalidStatus) throw new Error('Unknown status');
  if (meta.status === 'done' && !complete) throw new Error('Done status outside canonical done folder requires merge verification');
  const dependencies = meta.dependencies ?? [...new Set([...body.matchAll(/\*\*(?:Depends on|Dependency):\*\*([^\n]*)/gi)].flatMap(m => m[1].match(/[A-Z][A-Z0-9]*-\d{2,}/g) ?? []))];
  if (!meta.dependencies && scopes[file.split('/')[0]]) {
    for (const m of body.matchAll(/\b(?:build|run)\s+(?:this\s+)?after\s+(?:task\s+|prompt\s+)?(\d{2,})\b/gi)) dependencies.push(`${scopes[file.split('/')[0]]}-${m[1]}`);
  }
  const modelText = meta.model ?? (/\*\*Recommended model:\*\*([^\n]*)/i.exec(body)?.[1] ?? '').split('**')[0];
  // Never silently turn a design/Opus recommendation into a Sonnet implementation.
  const model = /^sonnet[.\s;]*(?:$|for implementation)/i.test(modelText.trim()) && !/opus for (?:design|initial|architecture)/i.test(modelText) ? 'sonnet' : null;
  const effortText = meta.effort ?? /\*\*(?:Estimated|Relative) effort\s*\/\s*usage:\*\*\s*(Low|Medium|High)/i.exec(body)?.[1] ?? 'unknown';
  // Plain-size words are accepted as aliases, so a natural wording never invalidates a prompt.
  const effort = ({ small: 'low', large: 'high' })[effortText.toLowerCase()] ?? effortText.toLowerCase();
  if (meta.effort && !['low', 'medium', 'high'].includes(effort)) throw new Error('Unknown effort');
  const priorityText = meta.priority ?? /\*\*Priority:\*\*\s*(\d+)/i.exec(body)?.[1] ?? '99';
  const priority = ({ high: 1, medium: 50, low: 90 })[priorityText] ?? Number(priorityText);
  if (!Number.isFinite(priority) || priority < 0) throw new Error('Invalid priority');
  return { id, file, hash: digest(normal), execution: meta.execution ?? 'scheduled', defaulted: !Object.hasOwn(meta, 'execution'),
    status, complete, excluded, deferred: proseDeferred || meta.execution === 'deferred' || meta.status === 'deferred', blocked,
    dependencies, conflicts: meta.conflicts ?? [], priority, model, effort, body };
}

export function scan(files) {
  const prompts = [], errors = [], warnings = [], seen = new Set();
  for (const [file, text] of files) {
    if (!/^(eq|analog-compressor|maximiser|shared)\/prompts\/.+\.md$/.test(file)) continue;
    if (['README.md', 'overnight-prompt.md'].includes(path.posix.basename(file))) continue;
    try {
      const prompt = parsePrompt(file, text);
      if (seen.has(prompt.id)) throw new Error(`Duplicate ID ${prompt.id}`);
      seen.add(prompt.id); prompts.push(prompt);
    } catch (error) {
      // Historical aliases without stable IDs are visible but never dependencies or work.
      (file.includes('/done/') ? warnings : errors).push({ file, reason: error.message });
    }
  }
  const roadmap = files.get('shared/prompts/README.md') ?? '';
  const delivery = roadmap.split('### Delivery priority')[1]?.split('\n## ')[0] ?? '';
  for (const p of prompts) {
    for (const line of delivery.split('\n')) {
      const match = /^\|\s*(\d+)\s*\|/.exec(line);
      if (match && new RegExp(`\\b${p.id}\\b`).test(line)) p.priority = Math.min(p.priority, Number(match[1]));
    }
    // Recognise explicit “after ID” ordering in the prompt's own README row.
    const readme = files.get(`${p.file.slice(0, p.file.indexOf('/prompts/'))}/prompts/README.md`) ?? '';
    for (const line of readme.split('\n').filter(line => line.includes(path.posix.basename(p.file)))) {
      for (const match of line.matchAll(/\bafter\s+([A-Z][A-Z0-9]*-\d{2,})/gi)) p.dependencies.push(match[1]);
      if (/\b(blocked|in.progress|claimed|deferred)\b/i.test(line)) p.blocked = true;
    }
    p.dependencies = [...new Set(p.dependencies)];
  }
  return { prompts, errors, warnings };
}

export function select(prompts, tasks, trigger, limit = 6) {
  if (!['daily', 'notify'].includes(trigger)) throw new Error('Unknown trigger');
  const complete = new Set(prompts.filter(p => p.complete).map(p => p.id));
  const selected = [], decisions = [];
  for (const p of [...prompts].sort((a, b) => (a.execution !== 'build-now') - (b.execution !== 'build-now') || a.priority - b.priority || a.id.localeCompare(b.id))) {
    const task = tasks[p.id];
    let reason = !isFunctionalPrompt(p) ? 'infrastructure excluded from overnight work' : p.complete ? 'merged archive' : p.excluded ? 'excluded folder' : p.deferred ? 'deferred policy' : p.blocked ? 'blocked status' : null;
    if (!reason && task && ['claimed', 'interrupted', 'awaiting-review', 'complete', 'dead-letter'].includes(task.state)) reason = `durable state: ${task.state}`;
    if (!reason && p.dependencies.some(id => !complete.has(id))) reason = 'dependency not merged or unknown';
    if (!reason && p.dependencies.includes(p.id)) reason = 'self dependency';
    if (!reason && trigger === 'notify' && p.execution !== 'build-now') reason = 'waiting for daily schedule';
    if (!reason && !p.model) reason = 'model recommendation requires review';
    if (!reason && !['low', 'medium'].includes(p.effort)) reason = 'large or unknown effort requires review';
    // A shared/unknown footprint takes a batch of one. Plugin-local work never overlaps.
    const area = p.file.split('/')[0];
    if (!reason && selected.some(q => area === 'shared' || q.file.startsWith('shared/') || q.file.split('/')[0] === area || p.conflicts.includes(q.id) || q.conflicts.includes(p.id))) reason = 'batch conflict';
    if (!reason && selected.length >= Math.min(6, limit)) reason = 'batch capacity';
    decisions.push({ id: p.id, file: p.file, execution: p.execution, defaulted: p.defaulted, priority: p.priority, model: p.model, effort: p.effort, dependencies: p.dependencies, reason: reason ?? 'candidate' });
    if (!reason) selected.push(p);
  }
  return { selected, decisions };
}
