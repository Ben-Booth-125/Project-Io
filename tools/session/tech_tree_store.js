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
// ---- the effects (RESEARCH.md § Effects no system reads yet; BL-1274) ----------------------
// Every node carries >= 1 machine effect; each is well-formed for its kind. Whether CODE reads an
// effect is nation_tech.cpp's declaration, and nation_tech_harness's R1 audit holds the two together
// (the header this store renders into, via gen_tech_tree_table.js, is what that audit reads).
const SUBJECTS = new Set(['extraction_rate', 'processing_yield', 'unit_upkeep', 'logistics_cost',
                          'wage_floor', 'collapse_strain', 'national_research',
                          'alarm_toward', 'alarm_tell', 'alarm_decay']); // modifier_set.hpp (BL-1432)
const POINT_SUBJECTS = new Set(['alarm_tell']); // added in points, not percent (BL-1432)
const SCOPE_RE = /^(all|resource:[a-z_]+|group:[A-Za-z &]+|recipe:[a-z_]+|building:[a-z_]+|site:[a-z_]+)$/;
const str = v => typeof v === 'string' && v.length > 0;
// An effect has two states (RESEARCH.md): read, or unread. No third.
const STATES_ONLY = new Set(['kind', 'subject', 'scope', 'op', 'value', 'recipe', 'structure', 'key', 'what', 'waits_on', 'items']);

// THE DIRECTION AUDIT (BL-1274 review #3): a modifier's sign and size must be the prose's. Every
// percentage in `advantage` is read as signed ("+5%", "-10%"; an unsigned one is unparseable
// unless an unread effect carries it); those an unread effect's own `what` carries are struck out
// in order; what remains must equal the node's modifiers, in order: multiply value = 1 + p/100,
// add value = p/100.
// "N% faster" reads as +N (BL-1432: "Alarm toward you decays 10% faster" is a x1.10 rate).
const pcts = s => [...String(s).replace(/−/g, '-').matchAll(/([+-]?)(\d+(?:\.\d+)?)%( faster)?/g)]
  .map(m => (m[1] ? (m[1] === '-' ? -1 : 1) * Number(m[2]) : m[3] ? Number(m[2]) : NaN));
const close = (a, b) => (Number.isNaN(a) && Number.isNaN(b)) || Math.abs(a - b) < 1e-9;
const unparsed = [];

for (const n of nodes) {
  if (!Array.isArray(n.effects) || !n.effects.length) { err(`${n.id}: needs at least one effect`); continue; }
  for (const e of n.effects) {
    const at = `${n.id}: effect ${JSON.stringify(e)}`;
    for (const k of Object.keys(e)) if (!STATES_ONLY.has(k)) err(`${at}: unknown field "${k}"`);
    switch (e.kind) {
      case 'modifier': {
        if (!SUBJECTS.has(e.subject)) err(`${at}: unknown modifier subject`);
        const scopes = Array.isArray(e.scope) ? e.scope : [e.scope];
        if (!scopes.length || !scopes.every(s => typeof s === 'string' && SCOPE_RE.test(s))) err(`${at}: bad scope`);
        if (Array.isArray(e.scope) && new Set(e.scope).size !== e.scope.length) err(`${at}: duplicate scope`);
        if (e.op !== 'multiply' && e.op !== 'add') err(`${at}: op must be multiply or add`);
        if (typeof e.value !== 'number' || !Number.isFinite(e.value)) err(`${at}: value must be a finite number`);
        if (e.op === 'multiply' && !(e.value > 0)) err(`${at}: a multiply must be positive`);
        break;
      }
      case 'unlock_recipe': if (!str(e.recipe)) err(`${at}: needs recipe`); break;
      case 'unlock_structure': if (!str(e.structure)) err(`${at}: needs structure`); break;
      case 'mechanic': if (!str(e.key)) err(`${at}: needs key`); break;
      // BL-1436 (Ben, 2026-11-03, ruling 7): the kit a node opens, by catalogue item id. The
      // catalogue's research_node is the read path; the cross-check below holds the two equal.
      case 'unlock_kit':
        if (!Array.isArray(e.items) || !e.items.length || !e.items.every(str)) err(`${at}: unlock_kit needs a list of item ids`);
        else if (new Set(e.items).size !== e.items.length) err(`${at}: duplicate kit item`);
        break;
      case 'unread':
        if (!str(e.what) || !str(e.waits_on)) err(`${at}: unread needs what and waits_on`);
        break;
      default: err(`${at}: unknown kind`);
    }
  }
  // The direction audit.
  {
    const prose = pcts(n.advantage);
    const rest = prose.slice();
    let ok = true;
    for (const e of n.effects.filter(x => x.kind === 'unread')) {
      for (const p of pcts(e.what)) {
        const i = rest.findIndex(q => close(q, p));
        if (i < 0) { ok = false; unparsed.push(`${n.id}: unread "${e.what}" names ${p}% the advantage does not`); }
        else rest.splice(i, 1);
      }
    }
    if (rest.some(Number.isNaN)) { ok = false; unparsed.push(`${n.id}: an unsigned percentage no unread effect carries, in "${n.advantage}"`); }
    // A POINT subject (BL-1432: alarm_tell, in Alarm points) is not a percentage, so it
    // sits outside the percentage audit; its own check is that the prose names the same
    // signed figure after the word Alarm.
    for (const e of n.effects.filter(x => x.kind === 'modifier' && POINT_SUBJECTS.has(x.subject))) {
      const fig = (e.value > 0 ? '+' : '') + e.value;
      const named = n.advantage.split(/[;.]/).some(c => {
        const at = c.indexOf('Alarm');
        if (at < 0) return false;
        const m = c.slice(at).match(/([+-]\d+(?:\.\d+)?)(?![\d%])/);
        return m !== null && Number(m[1]) === e.value;
      });
      if (e.op !== 'add' || !named) err(`${n.id}: point modifier ${e.subject} ${fig} is not named as "Alarm ... ${fig}" in "${n.advantage}"`);
    }
    const mods = n.effects.filter(x => x.kind === 'modifier' && !POINT_SUBJECTS.has(x.subject))
      .map(e => (e.op === 'multiply' ? (e.value - 1) * 100 : e.value * 100));
    if (ok && (mods.length !== rest.length || !mods.every((m, i) => close(Math.round(m * 1e6) / 1e6, rest[i])))) {
      err(`${n.id}: modifier direction/size [${mods.map(m => (m > 0 ? '+' : '') + Math.round(m * 1e6) / 1e6 + '%').join(', ')}]` +
          ` disagrees with the advantage's [${rest.map(p => (p > 0 ? '+' : '') + p + '%').join(', ')}]: "${n.advantage}"`);
    }
  }
  if (n.placeholder && !(n.effects.length === 1 && n.effects[0].kind === 'unread' && n.effects[0].waits_on === n.placeholder))
    err(`${n.id}: a placeholder carries one unread effect waiting on ${n.placeholder}`);
  if (n.tier === 'quest') {
    const d = n.deed_effect;
    if (!d || d.kind !== 'unread' || !str(d.waits_on)) err(`${n.id}: a quest's deed_effect is {kind: "unread", waits_on} until a deed reader exists`);
  } else if (n.deed_effect !== undefined) err(`${n.id}: only a quest carries a deed_effect`);
}
for (const u of unparsed) err(`direction audit cannot parse — ${u}`);

