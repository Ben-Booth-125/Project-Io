#!/usr/bin/env node
// doc_owner.js — which authority doc owns this source file.
//
// backlog_query.js --touches <doc> answers "what work touched this doc". This is
// the INVERSE, and nothing answered it before: a session that starts at the CODE —
// a bug, a review, a failing harness — had to guess its way through CLAUDE.md § 3
// by subject, which is exactly the kind of guess the routing table exists to remove.
//
// THE MAPPING IS DERIVED, NEVER HAND-MAINTAINED. Every backlog item already carries
// both `authority_doc` and `files`; the union of the hot store and the cold archive
// is therefore a weighted file -> doc index with no new bookkeeping and no new file
// to go stale. Ranking is by CITATION COUNT — how many items that touched this path
// named that doc as their authority — and the count is printed, so the ranking is
// legible rather than magical. Where the path is a UI one, the surface it belongs to
// in docs/ui/ui_elements.json is printed alongside.
//
// IT REPORTS WHAT THE WORK CITED, which means a path no work has ever cited comes
// back as UNOWNED. That is a finding, not an error: either the file is genuinely
// undesigned, or items touching it were filed without an authority_doc. Same shape
// as ui_coverage.js's orphan checks — the index doubles as its own staleness detector.
//
// USAGE:  node tools/session/doc_owner.js <path-or-fragment> [options]
//   src/world/market_clearing.cpp   full path
//   market_clearing                 a fragment matches too
//   --limit <n>                     how many docs to rank (default 6)
//   --items                         also list the citing items, newest id first
//   --json                          machine-readable dump
// EXIT:   0 always — this is an instrument, not a gate. It reports; it does not judge.
//
// Zero dependencies (fs only). Companion to backlog_query.js and ui_coverage.js.

'use strict';
const fs = require('fs');
const path = require('path');
const A = require('./archive_store');

const P = (rel) => path.join(A.ROOT, rel);
const BL_PATH = 'docs/development/backlog.json';
const ELEMENTS = 'docs/ui/ui_elements.json';
const ARCHIVE_DIR = 'docs/development/archive';

const argv = process.argv.slice(2);
const has = (f) => argv.includes(f);
const val = (f) => { const i = argv.indexOf(f); return i >= 0 ? argv[i + 1] : null; };

if (!argv.length || has('--help') || has('-h')) {
  console.log(fs.readFileSync(__filename, 'utf8').split('\n').slice(1, 30).map((l) => l.replace(/^\/\/ ?/, '')).join('\n'));
  process.exit(0);
}

const FLAGS_WITH_VALUE = new Set(['--limit']);
const positional = [];
for (let i = 0; i < argv.length; i += 1) {
  if (argv[i].startsWith('--')) { if (FLAGS_WITH_VALUE.has(argv[i])) i += 1; continue; }
  positional.push(argv[i]);
}
const query = (positional[0] || '').replace(/\\/g, '/').toLowerCase();
const limit = Number(val('--limit')) || 6;

if (!query) { console.error('doc_owner: give a path or a path fragment.'); process.exit(0); }

// --- the pool ---------------------------------------------------------------
//
// A.allItems is backlog_query.js's own union: the hot worklist plus the whole-row
// evictions in archive/backlog-design-*.json. It is NOT the whole history — the
// 2026-08 purge and complete/cancelled sweeps wrote their rows to sibling files
// (backlog-complete-*, backlog-cancelled-*, backlog-purged-*) that archive_store's
// design-file glob does not reach. Those rows are ~625 items of exactly the
// evidence this index is built from, and leaving them out changes the answer (a
// UI path resolves to the wrong doc without them), so they are read here too.
// Deliberately additive and id-deduplicated: the hot row always wins.
const backlog = JSON.parse(fs.readFileSync(P(BL_PATH), 'utf8'));
const pool = [];
const seen = new Set();
const add = (it) => { if (it && it.id && !seen.has(it.id)) { seen.add(it.id); pool.push(it); } };
for (const it of A.allItems(backlog, A.ROOT)) add(it);
for (const f of fs.readdirSync(P(ARCHIVE_DIR))) {
  if (!/^backlog-(complete|cancelled|purged)-.*\.json$/.test(f)) continue;
  const store = JSON.parse(fs.readFileSync(P(`${ARCHIVE_DIR}/${f}`), 'utf8'));
  for (const it of store.items || []) add(it);
}

