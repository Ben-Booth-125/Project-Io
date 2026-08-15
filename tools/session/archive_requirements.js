#!/usr/bin/env node
// archive_requirements.js — move the rows/resolution of RESOLVED requirement groups
// out of requirements.json.
//
// The write half of the requirements hot/cold split (see req_store.js for the why
// and the shape; archive_designs.js is the model). A terminal group's rows and
// resolution are frozen history: read when you look at that one group, never when
// you look across them. This moves them into
// docs/development/archive/requirements-<quarter>.json and leaves the group's
// index fields behind with an `archived` pointer.
//
// It also NORMALISES legacy statuses as it goes, per REQUIREMENTS.md § Group
// object ("normalise on sight"): group `completed`/`closed` → `complete`, row
// `completed` → `complete`.
//
// Reversible: --restore puts everything back. Nothing is deleted.
//
// USAGE:
//   node tools/session/archive_requirements.js --dry-run     what would move, and how many bytes
//   node tools/session/archive_requirements.js               move it
//   node tools/session/archive_requirements.js <brief> ...   move only these groups
//   node tools/session/archive_requirements.js --restore     pull every archived record back
//
// Run it as part of the Delivery close step, after flush_requirements.js.
// EXIT: 0 on success; 1 if a named brief does not exist or has not resolved.

'use strict';
const fs = require('fs');
const path = require('path');
const R = require('./req_store');

const argv = process.argv.slice(2);
const dryRun = argv.includes('--dry-run');
const restore = argv.includes('--restore');
const only = new Set(argv.filter((a) => !a.startsWith('--')));

const kb = (n) => `${(n / 1024).toFixed(1)} KB`;
const data = JSON.parse(fs.readFileSync(R.REQ_PATH, 'utf8'));
const before = fs.statSync(R.REQ_PATH).size;

// Write JSON back in the FILE'S OWN shape, not this tool's house style (NR-109):
// existing indent/line-endings win, new files get 2-space LF.
const writeJson = (abs, obj) => {
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
    let out = JSON.stringify(obj, null, indent);
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

    for (const brief of only) {
        const g = data.groups.find((x) => x.brief === brief);
        if (!g) { console.error(`  MISS  ${brief} — not in ${R.REQ_REL}`); bad++; continue; }
        if (!R.TERMINAL.has(g.status)) {
            console.error(`  OPEN  ${brief} — status "${g.status}" is not terminal; refusing to freeze a live group`);
            bad++;
        }
    }

    // 16 legacy groups carry `brief: null` (their titles are "BL-nnn — ..." prose).
    // The cold store keys on brief, so backfill a deterministic slug from the title —
    // which also makes those groups addressable by every reader that keys on brief.
    const taken = new Set(data.groups.map((g) => g.brief).filter(Boolean));
    const slugOf = (title) => (title || 'untitled').toLowerCase()
        .replace(/[^a-z0-9]+/g, '-').replace(/^-+|-+$/g, '').slice(0, 48).replace(/-+$/, '');
    for (const g of data.groups) {
        if (g.brief) continue;
        let slug = slugOf(g.title);
        for (let n = 2; taken.has(slug); n++) slug = `${slugOf(g.title)}-${n}`;
        taken.add(slug);
        g.brief = slug;
        console.log(`  SLUG  brief backfilled: "${slug}"`);
    }

    for (const g of data.groups) {
        if (only.size && !only.has(g.brief)) continue;
        if (!R.TERMINAL.has(g.status)) continue;
        if (g.archived) continue;

        const rec = {};
        for (const f of R.NARRATIVE) if (g[f] != null) rec[f] = g[f];
        if (rec.rows) rec.rows = rec.rows.map((r) => ({ ...r, status: R.normaliseRowStatus(r.status) }));
        if (!Object.keys(rec).length) continue;

        const rel = R.archivePath(R.bucketFor(g));
        if (!stores.has(rel)) stores.set(rel, R.loadArchive(rel) || R.newArchive(R.bucketFor(g)));
        stores.get(rel).records[g.brief] = rec;

        const size = Buffer.byteLength(JSON.stringify(rec));
        bytes += size;
        moved++;
        console.log(`  COLD  ${g.brief.padEnd(36)} ${kb(size).padStart(9)}  → ${rel}`);

        if (dryRun) continue;
        for (const f of R.NARRATIVE) delete g[f];
        g.status = R.normaliseGroupStatus(g.status);
        g.archived = rel;
    }

    if (dryRun) {
        console.log(`\ndry run: ${moved} group(s), ${kb(bytes)} would move into ${stores.size} cold file(s).`);
        console.log(`${R.REQ_REL} is ${kb(before)}; it would drop to roughly ${kb(before - bytes)}.`);
        process.exit(bad ? 1 : 0);
    }

    for (const [rel, store] of stores) {
        const keys = Object.keys(store.records).sort();
        store.records = Object.fromEntries(keys.map((k) => [k, store.records[k]]));
        writeJson(path.join(R.ROOT, rel), store);
    }
    writeJson(R.REQ_PATH, data);

    const after = fs.statSync(R.REQ_PATH).size;
    console.log(`\nArchived ${moved} group(s) into ${stores.size} cold file(s).`);
    const delta = before - after;
    const dir = delta >= 0 ? 'smaller' : 'larger';
    console.log(`${R.REQ_REL}: ${kb(before)} → ${kb(after)}  (${delta >= 0 ? '−' : '+'}${kb(Math.abs(delta))}, ${Math.abs((1 - after / before) * 100).toFixed(0)}% ${dir})`);
    verify();
    process.exit(bad ? 1 : 0);
}

// ---- restore ----------------------------------------------------------------

function doRestore() {
    let n = 0;
    const cache = new Map();
    for (let i = 0; i < data.groups.length; i++) {
        const g = data.groups[i];
        if (!g.archived) continue;
        if (only.size && !only.has(g.brief)) continue;
        const merged = R.resolve(g, R.ROOT, cache);
        if (merged === g) {
            console.error(`  DEAD  ${g.brief} — archived pointer ${g.archived} has no record`);
            continue;
        }
        data.groups[i] = merged;
        n++;
    }
    if (dryRun) { console.log(`dry run: would restore ${n} group(s).`); process.exit(0); }
    writeJson(R.REQ_PATH, data);
    console.log(`Restored ${n} group(s) into ${R.REQ_REL} (${kb(fs.statSync(R.REQ_PATH).size)}). Cold files left in place.`);
}

// ---- round-trip check -------------------------------------------------------
// Re-read from disk and confirm every archived group resolves back to something
// with the fields we removed. A silent half-write here would look like data loss.
function verify() {
    const fresh = JSON.parse(fs.readFileSync(R.REQ_PATH, 'utf8'));
    const cache = new Map();
    const broken = [];
    for (const g of fresh.groups) {
        if (!g.archived) continue;
        const back = R.resolve(g, R.ROOT, cache);
        if (back === g || !R.NARRATIVE.some((f) => back[f] != null)) broken.push(g.brief);
    }
    if (broken.length) {
        console.error(`\nVERIFY FAILED — ${broken.length} group(s) do not resolve: ${broken.join(', ')}`);
        process.exit(1);
    }
    console.log('verify: every archived group resolves back through req_store.resolve().');
}
