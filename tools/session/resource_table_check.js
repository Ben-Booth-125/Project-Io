#!/usr/bin/env node
// resource_table_check.js — the static join BL-414 is missing, in the cheap form.
//
// resource_type is transcribed by hand into FIVE places and no two of them are
// checked against each other:
//
//   1. src/world/components.hpp          the enum — the authority
//   2. src/world/recipe_registry.cpp     name -> enum, for recipes.lua / economy.lua
//   3. src/world/world_gen_config.cpp    name -> enum, for world_gen.lua
//   4. src/ui/presentation.cpp           a POSITIONAL array of display rows
//   5. src/core/verify_api.cpp           a POSITIONAL array of enum slugs, for
//                                        the verify hooks that take a good by name
//
// Every one of those has already failed in a way nobody caught at review:
//   * BL-286 added eleven values the Lua loader could not parse — eleven days
//     before anyone noticed (NR-237).
//   * A missing world_gen_config row killed the app at startup with
//     "Unknown resource 'medical_supplies'" (2026-08-12), which is what filed
//     BL-414.
//   * The habitability tranche shipped with NO presentation rows at all and
//     rendered as "(unnamed resource)" from 2026-08-11 until BL-457 found it
//     on 2026-08-17 — a positional array silently value-initialises its tail,
//     so the compiler never says a word.
//   * verify_api.cpp's slug table sat at 20 of 38 values and fell back to
//     iron_ore for the rest, so a check naming 'ordnance' seeded an IRON ORE
//     convoy and passed (NR-324, 2026-08-18). This guard reported OK throughout,
//     because it only knew about the other four.
//
// This is not BL-414's fix. BL-414 consolidates the tables so the desync cannot
// be authored; this only *detects* it, and should be deleted the day that lands.
// It exists because the detection is ~80 lines and has no build dependency,
// while the consolidation is a real refactor — and because the presentation
// array's failure mode (a silent zero-filled tail) is invisible to the compiler
// and to every harness, so nothing else can see it.
//
// Usage:  node tools/session/resource_table_check.js [--quiet]
// Exit 0 = all four agree. Exit 1 = a gap, printed per table.

const fs = require('fs');

const ENUM_FILE = 'src/world/components.hpp';
const MAPS = [
  ['recipe_registry', 'src/world/recipe_registry.cpp'],
  ['world_gen_config', 'src/world/world_gen_config.cpp'],
];
const PRESENTATION = 'src/ui/presentation.cpp';
const VERIFY_SLUGS = 'src/core/verify_api.cpp';

const quiet = process.argv.includes('--quiet');
const read = f => fs.readFileSync(f, 'utf8');

// --- 1. the enum, which wins over everything -------------------------------
function readEnum() {
  const body = read(ENUM_FILE).split('enum class resource_type')[1];
  if (!body) throw new Error('resource_type enum not found in ' + ENUM_FILE);
  const block = body.split('};')[0];
  const rows = [...block.matchAll(/^\s{4}([a-z_][a-z0-9_]*)\s*=\s*(\d+)/gm)]
    .map(m => ({ name: m[1], id: Number(m[2]) }));
  const count = rows.find(r => r.name === 'count');
  const values = rows.filter(r => r.name !== 'count');
  return { values, count: count ? count.id : null };
}

// --- 2/3. the two name -> enum maps ----------------------------------------
function readNameMap(file) {
  return new Set(
    [...read(file).matchAll(/\{\s*"([a-z_][a-z0-9_]*)"\s*,\s*resource_type::([a-z_][a-z0-9_]*)\s*\}/g)]
      .map(m => {
        if (m[1] !== m[2]) mismatched.push({ file, key: m[1], value: m[2] });
        return m[1];
      })
  );
}
const mismatched = [];

