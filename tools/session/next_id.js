#!/usr/bin/env node
// next_id.js — compute the next SAFE backlog id across ALL branches, not just the
// working file, so concurrent worktree sessions don't mint colliding BL-ids off a
// stale local max. Run this BEFORE authoring a new backlog item (DELIVERY.md
// § Parallel worktree coherence). This is the *preventive* half of the collision
// defence; backlog_lint's duplicate-id scan is the *backstop*.
//
// USAGE:   node tools/session/next_id.js [count] [--claim <SHORT_NAME>] [--allow-no-refs]
//   count           — how many consecutive ids to reserve for this session (default 1).
//   --claim <NAME>  — record the allocated id(s) in the reservation ledger immediately.
//   --allow-no-refs — deliberate override: proceed on a degraded (refless) scan anyway.
//
// OUTPUT:  the next free BL-id (or a reserved range if count > 1), the max seen and
//   where, plus a COLLISION warning if one id already maps to different items on
//   different refs (an in-flight collision the integrator must renumber).
//
// EXIT:    0 = trustworthy scan.  1 = the scan is DEGRADED and the id is not safe.
//   This tool used to exit 0 unconditionally and swallow every git failure, which meant
//   a total ref-scan failure was indistinguishable from a clean repo — it printed a
//   confident "next safe id" derived from the local file alone. That fail-open is how
//   BL-326..BL-333 each landed twice (BL-322). A collision-defence tool that fails open
//   is worse than no tool, because it is trusted: so a scan that finds zero refs in a
//   repo that HAS refs is now an error, not a result.
//
// Zero deps (git + fs only).
'use strict';
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const BACKLOG = 'docs/development/backlog.json';
// Append-only reservation ledger: the *preventive persistence* that survives between
// sessions and across worktrees BEFORE any backlog.json edit or commit exists. A session
// that allocates an id records the claim here (via --claim <SHORT_NAME>); the next scan
// folds these ids into the max, so two concurrent sessions can't mint the same id off a
// stale backlog max. One JSON object per line: {id, name, ts, branch}.
const LEDGER = 'docs/development/id_reservations.jsonl';

// backlog.json is ~830 KB and growing; node's default execFile maxBuffer is 1 MB. Once it
// crosses that line every `git show <ref>:backlog.json` starts throwing ENOBUFS — another
// silent-degradation path, since the old code caught and returned []. Give it real room.
const MAX_BUFFER = 256 * 1024 * 1024;

// Run git WITHOUT a shell. The original helper used execSync, which routes through
// /bin/sh — dash on Debian — where the unquoted `%(refname)` in the for-each-ref format
// is a syntax error ("Syntax error: \"(\" unexpected"). cmd.exe on the Windows dev box
// tolerates the bare parens, so the tool worked there and silently reported 0 refs on
// Linux. execFileSync passes argv straight to git: no shell, no quoting hazard at all.
function gitTry(args) {
  try {
    const out = execFileSync('git', args, {
      cwd: ROOT,
      encoding: 'utf8',
      stdio: ['ignore', 'pipe', 'pipe'],
      maxBuffer: MAX_BUFFER,
    });
    return { ok: true, out: out.trim() };
  } catch (e) {
    const stderr = String((e && e.stderr) || (e && e.message) || '').trim();
    return { ok: false, status: e && typeof e.status === 'number' ? e.status : null, stderr };
  }
}

const isId = (id) => /^BL-\d+$/.test(id);
const num = (id) => parseInt(id.slice(3), 10);
const pad = (n) => 'BL-' + String(n).padStart(3, '0');

// Anything that means "part of the scan didn't happen". A non-empty list makes the
// reported id untrustworthy, and the tool says so and exits non-zero.
const problems = [];

// Every ref whose backlog we should consult: local branches + remote-tracking, minus
// the symbolic origin/HEAD. Returns null (not []) when the ENUMERATION ITSELF failed —
// the distinction the old `catch { return [] }` destroyed.
function refs() {
  const r = gitTry(['for-each-ref', '--format=%(refname)', 'refs/heads', 'refs/remotes']);
  if (!r.ok) {
    problems.push(`ref enumeration failed: git for-each-ref exited ${r.status} — ${r.stderr}`);
    return null;
  }
  return r.out.split('\n').filter(Boolean).filter((x) => !x.endsWith('/HEAD'));
}

// Independent cross-check down a DIFFERENT plumbing path than for-each-ref, so "the scan
// returned zero" can be told apart from "this repo genuinely has no refs" (a fresh init).
// true = has refs, false = provably refless, null = couldn't tell.
function repoHasRefs() {
  const r = gitTry(['show-ref']);
  if (r.ok) return r.out.length > 0;
  if (r.status === 1) return false; // show-ref's documented "no refs found" exit
  return null;
}

// A ref that simply doesn't carry the file yet is expected (older branches predate it);
// any other failure is a hole in the scan and must be reported.
const BENIGN = /does not exist|exists on disk|unknown revision|invalid object name|path .* does not exist/i;
function noteReadFailure(ref, file, r) {
  if (BENIGN.test(r.stderr)) return;
  problems.push(`could not read ${file} on ${ref} (exit ${r.status}): ${r.stderr}`);
}

