#!/usr/bin/env node
// req_store.js — the cold half of the requirements ledger's hot/cold split.
//
// requirements.json is the permanent requirement history: one group per promoted
// item, rows[] per requirement. By 2026-08-14 it was 556 KB / ~142K tokens, and
// 223 of its 232 groups were frozen history (complete/cancelled) — the same shape
// backlog.json had before archive_store.js, so this is the same split applied to
// the same problem: index stays hot, narrative moves to a dated cold file.
//
// Shape:
//   hot   docs/development/req/requirements.json
//           { brief, title, status, resolved, ...,
//             archived: "docs/development/archive/requirements-2026-Q3.json" }
//           (rows + resolution removed — they live in the cold record)
//   cold  docs/development/archive/requirements-<bucket>.json
//           { _schema, bucket, records: { "<brief>": { rows, resolution } } }
//
// Nothing is deleted: eviction is a move, and resolve() reassembles the group for
// any reader that wants the whole thing.
//
// Used by: archive_requirements.js (writes), requirements_query.js (reads).

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const ARCHIVE_DIR = 'docs/development/archive';
const REQ_REL = 'docs/development/req/requirements.json';
const REQ_PATH = path.join(ROOT, REQ_REL);

// The narrative fields — read when looking AT one group, never when looking ACROSS
// them. Everything else in a group is index and stays hot.
const NARRATIVE = ['rows', 'resolution'];

// Terminal group statuses, INCLUDING the undocumented legacy spellings the policy
// says to normalise on sight (REQUIREMENTS.md § Group object): `completed` is a
// duplicate of `complete`; `closed` was written once by the NR-075 retroactive cut
// audit. normaliseGroupStatus() maps both to `complete`.
const TERMINAL = new Set(['complete', 'completed', 'cancelled', 'closed']);

const normaliseGroupStatus = (s) =>
    (s === 'completed' || s === 'closed') ? 'complete' : s;
const normaliseRowStatus = (s) => (s === 'completed' ? 'complete' : s);

// Which cold file a group belongs in: the quarter it resolved in. Bucketing by
// date keeps the files stable — next quarter's archive run appends a new file
// rather than rewriting an old one.
function bucketFor(group) {
    const m = typeof group.resolved === 'string' ? group.resolved.match(/^(\d{4})-(\d{2})/) : null;
    if (!m) return 'undated';
    return `${m[1]}-Q${Math.floor((Number(m[2]) - 1) / 3) + 1}`;
}

const archivePath = (bucket) => `${ARCHIVE_DIR}/requirements-${bucket}.json`;

function loadArchive(relPath, root = ROOT) {
    const abs = path.join(root, relPath);
    if (!fs.existsSync(abs)) return null;
    return JSON.parse(fs.readFileSync(abs, 'utf8'));
}

function newArchive(bucket) {
    return {
        _schema: 'requirements-archive/io-v1',
        _note: [
            `Cold store for requirement groups resolved in ${bucket}.`,
            'Written by tools/session/archive_requirements.js; read by requirements_query.js.',
            'Records here are FROZEN history. The live index is docs/development/req/',
            'requirements.json, which points at this file via each group\'s `archived`',
            'field. To amend a landed group\'s rows or resolution, edit them HERE — the',
            'hot file no longer carries a copy.',
        ],
        bucket,
        records: {},
    };
}

// Restore a group's narrative fields from its cold file. Returns a new object; the
// input is untouched. A group with no cold record comes back unchanged.
function resolve(group, root = ROOT, cache = new Map()) {
    if (!group || !group.archived) return group;
    if (!cache.has(group.archived)) cache.set(group.archived, loadArchive(group.archived, root));
    const store = cache.get(group.archived);
    const rec = store && store.records && store.records[group.brief];
    if (!rec) return group;
    const out = { ...group, ...rec };
    delete out.archived;
    return out;
}

module.exports = {
    ROOT, ARCHIVE_DIR, REQ_REL, REQ_PATH, NARRATIVE, TERMINAL,
    normaliseGroupStatus, normaliseRowStatus,
    bucketFor, archivePath, loadArchive, newArchive, resolve,
};
