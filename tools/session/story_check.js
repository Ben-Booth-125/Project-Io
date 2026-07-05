#!/usr/bin/env node
// story_check.js — coverage + testability linter for the user-story catalogue.
//
// The user-story catalogue (docs/development/user_stories.json) is the second
// route from docs to code: player intent -> UI surface + backlog item +
// requirement brief. This linter proves those links are real and that every
// story's declared testing mode is honest, so the catalogue can be used as a
// testing pillar rather than decaying into prose.
//
// It checks, per story:
//   FAIL  dead backlog trace     — a traces.backlog id not present in backlog.json
//   FAIL  dead requirement trace — a traces.requirements slug not a brief in requirements.json
//   FAIL  unroutable             — coverage != planned but surfaces/backlog/requirements empty
//   FAIL  dishonest mode         — testing.mode auto|mixed but no linked brief carries runnable
//                                  (golden/visual/headless) verification -> nothing to run
//   WARN  missing testing mode
// Plus INFO: the manual/auto/mixed split and, per auto|mixed story, the concrete
// verify commands (scripts/verify/*.lua goldens + headless harness hints) to run.
//
// USAGE:  node tools/session/story_check.js [--commands]
//   --commands  print only the shell/verify commands for the auto+mixed set (for a runner)
// EXIT:   0 = clean or warnings only;  1 = one or more FAILs.
//
// Zero dependencies (fs only). Companion to backlog_lint.js.

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const P = (rel) => path.join(ROOT, rel);
const STORIES = 'docs/development/user_stories.json';
const BACKLOG = 'docs/development/backlog.json';
const REQUIREMENTS = 'docs/development/req/requirements.json';

const fails = [];
const warns = [];
const fail = (m) => fails.push(m);
const warn = (m) => warns.push(m);

function loadJson(rel) {
  try { return JSON.parse(fs.readFileSync(P(rel), 'utf8')); }
  catch (e) { fail(`could not parse ${rel}: ${e.message}`); return null; }
}

const stories = loadJson(STORIES);
const backlog = loadJson(BACKLOG);
const reqs = loadJson(REQUIREMENTS);
if (!stories || !backlog || !reqs) report();

// ---- indexes ----
const backlogIds = new Set((backlog.items || []).map((it) => it.id));

// brief slug -> { verifications: [strings], runnable: bool, scripts: [lua paths] }
const briefs = new Map();
const RUNNABLE = /golden|visual|headless/i;
const LUA = /(scripts\/verify\/[\w./-]+\.lua)/;
for (const g of reqs.groups || []) {
  if (!g.brief) continue;
  const verifications = [];
  const scripts = new Set();
  for (const row of g.rows || []) {
    for (const v of row.verification || []) {
      verifications.push(v);
      const m = String(v).match(LUA);
      if (m) scripts.add(m[1]);
    }
  }
  briefs.set(g.brief, {
    verifications,
    runnable: verifications.some((v) => RUNNABLE.test(v)),
    scripts: [...scripts],
  });
}

// ---- per-story checks ----
const wantCommands = process.argv.includes('--commands');
const commandBlocks = [];
const modeCount = { manual: 0, auto: 0, mixed: 0, unset: 0 };
const referenced = new Set();

for (const s of stories.stories || []) {
  const t = s.traces || {};
  const label = `${s.id} (${s.cluster}/${s.testing ? s.testing.mode : '?'})`;

  for (const id of t.backlog || []) {
    referenced.add(id);
    if (!backlogIds.has(id)) fail(`${label}: dead backlog trace "${id}" — not in backlog.json.`);
  }
  for (const slug of t.requirements || []) {
    if (!briefs.has(slug)) fail(`${label}: dead requirement trace "${slug}" — no such brief in requirements.json.`);
  }

  const planned = s.coverage === 'planned';
  if (!planned) {
    if (!(t.surfaces || []).length) fail(`${label}: unroutable — no surfaces.`);
    if (!(t.backlog || []).length) fail(`${label}: unroutable — no backlog ids.`);
    if (!(t.requirements || []).length) fail(`${label}: untestable — no requirement briefs.`);
  }

  const mode = s.testing && s.testing.mode;
  if (!mode) { warn(`${s.id}: no testing.mode set.`); modeCount.unset++; }
  else modeCount[mode] = (modeCount[mode] || 0) + 1;

  // gather runnable verification across this story's linked briefs
  const linked = (t.requirements || []).map((b) => briefs.get(b)).filter(Boolean);
  const runnableBriefs = linked.filter((b) => b.runnable);
  const scripts = [...new Set(linked.flatMap((b) => b.scripts))];

  if ((mode === 'auto' || mode === 'mixed') && runnableBriefs.length === 0) {
    fail(`${label}: declared "${mode}" but none of its ${linked.length} linked brief(s) carry runnable (golden/visual/headless) verification — nothing to automate.`);
  }

  if (mode === 'auto' || mode === 'mixed') {
    const lines = scripts.length
      ? scripts.map((sc) => `  ProjectIo --verify ${sc}    # verifier-visual`)
      : ['  (headless/econ harness — see linked briefs; run via verifier-headless)'];
    commandBlocks.push(`# ${s.id} [${mode}] ${s.title}\n${lines.join('\n')}`);
  }
}

if (wantCommands) {
  console.log(commandBlocks.join('\n\n'));
  process.exit(fails.length ? 1 : 0);
}

// ---- reverse coverage (INFO): shipped player-facing items in no story ----
const PLAYER_FACING = new Set(['Canvas', 'Ledger', 'Trade', 'Supply', 'Discovery', 'UI', 'Menu', 'Workforce']);
const TERMINAL = new Set(['complete', 'shipped']);
const unstoried = (backlog.items || [])
  .filter((it) => TERMINAL.has(it.status) && PLAYER_FACING.has(it.category) && !referenced.has(it.id))
  .map((it) => it.id);

report(true);

function report(withInfo) {
  const line = (m) => `  - ${m}`;
  if (withInfo) {
    console.error(`story_check: ${(stories.stories || []).length} stories — ` +
      `manual ${modeCount.manual}, auto ${modeCount.auto}, mixed ${modeCount.mixed}` +
      (modeCount.unset ? `, unset ${modeCount.unset}` : ''));
    if (unstoried.length) {
      console.error(`\nstory_check: ${unstoried.length} shipped player-facing item(s) in NO story (candidates to cover):`);
      console.error(line(unstoried.join(', ')));
    }
  }
  if (fails.length) {
    console.error(`\nstory_check: ${fails.length} FAIL(s)`);
    fails.forEach((m) => console.error(line(m)));
  }
  if (warns.length) {
    console.error(`\nstory_check: ${warns.length} warning(s)`);
    warns.forEach((m) => console.error(line(m)));
  }
  if (!fails.length && !warns.length) console.error('\nstory_check: clean — every story routes to real surfaces/backlog/briefs and every auto|mixed story has something to run.');
  else console.error(`\nstory_check: ${fails.length} fail(s), ${warns.length} warning(s).`);
  process.exit(fails.length ? 1 : 0);
}
