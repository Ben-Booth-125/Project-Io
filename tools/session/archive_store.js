#!/usr/bin/env node
// archive_store.js — the cold half of the backlog's hot/cold split.
//
// backlog.json is the canonical METADATA INDEX. It had also become the permanent
// home of every item's narrative prose, including for work that landed months ago:
// of the 1.25 MB file, ~44% was the frozen history of 176 `complete` items. Every
// reader — gyre.py, backlog_view.js, the linter, a language agent answering "what's
// left?" — paid for all of it to reach 30 KB of live metadata.
//
// CLAUDE.md already states the rule this module enforces physically: authority
// TIME-SLICES. backlog.json owns the item while it is open; the subject's authority
// doc owns it once the work lands. So on landing, the narrative moves out of the hot
// file into a dated cold file, leaving the existing `@<doc>` design pointer in its
// place (a convention backlog.json's own _note already blesses).
//
// Shape:
//   hot   docs/development/backlog.json
//           { id, short_name, status, priority, files, summary, ...,
//             design: "@docs/development/archive/backlog-design-2026-Q3.json",
//             archived: "docs/development/archive/backlog-design-2026-Q3.json" }
//   cold  docs/development/archive/backlog-design-<bucket>.json
//           { _schema, bucket, records: { "BL-008": { design, resolution, ... } } }
//         docs/development/archive/backlog-{complete,cancelled,purged}-<date>.json
//           { _schema, items: [ { id, status, files, ... }, ... ] }
//         The second shape is older — the 2026-08 purge and sprint-close sweeps
//         predate the eviction store. Both are read; see § TWO SHAPES below.
//
// Nothing is lost and nothing is rewritten: eviction is a move, and resolve() puts
// the item back together for any reader that wants the whole thing.
//
// Used by: archive_designs.js (writes), backlog_query.js and backlog_view.js (read).

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const ARCHIVE_DIR = 'docs/development/archive';

// The narrative fields — prose a reader wants only when looking AT one item, never
// when looking ACROSS them. Everything else in an item is index and stays hot.
const NARRATIVE = ['design', 'resolution', 'completion_note', 'progress_note',
    // Widened 2026-08-23 (archival pass): `summary` is prose too — it was 184 KB of the
    // hot file across 324 landed items, none of it read when looking ACROSS the backlog.
    'summary', 'completion', 'progress'];

// Fields that are INDEX and must stay hot whatever their length.
const INDEX_KEEP = new Set(['id', 'short_name', 'title', 'status', 'priority', 'difficulty',
    'category', 'version_goal', 'files', 'touches', 'authority_doc', 'requires', 'blocked_on',
    'waits_on', 'parked', 'glyph', 'written', 'created', 'resolved', 'completed', 'raised',
    'superseded_by', 'archived', 'design']);

// A landed item sometimes grows an ad-hoc prose key (`measurement_r5`, `dead_start_fix`,
// ...) that no schema names. Anything non-index and longer than this is narrative.
const ADHOC_PROSE_MIN = 300;

// The narrative fields THIS item carries: the named set plus any ad-hoc long string.
function narrativeFieldsOf(item) {
    const out = new Set(NARRATIVE.filter((f) => item[f] != null));
    for (const [k, v] of Object.entries(item)) {
        if (INDEX_KEEP.has(k) || out.has(k)) continue;
        if (typeof v === 'string' && v.length >= ADHOC_PROSE_MIN) out.add(k);
        else if (Array.isArray(v) && JSON.stringify(v).length >= ADHOC_PROSE_MIN && v.every((x) => typeof x === 'string')) out.add(k);
    }
    return [...out];
}

// Terminal statuses. Kept in sync with the TERMINAL sets in gyre.py, backlog_view.js,
// backlog_lint.js, story_check.js, status.ps1 — `shipped` is the tolerated legacy value.
const TERMINAL = new Set(['complete', 'shipped']);

