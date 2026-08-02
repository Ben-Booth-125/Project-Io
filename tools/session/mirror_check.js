#!/usr/bin/env node
// mirror_check.js — is every generated Markdown mirror still what its JSON says?
//
// The project's standard shape is a canonical JSON plus a readable Markdown mirror
// (NEEDS_REVIEW, ACTIONS, USER_STORIES). Mirrors carry a "Generated file" stamp, but
// nothing enforced it: on 2026-08-02 a Q&A pass resolved 19 NEEDS_REVIEW entries in
// the JSON and left the Markdown describing them all as open. A stale mirror is worse
// than no mirror — it reads as authoritative and is wrong.
//
// This re-runs each registered renderer and compares the result with what is on disk.
// By default it LEAVES the regenerated file in place (a stale mirror is a bug, and the
// fix is free); --check restores the original and only reports, for use in CI.
//
// USAGE:
//   node tools/session/mirror_check.js            regenerate; report which were stale
//   node tools/session/mirror_check.js --check    report only, change nothing
// EXIT: 0 = every mirror was current; 1 = at least one was stale (or a renderer failed).
//
// Adding a pair: add a row to PAIRS. A mirror with no renderer belongs in a renderer,
// not in a hand-edit habit.

'use strict';
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..', '..');
const P = (rel) => path.join(ROOT, rel);
const checkOnly = process.argv.includes('--check');

const PAIRS = [
    {
        source: 'docs/development/NEEDS_REVIEW.json',
        renderer: 'tools/session/render_needs_review.js',
        mirrors: ['docs/development/NEEDS_REVIEW.md'],
    },
    {
        source: 'docs/ai/ACTIONS.json',
        renderer: 'tools/session/render_actions.js',
        mirrors: ['docs/ai/ACTIONS.md', 'docs/ai/ACTIONS_INDEX.json'],
    },
];

let stale = 0;
let failed = 0;

for (const pair of PAIRS) {
    const before = new Map();
    for (const m of pair.mirrors) {
        before.set(m, fs.existsSync(P(m)) ? fs.readFileSync(P(m), 'utf8') : null);
    }

    const run = spawnSync(process.execPath, [P(pair.renderer)], { cwd: ROOT, encoding: 'utf8' });
    if (run.status !== 0) {
        console.error(`  FAIL  ${pair.renderer} exited ${run.status}: ${(run.stderr || '').trim().split('\n')[0]}`);
        failed++;
        continue;
    }

    for (const m of pair.mirrors) {
        const after = fs.existsSync(P(m)) ? fs.readFileSync(P(m), 'utf8') : null;
        const wasStale = before.get(m) !== after;
        if (wasStale) {
            stale++;
            const delta = (after || '').length - (before.get(m) || '').length;
            console.error(`  STALE ${m} — did not match ${pair.source} (${delta >= 0 ? '+' : ''}${delta} chars)`);
            if (checkOnly && before.get(m) !== null) fs.writeFileSync(P(m), before.get(m));
        } else {
            console.log(`  ok    ${m}`);
        }
    }
}

if (!stale && !failed) {
    console.log('\nmirror_check: every generated mirror matches its canonical source.');
    process.exit(0);
}
if (stale) {
    console.error(`\nmirror_check: ${stale} stale mirror(s)${checkOnly ? ' (left untouched — rerun without --check to fix)' : ' — regenerated in place; commit them'}.`);
}
if (failed) console.error(`mirror_check: ${failed} renderer(s) failed to run.`);
process.exit(1);