// ---- the kit link (BL-1436; Ben, 2026-11-03, ruling 7) -------------------------------------
// The kit catalogue (src/world/unit_designs.cpp, the land `later(...)`, air `alater(...)` and ship `slater(...)`
// rows) names each later item's research node; the store's unlock_kit effects name each node's
// items. Both directions must agree: every catalogue item's node is in the store and carries an
// unlock_kit naming it, and every unlock_kit item is a catalogue item naming that node.
if (age === 'digitisation') {
  const cat = fs.readFileSync(path.join(root, 'src', 'world', 'unit_designs.cpp'), 'utf8');
  const itemNode = new Map();
  for (const m of cat.matchAll(/\b[as]?later\(\s*"([a-z0-9_.]+)"[^\n]*?"(DG-[A-Z0-9-]+)"/g)) {
    if (itemNode.has(m[1])) err(`catalogue: item ${m[1]} listed twice`);
    itemNode.set(m[1], m[2]);
  }
  if (!itemNode.size) err('catalogue: no later items parsed from unit_designs.cpp');
  const opened = new Map();
  for (const n of nodes) for (const e of n.effects || []) if (e.kind === 'unlock_kit' && Array.isArray(e.items))
    for (const it of e.items) {
      if (opened.has(it)) err(`${n.id}: kit item ${it} is also opened by ${opened.get(it)}`);
      opened.set(it, n.id);
    }
  for (const [it, node] of itemNode) {
    if (!byId.has(node)) err(`catalogue: ${it} names ${node}, not in the tree`);
    else if (opened.get(it) !== node) err(`catalogue: ${it} names ${node}, whose unlock_kit does not list it`);
  }
  for (const [it, node] of opened) if (itemNode.get(it) !== node)
    err(`${node}: unlock_kit lists ${it}, which the catalogue ${itemNode.has(it) ? 'puts on ' + itemNode.get(it) : 'does not hold'}`);
}

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
// Line endings are git's (a CRLF checkout of an LF render is not stale).
const stale = next.replace(/\r\n/g, '\n') !== doc.replace(/\r\n/g, '\n');

if (errors.length) { for (const e of errors) console.log('FAIL ' + e); }
if (checkOnly) {
  if (stale) console.log(`FAIL ${path.basename(docPath)}: generated tables are stale — run without --check`);
  console.log(errors.length || stale ? `tech_tree_store: ${age} FAIL` : `tech_tree_store: ${age} OK — ${nodes.length} nodes`);
  process.exit(errors.length || stale ? 1 : 0);
}
if (!errors.length && stale) fs.writeFileSync(docPath, next);
console.log(errors.length ? `tech_tree_store: ${age} FAIL (${errors.length})` : `tech_tree_store: ${age} OK — ${nodes.length} nodes${stale ? ', tables rendered' : ', tables current'}`);
process.exit(errors.length ? 1 : 0);
