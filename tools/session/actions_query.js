#!/usr/bin/env node
// actions_query.js — retrieval primitive over the action dictionary (BL-270).
//
// A language agent does not absorb ACTIONS.json whole (Ben, 2026-08-02): it
// holds the compact ACTIONS_INDEX.json in context and fetches full entries on
// demand. This script IS that fetch — the word-play harness wraps it (or its
// logic) as the LLM's lookup tool; humans use it from the shell.
//
// Usage:
//   node tools/session/actions_query.js <entry id>       exact id -> that entry
//   node tools/session/actions_query.js <family>         gameplay|canvas|lens|ledger|chrome -> all its entries
//   node tools/session/actions_query.js <keyword>        case-insensitive match over id/surface/press/expected_output/reason_to_select
//   node tools/session/actions_query.js --index          the compact [id, surface] index
//
// Output is JSON on stdout: an array of full entries (or the index object).
// Exit 1 with a message on stderr when nothing matches, so a harness can tell
// "no such action" from an empty-but-valid result.

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const SRC = path.join(ROOT, 'docs', 'ai', 'ACTIONS.json');
const FAMILIES = ['gameplay', 'canvas', 'lens', 'ledger', 'chrome'];

const { entries } = JSON.parse(fs.readFileSync(SRC, 'utf8'));
const argv = process.argv.slice(2);

if (!argv.length) {
  console.error('usage: actions_query.js <id|family|keyword> | --index');
  process.exit(1);
}

function out(result) {
  console.log(JSON.stringify(result, null, 1));
}

if (argv[0] === '--index') {
  out({ actions: entries.map((e) => [e.id, e.surface]) });
  process.exit(0);
}

const q = argv.join(' ');

const byId = entries.find((e) => e.id === q);
if (byId) { out([byId]); process.exit(0); }

if (FAMILIES.includes(q)) { out(entries.filter((e) => e.family === q)); process.exit(0); }

const needle = q.toLowerCase();
const hits = entries.filter((e) =>
  [e.id, e.surface, e.press, e.expected_output, e.reason_to_select]
    .some((s) => s.toLowerCase().includes(needle)));
if (!hits.length) {
  console.error(`no action matches '${q}' (not an id, family, or keyword hit)`);
  process.exit(1);
}
out(hits);