// --- matching ---------------------------------------------------------------
// A recorded `files` entry matches when it CONTAINS the query, so a full path, a bare
// fragment ("market_clearing") and a directory prefix ("src/ui/", which sweeps every
// file under it) all work. Containment is deliberately one-directional: matching the
// other way round would make a query for one file inherit every item filed against the
// whole directory, and those items are about the directory, not about that file.
const norm = (f) => String(f).replace(/\\/g, '/').toLowerCase();
const matches = (f) => norm(f).includes(query);

const citing = pool.filter((it) => (it.files || []).some(matches));
const paths = new Set();
for (const it of citing) for (const f of it.files || []) if (matches(f)) paths.add(norm(f));

const counts = new Map();
for (const it of citing) {
  const doc = it.authority_doc;
  if (!doc) continue;
  const rec = counts.get(doc) || { doc, count: 0, items: [] };
  rec.count += 1;
  rec.items.push(`${it.id} (${it.short_name || it.title || '?'})`);
  counts.set(doc, rec);
}
const ranked = [...counts.values()].sort((a, b) => b.count - a.count || a.doc.localeCompare(b.doc));
const uncited = citing.filter((it) => !it.authority_doc).length;

// --- the UI surface, where the path is one -----------------------------------
let surfaces = [];
if (fs.existsSync(P(ELEMENTS))) {
  const cat = JSON.parse(fs.readFileSync(P(ELEMENTS), 'utf8'));
  surfaces = (cat.elements || [])
    .filter((el) => (el.source || []).some(matches))
    .map((el) => ({
      id: el.id,
      name: el.name,
      kind: el.kind,
      docs: (el.source || []).filter((s) => norm(s).endsWith('.md')),
    }));
}

// --- output ------------------------------------------------------------------
if (has('--json')) {
  console.log(JSON.stringify({
    query: positional[0], matched_files: [...paths].sort(), citing_items: citing.length,
    items_without_authority_doc: uncited,
    owners: ranked.slice(0, limit).map(({ doc, count, items }) => ({ doc, count, items })),
    surfaces,
  }, null, 1));
  process.exit(0);
}

console.log(`doc_owner: ${positional[0]}`);
if (!citing.length) {
  console.log('\n  UNOWNED — no backlog item, hot or archived, records work on this path.');
  console.log('  That is a finding, not an error: either nothing has been designed against');
  console.log('  it, or the work that touched it was filed without a `files` entry.');
} else {
  console.log(`  ${paths.size} recorded path(s), cited by ${citing.length} item(s).`);
  if (!ranked.length) {
    console.log('\n  UNOWNED — work touched this path, but no item named an authority_doc.');
  } else {
    console.log('\nOWNING DOC (by how often work on this path cited it):');
    const w = Math.max(...ranked.slice(0, limit).map((r) => r.doc.length));
    for (const r of ranked.slice(0, limit)) {
      console.log(`  ${r.doc.padEnd(w)}  ${String(r.count).padStart(3)}`);
      if (has('--items')) for (const i of r.items.slice().reverse()) console.log(`      ${i}`);
    }
    if (ranked.length > limit) console.log(`  ... and ${ranked.length - limit} more doc(s) cited once or twice (--limit).`);
  }
  if (uncited) console.log(`\n  ${uncited} citing item(s) name no authority_doc at all.`);
}

// Both tails are capped: a directory sweep ("src/ui/") legitimately matches a hundred
// paths and forty surfaces, and printing all of them buries the ranking that is the answer.
const CAP = 12;
if (surfaces.length) {
  console.log(`\nUI SURFACE (docs/ui/ui_elements.json)${surfaces.length > CAP ? ` — ${surfaces.length}, first ${CAP}` : ''}:`);
  for (const s of surfaces.slice(0, CAP)) console.log(`  ${s.id}  ${s.name} (${s.kind})${s.docs.length ? '   ' + s.docs.join(', ') : ''}`);
}

if (paths.size > 1) {
  const all = [...paths].sort();
  console.log(`\nMATCHED PATHS${all.length > CAP ? ` — ${all.length}, first ${CAP} (--json for all)` : ''}:`);
  for (const p of all.slice(0, CAP)) console.log(`  ${p}`);
}
process.exit(0);
