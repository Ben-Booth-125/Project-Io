#!/usr/bin/env node
// ship_items.js — mark backlog items terminal (status: complete) in backlog.json
//
// `complete` is the single terminal status (2026-08-01). The old `shipped` value this
// script used to write was a synonym with no defined difference; the six items carrying
// it were normalised. Readers still tolerate it — see the TERMINAL sets in gyre.py,
// backlog_view.js, backlog_lint.js, story_check.js, status.ps1.
//
// Usage:
//   node tools/session/ship_items.js BL-XXX [BL-YYY ...]
//
// IDs match the `id` field in docs/development/backlog.json.

'use strict';
const fs   = require('fs');
const path = require('path');

const BL_PATH = path.join(__dirname, '../../docs/development/backlog.json');

const ids = process.argv.slice(2).filter(a => !a.startsWith('--'));

if (ids.length === 0) {
    console.error('Usage: node ship_items.js BL-XXX [BL-YYY ...]');
    process.exit(1);
}

const bl = JSON.parse(fs.readFileSync(BL_PATH, 'utf8'));
let updated = 0, skipped = 0, missing = 0;

for (const id of ids) {
    const item = bl.items.find(i => i.id === id);
    if (!item) {
        console.warn(`  MISS  ${id} — not found in backlog.json`);
        missing++;
        continue;
    }
    if (item.status === 'complete') {
        console.log(`  SKIP  ${id} — already complete`);
        skipped++;
        continue;
    }
    const prev = item.status;
    item.status = 'complete';
    console.log(`  SHIP  ${id}  (${prev} → complete)  ${item.title}`);
    updated++;
}

fs.writeFileSync(BL_PATH, JSON.stringify(bl, null, 2) + '\n');
console.log(`\nUpdated ${updated}  Skipped ${skipped}  Missing ${missing}`);
if (missing > 0) process.exit(1);
