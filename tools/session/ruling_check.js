#!/usr/bin/env node
// ruling_check.js — the sprint-close doc check, made repeatable (Ben, 2026-10-08:
// "make sure all the various decisions are documented ... add a check for docs").
//
// Two modes, both read-only:
//
//   node tools/session/ruling_check.js --since 2026-10-07
//       LIST every dated ruling "(Ben, YYYY-MM-DD" on or after the date in the authority
//       docs (docs/ minus docs/development and docs/research), grouped by doc, with the
//       line's bold lead. Flags, per ruling line: state-dependent words (landed, shipped,
//       "until now", "not yet built", ...) and a BL id cited with no short handle.
//
//   node tools/session/ruling_check.js --register tools/session/rulings/sprint-50.json
//       CHECK a sprint's register. Each entry:
//         { "ruling": "...", "doc": "docs/x.md", "anchor": "regex",
//           "stale": ["regex", ...], "owner": "BL-nnnn (handle) | built | none" }
//       doc ok    = `anchor` matches in `doc` (case-insensitive);
//       siblings  = no `stale` regex matches anywhere in docs/ outside docs/development
//                   (the same-day-ruling trap: grep the OLD wording, not the new);
//       owner     = echoed, for the reader to confirm against the backlog.
//       It cannot check that the code matches: that stays a reading of the cited code.
//
// Exit 1 when any register entry fails or any flagged ruling line is found; 0 otherwise.
// Add --json for machine output. No writes, no network.

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const DOCS = path.join(ROOT, 'docs');
const SKIP_DIRS = new Set(['development', 'research']);

function argVal(name) {
    const i = process.argv.indexOf(name);
    return i >= 0 ? process.argv[i + 1] : undefined;
}
const SINCE = argVal('--since');
const REGISTER = argVal('--register');
const JSON_OUT = process.argv.includes('--json');

if (!SINCE && !REGISTER || process.argv.includes('--help')) {
    console.log('usage: node tools/session/ruling_check.js (--since YYYY-MM-DD | --register <file>) [--json]');
    process.exit(SINCE || REGISTER ? 0 : 2);
}

function walk(dir, out) {
    for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
        const p = path.join(dir, e.name);
        if (e.isDirectory()) {
            if (dir === DOCS && SKIP_DIRS.has(e.name)) continue;
            walk(p, out);
        } else if (e.name.endsWith('.md')) out.push(p);
    }
    return out;
}
const files = walk(DOCS, []);
const text = new Map(files.map(f => [f, fs.readFileSync(f, 'utf8').split(/\r?\n/)]));
const rel = f => path.relative(ROOT, f).split(path.sep).join('/');

// State-dependent language an authority doc must not carry (io-standing-rules § Terms & docs).
// "landed" and "shipped" also name goods moving (a landed price, cargo shipped): read each flag.
const STATE_WORDS = /\b(landed(?!\s+(price|cost|at|in|on))|shipped|until now|not yet built|now built|already built|merged)\b/i;
// A BL id must travel with its short handle: "BL-123 (handle)", "BL-123, handle",
// or "BL-123's handle" — what follows the id is read.
const HANDLE_AFTER = /^(\s*\(|,\s*[a-z]|['’]s(\s|$)|\s*$)/i;

let failures = 0;
const report = {};

if (SINCE) {
    const RULING = /\(Ben[^)]{0,20}?(\d{4}-\d{2}-\d{2})/g;
    const rows = [];
    for (const [f, lines] of text) {
        lines.forEach((ln, i) => {
            let m;
            RULING.lastIndex = 0;
            while ((m = RULING.exec(ln))) {
                if (m[1] < SINCE) continue;
                // Read the ruling's sentence: this line plus the next two.
                const ctx = lines.slice(i, i + 3).join(' ');
                const lead = (ln.match(/\*\*([^*]+)\*\*/) || [, ln.trim().slice(0, 100)])[1];
                const flags = [];
                if (STATE_WORDS.test(ctx)) flags.push('state-word: ' + ctx.match(STATE_WORDS)[0]);
                // The owner a ruling cites sits in its own parenthesis: "(Ben, date; BL-n, handle)".
                const paren = lines.slice(i, i + 2).join(' ').slice(m.index);
                const own = paren.slice(0, paren.indexOf(')') + 1 || 160);
                const BL = /BL-\d+/g;
                let b;
                while ((b = BL.exec(own))) {
                    const after = paren.slice(b.index + b[0].length, b.index + b[0].length + 12);
                    if (!HANDLE_AFTER.test(after)) flags.push('bare id: ' + b[0]);
                }
                if (flags.length) failures++;
                rows.push({ doc: rel(f), line: i + 1, date: m[1], lead: lead.trim(), flags });
                break;
            }
        });
    }
    rows.sort((a, b) => a.doc.localeCompare(b.doc) || a.line - b.line);
    report.rulings = rows;
    if (!JSON_OUT) {
        let cur = '';
        for (const r of rows) {
            if (r.doc !== cur) { cur = r.doc; console.log('\n' + cur); }
            console.log(`  :${r.line}  ${r.date}  ${r.lead.slice(0, 110)}`);
            for (const fl of r.flags) console.log(`        FLAG ${fl}`);
        }
        console.log(`\nruling_check: ${rows.length} dated rulings since ${SINCE}; ${rows.filter(r => r.flags.length).length} flagged (read each flag; a quoted measurement may be a false hit).`);
    }
}

if (REGISTER) {
    const reg = JSON.parse(fs.readFileSync(path.resolve(ROOT, REGISTER), 'utf8'));
    const entries = reg.rulings || reg;
    const rows = [];
    for (const e of entries) {
        const row = { ruling: e.ruling, doc: e.doc, owner: e.owner || '', doc_ok: null, stale_hits: [] };
        if (e.doc && e.anchor) {
            const f = path.join(ROOT, e.doc);
            const body = text.get(f) ? text.get(f).join('\n')
                       : (fs.existsSync(f) ? fs.readFileSync(f, 'utf8') : '');
            row.doc_ok = new RegExp(e.anchor, 'i').test(body);
        }
        for (const s of e.stale || []) {
            const re = new RegExp(s, 'i');
            for (const [f, lines] of text) lines.forEach((ln, i) => {
                if (re.test(ln)) row.stale_hits.push(`${rel(f)}:${i + 1}`);
            });
        }
        if (row.doc_ok === false || row.stale_hits.length) failures++;
        rows.push(row);
    }
    report.register = rows;
    if (!JSON_OUT) {
        for (const r of rows) {
            const d = r.doc_ok === null ? 'n/a ' : r.doc_ok ? 'ok  ' : 'MISS';
            const s = r.stale_hits.length ? 'STALE' : 'ok   ';
            console.log(`doc ${d} siblings ${s} | ${r.ruling}  [${r.doc || '-'}; owner ${r.owner || '-'}]`);
            for (const h of r.stale_hits) console.log(`        old wording at ${h}`);
        }
        const bad = rows.filter(r => r.doc_ok === false || r.stale_hits.length).length;
        console.log(`\nruling_check: ${rows.length} rulings in ${REGISTER}; ${bad} failing.`);
    }
}

if (JSON_OUT) console.log(JSON.stringify(report, null, 1));
process.exit(failures ? 1 : 0);
