#!/usr/bin/env node
// tech_tree_store.js — check an age's tech-tree store and render its node tables into the tree's doc.
//
//   node tools/session/tech_tree_store.js [age] [--check]     (age defaults to digitisation)
//
// The store (docs/economy/tech/<age>_tree.json) is canonical for every node; the doc
// (docs/economy/tech/<AGE>_TREE.md) owns the prose and carries the generated tables between the
// BEGIN/END GENERATED markers. --check renders nothing and exits 1 if the doc's tables are stale.
// The grammar every check enforces is docs/economy/RESEARCH.md § The shape of an age's tree.
// Its sibling, tools/session/gen_tech_tree_table.js, renders the same store into the C++ table the
// sim reads (src/world/digitisation_tree_data.hpp); re-run both after any store edit.
'use strict';
const fs = require('fs');
const path = require('path');

const args = process.argv.slice(2);
const checkOnly = args.includes('--check');
const age = (args.find(a => !a.startsWith('--')) || 'digitisation').toLowerCase();
const root = path.resolve(__dirname, '..', '..');
const dir = path.join(root, 'docs', 'economy', 'tech');
const storePath = path.join(dir, `${age}_tree.json`);
const docPath = path.join(dir, `${age.toUpperCase()}_TREE.md`);

const store = JSON.parse(fs.readFileSync(storePath, 'utf8'));
const nodes = store.nodes;
const byId = new Map(nodes.map(n => [n.id, n]));
const errors = [];
const err = m => errors.push(m);

// ---- the grammar (RESEARCH.md § The shape of an age's tree) --------------------------------
const TIERS = new Set(['travel', 'minor', 'major', 'quest']);
if (byId.size !== nodes.length) err('duplicate node ids');
for (const n of nodes) {
  if (!TIERS.has(n.tier)) err(`${n.id}: unknown tier ${n.tier}`);
  if (!(n.ring >= 1 && n.ring <= 4)) err(`${n.id}: ring ${n.ring} outside 1..4`);
  if (n.cost_units !== store.tiers[n.tier] * n.ring) err(`${n.id}: cost_units ${n.cost_units} != base x ring`);
  if (!Array.isArray(n.requires)) err(`${n.id}: requires must be a list of groups`);
  for (const g of n.requires || []) {
    if (!Array.isArray(g) || !g.length) err(`${n.id}: empty requirement group`);
    for (const r of g) if (!byId.has(r)) err(`${n.id}: requires unknown ${r}`);
  }
  if (n.excludes) {
    const o = byId.get(n.excludes);
    if (!o || o.excludes !== n.id) err(`${n.id}: excludes is not symmetric`);
    if (n.tier !== 'major') err(`${n.id}: only majors form pick-one pairs`);
  }
  if (n.tier === 'major' && !n.placeholder && n.requires.length < 1) err(`${n.id}: a major needs its minors`);
  if (n.tier === 'quest' && (!n.deed || !n.keys_next_branch)) err(`${n.id}: a quest needs a deed and the branch it keys`);
}
// Every branch: exactly one quest, one mastery; every major either needs >=2 minors or is a pair side.
const branches = store.trees.flatMap(t => t.branches.map(b => b.key));
for (const b of branches) {
  const inB = nodes.filter(n => n.branch === b);
  if (inB.filter(n => n.tier === 'quest').length !== 1) err(`${b}: needs exactly one quest`);
  if (inB.filter(n => n.mastery).length !== 1) err(`${b}: needs exactly one mastery`);
  for (const m of inB.filter(n => n.tier === 'major')) {
    if (!m.excludes && m.requires.length < 2) err(`${m.id}: a major outside a pair needs both minors (two groups)`);
  }
}
// Roots: only first travel nodes are open from the start.
for (const n of nodes) if (!n.requires.length && !(n.tier === 'travel' && /-T1$/.test(n.id))) err(`${n.id}: only a branch's first travel node may have no requirement`);
// Reachability from the roots (placeholders count as reachable; they can be researched).
const held = new Set(nodes.filter(n => !n.requires.length).map(n => n.id));
for (let changed = true; changed;) {
  changed = false;
  for (const n of nodes) if (!held.has(n.id) && n.requires.every(g => g.some(r => held.has(r)))) { held.add(n.id); changed = true; }
}
for (const n of nodes) if (!held.has(n.id)) err(`${n.id}: unreachable`);

