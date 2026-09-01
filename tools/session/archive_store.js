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

const archiveFiles = (root = ROOT) => {
    const dir = path.join(root, ARCHIVE_DIR);
    if (!fs.existsSync(dir)) return [];
    return fs.readdirSync(dir)
        .filter((f) => /^backlog-design-.*\.json$/.test(f))
        .sort()
        .map((f) => `${ARCHIVE_DIR}/${f}`);
};

// Every id the cold store knows, whole-row or prose-only. This is the set next_id
// and the duplicate-id scan must union with the hot file: an id is SPENT whether or
// not its row still sits in backlog.json.
function landedIds(root = ROOT) {
    const out = new Set();
    for (const rel of archiveFiles(root)) {
        const store = loadArchive(rel, root);
        for (const id of Object.keys((store && store.records) || {})) out.add(id);
    }
    return out;
}

// Every landed item the cold store holds as a WHOLE ROW, reassembled. A record
// written by archive_designs.js alone carries prose and no `status`, so it is not an
// item and is skipped — only rows evicted by archive_landed.js come back here.
function landedItems(root = ROOT) {
    const out = [];
    for (const rel of archiveFiles(root)) {
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

// Hot items plus the landed rows that have left the hot file, de-duplicated with the
// HOT ROW WINNING. Hot wins because a restore-in-progress or a hand-edit lives there,
// and a stale cold copy must never shadow it.
function allItems(backlog, root = ROOT) {
    const hot = (backlog && backlog.items) || [];
    const seen = new Set(hot.map((i) => i.id));
    return hot.concat(landedItems(root).filter((i) => !seen.has(i.id)));
}

module.exports = {
    ROOT, ARCHIVE_DIR, NARRATIVE, INDEX_KEEP, TERMINAL,
    narrativeFieldsOf, isPointer, bucketFor, archivePath, loadArchive, newArchive, resolve, coldFraction,
    archiveFiles, landedIds, landedItems, allItems,
};
