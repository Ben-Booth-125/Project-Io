#!/usr/bin/env node
// next_save_version.js — compute the next SAFE `world_save_version` across ALL branches,
// not just the working copy, so concurrent worktree sessions don't both bump to the same
// number off a stale local header. Run this BEFORE editing `world_save_version` in
// src/world/world_save.hpp (DEVELOPMENT_PRACTICES.md § Save-format versions).
//
// This is next_id.js for the save format. Same failure it prevents, one field over: in
// Sprint 19 two worktree agents each read `= 13`, each wrote `= 14`, and the duplicate was
// caught only because a human read both diffs at the merge point. A save version is worse
// than a BL-id collision, because the number IS the compatibility contract — two different
// record layouts wearing one version means a save that reads as valid and is not.
//
// USAGE:   node tools/session/next_save_version.js [count] [--claim <OWNER>] [--allow-no-refs]
//   count           — how many consecutive versions to reserve (default 1). Use this when one
//                     item lands two independent record changes, or to stack a wave by hand.
//   --claim <OWNER> — record the allocated version(s) in the reservation ledger immediately.
//                     OWNER is what a human needs to read at the merge point: prefer the
//                     backlog short_name, e.g. --claim "BL-637 save-version-reservation".
//   --allow-no-refs — deliberate override: proceed on a degraded (refless) scan anyway.
//
// OUTPUT:  the next free version, the max seen and where, the claims currently in flight,
//   and a COLLISION block when two branches already carry the same unmerged version or one
//   version is claimed by two different owners.
//
// EXIT:    0 = trustworthy scan.  1 = the scan is DEGRADED and the version is not safe.
//   Same discipline as next_id.js, and for the same reason: a collision-defence tool that
//   fails open is worse than no tool, because it is trusted. Zero refs in a repo that HAS
//   refs is an error, not a result. A reported collision is NOT an exit-1 condition — it is
//   a finding the integrator must act on, and the printed version is still the safe one.
//
// Zero deps (git + fs only). Reads src/ ; never writes to it.
'use strict';
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const HEADER = 'src/world/world_save.hpp';
// Append-only reservation ledger, one JSON object per line: {version, owner, ts, branch}.
// JSONL rather than a JSON array for the same reason id_reservations.jsonl is: two worktrees
// each appending a line merge as a line-wise union, where two worktrees each appending an
// array element collide on the closing bracket. The store must survive exactly the situation
// it exists to defend against.
const LEDGER = 'docs/development/save_version_reservations.jsonl';

// Matched against next_id.js: git show of a source file is small, but keep one knob for both.
const MAX_BUFFER = 256 * 1024 * 1024;

// Run git WITHOUT a shell — execFileSync passes argv straight to git, so a `%(refname)`
// format string is not at the mercy of dash-vs-cmd quoting (the bug that made next_id.js
// silently report 0 refs on Linux).
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

// Anything that means "part of the scan didn't happen".
const problems = [];

function refs() {
  const r = gitTry(['for-each-ref', '--format=%(refname)', 'refs/heads', 'refs/remotes']);
  if (!r.ok) {
    problems.push(`ref enumeration failed: git for-each-ref exited ${r.status} — ${r.stderr}`);
    return null;
  }
  return r.out.split('\n').filter(Boolean).filter((x) => !x.endsWith('/HEAD'));
}

// Independent cross-check down a different plumbing path, so "the scan returned zero" can be
// told apart from "this repo genuinely has no refs".
function repoHasRefs() {
  const r = gitTry(['show-ref']);
  if (r.ok) return r.out.length > 0;
  if (r.status === 1) return false;
  return null;
}

const BENIGN = /does not exist|exists on disk|unknown revision|invalid object name|path .* does not exist/i;
function noteReadFailure(ref, file, r) {
  if (BENIGN.test(r.stderr)) return;
  problems.push(`could not read ${file} on ${ref} (exit ${r.status}): ${r.stderr}`);
}

// --- the constant ------------------------------------------------------------
// One declaration, one regex. Deliberately anchored on the assignment rather than the whole
// `inline constexpr uint32_t` spelling, so a type or storage-class change doesn't blind the
// scan; a `==` comparison (save_roundtrip's static_assert) is excluded by requiring a single '='.
const DECL = /\bworld_save_version\s*=\s*(\d+)\s*;/;
function versionFromText(txt, source) {
  const m = DECL.exec(txt);
  if (!m) {
    problems.push(`no world_save_version declaration found in ${source}`);
    return null;
  }
  return parseInt(m[1], 10);
}
function versionFromRef(ref) {
  const r = gitTry(['show', `${ref}:${HEADER}`]);
  if (!r.ok) { noteReadFailure(ref, HEADER, r); return null; }
  return versionFromText(r.out, `${ref}:${HEADER}`);
}
function versionFromWorking() {
  try {
    return versionFromText(fs.readFileSync(path.join(ROOT, HEADER), 'utf8'), '(working tree)');
  } catch (e) {
    problems.push(`could not read working-tree ${HEADER}: ${e.message}`);
    return null;
  }
}

