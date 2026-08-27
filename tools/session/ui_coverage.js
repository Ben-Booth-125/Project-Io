#!/usr/bin/env node
// ui_coverage.js — what a green visual run actually proves, per UI element.
//
// The UI element catalogue (docs/ui/ui_elements.json) says WHAT EXISTS ON SCREEN.
// This tool answers the next question, which nothing answered before Sprint 21:
// if I change this element, what goes red?
//
// It reads each element's hand-curated `checks` list, then derives the strength
// of those checks from the scripts themselves, so the derived half is never
// stale — a golden blessed or an assertion added shows up on the next run with
// no edit here.
//
//   GOLDEN        a committed scripts/verify/golden/<check>*.png diffs it every
//                 run. It fails by itself. There are two in the whole repo.
//   ASSERTED      no golden, but a covering check calls verify.expect on real
//                 content. It fails by itself.
//   CLIP-ONLY     the only assertion reaching it is expect_no_clipping. Green
//                 means "no string overran its box", NOT "this element is right".
//                 Called out separately because it is the exact shape of a check
//                 whose green means less than it appears (NR-663's family).
//   CAPTURE-ONLY  a check frames it and saves a PNG. Nothing fails. A human eye
//                 is the entire check.
//   NONE          no verify script drives it at all.
//
// It also reports ORPHAN CHECKS — scripts driving a surface no element claims.
// That is the catalogue's staleness detector: the 2026-07-31 snapshot had
// fourteen, which is how the four-week drift was found.
//
// USAGE:  node tools/session/ui_coverage.js [options]
//   (no args)          render docs/ui/UI_COVERAGE.md and print the tally
//   --class <NAME>     list one class only (GOLDEN|ASSERTED|CLIP-ONLY|CAPTURE-ONLY|NONE)
//   --element <UI-nnn> everything known about one element
//   --check <name>     reverse lookup: which elements a given check covers
//   --orphans          only the checks no element claims
//   --json             machine-readable dump, no render
//   --no-render        print only; leave UI_COVERAGE.md alone
// EXIT:   0 always — this is an instrument, not a gate. It reports; it does not judge.
//
// Zero dependencies (fs only). Companion to story_check.js and backlog_lint.js.

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const P = (rel) => path.join(ROOT, rel);
const ELEMENTS = 'docs/ui/ui_elements.json';
const SCRIPTS = 'scripts/verify';
const GOLDENS = 'scripts/verify/golden';
const OUT = 'docs/ui/UI_COVERAGE.md';

const CLASSES = ['GOLDEN', 'ASSERTED', 'CLIP-ONLY', 'CAPTURE-ONLY', 'NONE'];
const MEANING = {
  GOLDEN: 'A committed `scripts/verify/golden/*.png` diffs this element on every run. It fails by itself.',
  ASSERTED: 'No golden, but a covering check calls `verify.expect` on real content. It fails by itself.',
  'CLIP-ONLY': 'The only assertion reaching it is `expect_no_clipping`. Green means "no string overran its box", not "this element is right".',
  'CAPTURE-ONLY': 'A check frames it and saves a PNG. Nothing fails. A human eye is the entire check.',
  NONE: 'No verify script drives this element at all.',
};

// --- read ------------------------------------------------------------------

const catalogue = JSON.parse(fs.readFileSync(P(ELEMENTS), 'utf8'));
const elements = catalogue.elements;
const byId = Object.fromEntries(elements.map((e) => [e.id, e]));

const goldenFiles = fs.existsSync(P(GOLDENS))
  ? fs.readdirSync(P(GOLDENS)).filter((f) => f.endsWith('.png'))
  : [];
const goldenStems = goldenFiles.map((g) => g.replace(/\.png$/, ''));

