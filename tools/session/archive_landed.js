#!/usr/bin/env node
// archive_landed.js — move LANDED items out of backlog.json entirely.
//
// The second, deeper half of the hot/cold split. archive_designs.js evicts a landed
// item's PROSE and leaves its index row behind; this evicts the ROW. After it runs,
// backlog.json is the live worklist and nothing else.
//
// Ben, 2026-09-01, on being shown that 36% of the hot file was 49 landed rows'
// metadata: "Drop them."
//
// WHY THIS IS A SEPARATE TOOL AND NOT A FLAG ON archive_designs.js. Eviction of prose
// is safe for every reader, because the index it leaves behind is what readers index
// ON. Eviction of the row is NOT: four readers go blind, and one of them mints ids.
// A different blast radius deserves a different command and its own dry run.
//
// The four, and what was done about each (all now resolve through archive_store):
//   next_id.js     — unions archive_store.landedIds() into its max-id scan, on the
//                    working tree AND every ref. Its header records BL-326..BL-333
//                    each landing twice when this defence failed open; a dropped id
//                    it cannot see is an id it re-mints.
//   backlog_query  — unions landedItems() whenever a query could match landed work
//                    (--all, --status, --touches, explicit ids).
//   backlog_lint   — unions for the duplicate-id scan and `requires` resolution.
//   status.ps1     — reads the cold files for "DONE lately".
//
// Reversible: --restore puts every whole row back. Nothing is deleted, and the cold
// record keeps its prose alongside the index, so a restored row is the row that left.
//
// USAGE:
//   node tools/session/archive_landed.js --dry-run    what would move, and how many bytes
//   node tools/session/archive_landed.js              move it
//   node tools/session/archive_landed.js BL-123 ...   move only these items
//   node tools/session/archive_landed.js --restore    pull every whole row back
//
// EXIT: 0 on success; 1 if an id was named that does not exist or has not landed.

'use strict';
const fs = require('fs');
const path = require('path');
const A = require('./archive_store');

const ROOT = A.ROOT;
const BL_REL = 'docs/development/backlog.json';
const BL_PATH = path.join(ROOT, BL_REL);

const argv = process.argv.slice(2);
const dryRun = argv.includes('--dry-run');
const restore = argv.includes('--restore');
const only = new Set(argv.filter((a) => /^BL-\d+$/i.test(a)).map((a) => a.toUpperCase()));

const kb = (n) => `${(n / 1024).toFixed(1)} KB`;

