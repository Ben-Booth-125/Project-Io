#!/usr/bin/env node
// ladder_lint.js — consistency check for docs/research/ancient_tech_ladder.json (BL-307).
//
// Checks: JSON parses; ids unique and well-formed; band/domain fields agree with the id;
// prereqs resolve and never point at a later band; gate atoms and diffusion classes stay
// inside the declared vocabularies; quest/keystone references resolve; and a two-way
// cross-check that every node id in ANCIENT_TECH_LADDER.md exists here and vice versa.
// Exit 0 clean, 1 on any failure. Run: node tools/session/ladder_lint.js

const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..', '..');
const storePath = path.join(root, 'docs', 'research', 'ancient_tech_ladder.json');
const docPath = path.join(root, 'docs', 'research', 'ANCIENT_TECH_LADDER.md');

let failures = 0;
const fail = msg => { failures++; console.error('  FAIL  ' + msg); };

const store = JSON.parse(fs.readFileSync(storePath, 'utf8'));
const bands = store.bands.map(b => b.key);
const domains = store.domains.map(d => d.key);
const gateAtoms = new Set(store.gate_atoms);
const diffClasses = new Set(store.diffusion_classes);

const idPat = /^T[1-6]-(AG|MA|EN|TR|IN|ME)-\d{2}$/;
const regimePat = /^T[1-6]-MIL$/;

const byId = new Map();
for (const n of store.nodes) {
  if (byId.has(n.id)) fail(`duplicate node id ${n.id}`);
  byId.set(n.id, n);
}

for (const n of store.nodes) {
  const isRegime = n.kind === 'regime';
  if (!(isRegime ? regimePat : idPat).test(n.id)) fail(`${n.id}: id shape wrong for kind=${n.kind}`);
  if (n.id.slice(0, 2) !== n.band) fail(`${n.id}: band field ${n.band} disagrees with id`);
  const idDomain = isRegime ? 'MIL' : n.id.split('-')[1];
  if (idDomain !== n.domain) fail(`${n.id}: domain field ${n.domain} disagrees with id`);
  if (!bands.includes(n.band)) fail(`${n.id}: unknown band ${n.band}`);
  if (!domains.includes(n.domain)) fail(`${n.id}: unknown domain ${n.domain}`);
  for (const p of n.prereqs) {
    if (!byId.has(p)) fail(`${n.id}: prereq ${p} does not resolve`);
    else if (byId.get(p).band > n.band) fail(`${n.id}: prereq ${p} sits in a later band`);
  }
  if (n.gate !== null) for (const g of n.gate) if (!gateAtoms.has(g)) fail(`${n.id}: gate atom '${g}' not in vocabulary`);
  if (isRegime) {
    if (n.diffusion !== null) fail(`${n.id}: regime must have diffusion null`);
    if (n.roster_ref !== 'BL-274') fail(`${n.id}: regime missing roster_ref BL-274`);
  } else {
    if (!Array.isArray(n.diffusion) || n.diffusion.length === 0) fail(`${n.id}: tech needs a diffusion array`);
    else for (const d of n.diffusion) if (!diffClasses.has(d)) fail(`${n.id}: diffusion class '${d}' not in vocabulary`);
  }
}

// Effect typing (2026-08-05 effects pass) — kind/status vocabulary, and the rings T4–T5
// completeness rule: every object in the worked industrial region carries effects.
const effectKinds = new Set(store.effect_kinds || []);
const effectStatuses = new Set(store.effect_statuses || []);
const checkEffects = (label, fx) => {
  if (!Array.isArray(fx)) return fail(`${label}: effects must be an array`);
  if (!fx.length) return fail(`${label}: effects array is empty`);
  for (const e of fx) {
    if (!effectKinds.has(e.kind)) fail(`${label}: effect kind '${e.kind}' not in vocabulary`);
    if (!effectStatuses.has(e.status)) fail(`${label}: effect status '${e.status}' not in vocabulary`);
    if (!e.target) fail(`${label}: effect ${e.kind} has no target`);
  }
};
const TYPED_BANDS = ['T4', 'T5'];
for (const n of store.nodes) {
  if (n.effects) checkEffects(n.id, n.effects);
  else if (TYPED_BANDS.includes(n.band)) fail(`${n.id}: sits in the typed region but carries no effects`);
}
for (const k of store.keystones) for (const b of k.branches || []) {
  if (b.effects) checkEffects(`${k.id}/${b.key}`, b.effects);
  else if (TYPED_BANDS.includes(k.band)) fail(`${k.id}/${b.key}: typed region branch carries no effects`);
}
for (const q of store.quests) {
  if (q.effects) fail(`${q.id}: a quest's effect is always open -> opens; drop the effects array`);
  if (!q.opens) fail(`${q.id}: quest has no opens target (its implicit open effect)`);
}

