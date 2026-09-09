#!/usr/bin/env node
// tree_lint.js — the five adjacency rules and the store/doc cross-check for the three
// pre-game technology trees (docs/generation/trees/TREES.md § The five rules).
//
// Run: node tools/session/tree_lint.js [colonisation|empire|industry|all]   (default: all)
// Exit 0 clean, 1 on any failure. Prints one summary line per tree.
//
// Rules enforced (each named in the failure text):
//   R1  an edge spans at most one ring
//   R2  a milestone's `requires` (and `requires_fork` pair) are majors at its own ring (travel is OR, meaning is AND)
//   R3  never more than two minors in a row
//   R4  a milestone requires majors from >= 2 branches at its ring
//   R5  a cross-branch link passes through a minor, at most one per branch pair per ring
//   plus: <= 64 nodes; ids unique and shaped <TR>-<BR>-<ring><seq>; branch/ring agree with id;
//   the spire has no minors and alternates major/milestone by ring; one milestone per ring on
//   the spire; every node reachable from the root by links; forks are symmetric, same ring,
//   both majors, sharing a linked minor; effect kinds / gate atoms / diffusion classes stay in
//   vocabulary; minors carry exactly one modifier; milestones carry exactly one `open`;
//   invested trees carry `pursued_when` on every node; every node id in the doc exists in the
//   store and vice versa.

const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..', '..');
const dir = path.join(root, 'docs', 'generation', 'trees');

const TREES = {
  colonisation: { prefix: 'CO', doc: 'COLONISATION_TREE.md', store: 'colonisation_tree.json', invested: false },
  empire:       { prefix: 'EM', doc: 'EMPIRE_TREE.md',       store: 'empire_tree.json',       invested: true },
  industry:     { prefix: 'IN', doc: 'INDUSTRY_TREE.md',     store: 'industry_tree.json',     invested: true },
};

const EFFECT_KINDS = new Set(['unlock', 'upgrade', 'retire', 'modifier', 'access', 'reach', 'intel',
  'institution', 'doctrine', 'resource', 'open']);
const GATE_ATOMS = new Set(['ore_q', 'fuel', 'arable', 'coastal', 'grassland']);
const DIFFUSION = new Set(['practice', 'artifact', 'capacity']);
const MOD_TERMS = new Set(['reach', 'carrying_capacity', 'manpower', 'defence', 'industrial', 'stores',
  'cohesion', 'assimilation', 'plague', 'research', 'forage', 'muster_cost']);
const KINDS = new Set(['minor', 'major', 'milestone']);
const CAP = 64;

