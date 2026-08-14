#!/usr/bin/env node
// requirements_query.js — retrieval primitive over requirements.json.
//
// Same principle as backlog_query.js: a language agent does not absorb a 550 KB
// history file to answer "which requirement groups are in flight?". It holds a
// compact index and fetches full records on demand — and --full transparently
// resolves groups whose rows/resolution moved to the cold store (req_store.js),
// so an archived group reads exactly like a hot one.
//
// USAGE:
//   node tools/session/requirements_query.js                       in-flight groups, index fields
//   node tools/session/requirements_query.js --status complete
//   node tools/session/requirements_query.js --grep lens           brief/title/resolution/row text
//   node tools/session/requirements_query.js scarcity-lens-render  one group, everything, prose resolved
//   node tools/session/requirements_query.js --failed              row-level: failed/partial rows
//   node tools/session/requirements_query.js --class visual        row-level: rows by verification class
//   node tools/session/requirements_query.js --fields brief,resolved,batch
//   node tools/session/requirements_query.js --count               how many match, nothing else
//   node tools/session/requirements_query.js --all                 include terminal groups
//
// Output is JSON on stdout (add --table for an aligned terminal listing).
// EXIT: 0 with results; 1 when nothing matches, so a caller can tell "none" from "empty".

'use strict';
const fs = require('fs');
const R = require('./req_store');

const INDEX_FIELDS = ['brief', 'title', 'status', 'resolved'];
const ROW_FIELDS = ['brief', 'id', 'status', 'requirement', 'verification'];

const argv = process.argv.slice(2);
const has = (f) => argv.includes(f);
const val = (f) => { const i = argv.indexOf(f); return i >= 0 ? argv[i + 1] : null; };
const list = (f) => { const v = val(f); return v ? v.split(',').map((s) => s.trim()).filter(Boolean) : null; };

if (has('--help') || has('-h')) {
    console.log(fs.readFileSync(__filename, 'utf8').split('\n').slice(1, 23).map((l) => l.replace(/^\/\/ ?/, '')).join('\n'));
    process.exit(0);
}

const flags = new Set(['--full', '--all', '--count', '--table', '--failed',
    '--status', '--grep', '--class', '--fields', '--batch']);
const takesValue = new Set(['--status', '--grep', '--class', '--fields', '--batch']);
const briefs = new Set();
for (let i = 0; i < argv.length; i++) {
    if (takesValue.has(argv[i])) { i++; continue; }
    if (!flags.has(argv[i])) briefs.add(argv[i]);
}

const full = has('--full') || briefs.size > 0;
const wantFields = list('--fields');
const statuses = list('--status');
const batch = val('--batch');
const grep = val('--grep');
const cls = val('--class');
const failedOnly = has('--failed');
const rowMode = failedOnly || !!cls;
// Row-level queries (--failed, --class) are historical analytics: they sweep the
// whole ledger unless a --status narrows them.
const showAll = has('--all') || briefs.size > 0 || rowMode;

const data = JSON.parse(fs.readFileSync(R.REQ_PATH, 'utf8'));
const cache = new Map();
// Row-level queries and grep need the narrative, so resolve lazily per group;
// the cache means at most one read per cold file.
const resolved = (g) => R.resolve(g, R.ROOT, cache);

let hits = data.groups.filter((g) => {
    if (briefs.size) return briefs.has(g.brief);
    const status = R.normaliseGroupStatus(g.status);
    if (!showAll && !statuses && R.TERMINAL.has(g.status)) return false;  // default view is live work
    if (statuses && !statuses.includes(status) && !statuses.includes(g.status)) return false;
    if (batch && g.batch !== batch) return false;
    if (grep) {
        const needle = grep.toLowerCase();
        const whole = resolved(g);
        const hay = [whole.brief, whole.title, whole.resolution,
            ...(whole.rows || []).map((r) => `${r.requirement} ${r.notes}`)]
            .filter(Boolean).join(' ').toLowerCase();
        if (!hay.includes(needle)) return false;
    }
    return true;
});

// Newest resolution first; in-flight (null resolved) groups lead. Ties on brief.
hits.sort((a, b) => (b.resolved || '9999').localeCompare(a.resolved || '9999') || (a.brief || '').localeCompare(b.brief || ''));

let out;
if (rowMode) {
    out = [];
    for (const g of hits) {
        for (const r of (resolved(g).rows || [])) {
            if (failedOnly && !['failed', 'partial'].includes(R.normaliseRowStatus(r.status))) continue;
            if (cls && !(r.verification || []).some((v) => v.split(':')[0].trim() === cls)) continue;
            out.push({ brief: g.brief, ...r });
        }
    }
    if (wantFields) out = out.map((r) => Object.fromEntries(wantFields.filter((f) => r[f] !== undefined).map((f) => [f, r[f]])));
} else {
    if (full) hits = hits.map((g) => resolved(g));
    const fields = wantFields || (full ? null : INDEX_FIELDS);
    out = hits.map((g) => (fields ? Object.fromEntries(fields.filter((f) => g[f] !== undefined).map((f) => [f, g[f]])) : g));
}

if (has('--count')) {
    console.log(JSON.stringify(rowMode
        ? { matched_rows: out.length, of_groups: data.groups.length }
        : { matched: out.length, of: data.groups.length }));
    process.exit(out.length ? 0 : 1);
}

if (!out.length) {
    console.error('requirements_query: nothing matched.');
    process.exit(1);
}

if (has('--table')) {
    const cols = wantFields || (rowMode ? ROW_FIELDS : INDEX_FIELDS);
    const cell = (v) => (Array.isArray(v) ? v.join(' ') : v == null ? '' : String(v)).replace(/\s+/g, ' ').slice(0, 70);
    const widths = cols.map((c) => Math.max(c.length, ...out.map((r) => cell(r[c]).length)));
    const row = (vals) => vals.map((v, i) => String(v).padEnd(widths[i])).join('  ');
    console.log(row(cols));
    console.log(widths.map((w) => '-'.repeat(w)).join('  '));
    for (const r of out) console.log(row(cols.map((c) => cell(r[c]))));
    console.error(`\n${out.length} ${rowMode ? 'row' : 'group'}(s).`);
} else {
    console.log(JSON.stringify(out, null, 1));
}