const keystoneIds = new Set(store.keystones.map(k => k.id));
for (const q of store.quests) {
  if (!domains.includes(q.domain)) fail(`${q.id}: unknown domain ${q.domain}`);
  for (const c of q.conditions) {
    if (c.type === 'research' && !byId.has(c.node)) fail(`${q.id}: research condition node ${c.node} does not resolve`);
    if (c.type === 'keystone' && !keystoneIds.has(c.keystone)) fail(`${q.id}: keystone ${c.keystone} does not resolve`);
  }
  if (!bands.includes(q.opens.band) || !domains.includes(q.opens.domain)) fail(`${q.id}: opens target invalid`);
}
for (const k of store.keystones) {
  if (!domains.includes(k.domain)) fail(`${k.id}: unknown domain ${k.domain}`);
  if (!Array.isArray(k.branches) || k.branches.length !== 2) fail(`${k.id}: a keystone is a binary fork`);
}

// Two-way cross-check against the doc's tables and prose.
const doc = fs.readFileSync(docPath, 'utf8');
const docIds = new Set(doc.match(/\bT[1-6]-(?:AG|MA|EN|TR|IN|ME)-\d{2}\b|\bT[1-6]-MIL\b/g) || []);
for (const id of docIds) if (!byId.has(id)) fail(`doc names ${id} but the store lacks it`);
for (const id of byId.keys()) if (!docIds.has(id)) fail(`store has ${id} but the doc never names it`);

// Summary counts.
const techs = store.nodes.filter(n => n.kind === 'tech').length;
const regimes = store.nodes.filter(n => n.kind === 'regime').length;
const perBand = {};
for (const n of store.nodes) perBand[n.band] = (perBand[n.band] || 0) + 1;
console.log(`ladder_lint: ${techs} techs + ${regimes} regimes` +
  ` (${Object.entries(perBand).sort().map(([b, c]) => `${b}:${c}`).join(' ')})` +
  `, ${store.quests.length} quests, ${store.keystones.length} keystones, ${store.crossings.length} crossings`);

// Worked regions — one line each, counted from the store so the doc's prose can be checked
// against it. Add a row here when a pass works a new region of the web.
const regions = [
  { label: 'ring-1-to-2 neighbourhood', bands: ['T1', 'T2'], boundary: 'T1/T2' },
  { label: 'industrial neighbourhood ', bands: ['T4', 'T5'], boundary: 'T4/T5' },
];
for (const r of regions) {
  const t = store.nodes.filter(n => n.kind === 'tech' && r.bands.includes(n.band)).length;
  const g = store.nodes.filter(n => n.kind === 'regime' && r.bands.includes(n.band)).length;
  const q = store.quests.filter(x => x.boundary === r.boundary).length;
  const k = store.keystones.filter(x => r.bands.includes(x.band)).length;
  console.log(`  ${r.label}: ${t} techs + ${q} quests + ${k} keystones + ${g} regimes = ${t + q + k + g} objects`);
}
const fxAll = [
  ...store.nodes.flatMap(n => n.effects || []),
  ...store.keystones.flatMap(k => (k.branches || []).flatMap(b => b.effects || [])),
];
if (fxAll.length) {
  const byKind = {}, byStatus = {};
  for (const e of fxAll) { byKind[e.kind] = (byKind[e.kind] || 0) + 1; byStatus[e.status] = (byStatus[e.status] || 0) + 1; }
  console.log(`  effects: ${fxAll.length} typed — ` +
    Object.entries(byKind).sort((a, b) => b[1] - a[1]).map(([k, c]) => `${k}:${c}`).join(' ') +
    ` | ${Object.entries(byStatus).sort().map(([s, c]) => `${s}:${c}`).join(' ')}`);
}

if (failures) { console.error(`ladder_lint: ${failures} failure(s)`); process.exit(1); }
console.log('ladder_lint: clean');