function lintTree(name, spec) {
  let failures = 0;
  const fail = msg => { failures++; console.error(`  FAIL  [${name}] ${msg}`); };

  const storePath = path.join(dir, spec.store);
  const docPath = path.join(dir, spec.doc);
  if (!fs.existsSync(storePath)) { fail(`store missing: ${spec.store}`); return failures; }
  let store;
  try { store = JSON.parse(fs.readFileSync(storePath, 'utf8')); }
  catch (e) { fail(`store does not parse: ${e.message}`); return failures; }

  if (store.tree !== name) fail(`store.tree is ${store.tree}, expected ${name}`);
  if (store.cap !== CAP) fail(`store.cap is ${store.cap}, expected ${CAP}`);
  const branches = new Map((store.branches || []).map(b => [b.key, b]));
  if (!branches.has('SP')) fail('no spire branch "SP" declared');
  for (const b of branches.values()) if (!/^[A-Z]{2}$/.test(b.key)) fail(`branch key ${b.key} is not two upper-case letters`);

  const nodes = store.nodes || [];
  if (nodes.length > CAP) fail(`${nodes.length} nodes exceeds the cap of ${CAP}`);
  if (nodes.length === 0) { fail('no nodes'); return failures; }

  const idPat = new RegExp(`^${spec.prefix}-([A-Z]{2})-([1-9])([a-z]|m)$`);
  const byId = new Map();
  for (const n of nodes) {
    if (byId.has(n.id)) fail(`duplicate id ${n.id}`);
    byId.set(n.id, n);
  }

  // --- per-node field checks -------------------------------------------------------
  for (const n of nodes) {
    const m = idPat.exec(n.id || '');
    if (!m) { fail(`${n.id}: id not shaped ${spec.prefix}-<BR>-<ring><seq>`); continue; }
    const [, br, ringS, seq] = m;
    if (n.branch !== br) fail(`${n.id}: branch field ${n.branch} disagrees with id`);
    if (n.ring !== Number(ringS)) fail(`${n.id}: ring field ${n.ring} disagrees with id`);
    if (!branches.has(br)) fail(`${n.id}: branch ${br} not declared`);
    if (!KINDS.has(n.kind)) fail(`${n.id}: kind ${n.kind} not minor|major|milestone`);
    if ((seq === 'm') !== (n.kind === 'milestone')) fail(`${n.id}: the 'm' seq is for milestones only, and every milestone uses it`);
    if (n.kind === 'milestone' && br !== 'SP') fail(`${n.id}: milestones live on the spire`);
    if (br === 'SP' && n.kind === 'minor') fail(`${n.id}: the spire carries no minors`);
    if (!n.name) fail(`${n.id}: no name`);
    if (!Array.isArray(n.links)) fail(`${n.id}: links missing`);
    if (n.kind === 'major') {
      if (n.gate != null && !GATE_ATOMS.has(n.gate)) fail(`${n.id}: gate ${n.gate} not in vocabulary`);
      if (!DIFFUSION.has(n.diffusion)) fail(`${n.id}: major needs diffusion practice|artifact|capacity`);
    } else {
      if (n.gate != null) fail(`${n.id}: only majors carry a gate`);
      if (n.diffusion != null && n.diffusion !== (n.kind === 'minor' ? 'practice' : 'capacity'))
        fail(`${n.id}: ${n.kind} diffusion is fixed by kind`);
    }
    if (n.excludes != null && n.kind !== 'major') fail(`${n.id}: only majors fork`);
    if (n.requires && n.requires.length && n.kind !== 'milestone') fail(`${n.id}: only milestones carry requires`);
    const effects = n.effects || [];
    for (const e of effects) {
      if (!EFFECT_KINDS.has(e.kind)) fail(`${n.id}: effect kind ${e.kind} not in vocabulary`);
      if (e.kind === 'modifier' && !MOD_TERMS.has(e.target)) fail(`${n.id}: modifier target ${e.target} not a sim term`);
      if (e.kind === 'modifier' && typeof e.per_mille !== 'number') fail(`${n.id}: modifier needs per_mille`);
    }
    if (n.kind === 'minor' && !(effects.length === 1 && effects[0].kind === 'modifier'))
      fail(`${n.id}: a minor carries exactly one modifier`);
    if (n.kind === 'milestone' && !(effects.length === 1 && effects[0].kind === 'open'))
      fail(`${n.id}: a milestone carries exactly one open`);
    if (n.kind === 'major' && effects.length === 0) fail(`${n.id}: a major with no effect feeds nothing`);
    if (spec.invested) {
      if (!n.pursued_when || !n.pursued_when.term || !n.pursued_when.situation) fail(`${n.id}: pursued_when {term, situation} required`);
      else if (store.scorer_terms && !store.scorer_terms.includes(n.pursued_when.term)) fail(`${n.id}: pursued_when.term ${n.pursued_when.term} not in scorer_terms`);
    }
  }

  // --- adjacency ---------------------------------------------------------------------
  const adj = new Map(nodes.map(n => [n.id, new Set()]));
  for (const n of nodes) for (const l of (n.links || [])) {
    if (!byId.has(l)) { fail(`${n.id}: link ${l} does not exist`); continue; }
    if (l === n.id) { fail(`${n.id}: links to itself`); continue; }
    const o = byId.get(l);
    if (Math.abs(o.ring - n.ring) > 1) fail(`R1 ${n.id} -> ${l}: edge spans ${Math.abs(o.ring - n.ring)} rings`);
    adj.get(n.id).add(l); adj.get(l).add(n.id);
  }

  // R3: a minor between two minors is three in a row.
  for (const n of nodes) if (n.kind === 'minor') {
    const minorNb = [...adj.get(n.id)].filter(x => byId.get(x).kind === 'minor');
    if (minorNb.length >= 2) fail(`R3 ${n.id}: minor with minor neighbours ${minorNb.join(', ')} — three in a row`);
  }

  // R5: cross-branch edges pass through a minor; at most one per branch pair per ring.
  const bridges = new Map();
  for (const n of nodes) for (const l of adj.get(n.id)) if (n.id < l) {
    const o = byId.get(l);
    if (n.branch === o.branch || n.branch === 'SP' || o.branch === 'SP') continue;
    if (n.kind !== 'minor' && o.kind !== 'minor') fail(`R5 ${n.id} -> ${l}: cross-branch link with no minor at either end`);
    const ring = Math.min(n.ring, o.ring);
    const key = [n.branch, o.branch].sort().join('/') + '@' + ring;
    bridges.set(key, (bridges.get(key) || 0) + 1);
  }
  for (const [k, c] of bridges) if (c > 1) fail(`R5 ${k}: ${c} cross-branch links for one branch pair at one ring`);

  // Branches leave the spire at majors (spire has no minors, so: a non-spire node linked to the
  // spire must be linked to a major, not a milestone).
  for (const n of nodes) if (n.branch !== 'SP') for (const l of adj.get(n.id)) {
    const o = byId.get(l);
    if (o.branch === 'SP' && o.kind !== 'major') fail(`${n.id}: branch joins the spire at ${l}, which is not a major`);
  }

  // Spire shape: one major and one milestone per ring, chained major -> milestone -> major.
  const spire = nodes.filter(n => n.branch === 'SP');
  const rings = Math.max(...nodes.map(n => n.ring));
  for (let r = 1; r <= rings; r++) {
    const maj = spire.filter(n => n.ring === r && n.kind === 'major');
    const mil = spire.filter(n => n.ring === r && n.kind === 'milestone');
    if (maj.length !== 1) fail(`spire ring ${r}: ${maj.length} majors, expected 1`);
    if (mil.length !== 1) fail(`spire ring ${r}: ${mil.length} milestones, expected 1`);
    if (maj.length === 1 && mil.length === 1 && !adj.get(maj[0].id).has(mil[0].id)) fail(`spire ring ${r}: ${maj[0].id} and ${mil[0].id} not linked`);
    if (r > 1 && mil.length === 1) {
      const below = spire.find(n => n.ring === r - 1 && n.kind === 'milestone');
      const majHere = maj[0];
      if (below && majHere && !adj.get(below.id).has(majHere.id)) fail(`spire: ${below.id} not linked up to ${majHere.id}`);
    }
  }

  // R2 + R4: milestone requires.
  for (const n of nodes) if (n.kind === 'milestone') {
    const req = n.requires || [];
    const seen = new Set();
    for (const r of req) {
      const o = byId.get(r);
      if (!o) { fail(`${n.id}: requires ${r} does not exist`); continue; }
      if (o.kind !== 'major') fail(`R2 ${n.id}: requires ${r}, which is a ${o.kind} not a major`);
      if (o.ring !== n.ring) fail(`R2 ${n.id}: requires ${r} at ring ${o.ring}, not its own ring ${n.ring}`);
      if (o.branch !== 'SP') seen.add(o.branch);
    }
    // requires_any: any `count` of a set of majors at this ring. Breadth it guarantees is the
    // worst case — the fewest distinct branches `count` members can be drawn from.
    if (n.requires_any != null) {
      const ra = n.requires_any;
      if (!ra || !Array.isArray(ra.of) || typeof ra.count !== 'number' || ra.count < 1 || ra.count > ra.of.length)
        fail(`${n.id}: requires_any must be {count, of[]} with 1 <= count <= of.length`);
      else {
        const perBranch = {};
        for (const r of ra.of) {
          const o = byId.get(r);
          if (!o) { fail(`${n.id}: requires_any names ${r}, which does not exist`); continue; }
          if (o.kind !== 'major') fail(`R2 ${n.id}: requires_any names ${r}, a ${o.kind} not a major`);
          if (o.ring !== n.ring) fail(`R2 ${n.id}: requires_any names ${r} at ring ${o.ring}, not its own ring ${n.ring}`);
          if (o.branch !== 'SP') perBranch[o.branch] = (perBranch[o.branch] || 0) + 1;
        }
        const sorted = Object.values(perBranch).sort((a, b) => b - a);
        let left = ra.count, guaranteed = 0;
        for (const c of sorted) { if (left <= 0) break; guaranteed++; left -= c; }
        for (let i = 0; i < guaranteed; i++) seen.add(`any#${i}`);
      }
    }
    if (n.requires_fork != null && Array.isArray(n.requires_fork) && n.requires_fork.length === 2)
      for (const r of n.requires_fork) { const o = byId.get(r); if (o && o.branch !== 'SP') seen.add(o.branch); }
    if (seen.size < 2) fail(`R4 ${n.id}: requires majors from ${seen.size} branch(es), need >= 2`);
    // requires_fork: a fork pair at this ring, either side satisfying.
    if (n.requires_fork != null) {
      const rf = n.requires_fork;
      if (!Array.isArray(rf) || rf.length !== 2) fail(`${n.id}: requires_fork must name exactly the two sides of a fork`);
      else {
        const [a, b] = rf.map(x => byId.get(x));
        if (!a || !b) fail(`${n.id}: requires_fork names a node that does not exist`);
        else {
          if (a.excludes !== b.id || b.excludes !== a.id) fail(`${n.id}: requires_fork ${a.id}/${b.id} is not a fork pair`);
          if (a.ring !== n.ring || b.ring !== n.ring) fail(`R2 ${n.id}: requires_fork pair is not at its own ring`);
        }
      }
    }
  }

  // Forks.
  for (const n of nodes) if (n.excludes != null) {
    const o = byId.get(n.excludes);
    if (!o) { fail(`${n.id}: excludes ${n.excludes} does not exist`); continue; }
    if (o.excludes !== n.id) fail(`${n.id}: fork not symmetric with ${o.id}`);
    if (o.kind !== 'major') fail(`${n.id}: fork partner ${o.id} is not a major`);
    if (o.ring !== n.ring) fail(`${n.id}: fork partner ${o.id} at a different ring`);
    const shared = [...adj.get(n.id)].filter(x => adj.get(o.id).has(x) && byId.get(x).kind === 'minor');
    if (shared.length === 0) fail(`${n.id} / ${o.id}: fork does not share a linked minor`);
  }

  // Reachability from the root (spire ring-1 major).
  const rootNode = spire.find(n => n.ring === 1 && n.kind === 'major');
  if (rootNode) {
    const seen = new Set([rootNode.id]);
    const q = [rootNode.id];
    while (q.length) for (const x of adj.get(q.shift())) if (!seen.has(x)) { seen.add(x); q.push(x); }
    for (const n of nodes) if (!seen.has(n.id)) fail(`${n.id}: unreachable from the root ${rootNode.id}`);
  }

  // Doc cross-check.
  if (!fs.existsSync(docPath)) fail(`doc missing: ${spec.doc}`);
  else {
    const doc = fs.readFileSync(docPath, 'utf8');
    const docIds = new Set(doc.match(new RegExp(`\\b${spec.prefix}-[A-Z]{2}-[1-9](?:[a-z]|m)\\b`, 'g')) || []);
    for (const n of nodes) if (!docIds.has(n.id)) fail(`${n.id}: in the store, not in ${spec.doc}`);
    for (const id of docIds) if (!byId.has(id)) fail(`${id}: in ${spec.doc}, not in the store`);
  }

  const counts = { minor: 0, major: 0, milestone: 0 };
  for (const n of nodes) if (counts[n.kind] != null) counts[n.kind]++;
  const forks = nodes.filter(n => n.excludes != null).length / 2;
  console.log(`${failures ? 'FAIL' : 'OK  '} ${name}: ${nodes.length}/${CAP} nodes — ${counts.major} major, ${counts.minor} minor, ${counts.milestone} milestone, ${forks} fork(s), ${rings} rings, ${branches.size - 1} branches + spire${failures ? `, ${failures} failure(s)` : ''}`);
  return failures;
}

const which = process.argv[2] || 'all';
const names = which === 'all' ? Object.keys(TREES) : [which];
let total = 0;
for (const n of names) {
  if (!TREES[n]) { console.error(`unknown tree ${n}`); process.exit(2); }
  total += lintTree(n, TREES[n]);
}
process.exit(total ? 1 : 0);
