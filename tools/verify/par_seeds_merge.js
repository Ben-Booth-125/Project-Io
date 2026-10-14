#!/usr/bin/env node
// par_seeds_merge.js — merge the shards par_seeds.sh ran into one verdict.
//
//   node tools/verify/par_seeds_merge.js <harness> <dir> <jobs> <seconds> <shard seeds...>
//
// Prints each shard's verdict lines, then a MERGED block. A harness's own
// pooled figures are RECOMPUTED from the absolute numerators each shard prints,
// never averaged across shards (an average of ratios is not the pooled ratio):
//   market_viability  — G1 run/built, G1 t50, G1b starved/built, G2 income
//                       ratio, G3 alive/handoff, against the harness's targets.
//   nation_targets_harness — summed checks and failures.
//   history_patterns  — summed hits per pattern.
// Every harness: exit 0 only if every shard exited 0.
'use strict';
const fs = require('fs');
const path = require('path');

const [harness, dir, jobsArg, secsArg, ...shards] = process.argv.slice(2);
const jobs = Number(jobsArg);
let failed = 0;
const texts = [];

for (let k = 0; k < jobs; ++k) {
  const txt = fs.existsSync(path.join(dir, `${harness}.shard${k}.txt`))
    ? fs.readFileSync(path.join(dir, `${harness}.shard${k}.txt`), 'utf8') : '';
  const code = fs.existsSync(path.join(dir, `${harness}.shard${k}.exit`))
    ? fs.readFileSync(path.join(dir, `${harness}.shard${k}.exit`), 'utf8').trim() : 'missing';
  if (code !== '0') ++failed;
  texts.push(txt);
  console.log(`--- shard ${k} [seeds ${shards[k]}] exit ${code}`);
  const verdict = txt.split(/\r?\n/).filter(l =>
    /^\s*(PASS|FAIL)\b/.test(l) || /ALL PASS|FAILURES|^SUMMARY|^market_viability:|^history_patterns over|^\s{2}[a-z][a-z ]+\s+\d+\/\d+\s+\(/.test(l));
  for (const l of verdict.slice(-12)) console.log('  ' + l.trim());
}

console.log(`\n=== MERGED ${harness}: ${jobs} shard(s), ${shards.join(' | ')}, ${secsArg} s wall ===`);

const num = (re, t) => { const m = t.match(re); return m ? m.slice(1).map(Number) : null; };

if (harness === 'market_viability') {
  let run = 0, built = 0, r50 = 0, b50 = 0, starved = 0, sb = 0, inc = 0, settle = 0, alive = 0, hand = 0, ok = true;
  for (const t of texts) {
    const a = num(/abs run\/built h (\d+)\/(\d+) t50 (\d+)\/(\d+)/, t);
    const b = num(/G1b processors input-starved at handoff\s+[\d.]+%\s+\((\d+) \/ (\d+)/, t);
    const g2 = num(/G2 field income play 26-50 mean \/ settle mean\s+[\d.]+%\s+\((\d+) \/ (\d+)/, t);
    const g3 = num(/G3 firms alive t400 \/ handoff\s+[\d.]+%\s+\((\d+) \/ (\d+)/, t);
    if (!a || !b || !g2 || !g3) { ok = false; continue; }
    run += a[0]; built += a[1]; r50 += a[2]; b50 += a[3];
    starved += b[0]; sb += b[1]; inc += g2[0]; settle += g2[1]; alive += g3[0]; hand += g3[1];
  }
  if (!ok) { console.log('  could not read every shard\'s absolute figures — see the shard files'); ++failed; }
  else {
    const pct = (n, d) => d > 0 ? 100 * n / d : 0;
    const rows = [
      ['G1  processors running at handoff', pct(run, built), '>=', 85, `${run}/${built}`],
      ['G1  processors running at tick 50', pct(r50, b50), '>=', 85, `${r50}/${b50}`],
      ['G1b processors input-starved      ', pct(starved, sb), '<=', 5, `${starved}/${sb}`],
      ['G2  field income ratio            ', pct(inc, settle), '>=', 50, `${inc}/${settle}`],
      ['G3  firms alive t400 / handoff    ', pct(alive, hand), '>=', 70, `${alive}/${hand}`],
    ];
    for (const [name, v, op, tgt, abs] of rows) {
      const pass = op === '>=' ? v >= tgt : v <= tgt;
      console.log(`  ${pass ? 'PASS' : 'FAIL'}  ${name} ${v.toFixed(1)}%  (${abs}; target ${op} ${tgt})`);
    }
    console.log(`  market_viability (pooled): G1 ${rows[0][1].toFixed(1)}/85 G1t50 ${rows[1][1].toFixed(1)}/85 ` +
                `G1b ${rows[2][1].toFixed(1)}/5 G2 ${rows[3][1].toFixed(1)}/50 G3 ${rows[4][1].toFixed(1)}/70`);
  }
} else if (harness === 'nation_targets_harness') {
  let checks = 0, fails = 0;
  for (const t of texts) {
    const m = num(/(\d+) check\(s\), (\d+) failure\(s\)/, t);
    if (m) { checks += m[0]; fails += m[1]; }
  }
  console.log(`  grid: ${checks} check(s), ${fails} failure(s) -- ${fails === 0 ? 'PASS' : 'FAIL'}`);
} else if (harness === 'history_patterns') {
  const sums = {};
  for (const t of texts)
    for (const m of t.matchAll(/^\s{2}([a-z][a-z ]+?)\s+(\d+)\/(\d+)\s+\((.*)\)/gm)) {
      const s = sums[m[1]] || (sums[m[1]] = { hit: 0, n: 0, kind: m[4].split(')')[0] });
      s.hit += Number(m[2]); s.n += Number(m[3]);
    }
  for (const [k, s] of Object.entries(sums)) console.log(`  ${k.padEnd(14)} ${s.hit}/${s.n}   (${s.kind})`);
}

console.log(failed === 0 ? `par_seeds: ALL SHARDS EXIT 0` : `par_seeds: ${failed} shard(s) FAILED or unreadable`);
process.exit(failed === 0 ? 0 : 1);