// --- 4. the positional presentation array ----------------------------------
// Counts EVERY row, including the `{ nullptr, nullptr, 0 }` placeholders, since
// those are what hold the alignment. A row is "authored" if it has a name.
function readPresentation() {
  const tbl = read(PRESENTATION).split('resource_table[resource_count] = {')[1];
  if (!tbl) throw new Error('resource_table not found in ' + PRESENTATION);
  const block = tbl.split('\n};')[0];
  return [...block.matchAll(/^\s*\{\s*(?:"([^"]*)"|nullptr)/gm)].map(m => m[1] ?? null);
}

// --- 5. the positional slug array the verify hooks resolve names through -----
// Same failure shape as presentation.cpp, and the same reason a compiler cannot
// see it: the entries are plain strings, so a short or reordered table is legal.
// verify_api.cpp carries its own static_assert on the LENGTH; this checks the
// ORDER, which the assert cannot.
function readVerifySlugs() {
  const tbl = read(VERIFY_SLUGS).split('k_resource_slugs[] = {')[1];
  if (!tbl) throw new Error('k_resource_slugs not found in ' + VERIFY_SLUGS);
  return [...tbl.split('\n};')[0].matchAll(/"([a-z_][a-z0-9_]*)"/g)].map(m => m[1]);
}

const { values, count } = readEnum();
const present = readPresentation();
const slugs = readVerifySlugs();
const maps = MAPS.map(([label, file]) => [label, file, readNameMap(file)]);

const problems = [];

if (count !== values.length) {
  problems.push(`components.hpp: count = ${count} but ${values.length} values are declared`);
}

for (const [label, , set] of maps) {
  for (const v of values) if (!set.has(v.name)) problems.push(`${label}: no row for '${v.name}'`);
  for (const k of set) if (!values.some(v => v.name === k)) problems.push(`${label}: stale row '${k}' — no such enum value`);
}

for (const { key, value, file } of mismatched) {
  problems.push(`${file}: "${key}" maps to resource_type::${value} — string and enum disagree`);
}

// The presentation array is positional: a missing tail row is value-initialised
// to nulls and renders as "(unnamed resource)" with no compiler diagnostic.
if (present.length !== values.length) {
  problems.push(
    `presentation.cpp: ${present.length} rows for ${values.length} enum values — ` +
    `indices ${present.length}..${values.length - 1} are value-initialised and will render as "(unnamed resource)"`
  );
}
for (let i = 0; i < Math.min(present.length, values.length); ++i) {
  if (present[i] === null) continue; // an explicit, deliberate placeholder
  const enumWord = values[i].name.replace(/_/g, '').toLowerCase();
  const dispWord = present[i].replace(/[^A-Za-z]/g, '').toLowerCase();
  const agree = enumWord.startsWith(dispWord.slice(0, 4)) || dispWord.startsWith(enumWord.slice(0, 4));
  if (!agree) {
    problems.push(`presentation.cpp: index ${i} is '${values[i].name}' but the row reads "${present[i]}" — the array has shifted`);
  }
}

// The slug array is positional AND its entries are the enum names verbatim, so a
// mismatch at index i is unambiguous — no fuzzy display-name comparison needed.
if (slugs.length !== values.length) {
  problems.push(
    `verify_api.cpp: ${slugs.length} slugs for ${values.length} enum values — ` +
    `a verify script naming any of the missing goods silently gets iron_ore instead`
  );
}
for (let i = 0; i < Math.min(slugs.length, values.length); ++i) {
  if (slugs[i] !== values[i].name) {
    problems.push(`verify_api.cpp: index ${i} is '${values[i].name}' but the slug reads '${slugs[i]}' — the array has shifted`);
  }
}

if (problems.length) {
  console.error(`resource_table_check: ${problems.length} problem(s)\n`);
  for (const p of problems) console.error('  FAIL  ' + p);
  console.error(`\n${values.length} enum values checked against 4 transcriptions.`);
  process.exit(1);
}

if (!quiet) {
  console.log(`resource_table_check: OK — ${values.length} resource_type values agree across`);
  console.log('  components.hpp, recipe_registry.cpp, world_gen_config.cpp, presentation.cpp,');
  console.log('  verify_api.cpp');
}
