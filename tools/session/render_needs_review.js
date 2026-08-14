#!/usr/bin/env node
// render_needs_review.js — regenerate docs/development/NEEDS_REVIEW.md from the
// canonical NEEDS_REVIEW.json.
//
// WHY THIS EXISTS. The pair is the project's standard shape (backlog.json/BACKLOG.md,
// user_stories.json/USER_STORIES.md): JSON is canonical, Markdown is the readable
// mirror. Mirrors kept by hand drift the moment a session answers entries in bulk —
// which is exactly what happened on 2026-08-02, when a Q&A pass resolved 19 entries
// in the JSON and left the Markdown describing all of them as open. A stale mirror is
// worse than no mirror: it reads as authoritative and is wrong.
//
// Usage:  node tools/session/render_needs_review.js
//
// Regenerates the whole file. The prose comes from the JSON fields verbatim, so write
// the JSON well and the mirror reads well; do not hand-edit the Markdown, it will be
// overwritten. The same renderer shape suits UX_QUESTIONS.{json,md} (BL-260).

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const SRC = path.join(ROOT, 'docs', 'development', 'NEEDS_REVIEW.json');
const OUT = path.join(ROOT, 'docs', 'development', 'NEEDS_REVIEW.md');

const data = JSON.parse(fs.readFileSync(SRC, 'utf8'));
const items = data.items || [];

const KIND_LABEL = {
  question: 'question',
  'decision-taken': 'decision taken on your behalf',
  observation: 'observation',
};

function entry(it) {
  const out = [];
  out.push(`### ${it.id} — ${it.title}`);
  const kind = KIND_LABEL[it.kind] || it.kind || 'entry';
  out.push(`*${kind} · raised ${it.raised || '?'}${it.source ? ` · from ${it.source}` : ''}*`);
  out.push('');
  if (it.what) { out.push(it.what); out.push(''); }
  if (it.why_it_matters) { out.push(`**Why it matters.** ${it.why_it_matters}`); out.push(''); }
  if (Array.isArray(it.options) && it.options.length) {
    for (const o of it.options) out.push(`- ${o}`);
    out.push('');
  }
  if (it.recommendation) { out.push(`> **Recommendation:** ${it.recommendation}`); out.push(''); }
  if (it.resolution) { out.push(`> **RESOLVED.** ${it.resolution}`); out.push(''); }
  if (Array.isArray(it.files) && it.files.length) {
    out.push(`*Files: ${it.files.map((f) => `\`${f}\``).join(', ')}*`);
    out.push('');
  }
  return out.join('\n');
}

const open = items.filter((i) => i.status !== 'resolved');
const done = items.filter((i) => i.status === 'resolved');

const md = [];
md.push('# Project Io — Needs Review');
md.push('');
md.push('**Ben\'s review queue.** Readable mirror of [`NEEDS_REVIEW.json`](NEEDS_REVIEW.json),');
md.push('which is canonical — the JSON wins on any disagreement.');
md.push('');
md.push('> **Generated file.** Produced by `node tools/session/render_needs_review.js`.');
md.push('> Edit the JSON, then re-run; hand edits here are overwritten.');
md.push('');
md.push('Things here are waiting on **your judgement**, not on work. Three kinds:');
md.push('');
md.push('| Kind | Meaning |');
md.push('|---|---|');
md.push('| **question** | An open call nobody has made. Not blocking — a blocking item is a backlog entry with `blocked_on` set. |');
md.push('| **decision-taken** | A call made **on your behalf** so work could continue. Recorded so it can be *overturned* rather than quietly becoming precedent. |');
md.push('| **observation** | Something noticed in passing, too small or too cross-cutting to file, that a human should still see. |');
md.push('');
md.push('**How this differs from the neighbours.** [`review.json`](review.json) is a *blocker* list —');
md.push('items blocked on a visual artifact only you can produce; work there cannot proceed at all.');
md.push('[`backlog.json`](backlog.json) is *work*. Entries here are neither: they are questions and');
md.push('reversible calls. If an answer creates work, file a backlog item and resolve the entry with');
md.push('that item\'s id.');
md.push('');
md.push('This queue is **transient**: resolved entries are pruned promptly rather than kept for');
md.push('posterity — the reasoning lands in code, an authority doc, or a backlog item at the moment');
md.push('the work happens, and that is the durable record. What stays here is what is still open.');
md.push('');
md.push(`*${items.length} entries — ${open.length} open, ${done.length} resolved.*`);
md.push('');
md.push('---');
md.push('');
md.push('## Open');
md.push('');
if (open.length === 0) md.push('*Nothing open.*\n');
for (const it of open) md.push(entry(it));
md.push('---');
md.push('');
md.push('## Resolved');
md.push('');
md.push('Kept, not pruned: the reasoning is the point. Prune only in a deliberate sweep, once the');
md.push('answer has landed in an authority doc.');
md.push('');
for (const it of done) md.push(entry(it));

fs.writeFileSync(OUT, md.join('\n').replace(/\n{3,}/g, '\n\n') + '\n');
console.log(`render_needs_review: ${items.length} entries (${open.length} open, ${done.length} resolved) -> ${path.relative(ROOT, OUT)}`);
