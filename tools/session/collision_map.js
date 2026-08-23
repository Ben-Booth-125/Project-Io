#!/usr/bin/env node
// collision_map.js — which open backlog items collide on which files, and how to
// batch them so worktree agents carve along real seams instead of guessed ones.
//
//   node tools/session/collision_map.js                    hotspots across all near-term open work
//   node tools/session/collision_map.js --version v0.1.24  restrict to one minor
//   node tools/session/collision_map.js --items BL-538,BL-539,...   an explicit set
//   node tools/session/collision_map.js --lanes            pack the set into parallel-safe lanes
//   node tools/session/collision_map.js --min 3            hotspot threshold (default 3)
//
// WHY THIS IS A TOOL AND NOT A DOCUMENT. A collision map is a function of
// backlog.json's `files` fields, and those change every session. A map pasted
// into a doc is stale the next time an item is filed, and a stale map is worse
// than none: it sends two agents into the same file believing they are disjoint.
//
// WHAT IT MEASURES, AND WHAT IT DOES NOT. It reads DECLARED scope — the `files`
// list on each item. That is a claim, and claims rot: BL-377 named three files
// that were never written and sat marked complete for ten days. So treat a lane
// packing as a HYPOTHESIS to check against the code, never as proof of
// disjointness. DELIVERY.md is explicit that worktrees are the safety mechanism
// and the collision map is a SPLITTING HEURISTIC, not a gate.
//
// Two collisions it cannot see, both real:
//   * REGION collisions. Two items touching a 1,200-line file at opposite ends
//     do not truly collide; this tool counts them as if they do.
//   * ORDERING collisions. Two items that append to the same serialised enum
//     never share a line and still must land in a fixed order (append-only).
// Both need a human or an agent reading the code. The tool narrows where to look.

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const backlog = JSON.parse(fs.readFileSync(path.join(ROOT, 'docs/development/backlog.json'), 'utf8'));

const argv = process.argv.slice(2);
const flag = (name, dflt) => {
  const i = argv.indexOf(name);
  return i >= 0 && argv[i + 1] && !argv[i + 1].startsWith('--') ? argv[i + 1] : dflt;
};
const has = (name) => argv.includes(name);

const MIN = parseInt(flag('--min', '3'), 10);
const VERSION = flag('--version', null);
const ONLY = flag('--items', null);

// Strip a trailing annotation — "src/world/x.cpp (new)" — then expand a brace
// glob: "src/world/x.{hpp,cpp}" is TWO files, and counting it as one is how a
// collision hides.
const clean = (f) => String(f).replace(/\s*\([^)]*\)\s*$/, '').trim();
const expand = (f) => {
  const c = clean(f);
  const m = c.match(/^(.*)\{([^}]*)\}(.*)$/);
  return m ? m[2].split(',').map((x) => m[1] + x.trim() + m[3]) : [c];
};
// Only real build inputs. A doc is not a collision worth planning around.
const sourcesOf = (item) => [...new Set(
  (item.files || []).filter((x) => typeof x === 'string').flatMap(expand)
    .filter((f) => /^(src|scripts|tools)\//.test(f))
)];

let items = backlog.items.filter((i) => i.status !== 'complete' && !i.parked);
if (VERSION) items = items.filter((i) => i.version_goal === VERSION);
if (ONLY) {
  const want = new Set(ONLY.split(',').map((s) => s.trim()));
  items = backlog.items.filter((i) => want.has(i.id));
}
const rows = items.map((i) => ({
  id: i.id, sn: i.short_name || '', p: i.priority, v: i.version_goal,
  req: i.requires || [], files: sourcesOf(i),
})).filter((r) => r.files.length);

if (!rows.length) {
  console.log('collision_map: no items in scope carry a source file list.');
  process.exit(0);
}

console.log(`collision_map: ${rows.length} open item(s) in scope${VERSION ? ' at ' + VERSION : ''}\n`);

// ---- hotspots --------------------------------------------------------------
const touch = new Map();
rows.forEach((r) => r.files.forEach((f) => {
  if (!touch.has(f)) touch.set(f, []);
  touch.get(f).push(r.id);
}));
const hot = [...touch.entries()].filter(([, ids]) => ids.length >= MIN)
  .sort((a, b) => b[1].length - a[1].length);

if (hot.length) {
  console.log(`=== HOTSPOTS (${MIN}+ items declare the same file) ===`);
  hot.forEach(([f, ids]) => console.log(String(ids.length).padStart(3) + '  ' + f.padEnd(38) + ids.join(' ')));
  console.log('\n  A hotspot is where worktree isolation earns its cost, and where a REGION');
  console.log('  audit pays: two items at opposite ends of a long file are not really in');
  console.log('  each other\'s way, and this count cannot tell you that.');
} else {
  console.log(`=== no file is declared by ${MIN}+ items in this scope ===`);
}

// ---- lanes -----------------------------------------------------------------
if (has('--lanes')) {
  console.log('\n=== LANES (no declared-file overlap inside a lane; requires land earlier) ===');
  const inScope = new Set(rows.map((r) => r.id));
  const landed = new Set();
  let pool = [...rows].sort((a, b) => String(a.p).localeCompare(String(b.p)) || a.id.localeCompare(b.id));
  let n = 0;
  while (pool.length && n < 12) {
    const lane = [];
    for (const it of pool) {
      // A dependency outside this scope is assumed already landed; one inside must
      // have been placed in an EARLIER lane.
      if (!it.req.every((r) => landed.has(r) || !inScope.has(r))) continue;
      if (lane.some((l) => l.files.some((f) => it.files.includes(f)))) continue;
      lane.push(it);
    }
    if (!lane.length) break;
    n++;
    console.log(`\n  LANE ${n}  (${lane.length} item(s))`);
    lane.forEach((x) => console.log('     ' + x.id.padEnd(8) + String(x.p).padEnd(3) +
      x.sn.slice(0, 32).padEnd(33) + x.files.length + ' file(s)'));
    lane.forEach((l) => landed.add(l.id));
    pool = pool.filter((p) => !lane.includes(p));
  }
  if (pool.length) {
    console.log('\n  UNPLACED (a dependency cycle, or a dep that never lands in scope):');
    pool.forEach((x) => console.log('     ' + x.id.padEnd(8) + x.sn.slice(0, 32).padEnd(33) +
      'needs ' + x.req.join(',')));
  }
  console.log('\n  A lane is a HYPOTHESIS. Check it against the code before briefing agents:');
  console.log('  a declared file list can name a file the item will not touch, or miss one');
  console.log('  it obviously will.');
}

// ---- pairwise --------------------------------------------------------------
if (has('--pairs')) {
  console.log('\n=== PAIRWISE COLLISIONS ===');
  const out = [];
  rows.forEach((a, ai) => rows.slice(ai + 1).forEach((b) => {
    const sh = a.files.filter((f) => b.files.includes(f));
    if (sh.length) out.push([`${a.id} x ${b.id}`, sh]);
  }));
  out.sort((x, y) => y[1].length - x[1].length)
    .forEach(([k, v]) => console.log('  ' + k.padEnd(22) + v.join(', ')));
  if (!out.length) console.log('  none — every item in scope is declared-disjoint.');
}
