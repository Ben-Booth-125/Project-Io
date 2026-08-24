#!/usr/bin/env node
// render_sprints.js — regenerate docs/development/SPRINTS.md from the canonical
// sprints.json plus the cold archives (docs/development/archive/sprints-*.json).
//
// Same shape as render_needs_review.js: the JSON is canonical, the Markdown is a
// generated mirror. The old hand-kept "Where things stand" table and the store's
// own status_table array drifted against each other in opposite directions inside
// one day (NR-598). Ben's ruling the same day, 2026-08-24: sprints live fully in
// JSON, the mirror is generated, completed sprints go cold. status_table is
// retired — each sprint carries its own status_line, so there is no parallel
// array left to drift.
//
// Usage:  node tools/session/render_sprints.js
//
// Reads hot + cold; the table indexes every sprint ever run, the "Open now"
// section renders full prose for the non-terminal ones only.

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const SRC = path.join(ROOT, 'docs', 'development', 'sprints.json');
const OUT = path.join(ROOT, 'docs', 'development', 'SPRINTS.md');
const ARCHIVE_DIR = path.join(ROOT, 'docs', 'development', 'archive');

// Keep in sync with archive_sprints.js.
const TERMINAL = new Set(['closed', 'superseded', 'subsumed', 'retro-recorded', 'landed']);

const data = JSON.parse(fs.readFileSync(SRC, 'utf8'));
const hot = data.sprints || [];

// Cold entries, sorted chronologically. Key order inside a records object is NOT
// historical — JS hoists integer-like keys ("27" before "2a") — so order by date:
// opened, else closed, else the first date named in the status_line (covers the
// never-opened superseded entries), undatable last; ties keep read order.
const cold = [];
if (fs.existsSync(ARCHIVE_DIR)) {
  const files = fs.readdirSync(ARCHIVE_DIR).filter((f) => /^sprints-.*\.json$/.test(f)).sort();
  for (const f of files) {
    const store = JSON.parse(fs.readFileSync(path.join(ARCHIVE_DIR, f), 'utf8'));
    for (const rec of Object.values(store.records || {})) cold.push(rec);
  }
}
const when = (sp) => sp.opened_date || sp.closed_date ||
  (String(sp.status_line || '').match(/\d{4}-\d{2}-\d{2}/) || ['9999-99-99'])[0];
cold.sort((a, b) => when(a) < when(b) ? -1 : when(a) > when(b) ? 1 : 0);

const cell = (s) => String(s || '').replace(/\|/g, '\\|').replace(/\r?\n/g, ' ');
const cap = (s) => (s ? s[0].toUpperCase() + s.slice(1) : '?');
const statusLine = (sp) =>
  sp.status_line || `${cap(sp.state)}${sp.closed_date ? ` ${sp.closed_date}` : ''}`;
const prose = (v) => (Array.isArray(v) ? v.join('\n\n') : v || '');

function detail(sp) {
  const out = [];
  const meta = [cap(sp.state)];
  if (sp.opened_date) meta.push(`opened ${sp.opened_date}`);
  if (sp.author) meta.push(sp.author);
  out.push(`### Sprint ${sp.id} — ${sp.theme}`);
  out.push(`*${meta.join(' · ')}*`);
  out.push('');
  if (sp.goal) { out.push(`**Goal.** ${prose(sp.goal)}`); out.push(''); }
  if (Array.isArray(sp.planned) && sp.planned.length) {
    out.push('**Planned.**');
    for (const p of sp.planned) out.push(`- ${p}`);
    out.push('');
  }
  if (sp.done_when) { out.push(`**Done when.** ${prose(sp.done_when)}`); out.push(''); }
  if (sp.risk) { out.push(`**Risk.** ${prose(sp.risk)}`); out.push(''); }
  if (sp.notes) { out.push(prose(sp.notes)); out.push(''); }
  return out.join('\n');
}

const active = hot.filter((s) => !TERMINAL.has(s.state));
const hotTerminal = hot.filter((s) => TERMINAL.has(s.state));

const md = [];
md.push('# Project Io — Sprints');
md.push('');
md.push('> **Generated file.** Produced by `node tools/session/render_sprints.js` from');
md.push('> [`sprints.json`](sprints.json) (canonical, open sprints) and `archive/sprints-*.json`');
md.push('> (completed sprints, cold). Edit the JSON, then re-run; hand edits here are overwritten.');
md.push('');
for (const p of data.header || []) { md.push(p); md.push(''); }
md.push('## Format');
md.push('');
md.push('```');
md.push(String(data.format || '').trimEnd());
md.push('```');
md.push('');
for (const p of data.format_notes || []) { md.push(p); md.push(''); }
md.push('---');
md.push('');
for (const p of data.rulings || []) { md.push(p); md.push(''); }
md.push('## Open now');
md.push('');
if (active.length === 0) md.push('*Nothing open.*\n');
for (const sp of active) md.push(detail(sp));
md.push('## Where things stand');
md.push('');
md.push('| Sprint | Theme | State |');
md.push('|---|---|---|');
for (const sp of [...cold, ...hot]) {
  md.push(`| ${cell(sp.id)} | ${cell(sp.theme)} | ${cell(statusLine(sp))} |`);
}
md.push('');
if (data.next_up) { md.push(`**Next up.** ${data.next_up}`); md.push(''); }
for (const p of data.footer_notes || []) { md.push(p); md.push(''); }
md.push(`*${cold.length} sprints archived cold; ${active.length} open/gated in the hot store` +
  (hotTerminal.length ? ` (${hotTerminal.length} completed and awaiting archive_sprints.js)` : '') +
  '.*');

fs.writeFileSync(OUT, md.join('\n').replace(/\n{3,}/g, '\n\n') + '\n');
console.log(`render_sprints: ${cold.length + hot.length} sprints (${cold.length} cold, ` +
  `${active.length} active${hotTerminal.length ? `, ${hotTerminal.length} hot-but-terminal` : ''}) ` +
  `-> ${path.relative(ROOT, OUT)}`);
