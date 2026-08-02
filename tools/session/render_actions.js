#!/usr/bin/env node
// render_actions.js — regenerate docs/ai/ACTIONS.md from the canonical
// docs/ai/ACTIONS.json (BL-270, the action dictionary).
//
// The pair follows the project's standard shape (backlog.json/BACKLOG.md,
// user_stories.json/USER_STORIES.md, NEEDS_REVIEW.{json,md}): JSON canonical,
// Markdown a generated mirror. The JSON is the machine-consumable half — the
// word-play harness and the word-driven generation work read it directly — so
// this renderer also doubles as the parse check: if it runs clean, the store is
// well-formed (requirements.json § action-dictionary R2).
//
// Usage:  node tools/session/render_actions.js
//
// Regenerates the whole Markdown file; do not hand-edit it.

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const SRC = path.join(ROOT, 'docs', 'ai', 'ACTIONS.json');
const OUT = path.join(ROOT, 'docs', 'ai', 'ACTIONS.md');

const data = JSON.parse(fs.readFileSync(SRC, 'utf8'));
const entries = data.entries || [];

// Shape check — every entry carries exactly the dictionary fields (R2). A
// malformed entry fails the render loudly rather than producing a plausible
// mirror over a broken store.
const REQUIRED = ['id', 'family', 'surface', 'press', 'args', 'preconditions',
                  'expected_output', 'reason_to_select'];
const FAMILIES = ['gameplay', 'canvas', 'lens', 'ledger', 'chrome'];
for (const e of entries) {
  for (const f of REQUIRED)
    if (!(f in e)) throw new Error(`entry ${e.id || '<no id>'} missing field '${f}'`);
  if (!FAMILIES.includes(e.family))
    throw new Error(`entry ${e.id} has unknown family '${e.family}'`);
  if (!e.id.startsWith(e.family + '.'))
    throw new Error(`entry ${e.id} id is not prefixed by its family '${e.family}'`);
  if (!Array.isArray(e.args) || !Array.isArray(e.preconditions))
    throw new Error(`entry ${e.id}: args and preconditions must be arrays`);
}
const ids = new Set();
for (const e of entries) {
  if (ids.has(e.id)) throw new Error(`duplicate entry id ${e.id}`);
  ids.add(e.id);
}

const FAMILY_TITLE = {
  gameplay: 'Gameplay — presses that mutate the world',
  canvas:   'Canvas — looking around: navigation, selection, hover, time',
  lens:     'Lenses — re-skinning the map to answer a question',
  ledger:   'Ledgers & panels — opening and steering the information surfaces',
  chrome:   'Chrome — startup, the system menu, settings, F-keys',
};

function entryMd(e) {
  const out = [];
  out.push(`### \`${e.id}\` — ${e.surface}`);
  out.push('');
  out.push(`**Press.** ${e.press}`);
  out.push('');
  if (e.args.length) {
    out.push('| Arg | Type | Meaning |');
    out.push('|---|---|---|');
    for (const a of e.args) out.push(`| \`${a.name}\` | \`${a.type}\` | ${a.meaning} |`);
    out.push('');
  }
  if (e.preconditions.length) {
    out.push('**Valid when:**');
    for (const p of e.preconditions) out.push(`- ${p}`);
    out.push('');
  }
  out.push(`**Expected output.** ${e.expected_output}`);
  out.push('');
  out.push(`**Reason to select.** ${e.reason_to_select}`);
  out.push('');
  return out.join('\n');
}

const md = [];
md.push('# Project Io — the action dictionary');
md.push('');
md.push('Every control in the game: what pressing it does, and why you would. Readable');
md.push('mirror of [`ACTIONS.json`](ACTIONS.json), which is canonical — the JSON is the');
md.push('machine-consumable half an AI player reads (BL-270). Pair it with the corp');
md.push('blackboard export (BL-206, the read channel) and the corp-command seam');
md.push('(`src/world/corp_command.hpp`, the write channel) and an LLM has the full');
md.push('word interface to play through.');
md.push('');
md.push('> **Generated file.** Produced by `node tools/session/render_actions.js`.');
md.push('> Edit the JSON, then re-run; hand edits here are overwritten.');
md.push('');
const counts = FAMILIES.map((f) => `${entries.filter((e) => e.family === f).length} ${f}`).join(' · ');
md.push(`*${entries.length} entries — ${counts}.*`);
md.push('');
for (const fam of FAMILIES) {
  const fe = entries.filter((e) => e.family === fam);
  if (!fe.length) continue;
  md.push('---');
  md.push('');
  md.push(`## ${FAMILY_TITLE[fam]}`);
  md.push('');
  for (const e of fe) md.push(entryMd(e));
}

fs.writeFileSync(OUT, md.join('\n').replace(/\n{3,}/g, '\n\n') + '\n');
console.log(`render_actions: ${entries.length} entries -> ${path.relative(ROOT, OUT)}`);
