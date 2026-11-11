#!/usr/bin/env node
// nation_tech_pool.js — BL-1561 (tech pacing calibration). Run nation_tech_harness's R0
// one seed per process in parallel, then pool every nation's NATQ line into the
// PACING percentiles R0 prints over a serial run (quarters to DG-PR-EX-A, the first
// ring-2 major; never = quarters + 1). A calibration sweep reads this; the serial
// run is still the gate.
//
//   node tools/verify/nation_tech_pool.js [-j N] [--seeds a,b,...] [--tag T] [-- <harness dial args>]
//   e.g. node tools/verify/nation_tech_pool.js -- --sqrt 8
//
// Seeds default to the 16 curated library seeds. Each shard runs
//   build_gen/verify/nation_tech_harness.exe --r0-only --once --seed-list <s> <args>
// and writes build_gen/verify/out/nation_tech_pool/seed<s>.txt.
'use strict';
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const root = path.resolve(__dirname, '..', '..');
const exe = path.join(root, 'build_gen', 'verify', 'nation_tech_harness.exe');
let out = path.join(root, 'build_gen', 'verify', 'out', 'nation_tech_pool');

let jobs = 8, seeds = null, extra = [];
const argv = process.argv.slice(2);
for (let i = 0; i < argv.length; ++i) {
  if (argv[i] === '--') { extra = argv.slice(i + 1); break; }
  else if (argv[i] === '-j') jobs = Number(argv[++i]);
  else if (argv[i] === '--seeds') seeds = argv[++i].split(',').map(Number);
  else if (argv[i] === '--tag') out = path.join(out, argv[++i]); // concurrent sweeps write apart
  else { console.error(`nation_tech_pool: unknown argument ${argv[i]}`); process.exit(2); }
}
fs.mkdirSync(out, { recursive: true });
if (!seeds) {
  const lib = JSON.parse(fs.readFileSync(path.join(root, 'docs', 'generation', 'seed_library.json'), 'utf8'));
  seeds = lib.seeds.map((s) => s.seed);
}

const pct = (v, q) => {
  if (!v.length) return -1;
  const s = [...v].sort((a, b) => a - b);
  return s[Math.min(s.length - 1, Math.floor(q * s.length))];
};

const t0 = Date.now();
let next = 0, running = 0, failed = 0;
const run = () => new Promise((resolve) => {
  const pump = () => {
    while (running < jobs && next < seeds.length) {
      const s = seeds[next++];
      ++running;
      const file = path.join(out, `seed${s}.txt`);
      const fd = fs.openSync(file, 'w');
      const p = spawn(exe, ['--r0-only', '--once', '--seed-list', String(s), ...extra],
                      { cwd: root, stdio: ['ignore', fd, fd] });
      p.on('close', (code) => {
        fs.closeSync(fd);
        // R0's PACING rows fail on a single seed by design; a build/seat failure prints FAIL on R0a.
        const txt = fs.readFileSync(file, 'utf8');
        if (/FAIL\s+R0a/.test(txt) || !/NATQ/.test(txt)) { ++failed; console.log(`seed ${s}: no rows (exit ${code})`); }
        --running;
        if (next >= seeds.length && running === 0) resolve(); else pump();
      });
    }
  };
  pump();
});

run().then(() => {
  const q = [], held = [];
  let dials = '', quarters = 0;
  for (const s of seeds) {
    const file = path.join(out, `seed${s}.txt`);
    if (!fs.existsSync(file)) continue;
    const txt = fs.readFileSync(file, 'utf8');
    for (const m of txt.matchAll(/NATQ seed \d+ nation \d+ q (\d+) held (\d+)/g)) {
      q.push(Number(m[1])); held.push(Number(m[2]));
    }
    const d = txt.match(/dials: .*/); if (d) dials = d[0];
    const qq = txt.match(/(\d+) live quarters/); if (qq) quarters = Number(qq[1]);
  }
  const reach = q.filter((x) => x <= quarters).length;
  console.log(`nation_tech_pool: ${seeds.length} seeds, ${q.length} nations, ${((Date.now() - t0) / 1000).toFixed(0)} s, args [${extra.join(' ')}]`);
  console.log(`  ${dials}`);
  console.log(`  reach DG-PR-EX-A within ${quarters} quarters: ${reach}/${q.length} (${(100 * reach / Math.max(1, q.length)).toFixed(1)}%)`);
  console.log(`  quarters-to-major: p10 ${pct(q, 0.1)}  p25 ${pct(q, 0.25)}  p50 ${pct(q, 0.5)}  p75 ${pct(q, 0.75)}  p90 ${pct(q, 0.9)}`);
  console.log(`  nodes held at the horizon: p10 ${pct(held, 0.1)}  p50 ${pct(held, 0.5)}  p90 ${pct(held, 0.9)}`);
  if (failed) { console.log(`  ${failed} seed(s) produced no rows`); process.exit(1); }
});
