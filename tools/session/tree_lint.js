#!/usr/bin/env node
// tree_lint.js — the five adjacency rules and the store/doc cross-check for the four
// pre-game technology trees (docs/generation/trees/TREES.md § The five rules).
//
// Run: node tools/session/tree_lint.js [colonisation|empire|exploration|industry|all]   (default: all)
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
//   vocabulary; minors carry exactly one modifier; milestones carry exactly one `open`, and a
//   `ring N` open is the milestone's own ring + 1;
//   invested trees carry `pursued_when` on every node; every node id in the doc exists in the
//   store and vice versa;
//   exactly one node per tree derives as ROOT by the generator's own rule (its OWN `links` is
//   empty) and it is the spire's ring-1 major — so each edge is declared once, on the node
//   farther from the root (BL-974);
//   where a generated header exists on disk (src/world/<tree>_tree_data.hpp), the table
//   regenerated in memory from the store matches it byte for byte, line endings normalised —
//   a stale header is a failure naming the regeneration command (BL-974);
//   FORK REACHABILITY (BL-1038, the check BL-930's commit asked for): each side of every fork
//   can still be bought while its partner is refused, under the SIM's availability rules
//   (history_sim.cpp `*_node_available`: excludes, the ring lock read off `open "ring N"`,
//   rule 2's OR travel with the root exempt, a milestone's `requires` AND set and its
//   `requires_fork` either-side), with every endowment gate assumed satisfiable — a gate is
//   a question about the map, a fork side no map can reach is a question about the store.
//   The graph-only reachability above cannot see this: the pre-cut industry store had
//   Charcoal Iron reachable as a graph node, yet only THROUGH Furnace Practice, which was
//   itself reachable only through Coke Smelting — the partner that closes Charcoal.
//
// Run on a store other than the canonical one (a pre-cut store from git, say) with
//   node tools/session/tree_lint.js <tree> --store <path>
// The generated-header comparison is skipped then (the header transcribes the canonical
// store, not the override), and the summary line says so.

const fs = require('fs');
const path = require('path');
const gen = require('./gen_empire_tree_table.js');

const root = path.resolve(__dirname, '..', '..');
const dir = path.join(root, 'docs', 'generation', 'trees');

const TREES = {
  colonisation: { prefix: 'CO', doc: 'COLONISATION_TREE.md', store: 'colonisation_tree.json', invested: false },
  empire:       { prefix: 'EM', doc: 'EMPIRE_TREE.md',       store: 'empire_tree.json',       invested: true },
  exploration:  { prefix: 'EX', doc: 'EXPLORATION_TREE.md',  store: 'exploration_tree.json',  invested: true },
  industry:     { prefix: 'IN', doc: 'INDUSTRY_TREE.md',     store: 'industry_tree.json',     invested: true },
};

const EFFECT_KINDS = new Set(['unlock', 'upgrade', 'retire', 'modifier', 'access', 'reach', 'intel',
  'institution', 'doctrine', 'resource', 'open']);
const GATE_ATOMS = new Set(['ore_q', 'fuel', 'arable', 'coastal', 'grassland']);
const DIFFUSION = new Set(['practice', 'artifact', 'capacity']);
const MOD_TERMS = new Set(['reach', 'carrying_capacity', 'manpower', 'defence', 'industrial', 'stores',
  'cohesion', 'assimilation', 'plague', 'research', 'forage', 'muster_cost']);
// The machine keys a non-modifier effect may carry when the sim reads it by identity
// (TREES.md § Effects; src/world/tree_effect.hpp `tree_effect_key` is the C++ twin, and
// gen_empire_tree_table.js carries the same list — change all three together, BL-973).
const EFFECT_KEYS = new Set(['sea_legs', 'post_roads']);
const KINDS = new Set(['minor', 'major', 'milestone']);
const CAP = 64;

