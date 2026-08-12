#!/usr/bin/env node
// archive_designs.js — move the narrative prose of LANDED items out of backlog.json.
//
// The write half of the hot/cold split (see archive_store.js for the why and the shape).
// A terminal item's design prose, resolution, completion note and progress notes are
// frozen history: read when you look at that one item, never when you look across the
// backlog. This moves them into docs/development/archive/backlog-design-<quarter>.json
// and leaves the item's existing `@<doc>` design pointer behind.
//
// Reversible: --restore puts a bucket (or everything) back. Nothing is deleted.
//
// USAGE:
//   node tools/session/archive_designs.js --dry-run    what would move, and how many bytes
//   node tools/session/archive_designs.js              move it
//   node tools/session/archive_designs.js BL-123 ...   move only these items
//   node tools/session/archive_designs.js --restore    pull every archived record back
//
// Run it as part of the Delivery close step, right after ship_items.js.
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
const only = new Set(argv.filter((a) => /^BL-\d+$/.test(a)));

const kb = (n) => `${(n / 1024).toFixed(1)} KB`;
const backlog = JSON.parse(fs.readFileSync(BL_PATH, 'utf8'));
const before = fs.statSync(BL_PATH).size;

// Write JSON back in the FILE'S OWN shape, not this tool's house style (NR-109).
// backlog.json is stored at 1-space indent with LF; the cold stores are at 2 with
// CRLF. Serialising everything at 2/LF made every archive run rewrite all ~7,500
// lines of backlog.json, which buried the real change inside a whole-file diff and
// made a merge conflict near-certain. Existing file wins; new files get 2-space.
const writeJson = (abs, data) => {
    fs.mkdirSync(path.dirname(abs), { recursive: true });

    let indent = 2;
    let crlf = false;
    let trailingNewline = true;
    if (fs.existsSync(abs)) {
        const prev = fs.readFileSync(abs, 'utf8');
        crlf = prev.includes('\r\n');
        trailingNewline = /\n$/.test(prev);
        // The first nested key's leading run of spaces is the file's indent unit.
        const m = prev.replace(/\r\n/g, '\n').match(/^\{\n( +)"/);
        if (m) indent = m[1].length;
    }

    let out = JSON.stringify(data, null, indent);
    if (crlf) out = out.replace(/\n/g, '\r\n');
    if (trailingNewline) out += crlf ? '\r\n' : '\n';
    fs.writeFileSync(abs, out);
};

if (restore) {
    doRestore();
} else {
    doArchive();
}

// ---- archive ----------------------------------------------------------------

function doArchive() {
    const stores = new Map();   // relPath -> archive object
    let moved = 0;
    let bytes = 0;
    let bad = 0;

    for (const id of only) {
        const item = backlog.items.find((i) => i.id === id);
        if (!item) { console.error(`  MISS  ${id} — not in ${BL_REL}`); bad++; continue; }
        if (!A.TERMINAL.has(item.status)) {
            console.error(`  OPEN  ${id} — status "${item.status}" is not terminal; refusing to freeze a live item`);
            bad++;
        }
    }

    for (const item of backlog.items) {
        if (only.size && !only.has(item.id)) continue;
        if (!A.TERMINAL.has(item.status)) continue;

        // Collect whatever narrative this item actually carries. A legacy "@BACKLOG.md"
        // design is already a pointer — leave it alone, but still move its neighbours.
        const rec = {};
        for (const f of A.NARRATIVE) {
            if (item[f] == null) continue;
            if (f === 'design' && A.isPointer(item[f])) continue;
            rec[f] = item[f];
        }
        if (!Object.keys(rec).length) continue;

        const rel = A.archivePath(A.bucketFor(item));
        if (!stores.has(rel)) {
            stores.set(rel, A.loadArchive(rel) || A.newArchive(A.bucketFor(item)));
        }
        const store = stores.get(rel);
        store.records[item.id] = { ...(store.records[item.id] || {}), ...rec };

        const size = Buffer.byteLength(JSON.stringify(rec));
        bytes += size;
        moved++;
        console.log(`  COLD  ${item.id}  ${kb(size).padStart(9)}  → ${rel}  ${item.short_name || ''}`);

        if (dryRun) continue;
        for (const f of Object.keys(rec)) delete item[f];
        item.design = `@${rel}`;
        item.archived = rel;
    }

    if (dryRun) {
        console.log(`\ndry run: ${moved} item(s), ${kb(bytes)} would move into ${stores.size} cold file(s).`);
        console.log(`${BL_REL} is ${kb(before)}; it would drop to roughly ${kb(before - bytes)}.`);
        process.exit(bad ? 1 : 0);
    }

    for (const [rel, store] of stores) {
        const keys = Object.keys(store.records).sort();
        store.records = Object.fromEntries(keys.map((k) => [k, store.records[k]]));
        writeJson(path.join(ROOT, rel), store);
    }
    writeJson(BL_PATH, backlog);

    const after = fs.statSync(BL_PATH).size;
    console.log(`\nArchived ${moved} item(s) into ${stores.size} cold file(s).`);
    // Signed, so a file that GREW does not report as "−-11.4 KB, -1% smaller" (NR-109).
    const delta = before - after;
    const dir = delta >= 0 ? 'smaller' : 'larger';
    console.log(`${BL_REL}: ${kb(before)} → ${kb(after)}  (${delta >= 0 ? '−' : '+'}${kb(Math.abs(delta))}, ${Math.abs((1 - after / before) * 100).toFixed(0)}% ${dir})`);
    verify();
    process.exit(bad ? 1 : 0);
}

// ---- restore ----------------------------------------------------------------

function doRestore() {
    let n = 0;
    const cache = new Map();
    for (const item of backlog.items) {
        if (!item.archived) continue;
        if (only.size && !only.has(item.id)) continue;
        const merged = A.resolve(item, ROOT, cache);
        if (merged === item) {
            console.error(`  DEAD  ${item.id} — archived pointer ${item.archived} has no record`);
            continue;
        }
        Object.assign(item, merged);
        delete item.archived;
        n++;
    }
    if (dryRun) { console.log(`dry run: would restore ${n} item(s).`); process.exit(0); }
    writeJson(BL_PATH, backlog);
    console.log(`Restored ${n} item(s) into ${BL_REL} (${kb(fs.statSync(BL_PATH).size)}). Cold files left in place.`);
}

// ---- round-trip check -------------------------------------------------------
// Re-read from disk and confirm every archived item resolves back to something with
// the fields we removed. A silent half-write here would look like data loss.
function verify() {
    const fresh = JSON.parse(fs.readFileSync(BL_PATH, 'utf8'));
    const cache = new Map();
    const broken = [];
    for (const item of fresh.items) {
        if (!item.archived) continue;
        const back = A.resolve(item, ROOT, cache);
        if (back === item || !A.NARRATIVE.some((f) => back[f] != null)) broken.push(item.id);
    }
    if (broken.length) {
        console.error(`\nVERIFY FAILED — ${broken.length} item(s) do not resolve: ${broken.join(', ')}`);
        process.exit(1);
    }
    console.log('verify: every archived item resolves back through archive_store.resolve().');
}
