#!/usr/bin/env node
// next_id.js — compute the next SAFE backlog id across ALL branches, not just the
// working file, so concurrent worktree sessions don't mint colliding BL-ids off a
// stale local max. Run this BEFORE authoring a new backlog item (DELIVERY.md
// § Parallel worktree coherence). This is the *preventive* half of the collision
// defence; backlog_lint's duplicate-id scan is the *backstop*.
//
// USAGE:   node tools/session/next_id.js [count]
//   count  — how many consecutive ids to reserve for this session (default 1).
//
// OUTPUT:  the next free BL-id (or a reserved range if count > 1), the max seen and
//   where, plus a COLLISION warning if one id already maps to different items on
//   different refs (an in-flight collision the integrator must renumber).
//
// EXIT:    0 always (advisory tool). Zero deps (git + fs only). Runs on the Windows
//   dev box now that Node is installed (memory windows-build-setup).
'use strict';
const { execSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const BACKLOG = 'docs/development/backlog.json';
const sh = (cmd) => execSync(cmd, { cwd: ROOT, encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] }).trim();
const isId = (id) => /^BL-\d+$/.test(id);
const num = (id) => parseInt(id.slice(3), 10);
const pad = (n) => 'BL-' + String(n).padStart(3, '0');

// Every ref whose backlog we should consult: local branches + remote-tracking, minus
// the symbolic origin/HEAD.
function refs() {
  let out = '';
  try { out = sh('git for-each-ref --format=%(refname) refs/heads refs/remotes'); } catch { return []; }
  return out.split('\n').filter(Boolean).filter((r) => !r.endsWith('/HEAD'));
}

function itemsFromText(txt) {
  let j; try { j = JSON.parse(txt); } catch { return []; }
  return (j.items || []).filter((i) => isId(i.id)).map((i) => ({ id: i.id, name: i.short_name || '?' }));
}
function itemsFromRef(ref) {
  try { return itemsFromText(sh(`git show ${ref}:${BACKLOG}`)); } catch { return []; }
}
function itemsFromWorking() {
  try { return itemsFromText(fs.readFileSync(path.join(ROOT, BACKLOG), 'utf8')); } catch { return []; }
}

// id -> Map(short_name -> Set(sources)). Divergent short_names for one id = collision.
const byId = new Map();
const record = (source, items) => {
  for (const { id, name } of items) {
    if (!byId.has(id)) byId.set(id, new Map());
    const names = byId.get(id);
    if (!names.has(name)) names.set(name, new Set());
    names.get(name).add(source);
  }
};

const allRefs = refs();
for (const r of allRefs) record(r, itemsFromRef(r));
record('(working tree)', itemsFromWorking());

let max = 0, maxWhere = [];
for (const [id, names] of byId) {
  const n = num(id);
  if (n > max) { max = n; maxWhere = [...new Set([].concat(...[...names.values()].map((s) => [...s])))]; }
}

const count = Math.max(1, parseInt(process.argv[2] || '1', 10));
const first = max + 1;

console.log(`Scanned ${byId.size} distinct BL-ids across ${allRefs.length} ref(s) + working tree.`);
console.log(`Highest: ${pad(max)}  (on: ${maxWhere.join(', ')})`);
if (count === 1) console.log(`\n>>> Next safe id: ${pad(first)}`);
else console.log(`\n>>> Reserve ${count}: ${pad(first)} .. ${pad(max + count)}`);

// Collision report: an id that maps to >1 distinct short_name across refs is already
// duplicated in-flight — the integrator must keep one and renumber the other(s).
const collisions = [...byId.entries()].filter(([, names]) => names.size > 1);
if (collisions.length) {
  console.log(`\n!!! ${collisions.length} in-flight COLLISION(s) — same id, different items:`);
  for (const [id, names] of collisions) {
    for (const [name, srcs] of names) console.log(`  ${id} = ${name}   [${[...srcs].join(', ')}]`);
  }
  console.log('  Resolve by keeping the earliest/shared item and renumbering the rest (+ their cross-refs).');
}