// FORK REACHABILITY (BL-1038). The sim's availability test, transcribed from
// history_sim.cpp's `empire_node_available` / `exploration_node_available` /
// `industry_node_available` (one shape, three trees), over a held SET of ids:
//   - never a held node, never a node whose fork partner is held;
//   - ring r > 1 only once a held node carries `open "ring r"`;
//   - rule 2: the root (own `links` empty, gen.isRootNode) is exempt; any other node with a
//     neighbour needs one neighbour held;
//   - a milestone needs its whole `requires` set and, where it has one, either side of its
//     `requires_fork`. (`requires_any` is honoured too, though no invested store uses it and
//     the generator does not emit it; a store that adopts it gets the stricter reading.)
// Endowment gates are NOT applied: every gate is assumed satisfiable.
function forkReachabilityFailures(nodes, byId, adj) {
  const opensRing = new Map(); // ring -> Set of node ids carrying `open "ring N"`
  for (const n of nodes) for (const e of (n.effects || [])) {
    const m = e.kind === 'open' ? /^ring (\d+)$/.exec(e.target || '') : null;
    if (!m) continue;
    const r = Number(m[1]);
    if (!opensRing.has(r)) opensRing.set(r, new Set());
    opensRing.get(r).add(n.id);
  }
  const available = (held, n) => {
    if (held.has(n.id)) return false;
    if (n.excludes != null && held.has(n.excludes)) return false;
    if (n.ring > 1) {
      const openers = opensRing.get(n.ring);
      if (!openers || ![...openers].some(x => held.has(x))) return false;
    }
    const nb = adj.get(n.id);
    if (!gen.isRootNode(n) && nb.size > 0 && ![...nb].some(x => held.has(x))) return false;
    if (n.kind === 'milestone') {
      for (const r of (n.requires || [])) if (!held.has(r)) return false;
      if (Array.isArray(n.requires_fork) && n.requires_fork.length === 2
          && !n.requires_fork.some(x => held.has(x))) return false;
      if (n.requires_any && Array.isArray(n.requires_any.of)
          && n.requires_any.of.filter(x => held.has(x)).length < n.requires_any.count) return false;
    }
    return true;
  };
  // Monotone closure: buy everything available and not refused, until nothing moves. With one
  // side of EVERY fork refused up front no excludes can bite mid-closure, so the order of
  // purchase cannot change the fixed point.
  const closure = refused => {
    const held = new Set();
    let moved = true;
    while (moved) {
      moved = false;
      for (const n of nodes) {
        if (refused.has(n.id) || !available(held, n)) continue;
        held.add(n.id);
        moved = true;
      }
    }
    return held;
  };
  const pairs = nodes.filter(n => n.excludes != null && byId.has(n.excludes) && n.id < n.excludes)
                     .map(n => [n.id, n.excludes]);
  const out = [];
  for (let p = 0; p < pairs.length; ++p) {
    const others = pairs.filter((_, q) => q !== p);
    for (const [side, partner] of [[pairs[p][0], pairs[p][1]], [pairs[p][1], pairs[p][0]]]) {
      // Reachable if SOME choice of side at every other fork lets it be bought: a side that
      // needs its sibling fork resolved one way is still reachable.
      let reached = false;
      for (let mask = 0; mask < (1 << others.length) && !reached; ++mask) {
        const refused = new Set([partner]);
        others.forEach(([a, b], k) => refused.add((mask >> k) & 1 ? a : b));
        reached = closure(refused).has(side);
      }
      if (!reached) out.push(`fork: ${side} can never be bought while its partner ${partner} is refused — `
        + `under the sim's availability rules (gates assumed open) no path reaches it except through the side it excludes`);
    }
  }
  return out;
}

