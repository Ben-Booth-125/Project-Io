#!/usr/bin/env node
// archive_sprints.js — move COMPLETED sprints out of docs/development/sprints.json.
//
// Ben, 2026-08-24: sprints live fully in JSON, and completed ones are archived.
// Same hot/cold move as archive_reviews.js / archive_designs.js: a sprint whose
// state is terminal (closed, superseded, subsumed, retro-recorded, landed) goes
// whole into docs/development/archive/sprints-<quarter>.json, bucketed by the
// quarter it closed in (falling back to when it opened), and the hot file keeps
// only open / proposed / gated sprints. Records are FROZEN — the entry verbatim,
// retro included.
//
// Open sprints are never touched. Reversible by hand: the cold record is the
// entry verbatim. Re-render the mirror afterwards:
//   node tools/session/render_sprints.js
//
// USAGE:
//   node tools/session/archive_sprints.js --dry-run
//   node tools/session/archive_sprints.js
//   node tools/session/archive_sprints.js 19 "18 retro" ...   only these (must be terminal)
// EXIT: 0 on success; 1 if a named id is missing or not terminal.

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const HOT_REL = 'docs/development/sprints.json';
const HOT = path.join(ROOT, HOT_REL);
const ARCHIVE_DIR = 'docs/development/archive';

// Keep in sync with render_sprints.js.
const TERMINAL = new Set(['closed', 'superseded', 'subsumed', 'retro-recorded', 'landed']);

const argv = process.argv.slice(2);
const dryRun = argv.includes('--dry-run');
const only = new Set(argv.filter((a) => a !== '--dry-run'));
const kb = (n) => `${(n / 1024).toFixed(1)} KB`;

const bucketFor = (sp) => {
    const stamp = sp.closed_date || sp.opened_date || '';
    const m = typeof stamp === 'string' ? stamp.match(/^(\d{4})-(\d{2})/) : null;
    return m ? `${m[1]}-Q${Math.floor((Number(m[2]) - 1) / 3) + 1}` : 'undated';
};
const archivePath = (bucket) => `${ARCHIVE_DIR}/sprints-${bucket}.json`;

// Preserve the file's own indent / line ending (same reasoning as archive_designs.js).
const writeJson = (abs, data) => {
    let indent = 2, crlf = false, nl = true;
    if (fs.existsSync(abs)) {
        const prev = fs.readFileSync(abs, 'utf8');
        crlf = prev.includes('\r\n');
        nl = /\n$/.test(prev);
        const m = prev.replace(/\r\n/g, '\n').match(/^\{\n( +)"/);
        if (m) indent = m[1].length;
    }
    let out = JSON.stringify(data, null, indent);
    if (crlf) out = out.replace(/\n/g, '\r\n');
    if (nl) out += crlf ? '\r\n' : '\n';
    fs.writeFileSync(abs, out);
};

const hot = JSON.parse(fs.readFileSync(HOT, 'utf8'));
const before = fs.statSync(HOT).size;
let bad = 0;
for (const id of only) {
    const sp = hot.sprints.find((s) => s.id === id);
    if (!sp) { console.error(`  MISS  ${id}`); bad++; }
    else if (!TERMINAL.has(sp.state)) { console.error(`  OPEN  ${id} — still "${sp.state}", refusing`); bad++; }
}

const stores = new Map();
const keep = [];
let moved = 0, bytes = 0;
for (const sp of hot.sprints) {
    const take = TERMINAL.has(sp.state) && (!only.size || only.has(sp.id));
    if (!take) { keep.push(sp); continue; }
    const bucket = bucketFor(sp);
    const rel = archivePath(bucket);
    if (!stores.has(rel)) {
        const abs = path.join(ROOT, rel);
        stores.set(rel, fs.existsSync(abs) ? JSON.parse(fs.readFileSync(abs, 'utf8')) : {
            _schema: 'sprints-archive/io-v1',
            _note: [
                `Cold store for sprints.json entries completed in ${bucket}.`,
                'Written by tools/session/archive_sprints.js. Records are FROZEN: the sprint entry',
                'verbatim, retro and notes included. The live store is docs/development/sprints.json,',
                'which holds open/proposed/gated sprints only. Sprint NUMBERS may be reused after a',
                'purge (Ben, 2026-08-24) — a key here can collide with a later sprint of the same id,',
                'so colliding keys carry their date; the id field inside the record is authoritative.',
            ],
            bucket,
            records: {},
        });
    }
    const records = stores.get(rel).records;
    // Freed numbers may be re-used; on collision the later record keys by date too.
    let key = sp.id;
    if (records[key]) key = `${sp.id} (${sp.closed_date || sp.opened_date || 'undated'})`;
    records[key] = sp;
    const size = Buffer.byteLength(JSON.stringify(sp));
    bytes += size; moved++;
    console.log(`  COLD  ${String(sp.id).padEnd(9)} ${kb(size).padStart(9)}  → ${rel}  ${(sp.theme || '').slice(0, 55)}`);
}

if (dryRun) {
    console.log(`\ndry run: ${moved} sprint(s), ${kb(bytes)} would move into ${stores.size} cold file(s).`);
    console.log(`${HOT_REL} is ${kb(before)}; it would drop to roughly ${kb(before - bytes)}.`);
    process.exit(bad ? 1 : 0);
}

for (const [rel, store] of stores) writeJson(path.join(ROOT, rel), store);
hot.sprints = keep;
writeJson(HOT, hot);
const after = fs.statSync(HOT).size;
console.log(`\nArchived ${moved} sprint(s) into ${stores.size} cold file(s).`);
console.log(`${HOT_REL}: ${kb(before)} → ${kb(after)}  (−${kb(before - after)}, ${((1 - after / before) * 100).toFixed(0)}% smaller)`);
console.log('Re-render the mirror: node tools/session/render_sprints.js');
process.exit(bad ? 1 : 0);
