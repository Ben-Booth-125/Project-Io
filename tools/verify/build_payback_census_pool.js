// Pools the shards par_seeds wrote for build_payback_census (BL-1496), or the files named.
// Usage: node tools/verify/build_payback_census_pool.js [file ...]
const fs = require('fs');
const path = require('path');
let files = process.argv.slice(2);
if (!files.length) {
  const dir = 'build_gen/verify/out/par/build_payback_census';
  files = fs.existsSync(dir) ? fs.readdirSync(dir).filter(f => f.includes('shard')).map(f => path.join(dir, f)) : [];
}
const parse = (line) => Object.fromEntries(line.trim().split(/\s+/).slice(1).map(kv => {
  const i = kv.indexOf('=');
  const k = kv.slice(0, i), v = kv.slice(i + 1), n = Number(v);
  return [k, Number.isNaN(n) ? v : n];
}));
const site = [], qtr = [], laps = [], actual = [];
for (const f of files)
  for (const l of fs.readFileSync(f, 'utf8').split(/\r?\n/)) {
    if (l.startsWith('SITE ')) site.push(parse(l));
    else if (l.startsWith('QTR ')) qtr.push(parse(l));
    else if (l.startsWith('LAPS ')) laps.push(parse(l));
    else if (l.startsWith('ACTUAL ')) actual.push(parse(l));
  }
const med = (a) => { if (!a.length) return NaN; const s = [...a].sort((x, y) => x - y); return s[Math.floor(s.length / 2)]; };
const pct = (a, p) => { if (!a.length) return NaN; const s = [...a].sort((x, y) => x - y); return s[Math.min(s.length - 1, Math.floor(s.length * p))]; };
const f1 = (x) => Number.isFinite(x) ? x.toFixed(1) : '-';
const f0 = (x) => Number.isFinite(x) ? x.toFixed(0) : '-';

console.log(`files ${files.length}, SITE rows ${site.length}, QTR rows ${qtr.length}`);

// Spread and payback, one line per kind.
for (const kind of ['extract', 'process']) {
  const r = site.filter(x => x.kind === kind);
  if (!r.length) continue;
  const earners = r.filter(x => x.best > 0);
  const both = r.filter(x => x.best > 0 && x.med > 0);
  console.log(`\n${kind}: ${r.length} major-rows, ${earners.length} with any paying site, ${r.filter(x => x.med > 0).length} whose median site pays`);
  console.log(`  capex at the best site, median ${f0(med(r.map(x => x.capex_best)))}`);
  console.log(`  best-site payback years: p10 ${f1(pct(earners.map(x => x.payback_best), .1))}  median ${f1(med(earners.map(x => x.payback_best)))}  p90 ${f1(pct(earners.map(x => x.payback_best), .9))}`);
  console.log(`  median-site payback years, where it pays: median ${f1(med(both.map(x => x.payback_med)))}`);
  console.log(`  best / median net where both pay: median ${f1(med(both.map(x => x.best / x.med)))}  p90 ${f1(pct(both.map(x => x.best / x.med), .9))}`);
  console.log(`  share of a row's sites that pay: median ${f0(100 * med(r.map(x => x.positive / x.sites)))}%`);
}

// Per good or recipe.
const byName = new Map();
for (const r of site) { const k = r.kind + ' ' + r.name; if (!byName.has(k)) byName.set(k, []); byName.get(k).push(r); }
const rows = [...byName.entries()].map(([k, v]) => ({
  k, majors: v.length, sites: med(v.map(x => x.sites)), best: med(v.map(x => x.best)), medn: med(v.map(x => x.med)),
  capex: med(v.map(x => x.capex_best)), pay: med(v.filter(x => x.best > 0).map(x => x.payback_best)), pays: v.filter(x => x.best > 0).length,
})).sort((a, b) => (a.pay || 1e9) - (b.pay || 1e9));
console.log('\nrow | majors | sites | capex | best net/q | median net/q | best payback y | majors paying');
for (const r of rows) console.log(`${r.k.padEnd(38)} ${String(r.majors).padStart(3)} ${f0(r.sites).padStart(6)} ${f0(r.capex).padStart(6)} ${f1(r.best).padStart(8)} ${f1(r.medn).padStart(8)} ${f1(r.pay).padStart(7)} ${String(r.pays).padStart(4)}`);

// The curve, by year, across majors.
if (qtr.length) {
  const years = new Map();
  for (const q of qtr) { const y = Math.ceil(q.q / 4); if (!years.has(y)) years.set(y, []); years.get(y).push(q); }
  console.log('\nyear | treasury | net/q | delta/q | resid/q | income | expenditure | maint | wages | upkeep | taxes | built | constr | idled | men');
  for (const [y, v] of [...years.entries()].sort((a, b) => a[0] - b[0])) {
    const m = (k) => med(v.map(x => x[k]));
    console.log(`${String(y).padStart(3)} ${f0(m('treasury')).padStart(9)} ${f0(m('net')).padStart(7)} ${f0(m('delta')).padStart(7)} ${f0(m('resid')).padStart(7)} ${f0(m('income')).padStart(7)} ${f0(m('expenditure')).padStart(7)} ${f0(m('maintenance')).padStart(6)} ${f0(m('wages')).padStart(6)} ${f0(m('upkeep')).padStart(6)} ${f0(m('taxes')).padStart(6)} ${f0(m('built')).padStart(4)} ${f0(m('constructing')).padStart(3)} ${f0(m('idled')).padStart(3)} ${f0(m('men')).padStart(6)}`);
  }
}

// Where a tick's money moves: mean per major-quarter, by lap, and what the report names.
if (laps.length) {
  const mean = (k) => laps.reduce((s, x) => s + (x[k] || 0), 0) / laps.length;
  console.log(`\nmean treasury move per major-quarter: convoys ${f0(mean('l0'))}  economy+trade ${f0(mean('l1'))}  clear ${f0(mean('l2'))}  budget+nation step ${f0(mean('l3'))}  l4 ${f0(mean('l4'))}  l5 ${f0(mean('l5'))}`);
  console.log(`  named by the report: research ${f0(mean('research'))}  national budget ${f0(mean('budget'))}`);
}

// What a standing plant truly earned, beside the door's quote for it.
if (actual.length) {
  const by = new Map();
  for (const a of actual) { const k = a.q + ' ' + a.name; if (!by.has(k)) by.set(k, []); by.get(k).push(a); }
  console.log('\nq recipe | plants | idled | run | actual net/q | quoted net/q');
  for (const [k, v] of [...by.entries()].sort()) console.log(`${k.padEnd(36)} ${String(v.length).padStart(4)} ${String(v.filter(x => x.idled).length).padStart(4)} ${med(v.map(x => x.run)).toFixed(2).padStart(6)} ${f1(med(v.map(x => x.actual))).padStart(9)} ${f1(med(v.map(x => x.quoted))).padStart(9)}`);
}
