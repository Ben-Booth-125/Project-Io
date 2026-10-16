#!/usr/bin/env node
// play_vs_ai.js — compare how the player played a seated session with how the
// AI majors played the same world, from one play log (src/core/play_log.hpp).
//
// USAGE:   node tools/session/play_vs_ai.js <play_logs/....jsonl> [--json]
//
// The log has three record kinds: one "header", a "cmd" per command the human
// (or an attached agent) issued, and a "quarter" per econ tick plus an opening
// snapshot. This reads them and reports, player against the median AI major:
//   1. commands by verb, and over time (per in-game year);
//   2. the budget-weight profile (mean weight per line over the session);
//   3. research order: the first 10 majors and their dates;
//   4. treasury / champion income / tech at each in-game year;
//   5. what the player did that no AI did — verbs, budget lines, majors.
//
// CAVEATS it prints rather than hides: the player's budget weights are written
// by the budget scorer's stand-in until budget verbs exist (the quarter row's
// "budget_by"), so section 2 compares two scorers until then; and the AI's
// verbs are its CORPORATIONS' scored decisions plus its nations' research
// picks — a nation-level verb the scorer never emits (lay_track, say) reads as
// "no AI did it" because none did.
//
// Zero deps. Exit 0 on a readable log, 1 on a missing/unreadable one.
'use strict';
const fs = require('fs');

const args = process.argv.slice(2);
const asJson = args.includes('--json');
const file = args.find(a => !a.startsWith('--'));
if (!file) {
  console.error('usage: node tools/session/play_vs_ai.js <play_log.jsonl> [--json]');
  process.exit(1);
}
let text;
try { text = fs.readFileSync(file, 'utf8'); } catch (e) {
  console.error(`play_vs_ai: cannot read ${file}: ${e.message}`);
  process.exit(1);
}

let header = null;
const cmds = [];
const quarters = [];
let bad = 0;
for (const line of text.split(/\r?\n/)) {
  if (!line.trim()) continue;
  let r;
  try { r = JSON.parse(line); } catch { bad++; continue; } // a crash can cut the last line
  if (r.kind === 'header') header = r;
  else if (r.kind === 'cmd') cmds.push(r);
  else if (r.kind === 'quarter') quarters.push(r);
}
if (!header) { console.error(`play_vs_ai: ${file} has no header line`); process.exit(1); }

const playerId = header.seat.nation;
const nameOf = new Map(header.nations.map(n => [n.id, n.name]));
const aiMajors = header.nations.filter(n => n.major && n.id !== playerId).map(n => n.id);

const median = xs => {
  const v = xs.filter(x => typeof x === 'number' && isFinite(x)).sort((a, b) => a - b);
  if (!v.length) return null;
  const m = v.length >> 1;
  return v.length % 2 ? v[m] : (v[m - 1] + v[m]) / 2;
};
const rowOf = (q, id) => q.nations.find(n => n.id === id);
const played = quarters.filter(q => !q.opening);

// --- 1. commands -----------------------------------------------------------
const byVerb = {};
const byYear = {};
for (const c of cmds) {
  const k = c.verb;
  byVerb[k] = byVerb[k] || { issued: 0, applied: 0, sources: {} };
  byVerb[k].issued++;
  if (c.result === 'applied') byVerb[k].applied++;
  byVerb[k].sources[c.source] = (byVerb[k].sources[c.source] || 0) + 1;
  byYear[c.year] = byYear[c.year] || {};
  byYear[c.year][k] = (byYear[c.year][k] || 0) + 1;
}

// The AI's verbs: corp decisions by verb (per nation), and nation research picks.
const aiVerbTotals = {};       // verb -> count, all AI majors
const aiVerbPerNation = {};    // nation -> verb -> count
for (const q of played) {
  for (const n of q.nations) {
    if (n.player) continue;
    const per = (aiVerbPerNation[n.id] = aiVerbPerNation[n.id] || {});
    for (const [v, c] of Object.entries((n.ai_decisions || {}).by_verb || {})) {
      aiVerbTotals[v] = (aiVerbTotals[v] || 0) + c;
      per[v] = (per[v] || 0) + c;
    }
    const picks = ((n.research || {}).ai_picks || []).filter(p => p.queued).length;
    if (picks) {
      aiVerbTotals.queue_research = (aiVerbTotals.queue_research || 0) + picks;
      per.queue_research = (per.queue_research || 0) + picks;
    }
  }
}
const aiMedianVerb = {};
for (const v of Object.keys(aiVerbTotals))
  aiMedianVerb[v] = median(aiMajors.map(id => (aiVerbPerNation[id] || {})[v] || 0));

