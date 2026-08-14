#!/usr/bin/env node
// agent_base_check.js — verify worktree-agent branches are based on the current
// integration tip BEFORE merging them.
//
// The failure this closes is documented twice (SPRINTS.md 2026-08-early-August
// retro; memory io-worktree-agents-stale-base): agents branched from a base that
// had already moved produced code that would not merge cleanly, and one agent's
// commit swallowed a whole 328-file fast-forward. Worktrees isolate WRITES, not
// HISTORY — this is the missing history check, run by the integrator before any
// merge.
//
// USAGE:
//   node tools/session/agent_base_check.js                  check every worktree-agent-* branch ahead of main
//   node tools/session/agent_base_check.js <branch> ...     check only these branches
//   node tools/session/agent_base_check.js --base <ref>     integration tip to check against (default: main)
//
// For each branch: PASS if merge-base(base, branch) == tip(base) — the branch
// grew from the current tip. STALE otherwise, with how far the base has moved.
// Also flags a branch whose first-parent history contains merge commits (an
// agent should have a linear slice, not its own integrations).
// EXIT: 0 all PASS; 1 any STALE/miss — gate the merge on it.

'use strict';
const { execSync } = require('child_process');

const argv = process.argv.slice(2);
const bIdx = argv.indexOf('--base');
const BASE = bIdx >= 0 ? argv[bIdx + 1] : 'main';
const named = argv.filter((a, i) => !a.startsWith('--') && (bIdx < 0 || i !== bIdx + 1));

const git = (cmd) => execSync(`git ${cmd}`, { encoding: 'utf8' }).trim();

const tip = git(`rev-parse ${BASE}`);
let branches = named;
if (!branches.length) {
    branches = git(`for-each-ref --format=%(refname:short) refs/heads/worktree-agent-*`)
        .split('\n').filter(Boolean)
        // Only branches with work on them — an old empty agent branch is noise.
        .filter((b) => git(`rev-list --count ${BASE}..${b}`) !== '0');
}

if (!branches.length) {
    console.log(`agent_base_check: no worktree-agent branches ahead of ${BASE}.`);
    process.exit(0);
}

let bad = 0;
for (const b of branches) {
    let mergeBase;
    try { mergeBase = git(`merge-base ${BASE} ${b}`); }
    catch { console.error(`  MISS   ${b} — no merge base with ${BASE}`); bad++; continue; }

    const ahead = git(`rev-list --count ${BASE}..${b}`);
    const merges = git(`rev-list --merges --count ${mergeBase}..${b}`);

    if (mergeBase !== tip) {
        const behind = git(`rev-list --count ${mergeBase}..${tip}`);
        console.error(`  STALE  ${b} — based ${behind} commit(s) behind ${BASE} tip (${ahead} ahead). Rebase onto ${BASE} before merging.`);
        bad++;
    } else if (merges !== '0') {
        console.error(`  MERGY  ${b} — carries ${merges} merge commit(s); an agent slice should be linear. Inspect before merging.`);
        bad++;
    } else {
        console.log(`  PASS   ${b} — based on current ${BASE} tip, ${ahead} linear commit(s) ahead.`);
    }
}
process.exit(bad ? 1 : 0);