// CLOSED = no longer open work. TERMINAL = delivered. The distinction matters and is
// not pedantry: a cancelled item is closed but was never built, so counting it in
// "71% delivered" would inflate the one number that says how much of the design
// exists. Use CLOSED to decide whether something is still on the worklist; use
// TERMINAL only for delivery statistics.
const CANCELLED = new Set(['cancelled', 'purged', 'superseded']);
const CLOSED = new Set([...TERMINAL, ...CANCELLED]);

// A `design` value of "@something" is a POINTER, not prose (backlog.json _note:
// "design: \"@<doc>\" is also valid as a pointer to an authority doc"). Legacy items
// carry "@BACKLOG.md"; archived items carry "@docs/development/archive/...".
const isPointer = (v) => typeof v === 'string' && v.startsWith('@');

// Which cold file an item belongs in: the quarter it landed in, falling back to when
// it was written. Bucketing by date (not by size) keeps the files stable — archiving
// again next quarter appends a new file rather than rewriting an old one.
function bucketFor(item) {
    // FOUR NAMES, because the filing convention has used four (Ben, 2026-08-28).
    // `resolved`/`written` are what this read for; `completed`/`created` are what
    // items have actually been filed with, and 31 archived records sat in an
    // `undated` bucket purely because of the mismatch (NR-709). Reading all four
    // is retroactively correct and costs nothing — the alternative was settling one
    // name and sweeping history, which is more risk for the same outcome.
    //
    // Landing dates come first: an item belongs in the quarter it LANDED, and only
    // falls back to when it was written if it never recorded a landing.
    const stamp = item.resolved || item.completed || item.written || item.created || null;
    const m = typeof stamp === 'string' ? stamp.match(/^(\d{4})-(\d{2})/) : null;
    if (!m) return 'undated';
    return `${m[1]}-Q${Math.floor((Number(m[2]) - 1) / 3) + 1}`;
}

const archivePath = (bucket) => `${ARCHIVE_DIR}/backlog-design-${bucket}.json`;

function loadArchive(relPath, root = ROOT) {
    const abs = path.join(root, relPath);
    if (!fs.existsSync(abs)) return null;
    return JSON.parse(fs.readFileSync(abs, 'utf8'));
}

function newArchive(bucket) {
    return {
        _schema: 'backlog-archive/io-v1',
        _note: [
            `Cold store for backlog items that landed in ${bucket}.`,
            'Written by tools/session/archive_designs.js; read by backlog_query.js --full.',
            'Records here are FROZEN history. The live index is docs/development/backlog.json,',
            'which points at this file via each item\'s `archived` field. To amend a landed',
            'item\'s prose, edit it HERE — the hot file no longer carries a copy.',
        ],
        bucket,
        records: {},
    };
}

// Restore an item's narrative fields from its cold file. Returns a new object; the
// input is untouched. A resolvable item with no cold record comes back unchanged.
function resolve(item, root = ROOT, cache = new Map()) {
    if (!item || !item.archived) return item;
    if (!cache.has(item.archived)) cache.set(item.archived, loadArchive(item.archived, root));
    const store = cache.get(item.archived);
    const rec = store && store.records && store.records[item.id];
    if (!rec) return item;
    const out = { ...item, ...rec };
    delete out.archived;
    return out;
}

// What fraction of a backlog's bytes is frozen history still sitting in the hot file.
// The signal backlog_lint.js gates on, and doc_weight.js reports.
function coldFraction(backlog) {
    let cold = 0;
    let total = 0;
    for (const item of backlog.items || []) {
        const bytes = Buffer.byteLength(JSON.stringify(item));
        total += bytes;
        if (!TERMINAL.has(item.status)) continue;
        for (const f of narrativeFieldsOf(item)) {
            if (f === 'design' && isPointer(item[f])) continue;
            cold += Buffer.byteLength(JSON.stringify(item[f]));
        }
    }
    return { cold, total, fraction: total ? cold / total : 0 };
}