// --- 2. budget profile -----------------------------------------------------
const meanBudget = id => {
  const acc = {}; let n = 0;
  for (const q of played.length ? played : quarters) {
    const r = rowOf(q, id);
    if (!r || !r.budget) continue;
    n++;
    for (const [k, v] of Object.entries(r.budget)) acc[k] = (acc[k] || 0) + v;
  }
  if (!n) return null;
  for (const k of Object.keys(acc)) acc[k] /= n;
  return acc;
};
const playerBudget = meanBudget(playerId);
const aiBudgets = aiMajors.map(meanBudget).filter(Boolean);
const budgetLines = [...new Set([...(playerBudget ? Object.keys(playerBudget) : []),
                                ...aiBudgets.flatMap(b => Object.keys(b))])];
const budget = budgetLines.map(line => ({
  line,
  player: playerBudget ? playerBudget[line] ?? 0 : null,
  ai_median: median(aiBudgets.map(b => b[line] ?? 0)),
  ai_max: aiBudgets.length ? Math.max(...aiBudgets.map(b => b[line] ?? 0)) : null,
}));
const budgetBy = (rowOf(quarters[quarters.length - 1] || { nations: [] }, playerId) || {}).budget_by || null;

// --- 3. research order -----------------------------------------------------
// The opening snapshot's "gained" is everything held at the start, not research.
const majorsOf = id => {
  const out = [];
  for (const q of played) {
    const r = rowOf(q, id);
    for (const g of ((r || {}).research || {}).gained || [])
      if (g.major) out.push({ id: g.id, name: g.name, date: q.date, n: q.n });
  }
  return out;
};
const playerMajors = majorsOf(playerId).slice(0, 10);
const aiMajorLists = aiMajors.map(id => ({ id, majors: majorsOf(id) }));
const research = [];
for (let k = 0; k < 10; k++) {
  const reached = aiMajorLists.filter(a => a.majors.length > k);
  const names = {};
  for (const a of reached) names[a.majors[k].name] = (names[a.majors[k].name] || 0) + 1;
  const common = Object.entries(names).sort((a, b) => b[1] - a[1])[0];
  const medQ = median(reached.map(a => a.majors[k].n));
  const medDate = medQ == null ? null
    : (played.find(q => q.n >= medQ) || {}).date || null;
  research.push({
    k: k + 1,
    player: playerMajors[k] ? { name: playerMajors[k].name, date: playerMajors[k].date } : null,
    ai_reached: reached.length,
    ai_median_date: medDate,
    ai_most_common: common ? { name: common[0], nations: common[1] } : null,
  });
}

// --- 4. curves per in-game year (the last quarter of each year) -----------
const lastOfYear = new Map();
for (const q of quarters) lastOfYear.set(q.year, q);
const curves = [...lastOfYear.entries()].sort((a, b) => a[0] - b[0]).map(([year, q]) => {
  const p = rowOf(q, playerId) || {};
  const ai = aiMajors.map(id => rowOf(q, id)).filter(Boolean);
  const pick = (r, f) => f(r);
  const champInc = r => (r.champion || {}).income;
  return {
    year, date: q.date,
    treasury: { player: p.treasury ?? null, ai_median: median(ai.map(r => r.treasury)) },
    champion_income: { player: champInc(p) ?? null, ai_median: median(ai.map(champInc)) },
    corp_income: { player: p.corp_income ?? null, ai_median: median(ai.map(r => r.corp_income)) },
    nodes_held: { player: pick(p, r => (r.research || {}).held) ?? null,
                  ai_median: median(ai.map(r => (r.research || {}).held)) },
    majors_held: { player: pick(p, r => (r.research || {}).majors_held) ?? null,
                   ai_median: median(ai.map(r => (r.research || {}).majors_held)) },
  };
});

// --- 5. what the player did that no AI did --------------------------------
const playerVerbs = Object.entries(byVerb).filter(([, v]) => v.applied > 0).map(([k]) => k);
const uniqueVerbs = playerVerbs.filter(v => v !== 'take_seat' && !aiVerbTotals[v]);
const uniqueLines = budget.filter(b => (b.player || 0) > 0 && !(b.ai_max > 0) && b.line !== 'reserve_fraction')
  .map(b => b.line);
const aiMajorNames = new Set(aiMajorLists.flatMap(a => a.majors.map(m => m.name)));
const uniqueMajors = majorsOf(playerId).map(m => m.name).filter(n => !aiMajorNames.has(n));

