#!/usr/bin/env node
// seed_library.js — the curated generation seeds, and what each one is for.
//
// WHY THIS EXISTS. Generation is deterministic from a world descriptor, so a
// SEED IS THE SAVE: there is no on-disk snapshot of a generated world short of
// a full save file, and none is needed as long as the seed reproduces it. What
// a seed does not carry is WHY anyone chose it. This store is that: sixteen
// worlds picked off a 48-seed sweep because each one puts a different question
// to the phase that comes next, with the readings that made it interesting and
// a fingerprint so a future session can tell whether the world it generates is
// still the world this was written about.
//
// THE FINGERPRINT IS NOT A GOLDEN. It is five counters from exploration_sweep
// .json, which carries both spans (it reports the Empires battle count beside
// its own). If a world-moving item lands, these move, and that is correct —
// re-run `--check` and, if the movement is authorised, `--bless` writes the new
// values with the date. The library's VALUE is the rationale, which survives.
//
// USAGE:
//   node tools/session/seed_library.js                 the whole library
//   node tools/session/seed_library.js --for trade     seeds tagged 'trade'
//   node tools/session/seed_library.js --seed 6        one seed, in full
//   node tools/session/seed_library.js --check         compare against the
//                                                      sweep artefacts at the
//                                                      repo root
//   node tools/session/seed_library.js --bless         rewrite the fingerprints
//                                                      from those artefacts
//
// The store is docs/generation/seed_library.json. Edit the rationale there by
// hand; let --bless write the numbers.
'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const STORE = path.join(ROOT, 'docs', 'generation', 'seed_library.json');
const EXPL = path.join(ROOT, 'exploration_sweep.json');

function readJson(p) {
  try { return JSON.parse(fs.readFileSync(p, 'utf8')); } catch { return null; }
}
function writeStore(store) {
  const raw = fs.readFileSync(STORE, 'utf8');
  let out = JSON.stringify(store, null, 1) + '\n';
  if (raw.includes('\r\n')) out = out.replace(/\n/g, '\r\n');
  fs.writeFileSync(STORE, out, 'utf8');
}

const store = readJson(STORE);
if (!store) { console.error('seed_library: no store at ' + path.relative(ROOT, STORE)); process.exit(1); }

const argv = process.argv.slice(2);
const flag = (name) => argv.includes(name);
const value = (name) => { const i = argv.indexOf(name); return i >= 0 ? argv[i + 1] : null; };

/// Sweep rows for one seed, or null when the artefact does not carry it.
function rowsFor(seed) {
  const e = readJson(EXPL);
  return { er: e && e.worlds ? e.worlds.find(r => r.seed === seed) : null };
}
function fingerprintOf(seed) {
  // The exploration sweep alone carries every field, INCLUDING the Empires
  // span's battle count, so one artefact answers for both spans and a library
  // seed beyond the checked-in sixteen can still be checked: run
  //   build_gen/verify/exploration_sweep.exe 48
  const { er } = rowsFor(seed);
  if (!er) return null;
  return {
    empire_battles:  er.empire_battles,
    expl_battles:    er.expl_battles,
    flows:           er.flows,
    treasury_median: er.treasury_median,
    living_polities: er.living_polities,
  };
}

if (flag('--check') || flag('--bless')) {
  let moved = 0, missing = 0;
  for (const s of store.seeds) {
    const now = fingerprintOf(s.seed);
    if (!now) { console.log(`  seed ${s.seed}: not in the sweep artefacts (run the sweeps at 48+ seeds)`); missing++; continue; }
    const was = s.fingerprint || {};
    const diff = Object.keys(now).filter(k => was[k] !== now[k]);
    if (!diff.length) { console.log(`  seed ${s.seed}: unchanged`); continue; }
    moved++;
    console.log(`  seed ${s.seed}: MOVED ${diff.map(k => `${k} ${was[k]} -> ${now[k]}`).join(', ')}`);
    if (flag('--bless')) { s.fingerprint = now; s.fingerprint_blessed = new Date().toISOString().slice(0, 10); }
  }
  if (flag('--bless')) { writeStore(store); console.log(`seed_library: blessed ${moved} fingerprint(s).`); }
  else console.log(`seed_library: ${moved} moved, ${missing} absent, ${store.seeds.length - moved - missing} unchanged.`);
  process.exit(0);
}

const one = value('--seed');
const tag = value('--for');
let picked = store.seeds;
if (one !== null) picked = picked.filter(s => String(s.seed) === String(one));
if (tag) picked = picked.filter(s => (s.tags || []).includes(tag));

if (one !== null || picked.length === 1) {
  for (const s of picked) {
    console.log(`seed ${s.seed} — ${s.name}`);
    console.log(`  tags: ${(s.tags || []).join(', ')}`);
    console.log(`  why:  ${s.why}`);
    if (s.fingerprint) console.log('  fingerprint: ' + Object.entries(s.fingerprint).map(([k, v]) => `${k}=${v}`).join(' '));
  }
  process.exit(0);
}

console.log(`seed library — ${store.seeds.length} worlds${tag ? ` tagged '${tag}'` : ''}`);
console.log(`(${store._note[0]})\n`);
const pad = (s, n) => String(s).padEnd(n);
console.log(pad('seed', 6) + pad('name', 32) + 'why');
for (const s of picked) console.log(pad(s.seed, 6) + pad(s.name, 32) + s.why);
