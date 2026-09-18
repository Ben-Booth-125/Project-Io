#!/usr/bin/env node
// charter_cost_merge.js — fold one `player_seed_sweep --charter-cost --out` run into
// a checked-in cost table as a NAMED RUN, leaving every byte already there as it was.
//
//   node tools/verify/charter_cost_merge.js <table.json> <run.json> <key> [--note TEXT] [--dry-run]
//
// WHY (BL-1039). charter_cost_sweep.json holds BL-1033's 33 rows. A later sweep
// (BL-1039's square-root rows) must land in the same table "with its note", and
// `--out charter_cost_sweep.json` would overwrite the rows it is meant to sit
// beside. So the later run is written to its own file and folded in here, under
// `runs.<key>`, as the whole run document (its argv, build, notes and seeds).
//
// TEXTUAL, NOT RE-SERIALISED: the table's bytes are kept verbatim up to its last
// closing brace, and the run is appended there — so a diff of the table shows
// only the added run, never a reformatted file. The result is re-parsed and every
// key the table already held is checked deep-equal to the original before the
// file is written. Refuses: an aborted run, a run that is not a --charter-cost
// run, a key already present, a table whose last key is not `runs` once `runs`
// exists (it could not be appended to textually).
'use strict';
const fs = require('fs');

function die(msg) { console.error('charter_cost_merge: ' + msg); process.exit(2); }

const args = process.argv.slice(2);
const pos = [];
let note = null, dry = false;
for (let i = 0; i < args.length; ++i) {
    if (args[i] === '--note') { if (i + 1 >= args.length) die('--note needs a value'); note = args[++i]; }
    else if (args[i] === '--dry-run') dry = true;
    else if (args[i].startsWith('--')) die(`unknown flag ${args[i]}`);
    else pos.push(args[i]);
}
if (pos.length !== 3)
    die('usage: node tools/verify/charter_cost_merge.js <table.json> <run.json> <key> [--note TEXT] [--dry-run]');
const [tablePath, runPath, key] = pos;
if (!/^[A-Za-z0-9_.-]+$/.test(key)) die(`key '${key}' must be [A-Za-z0-9_.-]+`);

const tableText = fs.readFileSync(tablePath, 'utf8');
const runText = fs.readFileSync(runPath, 'utf8');
let table, run;
try { table = JSON.parse(tableText); } catch (e) { die(`${tablePath} is not JSON: ${e.message}`); }
try { run = JSON.parse(runText); } catch (e) { die(`${runPath} is not JSON: ${e.message}`); }

if (run.tool !== 'player_seed_sweep --charter-cost') die(`${runPath} is not a --charter-cost run (tool '${run.tool}')`);
if (run.aborted !== null && run.aborted !== undefined) die(`${runPath} is an ABORTED run: ${JSON.stringify(run.aborted)}`);
if (!Array.isArray(run.seeds) || run.seeds.length === 0) die(`${runPath} carries no seeds`);
if (table.runs !== undefined && (typeof table.runs !== 'object' || Array.isArray(table.runs)))
    die(`${tablePath}: 'runs' is not an object`);
if (table.runs && Object.prototype.hasOwnProperty.call(table.runs, key))
    die(`${tablePath} already holds runs.${key}; pick another key (nothing is overwritten)`);

const entry = Object.assign({ merge_note: note !== null ? note : run.note }, run);
const body = JSON.stringify(entry, null, 2).split('\n').map((l, i) => (i === 0 ? l : '    ' + l)).join('\n');
const eol = tableText.includes('\r\n') ? '\r\n' : '\n';

const trimmed = tableText.replace(/\s+$/, '');
if (!trimmed.endsWith('}')) die(`${tablePath} does not end in '}'`);
let out;
if (table.runs === undefined) {
    out = trimmed.slice(0, -1).replace(/\s+$/, '')
        + `,${eol}  "runs": {${eol}    "${key}": ${body.replace(/\n/g, eol)}${eol}  }${eol}}${eol}`;
} else {
    const keys = Object.keys(table);
    if (keys[keys.length - 1] !== 'runs') die(`${tablePath}: 'runs' is not the last key, so it cannot be appended to textually`);
    // The table ends "...}<ws>}<ws>}": the last run, the runs object, the table.
    const m = trimmed.match(/\}(\s*\}\s*\})$/);
    if (!m) die(`${tablePath}: cannot find the end of 'runs'`);
    const at = trimmed.length - m[1].length;   // just past the last run's '}'
    out = trimmed.slice(0, at) + `,${eol}    "${key}": ${body.replace(/\n/g, eol)}${eol}  }${eol}}${eol}`;
}

let check;
try { check = JSON.parse(out); } catch (e) { die(`the merged text does not parse (${e.message}); nothing written`); }
for (const k of Object.keys(table)) {
    if (k === 'runs') continue;
    if (JSON.stringify(check[k]) !== JSON.stringify(table[k])) die(`merging would change '${k}'; nothing written`);
}
if (table.runs)
    for (const k of Object.keys(table.runs))
        if (JSON.stringify(check.runs[k]) !== JSON.stringify(table.runs[k])) die(`merging would change runs.${k}; nothing written`);
if (JSON.stringify(check.runs[key]) !== JSON.stringify(entry)) die('the merged run does not read back as written; nothing written');

const rows = run.seeds.reduce((n, s) => n + (Array.isArray(s.configs) ? s.configs.length : 0), 0);
if (dry) {
    console.log(`dry run: would add runs.${key} (${run.seeds.length} seeds, ${rows} rows) to ${tablePath}`);
} else {
    fs.writeFileSync(tablePath, out);
    console.log(`added runs.${key} (${run.seeds.length} seeds, ${rows} rows) to ${tablePath}; every existing key unchanged`);
}