// --- The WHOLE-ROW eviction (Ben, 2026-09-01: "drop them") -------------------
//
// archive_designs.js evicts a landed item's PROSE and leaves its index behind, so
// the hot file still answers "what shipped" and "what touches this doc". Dropping
// the index too makes backlog.json the LIVE WORKLIST and nothing else — but it
// also means four readers can no longer see a landed item in the hot file at all,
// and one of them is safety-critical:
//
//   next_id.js      — derives the next BL-id from the max id it can see. Its own
//                     header records BL-326..BL-333 each landing TWICE when this
//                     defence failed open. A dropped id it cannot see is an id it
//                     will re-mint.
//   backlog_query   — --touches <doc> is what CLAUDE.md names as the way to answer
//                     "is this built?". That is a question about LANDED work.
//   backlog_lint    — resolves `requires` targets, most of which are landed.
//   status.ps1      — "DONE lately".
//
// So the cold store stops being prose-only and becomes the whole record. These two
// accessors are how a reader gets the landed half back; every one of the four calls
// one of them.

// --- TWO SHAPES, ONE COLD STORE ---------------------------------------------
//
// The archive holds backlog rows in two file shapes, because the filing convention
// changed under it:
//
//   EVICTION STORE   backlog-design-<bucket>.json   { records: { "BL-008": {...} } }
//     What archive_designs.js and archive_landed.js write, one record per id. A
//     record carrying a `status` is a whole evicted ROW; one without is prose only.
//
//   SWEEPS           backlog-complete-<date>.json   { items: [ {...}, ... ] }
//                    backlog-cancelled-<date>.json
//                    backlog-purged-<date>.json
//     Verbatim snapshots taken at the 2026-08 purge and the sprint-close sweeps,
//     which predate archive_landed.js and use a top-level ARRAY. Every entry is a
//     whole row, and most carry the `files` array --touches reads.
//
// Both shapes are the same evidence and a reader wants both: CLAUDE.md routes "is
// this built?" at backlog_query --touches, and that question is answered by the
// whole archive or it is answered wrongly. The normalisation belongs here rather
// than in each caller.
//
// DE-DUPLICATION is by id, FIRST FILE WINS, and the order is fixed so the answer is
// predictable rather than filesystem-dependent:
//   1. the hot file        (allItems — a restore-in-progress or hand-edit lives there)
//   2. the eviction store  (the current mechanism, and the newest record of a row)
//   3. the sweeps, NEWEST FIRST (a later snapshot of an id is the later truth)

const DESIGN_RE = /^backlog-design-.*\.json$/;

// ANY cold backlog file that is not the eviction store is a SWEEP. Deliberately open
// rather than a list of the three families that exist today: the glob this block
// replaced was itself a one-word allow-list that fell behind the filing convention and
// cost the union 625 cold rows (609 net of ids the hot file still held), and a `backlog-superseded-2026-*.json` filed next quarter
// would repeat that failure exactly — gone from the union with no error and no test.
// An unrecognised family is CLASSIFIED below, loudly, rather than dropped or believed.
const SWEEP_RE = /^backlog-(?!design-)[a-z]+-.*\.json$/;
const familyOf = (f) => (path.basename(f).match(/^backlog-([a-z]+)-/) || [])[1] || '';

const coldDirFiles = (root, re) => {
    const dir = path.join(root, ARCHIVE_DIR);
    if (!fs.existsSync(dir)) return [];
    return fs.readdirSync(dir).filter((f) => re.test(f)).sort();
};

// The eviction store: the `records`-shaped files this module's writers own.
const designFiles = (root = ROOT) => coldDirFiles(root, DESIGN_RE).map((f) => `${ARCHIVE_DIR}/${f}`);