function lintTree(name, spec, storeOverride) {
  let failures = 0;
  const fail = msg => { failures++; console.error(`  FAIL  [${name}] ${msg}`); };

  const storePath = storeOverride ? path.resolve(storeOverride) : path.join(dir, spec.store);
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
      if (e.key != null) {
        if (e.kind === 'modifier') fail(`${n.id}: a modifier effect carries no key (its target is the term)`);
        if (!EFFECT_KEYS.has(e.key)) fail(`${n.id}: effect key ${e.key} not in vocabulary`);
      }
      if (e.kind === 'open' && !/^ring \d+$/.test(e.target) && !/^[a-z]+ tree$/.test(e.target))
        fail(`${n.id}: open target must be "ring N" or "<tree> tree", got "${e.target}"`);
      // A milestone opens the ring above its own. The format check alone passed IN-SP-1m opening
      // ring 3 and IN-SP-2m opening ring 4, which would have stranded the tree at ring 1.
      const ringOpen = e.kind === 'open' ? /^ring (\d+)$/.exec(e.target) : null;
      if (ringOpen && Number(ringOpen[1]) !== n.ring + 1)
        fail(`${n.id}: a ring-${n.ring} milestone opens ring ${n.ring + 1}, not "${e.target}"`);
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

  // Fork reachability under the sim's rules (BL-1038) — see forkReachabilityFailures.
  for (const msg of forkReachabilityFailures(nodes, byId, adj)) fail(msg);

  // Reachability from the root (spire ring-1 major).
  const rootNode = spire.find(n => n.ring === 1 && n.kind === 'major');
  if (rootNode) {
    const seen = new Set([rootNode.id]);
    const q = [rootNode.id];
    while (q.length) for (const x of adj.get(q.shift())) if (!seen.has(x)) { seen.add(x); q.push(x); }
    for (const n of nodes) if (!seen.has(n.id)) fail(`${n.id}: unreachable from the root ${rootNode.id}`);
  }

  // The root by DERIVATION, not by reachability. The generated table (and the sim's rule-2
  // skip) call a node the root iff its OWN declared `links` is empty — gen.isRootNode is that
  // one rule. So the store must declare each edge once, on the node farther from the root:
  // every other node then declares at least the edge toward the root, and only the root is
  // empty. A store that declares edges outward from the lower id instead derives every leaf
  // as a root and the true root as none — an unlockable tree in C++ (BL-930's defect, latent
  // in the other stores until BL-974).
  const derivedRoots = nodes.filter(n => gen.isRootNode(n)).map(n => n.id);
  if (derivedRoots.length !== 1)
    fail(`root: ${derivedRoots.length} node(s) derive as root by empty links (${derivedRoots.join(', ') || 'none'}); exactly one expected — declare each edge once, on the node farther from the root`);
  else if (rootNode && derivedRoots[0] !== rootNode.id)
    fail(`root: ${derivedRoots[0]} derives as root by empty links, but the spire's ring-1 major is ${rootNode.id}`);

  // The generated header, where one exists on disk: regenerate the table in memory through
  // the generator's own function and compare. Nothing else in the loop (edit JSON → lint →
  // regenerate → rebuild) checks that the regeneration happened; a stale header silently runs
  // the old tree. Line endings are normalised because git may check the header out as CRLF
  // while the generator writes LF.
  let headerChecked = false;
  const headerPath = gen.headerPath(name);
  if (!storeOverride && fs.existsSync(headerPath)) {
    headerChecked = true;
    const norm = s => s.replace(/\r\n/g, '\n');
    try {
      const expected = norm(gen.generate(name).text);
      const actual = norm(fs.readFileSync(headerPath, 'utf8'));
      if (expected !== actual) {
        const e = expected.split('\n'), a = actual.split('\n');
        let i = 0; while (i < e.length && i < a.length && e[i] === a[i]) i++;
        const show = x => x === undefined ? '<eof>' : JSON.stringify(x);
        fail(`header stale: src/world/${path.basename(headerPath)} differs from the store at line ${i + 1} (header: ${show(a[i])}; store: ${show(e[i])}) — regenerate: node tools/session/gen_empire_tree_table.js ${name}`);
      }
    } catch (e) {
      fail(`header: could not regenerate the table from the store: ${e.message}`);
    }
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
  console.log(`${failures ? 'FAIL' : 'OK  '} ${name}: ${nodes.length}/${CAP} nodes — ${counts.major} major, ${counts.minor} minor, ${counts.milestone} milestone, ${forks} fork(s), ${rings} rings, ${branches.size - 1} branches + spire${headerChecked ? ', header checked' : ''}${storeOverride ? `, store ${storeOverride} (header not compared)` : ''}${failures ? `, ${failures} failure(s)` : ''}`);
  return failures;
}

const args = process.argv.slice(2);
let storeOverride = null;
const storeAt = args.indexOf('--store');
if (storeAt >= 0) {
  storeOverride = args[storeAt + 1];
  if (!storeOverride) { console.error('--store needs a path'); process.exit(2); }
  args.splice(storeAt, 2);
}
const which = args[0] || 'all';
if (storeOverride && which === 'all') { console.error('--store names one tree\'s store; pass the tree too'); process.exit(2); }
const names = which === 'all' ? Object.keys(TREES) : [which];
let total = 0;
for (const n of names) {
  if (!TREES[n]) { console.error(`unknown tree ${n}`); process.exit(2); }
  total += lintTree(n, TREES[n], storeOverride);
}
process.exit(total ? 1 : 0);
