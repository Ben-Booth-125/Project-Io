#!/usr/bin/env node
// render_question_log.js — generate docs/ui/QUESTION_LOG.md from question_log.json.
//
// The JSON is canonical; the markdown is a read-only mirror, the same split
// ACTIONS.json/ACTIONS.md and backlog.json/BACKLOG.md already use. Running this
// is also the store's parse/shape check — a malformed entry fails here rather
// than being noticed by nobody.
//
// It deliberately does NOT audit. BL-260 is explicit (Ben, 2026-08-01): "the docs
// are the audit, we don't need an audit method or arbitrary resolution." So a
// missing surface is a thing a human sees while reading, not a non-zero exit.
// What this DOES do is make the two coverage gaps visible by sorting drafted
// entries to their own section — a drafted justification is an open question
// wearing a sentence, and burying it in alphabetical order would hide it.
//
// USAGE: node tools/session/render_question_log.js
// EXIT:  0 on success; 1 only on a malformed store (parse/shape), never on content.

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const SRC = path.join(ROOT, 'docs/ui/question_log.json');
const OUT = path.join(ROOT, 'docs/ui/QUESTION_LOG.md');

let store;
try {
    store = JSON.parse(fs.readFileSync(SRC, 'utf8'));
} catch (err) {
    console.error(`render_question_log: cannot read ${SRC}\n  ${err.message}`);
    process.exit(1);
}

const entries = store.entries;
if (!entries || typeof entries !== 'object') {
    console.error('render_question_log: store has no `entries` object.');
    process.exit(1);
}

const REQUIRED = ['surface', 'answers', 'because', 'backlog_item', 'files', 'status'];
const bad = [];
for (const [id, e] of Object.entries(entries)) {
    for (const f of REQUIRED) if (e[f] === undefined) bad.push(`${id}: missing \`${f}\``);
    if (e.status && !['settled', 'drafted'].includes(e.status))
        bad.push(`${id}: status "${e.status}" is neither settled nor drafted`);
}
if (bad.length) {
    console.error('render_question_log: malformed entries —\n  ' + bad.join('\n  '));
    process.exit(1);
}

const ids = Object.keys(entries).sort();
const settled = ids.filter(i => entries[i].status === 'settled');
const drafted = ids.filter(i => entries[i].status === 'drafted');

const L = [];
L.push('# UI question log — every surface states what it answers');
L.push('');
L.push('> **Generated file — do not hand-edit.** Source of truth is');
L.push('> [`question_log.json`](question_log.json); regenerate with');
L.push('> `node tools/session/render_question_log.js`.');
L.push('');
L.push('Every information surface declares the **question it answers** and **why it earns its');
L.push('space**, with the backlog item that demanded it. The pair is required. Enforcement is');
L.push('authorship, not machinery — there is deliberately no audit check against this file');
L.push('(BL-260, Ben 2026-08-01: *"the docs are the audit"*).');
L.push('');
L.push(`**${ids.length} surfaces** — ${settled.length} settled, ${drafted.length} awaiting Ben's wording.`);
L.push('');

const row = (id) => {
    const e = entries[id];
    const items = (Array.isArray(e.backlog_item) ? e.backlog_item : [e.backlog_item]).join(', ');
    const files = (Array.isArray(e.files) ? e.files : [e.files]).map(f => `\`${f}\``).join(', ');
    L.push(`### ${e.surface}`);
    L.push('');
    L.push(`**Answers:** ${e.answers}`);
    L.push('');
    L.push(`**Because:** ${e.because}`);
    L.push('');
    L.push(`*Demanded by ${items} · ${files} · id \`${id}\`*`);
    L.push('');
};

if (drafted.length) {
    L.push('---');
    L.push('');
    L.push("## Awaiting Ben's wording");
    L.push('');
    L.push('These were **drafted by an implementer for Ben to accept or rewrite**, not authored by');
    L.push('him. BL-260 is explicit that writing the pair *is* the design check — so each of these');
    L.push('is an open question wearing a sentence, and they sit first rather than being buried in');
    L.push('alphabetical order.');
    L.push('');
    drafted.forEach(row);
}

L.push('---');
L.push('');
L.push('## Settled');
L.push('');
if (settled.length) settled.forEach(row);
else L.push('_None yet._');
L.push('');

fs.writeFileSync(OUT, L.join('\n'));
console.log(`render_question_log: ${ids.length} surfaces (${settled.length} settled, ${drafted.length} drafted) -> docs/ui/QUESTION_LOG.md`);