// The sweeps, NEWEST FIRST — ordered by the DATE in the name, not by the name, because
// a plain reverse sort ranks the family word ahead of the date and would put a purge
// snapshot above a later sprint close. On the same date a decision file (complete /
// cancelled) outranks a purge, which is a raw snapshot of whatever the hot file held.
// This is load-bearing: BL-599 and BL-600 name one pair of items in the 08-24 sweep and
// a different pair in the 08-26 one — an id collision the project resolved in favour of
// the later row, and the later row is what this order returns.
const SWEEP_RANK = { complete: 0, cancelled: 1, purged: 2 };
const sweepFiles = (root = ROOT) => coldDirFiles(root, SWEEP_RE)
    .map((f) => ({
        f,
        date: (f.match(/(\d{4}-\d{2}-\d{2})/) || [''])[0],
        rank: SWEEP_RANK[familyOf(f)] ?? 9,
    }))
    .sort((a, b) => b.date.localeCompare(a.date) || a.rank - b.rank || a.f.localeCompare(b.f))
    .map(({ f }) => `${ARCHIVE_DIR}/${f}`);

// --- THE FILE IS THE ASSERTION, NOT THE FROZEN FIELD -------------------------
//
// A swept row's `status` is FROZEN at the instant the sweep took it, and no sweep
// rewrote it on the way out. backlog-purged-2026-08-23.json holds 156 rows reading
// `designed` and 31 reading `design-owed` while its own _note says "Nothing here is
// open work"; backlog-cancelled-2026-08-31.json holds 17 more under "CANCELLED, NOT
// COMPLETED. Nothing here shipped." Believing those fields turns the false negative
// this module was widened to fix into a false positive of the same size: a `--open`
// search answering with 18 rows of work nobody is doing, which is the opposite
// failure and just as silent.
//
// THE RULE (BL-792, the cold union): a cold row's state comes from the FILE
// it is archived in, not from its own status field. backlog-purged-* and
// backlog-cancelled-* are CLOSED BY CONSTRUCTION — the file is the assertion, and the
// frozen field is an artefact of the row at the moment it was swept. backlog-complete-*
// is complete. Only the hot file and the eviction store carry a status worth reading.
//
// It is applied HERE, at the union, exactly once. Not in a view: the whole point of
// the widening is that callers trust the union, and a rule enforced in backlog_query.js
// alone leaves backlog_lint.js and doc_owner.js reading raw rows.
//
// A row that ALREADY reads closed keeps its own value: `complete` and `cancelled` are
// honest statements of HOW it closed, and overwriting them would move an item between
// the delivered count and the culled one. Only a row still claiming to be open is
// corrected — and the value it claimed is preserved as `status_filed`, beside an
// `archived_in` naming the file that made the call, so the frozen history stays
// readable and no row is normalised without provenance.
const SWEEP_STATE = { complete: 'complete', cancelled: 'cancelled', purged: 'purged' };

// A family SWEEP_STATE does not name is still cold, so it is read as CLOSED-not-built:
// wrong about HOW it closed is recoverable, wrong about WHETHER it is open is the
// defect above. It says so on stderr rather than deciding quietly.
const UNKNOWN_SWEEP_STATE = 'cancelled';
const warnedFamilies = new Set();
function sweepStateFor(rel) {
    const fam = familyOf(rel);
    if (SWEEP_STATE[fam]) return SWEEP_STATE[fam];
    if (!warnedFamilies.has(fam)) {
        warnedFamilies.add(fam);
        process.stderr.write(`archive_store: cold file family "${fam}" (${rel}) is not named in `
            + `SWEEP_STATE — its rows read as "${UNKNOWN_SWEEP_STATE}". Name it to say how it closed.
`);
    }
    return UNKNOWN_SWEEP_STATE;
}

// Every cold backlog file, in de-duplication precedence order. NO IN-TREE CALLER
// since the sweep families were split out - designFiles() and sweepFiles() are what
// the accessors below use, and archive_landed.js is deliberately scoped to the
// eviction store alone. Kept exported as the composed view a reader reaches for
// first; delete it only if nothing outside this repo has taken it up.
const archiveFiles = (root = ROOT) => [...designFiles(root), ...sweepFiles(root)];