// Per-script strength, derived fresh every run.
const checks = {};
for (const f of fs.readdirSync(P(SCRIPTS)).filter((x) => x.endsWith('.lua') && x !== 'lib.lua')) {
  const name = f.replace(/\.lua$/, '');
  const t = fs.readFileSync(path.join(P(SCRIPTS), f), 'utf8');
  const count = (re) => (t.match(re) || []).length;
  checks[name] = {
    name,
    // expect_no_clipping is deliberately NOT counted as an expect: it asserts
    // layout, never content, and conflating the two is what CLIP-ONLY exists
    // to prevent.
    expect: count(/verify\.expect\(/g),
    clip: count(/expect_no_clipping/g),
    clicks: count(/verify\.click|verify\.press/g),
    captures: count(/verify\.capture\(|\bshot\(/g),
    // A golden belongs to a check when it is named <check> or <check>_<something>,
    // which is the naming every capture in the suite follows.
    goldens: goldenStems.filter((g) => g === name || g.startsWith(name + '_')),
    header: (t.match(/^--\s?(.*)$/m) || [, ''])[1].trim(),
  };
}

// --- classify --------------------------------------------------------------

const pathOf = (e) => {
  const parts = [];
  for (let c = e; c; c = c.parent ? byId[c.parent] : null) parts.unshift(c.name);
  return parts.join(' > ');
};

const rows = elements.map((e) => {
  const named = e.checks || [];
  const live = named.filter((c) => checks[c]);
  const missing = named.filter((c) => !checks[c]);
  const sum = (k) => live.reduce((a, c) => a + checks[c][k], 0);
  const goldens = live.flatMap((c) => checks[c].goldens);
  const expect = sum('expect');
  const clip = sum('clip');
  const cls = goldens.length ? 'GOLDEN'
    : expect > 0 ? 'ASSERTED'
    : clip > 0 ? 'CLIP-ONLY'
    : live.length ? 'CAPTURE-ONLY'
    : 'NONE';
  return {
    id: e.id, name: e.name, kind: e.kind, path: pathOf(e),
    cls, checks: live, missing, expect, clip, clicks: sum('clicks'),
    captures: sum('captures'), goldens,
  };
});

const claimed = new Set(rows.flatMap((r) => r.checks));
const orphans = Object.keys(checks).filter((c) => !claimed.has(c)).sort();
const brokenRefs = rows.filter((r) => r.missing.length);

const tally = Object.fromEntries(CLASSES.map((c) => [c, rows.filter((r) => r.cls === c).length]));

// --- render ----------------------------------------------------------------

function renderMarkdown() {
  const L = [];
  L.push('# UI element visual-check coverage');
  L.push('');
  L.push('*Generated by `node tools/session/ui_coverage.js` — do not hand-edit.*');
  L.push('');
  L.push('*STATE, not authority. This says what is checked today, never what should be.*');
  L.push('*The element rows and their `checks` lists live in `docs/ui/ui_elements.json`;*');
  L.push('*everything else here is derived from `scripts/verify/*.lua` on each run.*');
  L.push('');
  L.push('## Headline');
  L.push('');
  L.push(`**${elements.length} elements. ${goldenFiles.length} committed goldens in the whole repo** (${goldenFiles.join(', ') || 'none'}).`);
  const soft = tally['CAPTURE-ONLY'] + tally['CLIP-ONLY'] + tally.NONE;
  L.push('');
  L.push(`**${soft} of ${elements.length} elements can be changed without any check going red.**`);
  L.push('That is the number that matters to a UI pass: the re-verification cost is near zero,');
  L.push('and so is the safety net.');
  L.push('');
  L.push('| Class | Count | What green means |');
  L.push('|---|---|---|');
  for (const c of CLASSES) L.push(`| **${c}** | ${tally[c]} | ${MEANING[c]} |`);
  L.push('');
  for (const c of CLASSES) {
    L.push(`## ${c} (${tally[c]})`);
    L.push('');
    if (!tally[c]) { L.push('*None.*', ''); continue; }
    L.push('| Element | Kind | Covering checks | expect | clip | clicks |');
    L.push('|---|---|---|---|---|---|');
    for (const r of rows.filter((x) => x.cls === c)) {
      const cs = r.checks.map((x) => '`' + x + '`').join(', ') || '—';
      L.push(`| \`${r.id}\` ${r.path} | ${r.kind} | ${cs} | ${r.expect} | ${r.clip} | ${r.clicks} |`);
    }
    L.push('');
  }
  L.push('## Orphan checks');
  L.push('');
  L.push('Checks driving a surface no catalogue element claims. This is the catalogue\'s');
  L.push('staleness detector — a non-empty list means the UI grew and the spine did not.');
  L.push('');
  if (!orphans.length) L.push('*None — every check maps to an element.*');
  else for (const o of orphans) L.push(`- \`${o}\` (${checks[o].captures} captures, ${checks[o].expect} expects) — ${checks[o].header}`);
  L.push('');
  if (brokenRefs.length) {
    L.push('## Broken check references');
    L.push('');
    L.push('An element names a check that no longer exists in `scripts/verify/`.');
    L.push('');
    for (const r of brokenRefs) L.push(`- \`${r.id}\` ${r.name} → ${r.missing.map((m) => '`' + m + '`').join(', ')}`);
    L.push('');
  }
  return L.join('\n') + '\n';
}

// --- cli -------------------------------------------------------------------

const argv = process.argv.slice(2);
const flag = (n) => argv.includes(n);
const value = (n) => { const i = argv.indexOf(n); return i >= 0 ? argv[i + 1] : null; };

if (flag('--json')) {
  console.log(JSON.stringify({ elements: elements.length, goldens: goldenFiles, tally, rows, orphans, brokenRefs }, null, 1));
  process.exit(0);
}

const one = value('--element');
if (one) {
  const r = rows.find((x) => x.id.toLowerCase() === one.toLowerCase());
  if (!r) { console.error(`ui_coverage: no element ${one}`); process.exit(0); }
  console.log(`${r.id}  ${r.path}`);
  console.log(`  kind      ${r.kind}`);
  console.log(`  class     ${r.cls}`);
  console.log(`  meaning   ${MEANING[r.cls]}`);
  console.log(`  checks    ${r.checks.join(', ') || '(none)'}`);
  if (r.goldens.length) console.log(`  goldens   ${r.goldens.join(', ')}`);
  console.log(`  expect ${r.expect} · clip ${r.clip} · clicks ${r.clicks} · captures ${r.captures}`);
  if (r.missing.length) console.log(`  BROKEN    names a check that does not exist: ${r.missing.join(', ')}`);
  process.exit(0);
}

const rev = value('--check');
if (rev) {
  const covers = rows.filter((r) => r.checks.includes(rev));
  if (!checks[rev]) console.log(`ui_coverage: no script scripts/verify/${rev}.lua`);
  else console.log(`${rev} — ${checks[rev].header}\n  ${checks[rev].captures} captures · ${checks[rev].expect} expects · ${checks[rev].clip} clip · ${checks[rev].goldens.length} goldens`);
  console.log(`  covers ${covers.length} element(s):`);
  for (const r of covers) console.log(`    ${r.id}  ${r.path}`);
  process.exit(0);
}

// --captures [dir]: group a capture run BY ELEMENT rather than by filename, so a
// review pass walks surfaces instead of PNGs. Optional --since <minutes> keeps a
// fresh run separate from the ~300 stale PNGs a repo accumulates.
if (flag('--captures')) {
  const dir = value('--captures') && !value('--captures').startsWith('--') ? value('--captures') : 'screenshots';
  const sinceMin = Number(value('--since') || 0);
  if (!fs.existsSync(P(dir))) { console.log(`ui_coverage: no capture dir ${dir}`); process.exit(0); }
  const cutoff = sinceMin ? Date.now() - sinceMin * 60000 : 0;
  const shots = fs.readdirSync(P(dir))
    .filter((f) => f.endsWith('.png'))
    .filter((f) => !cutoff || fs.statSync(path.join(P(dir), f)).mtimeMs >= cutoff)
    .map((f) => f.replace(/\.png$/, ''));

  // A capture belongs to the check whose name is the LONGEST matching prefix —
  // longest, because `build_legibility` and `build_door_wide_roster` both prefix
  // `build_`, and the shortest match would file them together.
  const ownerOf = (shot) => {
    let best = null;
    for (const c of Object.keys(checks)) {
      if ((shot === c || shot.startsWith(c + '_')) && (!best || c.length > best.length)) best = c;
    }
    return best;
  };
  const byCheck = {};
  const unclaimed = [];
  for (const s of shots) {
    const o = ownerOf(s);
    if (!o) { unclaimed.push(s); continue; }
    (byCheck[o] = byCheck[o] || []).push(s);
  }
  const covered = rows.filter((r) => r.checks.some((c) => byCheck[c]));
  console.log(`ui_coverage: ${shots.length} capture(s) in ${dir}${sinceMin ? ` from the last ${sinceMin}m` : ''}`);
  console.log(`  ${covered.length} of ${rows.length} elements have a fresh capture to look at.\n`);
  for (const r of covered) {
    const mine = r.checks.filter((c) => byCheck[c]).flatMap((c) => byCheck[c]).sort();
    console.log(`${r.id}  ${r.path}   [${r.cls}]`);
    for (const m of mine) console.log(`    ${dir}/${m}.png`);
  }
  const dark = rows.filter((r) => !r.checks.some((c) => byCheck[c]));
  if (dark.length) {
    console.log(`\nNO CAPTURE THIS RUN (${dark.length}) — nothing to review, for good reasons or bad:`);
    for (const r of dark) console.log(`  ${r.id.padEnd(8)} ${r.cls.padEnd(13)} ${r.path}`);
  }
  if (unclaimed.length) {
    console.log(`\nCAPTURES MATCHING NO CHECK (${unclaimed.length}) — stale files, or a capture renamed away from its script:`);
    for (const u of unclaimed.slice(0, 40)) console.log(`  ${u}.png`);
    if (unclaimed.length > 40) console.log(`  ... and ${unclaimed.length - 40} more`);
  }
  process.exit(0);
}

if (flag('--orphans')) {
  console.log(`ui_coverage: ${orphans.length} check(s) claimed by no element`);
  for (const o of orphans) console.log(`  ${o.padEnd(28)} ${checks[o].captures} caps, ${checks[o].expect} expects — ${checks[o].header}`);
  process.exit(0);
}

const only = value('--class');
if (only) {
  const want = only.toUpperCase();
  if (!CLASSES.includes(want)) { console.error(`ui_coverage: --class must be one of ${CLASSES.join(' ')}`); process.exit(0); }
  console.log(`${want} — ${MEANING[want]}\n`);
  for (const r of rows.filter((x) => x.cls === want)) {
    console.log(`  ${r.id}  ${r.path}${r.checks.length ? '   [' + r.checks.join(', ') + ']' : ''}`);
  }
  process.exit(0);
}

if (!flag('--no-render')) fs.writeFileSync(P(OUT), renderMarkdown());

const soft = tally['CAPTURE-ONLY'] + tally['CLIP-ONLY'] + tally.NONE;
console.log(`ui_coverage: ${elements.length} elements, ${Object.keys(checks).length} checks, ${goldenFiles.length} goldens`);
for (const c of CLASSES) console.log(`  ${c.padEnd(13)} ${String(tally[c]).padStart(3)}`);
console.log(`  ${soft} of ${elements.length} elements change without any check going red.`);
if (orphans.length) console.log(`  ${orphans.length} orphan check(s) — the catalogue is behind the UI (--orphans).`);
if (brokenRefs.length) console.log(`  ${brokenRefs.length} element(s) name a check that does not exist.`);
if (!flag('--no-render')) console.log(`  -> ${OUT}`);
