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

module.exports = {
    ROOT, ARCHIVE_DIR, NARRATIVE, INDEX_KEEP, TERMINAL,
    narrativeFieldsOf, isPointer, bucketFor, archivePath, loadArchive, newArchive, resolve, coldFraction,
};