function itemsFromText(txt, source) {
  let j;
  try {
    j = JSON.parse(txt);
  } catch (e) {
    problems.push(`unparseable backlog JSON at ${source}: ${e.message}`);
    return [];
  }
  return (j.items || []).filter((i) => isId(i.id)).map((i) => ({ id: i.id, name: i.short_name || '?' }));
}
function itemsFromRef(ref) {
  const r = gitTry(['show', `${ref}:${BACKLOG}`]);
  if (!r.ok) { noteReadFailure(ref, BACKLOG, r); return []; }
  return itemsFromText(r.out, `${ref}:${BACKLOG}`);
}
function itemsFromWorking() {
  try {
    return itemsFromText(fs.readFileSync(path.join(ROOT, BACKLOG), 'utf8'), '(working tree)');
  } catch (e) {
    problems.push(`could not read working-tree ${BACKLOG}: ${e.message}`);
    return [];
  }
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

// Reservation ledger (working tree + every ref that has it) — folded in as ordinary
// sources so a reserved-but-uncommitted id still lifts the max.
function ledgerItems(txt) {
  const out = [];
  for (const line of txt.split('\n')) {
    const s = line.trim();
    if (!s) continue;
    try { const o = JSON.parse(s); if (isId(o.id)) out.push({ id: o.id, name: o.name || '(reserved)' }); } catch { /* skip */ }
  }
  return out;
}
function ledgerFromWorking() {
  try { return ledgerItems(fs.readFileSync(path.join(ROOT, LEDGER), 'utf8')); } catch { return []; }
}
function ledgerFromRef(ref) {
  const r = gitTry(['show', `${ref}:${LEDGER}`]);
  if (!r.ok) { noteReadFailure(ref, LEDGER, r); return []; }
  return ledgerItems(r.out);
}

// --- argv -------------------------------------------------------------------
let claimName = null;
const argv = process.argv.slice(2);
const ci = argv.indexOf('--claim');
if (ci !== -1) { claimName = argv[ci + 1] || '(reserved)'; argv.splice(ci, 2); }
const ai = argv.indexOf('--allow-no-refs');
const allowNoRefs = ai !== -1;
if (allowNoRefs) argv.splice(ai, 1);

// --- scan -------------------------------------------------------------------
const allRefs = refs();
const hasRefs = repoHasRefs();

// The gate. Zero scanned refs is only acceptable when the repo is PROVABLY refless.
// "Couldn't tell" counts as broken: a defence that isn't sure it ran hasn't run.
const scanBroken = allRefs === null || (allRefs.length === 0 && hasRefs !== false);

if (scanBroken && !allowNoRefs) {
  console.error('!!! REF SCAN FAILED — refusing to report a "safe" id.');
  console.error('');
  console.error(`    Scanned refs: ${allRefs === null ? '(enumeration failed)' : allRefs.length}`);
  console.error(`    Repo has refs: ${hasRefs === null ? '(could not determine)' : hasRefs}`);
  for (const p of problems) console.error(`    - ${p}`);
  console.error('');
  console.error('    This tool exists to stop two worktrees minting the same BL-id. With no');
  console.error('    refs it can only see the local file, so any id it printed would be a');
  console.error('    guess wearing the word "safe". Fix git access and re-run.');
  console.error('    Deliberate override (you accept the collision risk): --allow-no-refs');
  process.exit(1);
}

for (const r of allRefs || []) { record(r, itemsFromRef(r)); record(r + ' (ledger)', ledgerFromRef(r)); }
record('(working tree)', itemsFromWorking());
record('(ledger)', ledgerFromWorking());

let max = 0, maxWhere = [];
for (const [id, names] of byId) {
  const n = num(id);
  if (n > max) { max = n; maxWhere = [...new Set([].concat(...[...names.values()].map((s) => [...s])))]; }
}

const count = Math.max(1, parseInt(argv[0] || '1', 10));
const first = max + 1;

// --- report -----------------------------------------------------------------
console.log(`Scanned ${byId.size} distinct BL-ids across ${(allRefs || []).length} ref(s) + working tree.`);
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

// Persist the claim(s) so the next scan (this session or a concurrent worktree) can't
// re-mint them off a stale backlog max.
if (claimName) {
  let branch = '?';
  const b = gitTry(['rev-parse', '--abbrev-ref', 'HEAD']);
  if (b.ok) branch = b.out;
  const ts = new Date().toISOString();
  const lines = [];
  for (let n = first; n < first + count; n++) {
    lines.push(JSON.stringify({ id: pad(n), name: claimName, ts, branch }));
  }
  fs.appendFileSync(path.join(ROOT, LEDGER), lines.join('\n') + '\n');
  console.log(`\nClaimed ${count === 1 ? pad(first) : pad(first) + '..' + pad(max + count)} for "${claimName}" -> ${LEDGER} (commit it, or drop the line if you abandon the id).`);
}

// A partial scan still yields a number, but not a trustworthy one — say so out loud and
// exit non-zero rather than let a hole pass for a clean run.
if (problems.length) {
  console.error(`\n!!! SCAN INCOMPLETE — ${problems.length} source(s) could not be read; the id above is NOT verified:`);
  for (const p of problems) console.error(`    - ${p}`);
  process.exit(1);
}
if (scanBroken) {
  console.error('\n!!! Scan ran with NO refs (--allow-no-refs). The id above is a local guess.');
  process.exit(1);
}
