#!/usr/bin/env node
// apply_session_close.js — apply a completed session-close.json to the repo
//
// Reads docs/development/session-close.json (or a path given as argv[2]) and:
//   1. Ships all delivered_items in backlog.json
//   2. Marks all requirements_closed groups complete in requirements.json
//   3. Merges goldens_blessed entries into scripts/verify/golden_log.json
//
// Run once at the end of a delivery session, then commit the updated files.
// Usage: node tools/session/apply_session_close.js [path/to/session-close.json]

'use strict';
const fs   = require('fs');
const path = require('path');

const ROOT     = path.join(__dirname, '../..');
const SC_PATH  = process.argv[2] || path.join(ROOT, 'docs/development/session-close.json');
const BL_PATH  = path.join(ROOT, 'docs/development/backlog.json');
const REQ_PATH = path.join(ROOT, 'docs/development/req/requirements.json');
const GL_PATH  = path.join(ROOT, 'scripts/verify/golden_log.json');

function today() { return new Date().toISOString().slice(0, 10); }

const sc = JSON.parse(fs.readFileSync(SC_PATH, 'utf8'));
console.log(`Applying: ${sc.batch_name}  (${sc.date})\n`);

// 1. Ship items ---------------------------------------------------------------
if (sc.delivered_items && sc.delivered_items.length > 0) {
    console.log('--- Ship items ---');
    const bl = JSON.parse(fs.readFileSync(BL_PATH, 'utf8'));
    let n = 0;
    for (const entry of sc.delivered_items) {
        const item = bl.items.find(i => i.id === entry.id);
        if (!item) { console.warn(`  MISS  ${entry.id}`); continue; }
        if (item.status === 'shipped') { console.log(`  SKIP  ${entry.id}`); continue; }
        item.status = 'shipped';
        console.log(`  SHIP  ${entry.id}  ${item.title}`);
        n++;
    }
    fs.writeFileSync(BL_PATH, JSON.stringify(bl, null, 2) + '\n');
    console.log(`  → ${n} shipped\n`);
}

// 2. Close requirement groups -------------------------------------------------
if (sc.requirements_closed && sc.requirements_closed.length > 0) {
    console.log('--- Close requirements ---');
    const req = JSON.parse(fs.readFileSync(REQ_PATH, 'utf8'));
    let n = 0;
    for (const brief of sc.requirements_closed) {
        const group = req.groups.find(g => g.brief === brief);
        if (!group) { console.warn(`  MISS  ${brief}`); continue; }
        if (group.status === 'complete') { console.log(`  SKIP  ${brief}`); continue; }
        group.status   = 'complete';
        group.resolved = sc.date || today();
        if (!group.resolution) {
            group.resolution = `Complete. Delivered in ${sc.batch_name} (${sc.date}).`;
        }
        console.log(`  DONE  ${brief}`);
        n++;
    }
    fs.writeFileSync(REQ_PATH, JSON.stringify(req, null, 2) + '\n');
    console.log(`  → ${n} groups closed\n`);
}

// 3. Update golden log --------------------------------------------------------
if (sc.goldens_blessed && sc.goldens_blessed.length > 0) {
    console.log('--- Update golden log ---');
    const gl = JSON.parse(fs.readFileSync(GL_PATH, 'utf8'));
    let n = 0;
    for (const entry of sc.goldens_blessed) {
        const existing = gl.entries[entry.file] || {};
        gl.entries[entry.file] = Object.assign({}, existing, {
            blessed_date: sc.date || today(),
            reason_tag:   entry.reason_tag,
            reason:        entry.reason,
            batch:         sc.batch_name,
        });
        console.log(`  SET   ${entry.file}  [${entry.reason_tag}]`);
        n++;
    }
    fs.writeFileSync(GL_PATH, JSON.stringify(gl, null, 2) + '\n');
    console.log(`  → ${n} entries updated\n`);
}

console.log('Done. Commit: backlog.json, requirements.json, golden_log.json');