// Every id the EVICTION STORE knows, whole-row or prose-only. Scoped to that store on
// purpose: archive_landed.js proves an eviction landed by looking its ids up here, and
// a sweep row is a historical snapshot, not proof that the write happened. Readers who
// want the whole history want coldItems()/allItems() instead.
function landedIds(root = ROOT) {
    const out = new Set();
    for (const rel of designFiles(root)) {
        const store = loadArchive(rel, root);
        for (const id of Object.keys((store && store.records) || {})) out.add(id);
    }
    return out;
}

// Every landed item the EVICTION STORE holds as a WHOLE ROW, reassembled. A record
// written by archive_designs.js alone carries prose and no `status`, so it is not an
// item and is skipped — only rows evicted by archive_landed.js come back here. Also
// scoped to that store: backlog_lint reads it to catch a row that is hot AND evicted,
// which is an eviction fault; a row that is hot and also in a sweep is an item legitimately
// re-opened after the purge, and is not.
function landedItems(root = ROOT) {
    const out = [];
    for (const rel of designFiles(root)) {
        const store = loadArchive(rel, root);
        for (const [id, rec] of Object.entries((store && store.records) || {})) {
            if (!rec || typeof rec !== 'object' || rec.status === undefined) continue;
            // `archived` is the hot row's pointer AT its cold file, and the eviction
            // strips it going in (inside the file it is noise). Put it back on the way
            // out, so a row read from cold is byte-identical to the row that left and
            // --restore is a true inverse. Verified by round-trip: without this, every
            // restored row came back one field short.
            // `_row_keys` is archive_landed.js's restore bookkeeping, not item data.
            const { _row_keys, ...row } = rec;
            out.push({ id, ...row, archived: rel });
        }
    }
    return out;
}

// Every whole row the SWEEPS hold, newest sweep first, with its STATE taken from the
// file per the rule above. Nothing else is reconstructed. In particular `archived` is
// left exactly as filed: a swept row either points at its own prose in the eviction
// store or carries that prose inline, and overwriting the pointer with the sweep's own
// path would send resolve() to a file that has no record for it.
function sweptItems(root = ROOT) {
    const out = [];
    for (const rel of sweepFiles(root)) {
        const state = sweepStateFor(rel);
        const store = loadArchive(rel, root);
        for (const row of (store && store.items) || []) {
            if (!row || typeof row !== 'object') continue;
            if (typeof row.id !== 'string' || row.status === undefined) continue;
            const it = { ...row, archived_in: rel };
            if (!CLOSED.has(it.status)) {
                it.status_filed = it.status;
                it.status = state;
            }
            out.push(it);
        }
    }
    return out;
}

// The whole cold half, both shapes, de-duplicated by id in precedence order.
function coldItems(root = ROOT) {
    const out = [];
    const seen = new Set();
    for (const it of landedItems(root).concat(sweptItems(root))) {
        if (seen.has(it.id)) continue;
        seen.add(it.id);
        out.push(it);
    }
    return out;
}

// Hot items plus every cold row that has left the hot file, de-duplicated with the
// HOT ROW WINNING. Hot wins because a restore-in-progress or a hand-edit lives there,
// and a stale cold copy must never shadow it.
function allItems(backlog, root = ROOT) {
    const hot = (backlog && backlog.items) || [];
    const seen = new Set(hot.map((i) => i.id));
    return hot.concat(coldItems(root).filter((i) => !seen.has(i.id)));
}

module.exports = {
    ROOT, ARCHIVE_DIR, NARRATIVE, INDEX_KEEP, TERMINAL, CANCELLED, CLOSED,
    narrativeFieldsOf, isPointer, bucketFor, archivePath, loadArchive, newArchive, resolve, coldFraction,
    designFiles, sweepFiles, sweepStateFor, archiveFiles, landedIds, landedItems, sweptItems, coldItems, allItems,
};
