// opening_parity_pool.js — pool opening_parity's ROW / SEED lines (BL-1475).
// Reads every shard par_seeds wrote for opening_parity (or the files named) and
// prints, per nation-count pair: the spread of majors' handoff treasury,
// buildings and men within a seed (median and worst max/min over seeds), how
// many majors run their play quarters at a loss on average, and how many seeds
// have majors sharing a border.
//
// Usage: node tools/verify/opening_parity_pool.js [file ...]
//   default: build_gen/verify/out/par/opening_parity/*.txt|*.log|*.out
const fs = require('fs');
const path = require('path');

let files = process.argv.slice(2);
if (!files.length) {
  const dir = 'build_gen/verify/out/par/opening_parity';
  files = fs.existsSync(dir) ? fs.readdirSync(dir).map(f => path.join(dir, f)) : [];
}
const parse = line => Object.fromEntries(line.split(' ').slice(1).map(kv => kv.split('=')));
const rows = [], seeds = [];
for (const f of files) {
  for (const line of fs.readFileSync(f, 'utf8').split(/\r?\n/)) {
    if (line.startsWith('ROW ')) rows.push(parse(line));
    else if (line.startsWith('SEED ')) seeds.push(parse(line));
  }
}
const med = a => { const s = [...a].sort((x, y) => x - y); return s.length ? s[Math.floor(s.length / 2)] : 0; };
const fmt = (x, d = 0) => Number(x).toFixed(d);

for (const pair of [...new Set(seeds.map(s => s.pair))].sort()) {
  const S = seeds.filter(s => s.pair === pair);
  const R = rows.filter(r => r.pair === pair);
  const num = (A, k) => A.map(a => Number(a[k]));
  console.log(`PAIR ${pair}: ${S.length} seed(s), ${R.length} major(s)`);
  for (const k of ['treasury_ratio', 'built_ratio', 'men_ratio']) {
    const v = num(S, k);
    console.log(`  within-seed ${k.replace('_ratio', '')} max/min: median ${fmt(med(v), 2)}  worst ${fmt(Math.max(...v), 2)}`);
  }
  for (const k of ['treasury', 'built', 'men', 'net_mean', 'end_treasury']) {
    const v = num(R, k);
    console.log(`  majors ${k}: min ${fmt(Math.min(...v))}  median ${fmt(med(v))}  max ${fmt(Math.max(...v))}`);
  }
  const losing = R.filter(r => Number(r.net_mean) < 0).length;
  const broke = R.filter(r => Number(r.end_treasury) < 100).length;
  console.log(`  majors losing money on average: ${losing}/${R.length}  ending under 100 credits: ${broke}/${R.length}`);
  const touch = num(S, 'touching');
  console.log(`  seeds with majors sharing a border: ${touch.filter(t => t > 0).length}/${S.length}  (pairs: median ${med(touch)}, max ${Math.max(...touch)})`);
  console.log(`  minors: median treasury ${fmt(med(num(S, 'minor_treasury_med')))}  built ${med(num(S, 'minor_built_med'))}  men ${med(num(S, 'minor_men_med'))}`);
}
