#!/usr/bin/env node
// flush_requirements.js — mark requirement groups complete in requirements.json
//
// Usage:
//   node tools/session/flush_requirements.js group-id [group-id ...]
//   node tools/session/flush_requirements.js --date 2026-06-17 group-id ...
//   node tools/session/flush_requirements.js --resolution "All rows met." group-id ...
//
// Group IDs match the `brief` field in docs/development/req/requirements.json.

'use strict';
const fs   = require('fs');
const path = require('path');

const REQ_PATH = path.join(__dirname, '../../docs/development/req/requirements.json');

function today() { return new Date().toISOString().slice(0, 10); }

// --- parse args ---
const args = process.argv.slice(2);
let date       = today();
let resolution = null;
const groupIds = [];

for (let i = 0; i < args.length; i++) {
    if (args[i] === '--date'       && args[i + 1]) { date       = args[++i]; }
    else if (args[i] === '--resolution' && args[i + 1]) { resolution = args[++i]; }
    else { groupIds.push(args[i]); }
}

if (groupIds.length === 0) {
    console.error('Usage: node flush_requirements.js [--date YYYY-MM-DD] [--resolution TEXT] group-id...');
    process.exit(1);
}

// --- apply ---
const req = JSON.parse(fs.readFileSync(REQ_PATH, 'utf8'));
let updated = 0, skipped = 0, missing = 0;

for (const id of groupIds) {
    const group = req.groups.find(g => g.brief === id);
    if (!group) {
        console.warn(`  MISS  ${id} — not found in requirements.json`);
        missing++;
        continue;
    }
    if (group.status === 'complete') {
        console.log(`  SKIP  ${id} — already complete (${group.resolved})`);
        skipped++;
        continue;
    }
    group.status   = 'complete';
    group.resolved = date;
    if (resolution) group.resolution = resolution;
    console.log(`  DONE  ${id} → complete (${date})`);
    updated++;
}

fs.writeFileSync(REQ_PATH, JSON.stringify(req, null, 2) + '\n');
console.log(`\nUpdated ${updated}  Skipped ${skipped}  Missing ${missing}`);
if (missing > 0) process.exit(1);
