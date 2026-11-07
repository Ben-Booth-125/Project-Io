#!/usr/bin/env node
// rival_research_diff.js — compare two rival_research_order outputs.
//   node tools/verify/rival_research_diff.js <before.txt> <after.txt>
// Per seed: how many nations' lists moved, the first divergence (quarter, nation, what each side
// completed there), and the counts of one node across both sides when --node names it.
'use strict';
const fs = require('fs');
const args = process.argv.slice(2);
const nodeAt = args.indexOf('--node');
const watch = nodeAt >= 0 ? args.splice(nodeAt, 2)[1] : null;
const [a, b] = args;
const read = f => {
  const seeds = new Map();
  for (const l of fs.readFileSync(f, 'utf8').split(/\r?\n/)) {
    const m = l.match(/^(\d+) (\d+) (\d+) (\S+)$/);
    if (!m) continue;
    const s = seeds.get(m[1]) || new Map();
    seeds.set(m[1], s);
    const n = s.get(m[3]) || [];
    s.set(m[3], n);
    n.push({ q: Number(m[2]), node: m[4] });
  }
  return seeds;
};
const A = read(a), B = read(b);
for (const seed of new Set([...A.keys(), ...B.keys()])) {
  const sa = A.get(seed) || new Map(), sb = B.get(seed) || new Map();
  const nations = [...new Set([...sa.keys(), ...sb.keys()])].sort((x, y) => Number(x) - Number(y));
  let moved = 0, first = null;
  const count = s => [...s.values()].reduce((t, l) => t + l.length, 0);
  for (const nid of nations) {
    const la = sa.get(nid) || [], lb = sb.get(nid) || [];
    let i = 0;
    while (i < la.length && i < lb.length && la[i].q === lb[i].q && la[i].node === lb[i].node) ++i;
    if (i === la.length && i === lb.length) continue;
    ++moved;
    const q = Math.min(la[i] ? la[i].q : Infinity, lb[i] ? lb[i].q : Infinity);
    if (!first || q < first.q || (q === first.q && Number(nid) < Number(first.nid)))
      first = { q, nid, pick: i + 1, before: la.slice(i, i + 4), after: lb.slice(i, i + 4) };
  }
  const fmt = l => l.map(x => `q${x.q}:${x.node}`).join(' ') || '(none)';
  console.log(`seed ${seed}: ${moved} of ${nations.length} nations' lists moved; completions ${count(sa)} -> ${count(sb)}`);
  if (first) {
    console.log(`  first divergence: quarter ${first.q}, nation ${first.nid}, its completion #${first.pick}`);
    console.log(`    before: ${fmt(first.before)}`);
    console.log(`    after:  ${fmt(first.after)}`);
  }
  if (watch) {
    const holders = s => [...s.values()].filter(l => l.some(x => x.node === watch)).length;
    const earliest = s => Math.min(...[...s.values()].flat().filter(x => x.node === watch).map(x => x.q));
    console.log(`  ${watch}: nations completing ${holders(sa)} -> ${holders(sb)}; earliest quarter ${earliest(sa)} -> ${earliest(sb)}`);
  }
}
