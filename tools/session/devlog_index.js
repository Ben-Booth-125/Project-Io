#!/usr/bin/env node
// devlog_index.js — an index over DEVLOG.md, and a rollover for its older volumes.
//
// DEVLOG.md is 110 sessions and ~440 KB of append-only prose. Nothing reads it whole;
// what a reader actually wants is "which session touched X, and when" — then one entry.
// Without an index the only way to answer that is to load the file, so this generates
// DEVLOG_INDEX.md: one line per session (date, title, the BL ids it mentions), which is
// what CLAUDE.md should point a reader at.
//
// --rollover then moves entries older than a cutoff into docs/development/archive/
// DEVLOG-<year>.md, leaving DEVLOG.md as the live volume. The index still spans
// everything and names which volume each entry lives in, so nothing goes missing.
//
// USAGE:
//   node tools/session/devlog_index.js                    write DEVLOG_INDEX.md
//   node tools/session/devlog_index.js --rollover 2026-07 move entries before this month out
//   node tools/session/devlog_index.js --dry-run          report, write nothing
//
// Entries are newest-first and headed either
//   "## Session — <title> (YYYY-MM-DD)"   (current form)
//   "## YYYY-MM-DD — <title>"             (early form)
// Both are recognised; an unparseable heading is reported rather than silently dropped.

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const DEV = 'docs/development';
const DEVLOG = `${DEV}/DEVLOG.md`;
const INDEX = `${DEV}/DEVLOG_INDEX.md`;
const ARCHIVE_DIR = `${DEV}/archive`;
const P = (rel) => path.join(ROOT, rel);

const argv = process.argv.slice(2);
const dryRun = argv.includes('--dry-run');
const rollIdx = argv.indexOf('--rollover');
const cutoff = rollIdx >= 0 ? argv[rollIdx + 1] : null;

if (rollIdx >= 0 && !/^\d{4}(-\d{2})?$/.test(cutoff || '')) {
    console.error('--rollover needs a cutoff like 2026 or 2026-07 (entries strictly before it move out).');
    process.exit(1);
}

// ---- split the log into preamble + entries ----------------------------------

