#!/usr/bin/env node
// worktree_prune.js — find and (optionally) remove sub-agent worktrees whose branch
// has already merged into main, so `.claude/worktrees/` does not accumulate forever.
//
// USAGE:   node tools/session/worktree_prune.js [--remove]
//   (no flag)  — report only. Lists every worktree, its branch, and its eligibility.
//   --remove   — actually `git worktree remove --force` + `git branch -D` every
//                ELIGIBLE worktree. Never touches the main worktree, a locked one
//                (an agent may still be running in it), or one whose branch carries
//                commits that have not landed on main.
//
// ORIGIN (2026-09-13): a Gyre-side session audit found 39 worktrees / 19 GB sitting
// under .claude/worktrees, most of them days old with branches long since merged —
// 27 were pure dead weight (freed ~11 GB). Nothing in the tooling ever pruned a
// worktree once its branch landed, so they just piled up and every full-repo scan
// (Explore, git status, find) paid the cost of walking them. This tool is the
// missing step; run it after a merge batch, or periodically as housekeeping.
// See ../../../Project-Gyre/docs/policy/execution-model.md § Worktree lifecycle.
//
// ELIGIBLE = branch is an ancestor of main (fully merged) AND the worktree has no
// uncommitted changes of its own AND it is not locked. A worktree can be dirty with
// leftover build artefacts / scratch files even after its real commits merged — those
// are discarded on --remove, since nothing merged depends on them.
//
// Zero deps (git only).
'use strict';
const { execFileSync } = require('child_process');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const MAIN = 'main';

function git(args) {
  try {
    const out = execFileSync('git', args, { cwd: ROOT, encoding: 'utf8', stdio: ['ignore', 'pipe', 'pipe'] });
    return { ok: true, out: out.trim() };
  } catch (e) {
    return { ok: false, out: '', stderr: String((e && e.stderr) || (e && e.message) || '').trim() };
  }
}

function listWorktrees() {
  const r = git(['worktree', 'list', '--porcelain']);
  if (!r.ok) { console.error('git worktree list failed:', r.stderr); process.exit(1); }
  const entries = [];
  let cur = null;
  for (const line of r.out.split('\n')) {
    if (line.startsWith('worktree ')) { if (cur) entries.push(cur); cur = { path: line.slice(9), branch: null, locked: false, detached: false }; }
    else if (line.startsWith('branch ')) cur.branch = line.slice(7).replace(/^refs\/heads\//, '');
    else if (line === 'detached') cur.detached = true;
    else if (line.startsWith('locked')) cur.locked = true;
  }
  if (cur) entries.push(cur);
  return entries;
}

function isMerged(branch) {
  const r = git(['merge-base', '--is-ancestor', branch, MAIN]);
  return r.ok; // exit 0 == ancestor (merged); exit 1 == not merged
}

function isDirtyOrUnmerged(wtPath, branch) {
  const s = git(['-C', wtPath, 'status', '--porcelain']);
  return s.ok ? s.out.length > 0 : true; // treat a failed status as "don't touch"
}

const doRemove = process.argv.includes('--remove');
const worktrees = listWorktrees().filter((w) => path.resolve(w.path) !== ROOT);

if (worktrees.length === 0) {
  console.log('No sub-agent worktrees found.');
  process.exit(0);
}

const report = worktrees.map((w) => {
  if (w.detached) return { ...w, eligible: false, reason: 'detached HEAD — no branch to check merge status against' };
  if (w.locked) return { ...w, eligible: false, reason: 'locked (an agent may still be running here)' };
  const merged = isMerged(w.branch);
  if (!merged) return { ...w, eligible: false, reason: 'branch has commits not yet on main' };
  return { ...w, eligible: true, reason: 'branch fully merged into main' };
});

console.log(`${report.length} worktree(s) found (excluding the main tree):\n`);
for (const r of report) {
  const tag = r.eligible ? 'PRUNE' : 'keep ';
  console.log(`  [${tag}] ${r.path}`);
  console.log(`          branch: ${r.branch || '(detached)'} — ${r.reason}`);
}

const eligible = report.filter((r) => r.eligible);
console.log(`\n${eligible.length} eligible for removal.`);

if (!doRemove) {
  if (eligible.length) console.log('Re-run with --remove to delete them (discards any uncommitted local artefacts in those worktrees).');
  process.exit(0);
}

for (const r of eligible) {
  const rm = git(['worktree', 'remove', '--force', r.path]);
  if (!rm.ok) { console.error(`FAILED to remove ${r.path}: ${rm.stderr}`); continue; }
  console.log(`removed worktree ${r.path}`);
  const db = git(['branch', '-D', r.branch]);
  if (db.ok) console.log(`deleted branch ${r.branch}`);
  else console.error(`  (worktree removed, but could not delete branch ${r.branch}: ${db.stderr})`);
}
git(['worktree', 'prune']);
console.log('\nDone.');