// Write JSON back in the FILE'S OWN shape, not this tool's house style — the same
// rule (and the same reason) as archive_designs.js's writer: backlog.json is stored
// at 1-space indent with LF and the cold stores at 2 with CRLF, and normalising
// either buries a small change inside a whole-file diff.
const writeJson = (abs, data) => {
    fs.mkdirSync(path.dirname(abs), { recursive: true });
    let indent = 2;
    let crlf = false;
    let trailingNewline = true;
    if (fs.existsSync(abs)) {
        const prev = fs.readFileSync(abs, 'utf8');
        crlf = prev.includes('\r\n');
        trailingNewline = /\n$/.test(prev);
        const m = prev.replace(/\r\n/g, '\n').match(/^\{\n( +)"/);
        if (m) indent = m[1].length;
    }
    let out = JSON.stringify(data, null, indent);
    if (crlf) out = out.replace(/\n/g, '\r\n');
    if (trailingNewline) out += crlf ? '\r\n' : '\n';
    fs.writeFileSync(abs, out);
};

const backlog = JSON.parse(fs.readFileSync(BL_PATH, 'utf8'));
const before = fs.statSync(BL_PATH).size;

// ---------------------------------------------------------------------------
// --restore: every whole row in the cold store goes back into backlog.json.
// ---------------------------------------------------------------------------
if (restore) {
    const hot = new Set(backlog.items.map((i) => i.id));
    // RAW records, not A.landedItems(): that accessor strips `_row_keys` (it is this
    // tool's bookkeeping, not item data) and normalises key order for querying. Restore
    // needs exactly what it strips, so it reads the stores directly.
    const coming = [];
    for (const rel of A.archiveFiles(ROOT)) {
        const store = A.loadArchive(rel, ROOT);
        for (const [id, rec] of Object.entries((store && store.records) || {})) {
            if (!rec || typeof rec !== 'object' || rec.status === undefined) continue;
            if (hot.has(id) || (only.size && !only.has(id))) continue;
            coming.push({ ...rec, id, archived: rel });
        }
    }
    if (!coming.length) {
        console.log('Nothing to restore: every whole row in the cold store is already hot.');
        process.exit(0);
    }
    if (dryRun) {
        console.log(`dry run: ${coming.length} row(s) would return to ${BL_REL}.`);
        process.exit(0);
    }
    // Rebuild each row in its ORIGINAL shape and key order (see `_row_keys` at the
    // eviction site): the record is row-over-prose, and handing the whole merge back
    // would re-inline prose that archive_designs.js had already evicted.
    const rebuild = (rec) => {
        if (!Array.isArray(rec._row_keys)) {
            const { _row_keys, ...rest } = rec;
            return rest; // pre-_row_keys record: best effort, whole merge
        }
        const out = {};
        for (const k of rec._row_keys) if (rec[k] !== undefined) out[k] = rec[k];
        return out;
    };
    backlog.items = backlog.items.concat(coming.map(rebuild));
    // ROW CONTENT is restored exactly — every key, every value, in its original order.
    // ARRAY ORDER is normalised to numeric id, which is a deliberate difference: the
    // hot file's order is append-order and nothing records where a row sat, so this
    // sorts rather than pretending. The invariant the tool guarantees is per-row
    // identity, and the verification below asserts that and not byte-identity.
    backlog.items.sort((a, b) => parseInt(a.id.slice(3), 10) - parseInt(b.id.slice(3), 10));
    writeJson(BL_PATH, backlog);
    console.log(`Restored ${coming.length} row(s) into ${BL_REL}.`);
    console.log('The cold records are LEFT IN PLACE — a restore is a copy back, not a move,');
    console.log('so re-running the eviction is idempotent rather than destructive.');
    process.exit(0);
}

// ---------------------------------------------------------------------------
// Evict.
// ---------------------------------------------------------------------------
const unknown = [...only].filter((id) => !backlog.items.some((i) => i.id === id));
if (unknown.length) {
    console.error(`archive_landed: no such item in ${BL_REL}: ${unknown.join(', ')}`);
    process.exit(1);
}

const notLanded = [...only].filter((id) => {
    const it = backlog.items.find((i) => i.id === id);
    return it && !A.CLOSED.has(it.status);
});
if (notLanded.length) {
    console.error(`archive_landed: refusing to evict un-landed item(s): ${notLanded.join(', ')}`);
    console.error('An open item\'s row is the worklist. Only CLOSED items — delivered or cancelled — may leave.');
    process.exit(1);
}

const moving = backlog.items.filter((i) => A.CLOSED.has(i.status) && (!only.size || only.has(i.id)));
if (!moving.length) {
    console.log('Nothing to evict: no landed rows in the hot file.');
    process.exit(0);
}

const movedBytes = moving.reduce((n, i) => n + Buffer.byteLength(JSON.stringify(i)), 0);

if (dryRun) {
    const buckets = new Set(moving.map((i) => A.bucketFor(i)));
    console.log(`dry run: ${moving.length} row(s), ${kb(movedBytes)} would move into ${buckets.size} cold file(s).`);
    console.log(`${BL_REL} is ${kb(before)}; it would drop to roughly ${kb(before - movedBytes)}.`);
    console.log(`Remaining hot: ${backlog.items.length - moving.length} open item(s).`);
    process.exit(0);
}

// Merge each whole row into its bucket's record, keeping any prose already there.
// The record becomes { ...prose, ...index } — index last so a row's own fields win
// over a stale prose copy of the same key.
const stores = new Map();
for (const item of moving) {
    const rel = A.archivePath(A.bucketFor(item));
    if (!stores.has(rel)) stores.set(rel, A.loadArchive(rel, ROOT) || A.newArchive(A.bucketFor(item)));
    const store = stores.get(rel);
    const prev = store.records[item.id] || {};
    const row = { ...item };
    // `archived` is the hot file's pointer AT the cold file; inside the cold file it
    // is noise, and leaving it would make a restored row look already-evicted.
    delete row.archived;
    // `_row_keys` is what makes --restore a TRUE INVERSE, and both halves of it were
    // found by round-tripping rather than reasoned about:
    //   1. The record is a MERGE of this row over any prose archive_designs.js left
    //      here earlier. Without a record of which keys were the row, a restore hands
    //      the prose back inline too — silently undoing the earlier archival pass.
    //   2. JSON objects have no canonical key order, and rebuilding one from a spread
    //      put `id` first where the file has it seventeenth. Semantically identical,
    //      and it rewrites all 49 rows in the diff — the exact whole-file-churn
    //      problem archive_designs.js's own writer comment exists to avoid.
    // Keys of the ORIGINAL item, `archived` included at its own position — restore
    // fills that one from the cold file's path rather than from the record.
    store.records[item.id] = { ...prev, ...row, _row_keys: Object.keys(item) };
}

for (const [rel, store] of stores) {
    if (!Array.isArray(store._note)) store._note = [];
    const marker = 'Whole ROWS live here since 2026-09-01 (archive_landed.js), not just prose:';
    if (!store._note.some((l) => l.startsWith('Whole ROWS live here'))) {
        store._note.push(marker,
            'backlog.json is the live worklist and no longer carries landed items at all.',
            'Readers reach these through archive_store.landedItems()/landedIds().',
            'Reversible with: node tools/session/archive_landed.js --restore');
    }
    writeJson(path.join(ROOT, rel), store);
}

const evicted = new Set(moving.map((i) => i.id));
backlog.items = backlog.items.filter((i) => !evicted.has(i.id));
writeJson(BL_PATH, backlog);

const after = fs.statSync(BL_PATH).size;
console.log(`Evicted ${moving.length} landed row(s) into ${stores.size} cold file(s).`);
console.log(`${BL_REL}: ${kb(before)} → ${kb(after)}  (−${kb(before - after)}, ${Math.round(100 * (1 - after / before))}% smaller)`);
console.log(`Hot file now holds ${backlog.items.length} open item(s) and nothing else.`);

// ---------------------------------------------------------------------------
// Verify, before anyone trusts the smaller file. Two properties, and the second
// was earned: the first three attempts at this tool all passed an id check while
// handing back a DIFFERENT row — prose re-inlined, `archived` dropped, keys
// reordered. An eviction that cannot prove its own inverse is a deletion.
// ---------------------------------------------------------------------------
const ids = A.landedIds(ROOT);
const lost = [...evicted].filter((id) => !ids.has(id));
if (lost.length) {
    console.error(`\n!!! ${lost.length} id(s) did not land in the cold store: ${lost.join(', ')}`);
    console.error('    Restore immediately: node tools/session/archive_landed.js --restore');
    process.exit(1);
}

const wasById = new Map(moving.map((i) => [i.id, JSON.stringify(i)]));
const mismatched = [];
for (const rel of A.archiveFiles(ROOT)) {
    const store = A.loadArchive(rel, ROOT);
    for (const [id, rec] of Object.entries((store && store.records) || {})) {
        if (!wasById.has(id) || !Array.isArray(rec._row_keys)) continue;
        const rebuilt = {};
        for (const k of rec._row_keys) {
            const v = k === 'archived' ? rel : rec[k];
            if (v !== undefined) rebuilt[k] = v;
        }
        if (JSON.stringify(rebuilt) !== wasById.get(id)) mismatched.push(id);
    }
}
if (mismatched.length) {
    console.error(`\n!!! ${mismatched.length} row(s) do NOT rebuild to what they were: ${mismatched.slice(0, 8).join(', ')}`);
    console.error('    --restore would hand back a different item. Restore now and do not commit.');
    process.exit(1);
}
console.log(`verify: all ${moving.length} evicted rows rebuild EXACTLY — every key, value and key order.`);
console.log('        (--restore normalises ARRAY order to numeric id; row content is identical.)');