function parse(text, volume) {
    const lines = text.split('\n');
    const starts = [];
    lines.forEach((l, i) => { if (/^## /.test(l)) starts.push(i); });
    const preamble = lines.slice(0, starts.length ? starts[0] : lines.length).join('\n');
    const entries = starts.map((s, n) => {
        const end = n + 1 < starts.length ? starts[n + 1] : lines.length;
        const heading = lines[s].replace(/^##\s+/, '').trim();
        const body = lines.slice(s, end).join('\n');
        let date = null;
        let title = heading;
        let m = heading.match(/^(\d{4}-\d{2}-\d{2})\s+[—-]\s+(.*)$/);
        if (m) { [, date, title] = m; }
        else {
            // "Session — <title> (YYYY-MM-DD)", and the variants that qualify the
            // parenthetical — "(mobile, 2026-07-08)", "(worktree, 2026-07-06/07)".
            m = heading.match(/^(.*?)\s*\(([^()]*\d{4}-\d{2}-\d{2}[^()]*)\)\s*$/);
            if (m) {
                title = m[1].replace(/^Session\s*[—-]\s*/, '');
                date = m[2].match(/\d{4}-\d{2}-\d{2}/)[0];
            }
        }
        const ids = [...new Set(body.match(/BL-\d+/g) || [])].sort();
        return { date, title, ids, body, volume, heading };
    });
    return { preamble, entries };
}

const raw = fs.readFileSync(P(DEVLOG), 'utf8');
const live = parse(raw, DEVLOG);

// Already-rolled volumes are part of the index too.
const archived = [];
if (fs.existsSync(P(ARCHIVE_DIR))) {
    for (const f of fs.readdirSync(P(ARCHIVE_DIR)).filter((f) => /^DEVLOG-\d{4}\.md$/.test(f)).sort().reverse()) {
        const rel = `${ARCHIVE_DIR}/${f}`;
        archived.push(...parse(fs.readFileSync(P(rel), 'utf8'), rel).entries);
    }
}

const undated = [...live.entries, ...archived].filter((e) => !e.date);
if (undated.length) {
    console.error(`devlog_index: ${undated.length} entry heading(s) carry no parseable date and will sort last:`);
    undated.forEach((e) => console.error(`  - ${e.heading.slice(0, 80)}`));
}

// ---- rollover ---------------------------------------------------------------

let moved = 0;
if (cutoff) {
    const keep = [];
    const byYear = new Map();
    for (const e of live.entries) {
        if (e.date && e.date.slice(0, cutoff.length) < cutoff) {
            const year = e.date.slice(0, 4);
            if (!byYear.has(year)) byYear.set(year, []);
            byYear.get(year).push(e);
            moved++;
        } else {
            keep.push(e);
        }
    }
    console.log(`rollover: ${moved} entry(ies) before ${cutoff} → ${byYear.size} volume(s); ${keep.length} stay live.`);

    if (!dryRun && moved) {
        fs.mkdirSync(P(ARCHIVE_DIR), { recursive: true });
        for (const [year, group] of byYear) {
            const rel = `${ARCHIVE_DIR}/DEVLOG-${year}.md`;
            const existing = fs.existsSync(P(rel)) ? parse(fs.readFileSync(P(rel), 'utf8'), rel).entries : [];
            const all = [...group, ...existing].sort((a, b) => (b.date || '').localeCompare(a.date || ''));
            const header = [
                `# Project Io — Development Log, ${year}`,
                '',
                `Rolled-over volume. Entries are newest-first, same form as [\`../DEVLOG.md\`](../DEVLOG.md),`,
                'which holds the live sessions. The index across every volume is',
                '[`../DEVLOG_INDEX.md`](../DEVLOG_INDEX.md) — read that first.',
                '',
                '---',
                '',
            ].join('\n');
            fs.writeFileSync(P(rel), header + all.map((e) => e.body.replace(/\s*$/, '')).join('\n\n---\n\n') + '\n');
            // Re-point the entries and fold them back into the index input: they left
            // the live log this run, so nothing else would put them in the index.
            for (const e of group) { e.volume = rel; archived.push(e); }
            console.log(`  wrote ${rel} (${all.length} entries)`);
        }
        fs.writeFileSync(P(DEVLOG), live.preamble.replace(/\s*$/, '') + '\n\n' + keep.map((e) => e.body.replace(/\s*$/, '')).join('\n\n---\n\n') + '\n');
        live.entries = keep;
        console.log(`  ${DEVLOG} now ${(fs.statSync(P(DEVLOG)).size / 1024).toFixed(1)} KB`);
    }
}

// ---- index ------------------------------------------------------------------

const all = [...live.entries, ...archived]
    .sort((a, b) => (b.date || '').localeCompare(a.date || ''));

const rows = all.map((e) => {
    const where = e.volume === DEVLOG ? 'DEVLOG.md' : path.basename(e.volume);
    const link = e.volume === DEVLOG ? 'DEVLOG.md' : `archive/${path.basename(e.volume)}`;
    const ids = e.ids.length ? e.ids.join(' ') : '—';
    return `| ${e.date || '?'} | [${e.title.replace(/\|/g, '\\|')}](${link}) | ${ids} | ${where} |`;
});

const index = [
    '# Project Io — Development Log index',
    '',
    `> **Generated file.** Produced by \`node tools/session/devlog_index.js\`.`,
    '> Edit the log entries themselves, then re-run; hand edits here are overwritten.',
    '',
    `One line per session, newest first — ${all.length} entries across ${new Set(all.map((e) => e.volume)).size} volume(s).`,
    'Read this to find the session you want, then open only that entry. The full prose of',
    'the live sessions is in [`DEVLOG.md`](DEVLOG.md); older volumes are under',
    '[`archive/`](archive/).',
    '',
    '| Date | Session | Items | Volume |',
    '|---|---|---|---|',
    ...rows,
    '',
].join('\n');

if (dryRun) {
    console.log(`dry run: index would carry ${all.length} entries (${(Buffer.byteLength(index) / 1024).toFixed(1)} KB).`);
    process.exit(0);
}

fs.writeFileSync(P(INDEX), index);
console.log(`devlog_index: wrote ${INDEX} — ${all.length} entries, ${(Buffer.byteLength(index) / 1024).toFixed(1)} KB (log itself is ${(fs.statSync(P(DEVLOG)).size / 1024).toFixed(1)} KB).`);
