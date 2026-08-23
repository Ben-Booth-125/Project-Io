#!/usr/bin/env node
// archive_reviews.js — move RESOLVED entries out of NEEDS_REVIEW.json.
//
// NEEDS_REVIEW.json is a live queue of open calls (its own _note, Ben's 2026-08-14
// ruling: transient, prune resolved entries promptly). Pruning by deletion loses the
// resolution text, which is sometimes the only place a delegated call was recorded.
// So this is the same hot/cold move archive_designs.js makes for the backlog: a
// resolved entry goes whole into docs/development/archive/needs-review-<quarter>.json,
// bucketed by the quarter it was resolved in (falling back to when it was raised),
// and the hot file keeps only open entries.
//
// Open entries are never touched. Reversible by hand: the cold record is the entry
// verbatim. Re-render the mirror afterwards: node tools/session/render_needs_review.js
//
// USAGE:
//   node tools/session/archive_reviews.js --dry-run
//   node tools/session/archive_reviews.js
//   node tools/session/archive_reviews.js NR-123 ...   only these (must be resolved)
// EXIT: 0 on success; 1 if a named id is missing or still open.

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const HOT_REL = 'docs/development/NEEDS_REVIEW.json';
const HOT = path.join(ROOT, HOT_REL);
const ARCHIVE_DIR = 'docs/development/archive';

const argv = process.argv.slice(2);
const dryRun = argv.includes('--dry-run');
const only = new Set(argv.filter((a) => /^NR-\d+$/.test(a)));
const kb = (n) => `${(n / 1024).toFixed(1)} KB`;

const bucketFor = (e) => {
    const stamp = e.resolved_on || e.resolved || e.raised || '';
    const m = typeof stamp === 'string' ? stamp.match(/^(\d{4})-(\d{2})/) : null;
    return m ? `${m[1]}-Q${Math.floor((Number(m[2]) - 1) / 3) + 1}` : 'undated';
};
const archivePath = (bucket) => `${ARCHIVE_DIR}/needs-review-${bucket}.json`;

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
    const e = hot.items.find((i) => i.id === id);
    if (!e) { console.error(`  MISS  ${id}`); bad++; }
    else if (e.status !== 'resolved') { console.error(`  OPEN  ${id} — still "${e.status}", refusing`); bad++; }
}

const stores = new Map();
const keep = [];
let moved = 0, bytes = 0;
for (const e of hot.items) {
    const take = e.status === 'resolved' && (!only.size || only.has(e.id));
    if (!take) { keep.push(e); continue; }
    const bucket = bucketFor(e);
    const rel = archivePath(bucket);
    if (!stores.has(rel)) {
        const abs = path.join(ROOT, rel);
        stores.set(rel, fs.existsSync(abs) ? JSON.parse(fs.readFileSync(abs, 'utf8')) : {
            _schema: 'needs-review-archive/io-v1',
            _note: [
                `Cold store for NEEDS_REVIEW.json entries resolved in ${bucket}.`,
                'Written by tools/session/archive_reviews.js. Records are FROZEN: the entry verbatim,',
                'resolution included. The live queue is docs/development/NEEDS_REVIEW.json, which',
                'holds open entries only.',
            ],
            bucket,
            records: {},
        });
    }
    stores.get(rel).records[e.id] = e;
    const size = Buffer.byteLength(JSON.stringify(e));
    bytes += size; moved++;
    console.log(`  COLD  ${e.id}  ${kb(size).padStart(8)}  → ${rel}  ${(e.title || '').slice(0, 60)}`);
}

if (dryRun) {
    console.log(`\ndry run: ${moved} entr(ies), ${kb(bytes)} would move into ${stores.size} cold file(s).`);
    console.log(`${HOT_REL} is ${kb(before)}; it would drop to roughly ${kb(before - bytes)}.`);
    process.exit(bad ? 1 : 0);
}

for (const [rel, store] of stores) {
    const ids = Object.keys(store.records).sort((a, b) => Number(a.slice(3)) - Number(b.slice(3)));
    store.records = Object.fromEntries(ids.map((k) => [k, store.records[k]]));
    writeJson(path.join(ROOT, rel), store);
}
hot.items = keep;
writeJson(HOT, hot);
const after = fs.statSync(HOT).size;
console.log(`\nArchived ${moved} entr(ies) into ${stores.size} cold file(s).`);
console.log(`${HOT_REL}: ${kb(before)} → ${kb(after)}  (−${kb(before - after)}, ${((1 - after / before) * 100).toFixed(0)}% smaller)`);
process.exit(bad ? 1 : 0);
