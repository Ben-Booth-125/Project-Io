#!/usr/bin/env node
/*
 * doc_weight.js — what CLAUDE.md's reading order actually costs to read.
 *
 * CLAUDE.md opens with "Read the documents below before responding to any request."
 * That instruction was written when the doc set was small. It is worth knowing, as a
 * number rather than a feeling, whether it is still an instruction anyone can follow.
 *
 * This walks the doc paths CLAUDE.md names, measures each one, estimates its token
 * cost, and compares the total against a declared budget. It also lists the heaviest
 * files under docs/ that CLAUDE.md does NOT name — the ones a reader pays for only
 * when they go looking, which is exactly where a query tool earns its keep.
 *
 * Estimation is bytes÷4 — the usual English-prose rule of thumb, and wrong by maybe
 * ±15%. It is a budget instrument, not an accountant: the ratios are the signal.
 *
 * Usage:
 *   node tools/doc_weight.js                  the reading order, with a verdict
 *   node tools/doc_weight.js --budget 120000  set the ceiling (default 150000 tokens)
 *   node tools/doc_weight.js --all            every file under docs/, heaviest first
 *   node tools/doc_weight.js --json           machine-readable, for a lint or a chain
 *
 * Zero dependencies. Exit 1 when the reading order is over budget.
 */
'use strict';

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const argv = process.argv.slice(2);
const asJson = argv.includes('--json');
const showAll = argv.includes('--all');
const bIdx = argv.indexOf('--budget');
const BUDGET = bIdx >= 0 ? Number(argv[bIdx + 1]) : 150000;

const tokens = (bytes) => Math.round(bytes / 4);
const kb = (n) => `${(n / 1024).toFixed(1)}K`;
const num = (n) => n.toLocaleString('en-GB');

// ---- what CLAUDE.md tells a reader to read ----
// Paths appear as `docs/...` in backticks or as markdown links. Both forms, deduped,
// in first-mention order — which is the order a reader meets them.
// Cold stores and generated volumes are named in CLAUDE.md only to say where a record
// WENT — they are on-demand by construction, so counting them as reading order would
// punish exactly the move that shrank the reading order.
const ON_DEMAND = /^docs\/development\/archive\//;

const claude = fs.readFileSync(path.join(ROOT, 'CLAUDE.md'), 'utf8');
const named = [];
const seen = new Set();
for (const m of claude.matchAll(/(?:`|\]\()((?:docs\/|README\.md|CHANGELOG\.md)[^`)\s]*)/g)) {
  const rel = m[1].replace(/[.,]$/, '');
  if (ON_DEMAND.test(rel)) continue;
  if (seen.has(rel)) continue;
  seen.add(rel);
  named.push(rel);
}

function stat(rel) {
  const abs = path.join(ROOT, rel);
  try {
    const s = fs.statSync(abs);
    if (!s.isFile()) return null;
    return { rel, bytes: s.size, tokens: tokens(s.size) };
  } catch { return null; }
}

const readingOrder = named.map(stat).filter(Boolean);
const missing = named.filter((r) => !stat(r));

// ---- everything under docs/, so we can name what the reading order omits ----
function walk(dir, acc = []) {
  for (const e of fs.readdirSync(path.join(ROOT, dir), { withFileTypes: true })) {
    const rel = `${dir}/${e.name}`;
    if (e.isDirectory()) walk(rel, acc);
    else acc.push(stat(rel));
  }
  return acc.filter(Boolean);
}
const everything = walk('docs');
const inOrder = new Set(readingOrder.map((f) => f.rel));
const unnamed = everything.filter((f) => !inOrder.has(f.rel)).sort((a, b) => b.bytes - a.bytes);

const total = readingOrder.reduce((n, f) => n + f.tokens, 0);
const corpus = everything.reduce((n, f) => n + f.tokens, 0);
const over = total > BUDGET;

if (asJson) {
  console.log(JSON.stringify({
    budget: BUDGET, reading_order_tokens: total, corpus_tokens: corpus,
    over_budget: over, missing,
    reading_order: readingOrder, heaviest_unnamed: unnamed.slice(0, 15),
  }, null, 1));
  process.exit(over ? 1 : 0);
}

const table = (rows) => {
  const w = Math.max(...rows.map((f) => f.rel.length));
  for (const f of rows) {
    console.log(`  ${f.rel.padEnd(w)}  ${kb(f.bytes).padStart(8)}  ${num(f.tokens).padStart(9)} tok`);
  }
};

console.log(`\nReading order — the ${readingOrder.length} file(s) CLAUDE.md names, in first-mention order:\n`);
table(readingOrder);

if (missing.length) {
  console.log(`\n  ${missing.length} named path(s) do not resolve to a file:`);
  missing.forEach((m) => console.log(`    - ${m}`));
}

console.log(`\n  reading order   ~${num(total)} tokens  (budget ${num(BUDGET)}, ${over ? `OVER by ${num(total - BUDGET)}` : `${num(BUDGET - total)} to spare`})`);
console.log(`  whole docs/     ~${num(corpus)} tokens across ${everything.length} files`);

console.log(`\nHeaviest files the reading order does NOT name — reached on demand, so keep them queryable:\n`);
table((showAll ? unnamed : unnamed.slice(0, 10)));

if (over) {
  console.log(`\nOver budget. The lever is not deletion — it is making a file reachable by query`);
  console.log(`instead of by read: an index the reader holds, and a tool that fetches one record`);
  console.log(`(tools/session/backlog_query.js, actions_query.js, DEVLOG_INDEX.md are the pattern).`);
}
process.exit(over ? 1 : 0);