// --- the ledger --------------------------------------------------------------
function ledgerEntries(txt, source) {
  const out = [];
  for (const line of txt.split('\n')) {
    const s = line.trim();
    if (!s) continue;
    try {
      const o = JSON.parse(s);
      const v = typeof o.version === 'number' ? o.version : parseInt(o.version, 10);
      if (Number.isInteger(v)) out.push({ version: v, owner: o.owner || '(reserved)', branch: o.branch || '?', ts: o.ts || '?', source });
    } catch { /* a malformed line is not a reason to fail the whole scan */ }
  }
  return out;
}
function ledgerFromWorking() {
  try { return ledgerEntries(fs.readFileSync(path.join(ROOT, LEDGER), 'utf8'), '(ledger)'); } catch { return []; }
}
function ledgerFromRef(ref) {
  const r = gitTry(['show', `${ref}:${LEDGER}`]);
  if (!r.ok) { noteReadFailure(ref, LEDGER, r); return []; }
  return ledgerEntries(r.out, `${ref} (ledger)`);
}

// A branch and its origin mirror are ONE branch for collision purposes.
const shortBranch = (ref) => ref.replace(/^refs\/heads\//, '').replace(/^refs\/remotes\/[^/]+\//, '');

// --- argv --------------------------------------------------------------------
let claimOwner = null;
const argv = process.argv.slice(2);
const ci = argv.indexOf('--claim');
if (ci !== -1) { claimOwner = argv[ci + 1] || '(reserved)'; argv.splice(ci, 2); }
const ai = argv.indexOf('--allow-no-refs');
const allowNoRefs = ai !== -1;
if (allowNoRefs) argv.splice(ai, 1);

// --- scan --------------------------------------------------------------------
const allRefs = refs();
const hasRefs = repoHasRefs();

// Zero scanned refs is only acceptable when the repo is PROVABLY refless. "Couldn't tell"
// counts as broken: a defence that isn't sure it ran hasn't run.
const scanBroken = allRefs === null || (allRefs.length === 0 && hasRefs !== false);

if (scanBroken && !allowNoRefs) {
  console.error('!!! REF SCAN FAILED — refusing to report a "safe" save version.');
  console.error('');
  console.error(`    Scanned refs: ${allRefs === null ? '(enumeration failed)' : allRefs.length}`);
  console.error(`    Repo has refs: ${hasRefs === null ? '(could not determine)' : hasRefs}`);
  for (const p of problems) console.error(`    - ${p}`);
  console.error('');
  console.error('    This tool exists to stop two worktrees claiming the same world_save_version.');
  console.error('    With no refs it can only see the local header, so any number it printed would');
  console.error('    be a guess wearing the word "safe". Fix git access and re-run.');
  console.error('    Deliberate override (you accept the collision risk): --allow-no-refs');
  process.exit(1);
}

// version -> Set(branch) from the header on each ref. COMMITTED refs only: the working tree
// is this branch's uncommitted state, not a fourth branch, and folding it in here would make
// every ordinary local bump report itself as a collision with itself.
const headerBranches = new Map();
const refVersion = new Map(); // full refname -> version, so the baseline is not re-read
const noteHeader = (v, branch) => {
  if (v === null) return;
  if (!headerBranches.has(v)) headerBranches.set(v, new Set());
  headerBranches.get(v).add(branch);
};

const claims = [];
for (const ref of allRefs || []) {
  const v = versionFromRef(ref);
  refVersion.set(ref, v);
  noteHeader(v, shortBranch(ref));
  claims.push(...ledgerFromRef(ref));
}
const workingVersion = versionFromWorking();
claims.push(...ledgerFromWorking());

let currentBranch = '?';
{
  const b = gitTry(['rev-parse', '--abbrev-ref', 'HEAD']);
  if (b.ok) currentBranch = b.out;
}

// Highest number anyone has taken, from any source.
let max = 0;
let maxWhere = [];
for (const [v, branches] of headerBranches) {
  if (v > max) { max = v; maxWhere = [...branches]; }
  else if (v === max) maxWhere = [...new Set([...maxWhere, ...branches])];
}
if (workingVersion !== null) {
  if (workingVersion > max) { max = workingVersion; maxWhere = ['(working tree)']; }
  else if (workingVersion === max) maxWhere = [...new Set([...maxWhere, '(working tree)'])];
}
for (const c of claims) {
  if (c.version > max) { max = c.version; maxWhere = [`ledger: ${c.owner}`]; }
  else if (c.version === max) maxWhere = [...new Set([...maxWhere, `ledger: ${c.owner}`])];
}

const count = Math.max(1, parseInt(argv[0] || '1', 10));
const first = max + 1;

// --- report ------------------------------------------------------------------
console.log(`Scanned ${HEADER} across ${(allRefs || []).length} ref(s) + working tree, and ${claims.length} ledger claim(s).`);
console.log(`Working tree: world_save_version = ${workingVersion === null ? '?' : workingVersion}`);
console.log(`Highest taken: ${max}  (on: ${maxWhere.join(', ')})`);
if (count === 1) console.log(`\n>>> Next safe world_save_version: ${first}`);
else console.log(`\n>>> Reserve ${count}: ${first} .. ${max + count}`);

// Claims at or above the working header are the ones still in flight — the set a merging
// session needs to see. Older claims are history and stay quiet.
const floor = workingVersion === null ? 0 : workingVersion;
const inFlight = claims.filter((c) => c.version >= floor).sort((a, b) => a.version - b.version || a.ts.localeCompare(b.ts));
if (inFlight.length) {
  console.log(`\nIn-flight claims (>= ${floor}):`);
  const seen = new Set();
  for (const c of inFlight) {
    const key = `${c.version}|${c.owner}|${c.branch}`;
    if (seen.has(key)) continue; // the same line on N refs is one claim
    seen.add(key);
    console.log(`  v${c.version}  ${c.owner}   [${c.branch}, ${c.ts}]`);
  }
}

// --- collisions --------------------------------------------------------------
const collisions = [];

// (a) One version, two owners in the ledger. Unambiguous: someone skipped the tool.
const byVersion = new Map();
for (const c of claims) {
  if (!byVersion.has(c.version)) byVersion.set(c.version, new Map());
  byVersion.get(c.version).set(c.owner, c.branch);
}
for (const [v, owners] of byVersion) {
  if (owners.size > 1) {
    collisions.push(`v${v} is claimed by ${owners.size} owners: ` + [...owners].map(([o, b]) => `${o} [${b}]`).join(' / '));
  }
}

// (b) One version, two unmerged branches carrying it in the header. This is the Sprint 19
// failure verbatim. Everything at or below the integration baseline (main) is shared history
// and expected on many branches; only versions ABOVE it can be a double-claim.
const baselineRef = (allRefs || []).find((r) => r === 'refs/remotes/origin/main') || (allRefs || []).find((r) => r === 'refs/heads/main');
const baseline = baselineRef ? (refVersion.get(baselineRef) ?? null) : null;
if (baseline !== null) {
  const above = new Set([...headerBranches.keys()].filter((v) => v > baseline));
  if (workingVersion !== null && workingVersion > baseline) above.add(workingVersion);
  for (const v of above) {
    const committed = [...(headerBranches.get(v) || [])];
    const others = committed.filter((b) => b !== currentBranch);
    if (committed.length > 1) {
      collisions.push(`v${v} is carried by ${committed.length} branches above main (v${baseline}): ${committed.join(', ')}`);
    } else if (v === workingVersion && others.length) {
      collisions.push(`v${v} is your uncommitted working-tree bump, but branch ${others.join(', ')} already carries it (main is v${baseline})`);
    }
  }
} else {
  console.log('\n(note: no main ref found — the two-branches-one-version check did not run.)');
}

if (collisions.length) {
  console.log(`\n!!! ${collisions.length} save-version COLLISION(s) — resolve before merging:`);
  for (const c of collisions) console.log(`  ${c}`);
  console.log('  Stack them: keep the earlier claim, renumber the later one (its constant, its');
  console.log('  header comment, its reader/writer comments, and save_roundtrip\'s static_assert).');
}

// --- claim -------------------------------------------------------------------
if (claimOwner) {
  const ts = new Date().toISOString();
  const lines = [];
  for (let v = first; v < first + count; v++) {
    lines.push(JSON.stringify({ version: v, owner: claimOwner, ts, branch: currentBranch }));
  }
  fs.appendFileSync(path.join(ROOT, LEDGER), lines.join('\n') + '\n');
  const what = count === 1 ? `v${first}` : `v${first}..v${max + count}`;
  console.log(`\nClaimed ${what} for "${claimOwner}" -> ${LEDGER} (commit it, or drop the line if you abandon the bump).`);
}

// A partial scan still yields a number, but not a trustworthy one.
if (problems.length) {
  console.error(`\n!!! SCAN INCOMPLETE — ${problems.length} source(s) could not be read; the version above is NOT verified:`);
  for (const p of problems) console.error(`    - ${p}`);
  process.exit(1);
}
if (scanBroken) {
  console.error('\n!!! Scan ran with NO refs (--allow-no-refs). The version above is a local guess.');
  process.exit(1);
}