const summary = {
  file, seed: header.seed, epoch_year: header.epoch_year, build_git: header.build_git || null,
  player: { nation: playerId, name: header.seat.nation_name, champion: header.seat.champion_name },
  ai_majors: aiMajors.map(id => ({ id, name: nameOf.get(id) })),
  span: quarters.length ? { from: quarters[0].date, to: quarters[quarters.length - 1].date,
                            quarters_played: played.length } : null,
  unparsed_lines: bad,
  commands: { by_verb: byVerb, by_year: byYear, ai_median_by_verb: aiMedianVerb },
  budget: { player_weighted_by: budgetBy, lines: budget },
  research, curves,
  only_player: { verbs: uniqueVerbs, budget_lines: uniqueLines, majors: uniqueMajors },
};

if (asJson) { console.log(JSON.stringify(summary, null, 2)); process.exit(0); }

// --- plain text -----------------------------------------------------------
const f = v => v == null ? '-' : (Math.abs(v) >= 1000 ? Math.round(v).toLocaleString('en-US')
                                   : String(parseFloat(v.toFixed(3))));
const pad = (s, n) => String(s).padEnd(n);
const out = [];
out.push(`PLAY vs AI — ${file}`);
out.push(`seed ${summary.seed}, epoch ${summary.epoch_year}${summary.build_git ? `, build ${summary.build_git}` : ''}`);
out.push(`player: ${summary.player.name} (champion ${summary.player.champion}); ` +
         `${aiMajors.length} AI majors`);
if (summary.span) out.push(`span: ${summary.span.from} .. ${summary.span.to}, ${summary.span.quarters_played} quarters played`);
if (bad) out.push(`(${bad} unparsed line(s) — a crash can cut the last one)`);

out.push('', '1. COMMANDS BY VERB (player issued/applied vs median AI major\'s scored decisions)');
const verbs = [...new Set([...Object.keys(byVerb), ...Object.keys(aiMedianVerb)])].sort();
if (!verbs.length) out.push('  (none)');
for (const v of verbs) {
  const p = byVerb[v];
  out.push(`  ${pad(v, 22)} player ${pad(p ? `${p.issued}/${p.applied}` : '-', 9)} AI median ${f(aiMedianVerb[v] ?? 0)}`);
}
const years = Object.keys(byYear).sort();
if (years.length) {
  out.push('  over time:');
  for (const y of years)
    out.push(`    ${y}: ` + Object.entries(byYear[y]).map(([k, c]) => `${k} ${c}`).join(', '));
}

out.push('', `2. BUDGET WEIGHTS (mean over the session; player weighted by: ${budgetBy || 'the player'})`);
if (budgetBy === 'scorer_stand_in')
  out.push('  NOTE: the player\'s weights are the budget scorer\'s stand-in, not the player\'s choice.');
for (const b of budget)
  out.push(`  ${pad(b.line, 22)} player ${pad(f(b.player), 8)} AI median ${pad(f(b.ai_median), 8)} AI max ${f(b.ai_max)}`);

out.push('', '3. RESEARCH ORDER (first 10 majors)');
for (const r of research) {
  const p = r.player ? `${r.player.name} (${r.player.date})` : '-';
  const a = r.ai_reached ? `${r.ai_reached}/${aiMajors.length} reached, median ${r.ai_median_date}` +
            (r.ai_most_common ? `, most often ${r.ai_most_common.name} (${r.ai_most_common.nations})` : '') : 'none reached';
  out.push(`  #${pad(r.k, 3)} player ${pad(p, 40)} AI: ${a}`);
}

out.push('', '4. CURVES (last quarter of each in-game year; player / AI median)');
out.push(`  ${pad('year', 6)}${pad('treasury', 24)}${pad('champion income', 24)}${pad('home corp income', 24)}${pad('nodes', 12)}majors`);
for (const c of curves)
  out.push(`  ${pad(c.year, 6)}${pad(`${f(c.treasury.player)} / ${f(c.treasury.ai_median)}`, 24)}` +
           `${pad(`${f(c.champion_income.player)} / ${f(c.champion_income.ai_median)}`, 24)}` +
           `${pad(`${f(c.corp_income.player)} / ${f(c.corp_income.ai_median)}`, 24)}` +
           `${pad(`${f(c.nodes_held.player)} / ${f(c.nodes_held.ai_median)}`, 12)}` +
           `${f(c.majors_held.player)} / ${f(c.majors_held.ai_median)}`);

out.push('', '5. WHAT YOU DID THAT NO AI DID');
const none = '(nothing)';
out.push(`  verbs:        ${uniqueVerbs.join(', ') || none}`);
out.push(`  budget lines: ${uniqueLines.join(', ') || none}${budgetBy === 'scorer_stand_in' ? '  [stand-in weights]' : ''}`);
out.push(`  majors:       ${uniqueMajors.join(', ') || none}`);
console.log(out.join('\n'));
