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
// --summary sits between the two: the index fields plus the FIRST SENTENCE of design,
// one line of prose per item. It is the DEFAULT for --grep and --touches, the two
// sweeps that match many items at once and so would otherwise resolve thousands of
// words of cold design prose to answer a one-line question. --full overrides it.
//
// USAGE:
//   node tools/session/backlog_query.js                       every open item, index fields
//   node tools/session/backlog_query.js --status designed
//   node tools/session/backlog_query.js --priority S,SSS --open
//   node tools/session/backlog_query.js --version v0.1.0
//   node tools/session/backlog_query.js --category Canvas --touches src/ui/
//   node tools/session/backlog_query.js --grep selection      id/short_name/title/summary match,
//                                                             across landed work too (add --open for
//                                                             only what is still on the worklist)
//   node tools/session/backlog_query.js --summary             index fields + one line of design
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
    console.log(fs.readFileSync(__filename, 'utf8').split('\n').slice(1, 34).map((l) => l.replace(/^\/\/ ?/, '')).join('\n'));
    process.exit(0);
}

const ids = new Set(argv.filter((a) => /^BL-\d+$/i.test(a)).map((a) => a.toUpperCase()));
const full = has('--full');
const wantFields = list('--fields');
const summaryFlag = has('--summary');
const statuses = list('--status');
const priorities = list('--priority');
const categories = list('--category');
const version = val('--version');
const touches = val('--touches');
const grep = val('--grep');
const openOnly = has('--open');

// --grep AND --touches ARE SEARCHES, NOT WORKLIST VIEWS. The LIST views — a bare
// invocation, --status, --priority — output open work, so they drop terminal items by
// default. A search does not, and for the same reason in both cases: DELIVERY.md makes
// --grep the first step before authoring an item, to catch a subject the project has
// already shipped, and CLAUDE.md names --touches <doc> as the way to answer "is this
// built?" — a question ABOUT landed work. A search that hides everything shipped is
// blind to exactly the case each exists for. So both match across the union and print
// what they matched, terminal items included, with `status` carrying the distinction.
// `status` can carry it because archive_store.js normalises a cold row's state from
// the FILE it is archived in (§ THE FILE IS THE ASSERTION) — the sweeps froze the field
// at the moment they took the row, so believing it would make --open answer with work
// nobody is doing. A narrower search is available, but only by asking for it
// (--grep --open); it is never a silent drop.
const showAll = has('--all') || ids.size > 0 || ((!!grep || !!touches) && !openOnly);

// --grep and --touches are the many-item sweeps, so they summarise unless asked not to.
// --full and an explicit --fields both override; an explicit --summary turns it on anywhere.
const summary = !full && !wantFields && (summaryFlag || !!grep || !!touches);

const backlog = JSON.parse(fs.readFileSync(BL_PATH, 'utf8'));
const cache = new Map();

// LANDED WORK NO LONGER LIVES IN THE HOT FILE (archive_landed.js, 2026-09-01), so a
// query that could match a shipped item has to union the cold store back in.
//
// LAZILY, because the cold store is ~1.5 MB and the common query ("what's open at
// priority A?") cannot match a landed item by construction — the default view drops
// terminal items a few lines below. Four cases can:
//   --all / an explicit BL-id  — asked for everything, or for one item by name.
//   --status complete|shipped  — asked for landed work directly.
//   --touches <doc>            — CLAUDE.md names this as how to answer "is this
//                                built?", which is a question ABOUT landed work.
//   --grep                     — a search that silently skipped everything shipped
//                                would be worse than no search.
const wantsLanded = showAll || ids.size > 0 || !!touches || !!grep
    || (statuses || []).some((s) => A.CLOSED.has(s));
const pool = wantsLanded ? A.allItems(backlog, A.ROOT) : backlog.items;

let hits = pool.filter((it) => {
    if (ids.size) return ids.has(it.id);
    const terminal = A.CLOSED.has(it.status);
    if (openOnly && terminal) return false;
    if (!showAll && !statuses && terminal) return false;   // default view is live work
    if (statuses && !statuses.includes(it.status)) return false;
    if (priorities && !priorities.includes(it.priority)) return false;
    if (categories && !categories.includes(it.category)) return false;
    if (version && it.version_goal !== version) return false;
    if (touches && !(it.files || []).some((f) => f.includes(touches))) return false;
    if (grep) {
        const needle = grep.toLowerCase();
        // A landed item's summary is cold (archival pass 2026-08-23); pull it back for the match.
        const src = it.archived ? A.resolve(it, A.ROOT, cache) : it;
        const hay = [src.id, src.short_name, src.title, src.summary].filter(Boolean).join(' ').toLowerCase();
        if (!hay.includes(needle)) return false;
    }
    return true;
});

// Priority order runs SSS > S > A > B > C > F; ties fall back to id so output is stable.
const RANK = { SSS: 0, S: 1, A: 2, B: 3, C: 4, F: 5 };
hits.sort((a, b) => (RANK[a.priority] ?? 9) - (RANK[b.priority] ?? 9) || a.id.localeCompare(b.id));

if (has('--count')) {
    console.log(JSON.stringify({ matched: hits.length, of: pool.length }));
    process.exit(hits.length ? 0 : 1);
}

// --fields naming a cold field (summary, design ...) resolves too, not only --full.
const needsCold = full || summary || (wantFields || []).some((f) => A.NARRATIVE.includes(f));
if (needsCold) hits = hits.map((it) => A.resolve(it, A.ROOT, cache));

// First sentence of the design block. Falls back to summary, then title, then nothing —
// a stub item with no prose still prints its row rather than an empty field.
const firstSentence = (it) => {
    // A cold field can still be an unresolved '@path' pointer into a second archive
    // file; that is a location, not prose, so fall past it to the next best source.
    const usable = (v) => typeof v === 'string' && v.trim() && !v.trim().startsWith('@');
    const src = [it.design, it.summary, it.title].find(usable);
    if (!src) return null;
    const text = src.trim().replace(/\s+/g, ' ');
    const m = text.match(/^.*?[.!?]["'”’)\]]?(?=\s|$)/);
    const line = m ? m[0] : text;                 // no terminator: the whole (short) text
    return line.length > 240 ? `${line.slice(0, 237)}...` : line;
};

const SUMMARY_FIELDS = [...INDEX_FIELDS, 'line'];
const fields = wantFields || (full ? null : summary ? SUMMARY_FIELDS : INDEX_FIELDS);
const project = (it) => {
    const src = summary ? { ...it, line: firstSentence(it) } : it;
    return fields ? Object.fromEntries(fields.filter((f) => src[f] !== undefined).map((f) => [f, src[f]])) : src;
};
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