// ---- the render -------------------------------------------------------------------------------
const esc = s => String(s).replace(/\|/g, '\\|');
const reqText = n => n.requires.map(g => g.map(r => byId.get(r).name).join(' or ')).join(' + ') || '— (open)';
const kind = n => n.joint ? 'joint' : n.mastery ? 'mastery' : n.tier + (n.excludes ? ' (pair)' : '') + (n.placeholder ? ' (placeholder)' : '');
let out = '';
const count = t => nodes.filter(n => n.tier === t).length;
out += `**${nodes.length} nodes** — ${count('travel')} travel (${nodes.filter(n => n.joint).length} joints, ${nodes.filter(n => n.mastery).length} masteries), ${count('minor')} minor, ${count('major')} major, ${count('quest')} quest; ${nodes.filter(n => n.placeholder).length} placeholder nodes. Whole age: ${nodes.reduce((s, n) => s + n.cost_units, 0)} cost units.\n\n`;
for (const t of store.trees) {
  out += `### ${t.name} — ${t.thesis}\n\n`;
  for (const b of t.branches) {
    out += `#### ${b.name} (\`${b.key}\`)\n\n| Id | Node | Tier | Ring | Advantage | Requires |\n|---|---|---|---|---|---|\n`;
    for (const n of nodes.filter(x => x.branch === b.key)) {
      const adv = n.tier === 'quest' ? `${n.advantage}. **Deed:** ${n.deed}. **Keys next age:** ${n.keys_next_branch}` : n.advantage;
      out += `| \`${n.id}\` | ${esc(n.name)} | ${kind(n)} | ${n.ring} | ${esc(adv)} | ${esc(reqText(n))} |\n`;
    }
    out += '\n';
  }
}
out += `### Joints\n\n| Id | Node | Ring | Advantage | Requires |\n|---|---|---|---|---|\n`;
for (const n of nodes.filter(x => x.joint)) out += `| \`${n.id}\` | ${esc(n.name)} | ${n.ring} | ${esc(n.advantage)} | ${esc(reqText(n))} |\n`;

const BEGIN = '<!-- BEGIN GENERATED NODES (tools/session/tech_tree_store.js) -->';
const END = '<!-- END GENERATED NODES -->';
const doc = fs.readFileSync(docPath, 'utf8');
const i = doc.indexOf(BEGIN), j = doc.indexOf(END);
if (i < 0 || j < 0) err(`${path.basename(docPath)}: generated markers missing`);
const next = i >= 0 && j >= 0 ? doc.slice(0, i + BEGIN.length) + '\n\n' + out + '\n' + doc.slice(j) : doc;
const stale = next !== doc;

if (errors.length) { for (const e of errors) console.log('FAIL ' + e); }
if (checkOnly) {
  if (stale) console.log(`FAIL ${path.basename(docPath)}: generated tables are stale — run without --check`);
  console.log(errors.length || stale ? `tech_tree_store: ${age} FAIL` : `tech_tree_store: ${age} OK — ${nodes.length} nodes`);
  process.exit(errors.length || stale ? 1 : 0);
}
if (!errors.length && stale) fs.writeFileSync(docPath, next);
console.log(errors.length ? `tech_tree_store: ${age} FAIL (${errors.length})` : `tech_tree_store: ${age} OK — ${nodes.length} nodes${stale ? ', tables rendered' : ', tables current'}`);
process.exit(errors.length ? 1 : 0);
