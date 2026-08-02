#!/usr/bin/env node
// backlog_query.js — retrieval primitive over backlog.json.
//
// Same principle as actions_query.js (BL-270): a language agent does not absorb a
// 1 MB canonical file to answer "what's left at priority A?". It holds a compact
// index and fetches full records on demand. This is that fetch for the backlog.
//
// The default output is INDEX-ONLY — id, short_name, status, priority, version_goal.
// Prose is opt-in via --full or --fields, and --full transparently resolves records
// that have been moved to the cold store (see archive_store.js), so an archived item
// reads exactly like a hot one.
//
// USAGE:
//   node tools/session/backlog_query.js                       every open item, index fields
//   node tools/session/backlog_query.js --status designed
//   node tools/session/backlog_query.js --priority S,SSS --open
//   node tools/session/backlog_query.js --version v0.1.0
//   node tools/session/backlog_query.js --category Canvas --touches src/ui/
//   node tools/session/backlog_query.js --grep selection      id/short_name/title/summary match
//   node tools/session/backlog_query.js BL-270 --full         one item, everything, prose resolved
//   node tools/session/backlog_query.js --fields id,files,authority_doc
//   node tools/session/backlog_query.js --count               how many match, nothing else
//   node tools/session/backlog_query.js --all                 do not filter out terminal items
//
// Output is JSON on stdout (add --table for an aligned terminal listing).
// EXIT: 0 with results; 1 when nothing matches, so a caller can tell "none" from "empty".

'use strict';
const fs = require('fs');
const path = require('path');
const A = require('./archive_store');

const BL_PATH = path.join(A.ROOT, 'docs/development/backlog.json');
const INDEX_FIELDS = ['id', 'short_name', 'status', 'priority', 'version_goal'];

const argv = process.argv.slice(2);
const has = (f) => argv.includes(f);
const val = (f) => { const i = argv.indexOf(f); return i >= 0 ? argv[i + 1] : null; };
const list = (f) => { const v = val(f); return v ? v.split(',').map((s) => s.trim()).filter(Boolean) : null; };

if (has('--help') || has('-h')) {
    console.log(fs.readFileSync(__filename, 'utf8').split('\n').slice(1, 26).map((l) => l.replace(/^\/\/ ?/, '')).join('\n'));
    process.exit(0);
}

const ids = new Set(argv.filter((a) => /^BL-\d+$/i.test(a)).map((a) => a.toUpperCase()));
const full = has('--full');
const wantFields = list('--fields');
const statuses = list('--status');
const priorities = list('--priority');
const categories = list('--category');
const version = val('--version');
const touches = val('--touches');
const grep = val('--grep');
const showAll = has('--all') || ids.size > 0;
const openOnly = has('--open');

const backlog = JSON.parse(fs.readFileSync(BL_PATH, 'utf8'));
const cache = new Map();

let hits = backlog.items.filter((it) => {
    if (ids.size) return ids.has(it.id);
    const terminal = A.TERMINAL.has(it.status);
    if (openOnly && terminal) return false;
    if (!showAll && !statuses && terminal) return false;   // default view is live work
    if (statuses && !statuses.includes(it.status)) return false;
    if (priorities && !priorities.includes(it.priority)) return false;
    if (categories && !categories.includes(it.category)) return false;
    if (version && it.version_goal !== version) return false;
    if (touches && !(it.files || []).some((f) => f.includes(touches))) return false;
    if (grep) {
        const needle = grep.toLowerCase();
        const hay = [it.id, it.short_name, it.title, it.summary].filter(Boolean).join(' ').toLowerCase();
        if (!hay.includes(needle)) return false;
    }
    return true;
});

// Priority order runs SSS > S > A > B > C > F; ties fall back to id so output is stable.
const RANK = { SSS: 0, S: 1, A: 2, B: 3, C: 4, F: 5 };
hits.sort((a, b) => (RANK[a.priority] ?? 9) - (RANK[b.priority] ?? 9) || a.id.localeCompare(b.id));

if (has('--count')) {
    console.log(JSON.stringify({ matched: hits.length, of: backlog.items.length }));
    process.exit(hits.length ? 0 : 1);
}

if (full) hits = hits.map((it) => A.resolve(it, A.ROOT, cache));

const fields = wantFields || (full ? null : INDEX_FIELDS);
const project = (it) => (fields ? Object.fromEntries(fields.filter((f) => it[f] !== undefined).map((f) => [f, it[f]])) : it);
const out = hits.map(project);

if (!out.length) {
    console.error('backlog_query: nothing matched.');
    process.exit(1);
}

if (has('--table')) {
    const cols = fields || INDEX_FIELDS;
    const cell = (v) => (Array.isArray(v) ? v.join(' ') : v == null ? '' : String(v)).replace(/\s+/g, ' ').slice(0, 60);
    const widths = cols.map((c) => Math.max(c.length, ...out.map((r) => cell(r[c]).length)));
    const row = (vals) => vals.map((v, i) => String(v).padEnd(widths[i])).join('  ');
    console.log(row(cols));
    console.log(widths.map((w) => '-'.repeat(w)).join('  '));
    for (const r of out) console.log(row(cols.map((c) => cell(r[c]))));
    console.error(`\n${out.length} item(s).`);
} else {
    console.log(JSON.stringify(out, null, 1));
}
