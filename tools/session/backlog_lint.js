#!/usr/bin/env node
// backlog_lint.js — consistency linter for the Project Io backlog bookkeeping.
//
// Catches the "landed the code, skipped the close-out" drift that a 2026-07-04
// currency audit surfaced across ~30 loose ends: requirement groups left active
// after their item shipped, authority_doc never repointed off a generic doc,
// @BACKLOG.md design pointers that resolve to nothing, and pending rows under
// closed groups. Each of these is a mechanical contradiction between two files
// that agree if — and only if — a session ran its Delivery close step (DELIVERY.md
// step 5). This turns "forgot" into a red line.
//
// USAGE:   node tools/session/backlog_lint.js
// EXIT:    0 = clean or warnings only;  1 = one or more FAILs (gate a commit on this).
//
// Zero dependencies (fs only). Node isn't installed on the Windows dev box as of
// 2026-07-04 — this runs on the Linux dev box / CI (see memory windows-build-setup,
// BL-057/058). Wire it into the Delivery close step and/or a pre-commit hook.

'use strict';
const fs = require('fs');
const path = require('path');
const A = require('./archive_store');

const ROOT = path.resolve(__dirname, '..', '..');
const P = (rel) => path.join(ROOT, rel);
const BACKLOG_JSON = 'docs/development/backlog.json';
const REQUIREMENTS = 'docs/development/req/requirements.json';
const BACKLOG_MD = 'docs/development/BACKLOG.md';

// Terminal statuses — an item in one of these has landed; its bookkeeping must be closed.
// Every use of this set in this file asks the same question — "is this still open
// work?" — so it is CLOSED, not delivered-only: a cancelled item is off the worklist
// and must not be linted as though someone were about to build it. Delivery
// STATISTICS are status.ps1's job and keep the narrower TERMINAL.
const TERMINAL = A.CLOSED;
// authority_doc values that are process docs, not subject authorities. A landed item
// pointing here usually means the design was never propagated to its real home doc.
// Allowlist: items whose authority genuinely is a process doc (umbrella indexes).
const GENERIC_AUTHORITY = new Set(['docs/development/ROADMAP.md', 'docs/development/BACKLOG.md']);
const GENERIC_AUTHORITY_ALLOW = new Set(['BL-046']); // Layer-4 umbrella index — ROADMAP is correct

const fails = [];
const warns = [];
const fail = (m) => fails.push(m);
const warn = (m) => warns.push(m);

function loadJson(rel) {
  try {
    return JSON.parse(fs.readFileSync(P(rel), 'utf8'));
  } catch (e) {
    fail(`could not parse ${rel}: ${e.message}`);
    return null;
  }
}

const backlog = loadJson(BACKLOG_JSON);
const reqs = loadJson(REQUIREMENTS);
let backlogMd = '';
try { backlogMd = fs.readFileSync(P(BACKLOG_MD), 'utf8'); } catch { /* optional — drains to empty */ }

// If either core file failed to parse, that alone is a FAIL; stop here.
if (!backlog || !reqs) {
  report();
}

// ---- Invariant 0: id integrity (duplicates + malformed) ----
// Duplicate ids are the parallel-worktree collision hazard: two sessions minting the
// same next id off a stale local max (memory backlog-id-collision-hazard). backlog.json
// is the merge point where the dup lands, so catch it here as a hard FAIL — the Map
// index below would otherwise silently overwrite one and hide it. The *preventive* side
// is tools/session/next_id.js (allocate across all branches before authoring).
{
  const counts = new Map();
  for (const it of backlog.items || []) counts.set(it.id, (counts.get(it.id) || 0) + 1);
  for (const [id, n] of counts) {
    if (n > 1) fail(`duplicate id ${id} appears ${n}× in backlog.json — a collision; keep one and renumber the rest (+ their cross-refs). See tools/session/next_id.js.`);
  }
  for (const it of backlog.items || []) {
    if (!/^BL-\d+$/.test(it.id || '')) fail(`malformed id "${it.id}" (short_name ${it.short_name || '?'}) — ids must match BL-\\d+.`);
  }
  // A hot id that is ALSO in the cold store is the eviction's own failure mode
  // (archive_landed.js, 2026-09-01): the row was copied out and not removed, or
  // restored and not un-archived. Two rows for one id, in two files, with the query
  // layer preferring the hot one — so the cold copy silently rots. Not a duplicate
  // WITHIN the file, so the scan above cannot see it.
  // WHOLE ROWS only, not landedIds(): every landed item has a prose record from the
  // earlier archive_designs pass, and an item that is open again after being complete
  // legitimately keeps that prose. Only a duplicated ROW is the fault.
  const cold = new Set(A.landedItems(A.ROOT).map((i) => i.id));
  for (const it of backlog.items || []) {
    if (cold.has(it.id) && !A.CLOSED.has(it.status)) {
      fail(`${it.id} (${it.short_name || '?'}) is OPEN in backlog.json but also holds a whole row in the cold store — a restore that did not clear the cold copy. Re-run: node tools/session/archive_landed.js --restore, then re-evict.`);
    }
  }
}

// ---- index the backlog items by id ----
const items = new Map();
for (const it of backlog.items || []) items.set(it.id, it);

// Pull a BL-\d+ id from a requirements group (promoted_from is authoritative; title is the fallback).
function groupItemId(group) {
  for (const field of [group.promoted_from, group.title]) {
    if (typeof field === 'string') {
      const m = field.match(/BL-\d+/);
      if (m) return m[0];
    }
  }
  return null;
}

// ---- Invariant 1 & 2: item status vs its requirement group status ----
for (const group of reqs.groups || []) {
  const id = groupItemId(group);
  if (!id) continue;                       // group not tied to a backlog item (e.g. a harness phase)
  const item = items.get(id);
  const gStatus = group.status;
  const label = `req group "${group.title}" (${id})`;

  if (!item) {
    // A landed item is removed-then-retained under a terminal status by convention;
    // if it's simply gone AND the group is still open, that's worth flagging.
    if (gStatus === 'active') warn(`${label}: group active but ${id} is absent from backlog.json`);
    continue;
  }

  const terminal = TERMINAL.has(item.status);
  if (terminal && gStatus === 'active') {
    fail(`${label}: item is ${item.status} but the requirement group is still "active" — close it (DELIVERY step 5).`);
  }
  if (!terminal && gStatus === 'complete') {
    warn(`${label}: group is complete but item ${id} status is "${item.status}" — the item should be terminal too.`);
  }

  // ---- Invariant 6: pending rows under a complete/cancelled group ----
  if (gStatus === 'complete') {
    const pending = (group.rows || []).filter((r) => r.status === 'pending');
    if (pending.length) {
      warn(`${label}: group complete but ${pending.length} row(s) still pending (${pending.map((r) => r.id).join(', ')}) — back-fill or note them.`);
    }
  }
}

// ---- Invariant 3: landed item with a generic authority_doc ----
// ---- Invariant 4: @BACKLOG.md design pointer that resolves to nothing ----
for (const item of items.values()) {
  const terminal = TERMINAL.has(item.status);

  if (terminal && GENERIC_AUTHORITY.has(item.authority_doc) && !GENERIC_AUTHORITY_ALLOW.has(item.id)) {
    warn(`${item.id} (${item.status}): authority_doc is a process doc (${item.authority_doc}) — repoint to the subject's authority doc on landing.`);
  }

  if (item.design === '@BACKLOG.md') {
    // The pointer promises a body (or tombstone) in BACKLOG.md under this id. A
    // tombstone IS the intended terminal state per the drain policy, so its mere
    // presence is enough — only a pointer that resolves to nothing is a defect.
    const hasBody = backlogMd.includes(item.id);
    if (!hasBody) {
      fail(`${item.id}: design is "@BACKLOG.md" but no "${item.id}" body or tombstone exists in ${BACKLOG_MD} — dead pointer; inline the prose or tombstone.`);
    }
  }
}

// ---- Invariant 7: archive pointer integrity (the hot/cold split) ----
// A landed item's prose lives in the cold store (tools/session/archive_store.js) and
// the hot item keeps only an `archived` pointer. A pointer with no record behind it is
// data loss wearing a reference's clothes — the same defect class as a dead @BACKLOG.md
// pointer, and a hard FAIL for the same reason.
{
  const cache = new Map();
  for (const item of items.values()) {
    if (!item.archived) continue;
    if (!fs.existsSync(P(item.archived))) {
      fail(`${item.id}: archived → ${item.archived}, but that cold file does not exist.`);
      continue;
    }
    const back = A.resolve(item, ROOT, cache);
    if (back === item || !A.NARRATIVE.some((f) => back[f] != null)) {
      fail(`${item.id}: archived → ${item.archived}, but it holds no record for this id — dead pointer.`);
    }
    if (!A.TERMINAL.has(item.status)) {
      fail(`${item.id}: status "${item.status}" is not terminal but its prose is archived — restore it (archive_designs.js --restore ${item.id}).`);
    }
  }
}

// ---- Invariant 8: cold fraction — frozen history still sitting in the hot file ----
// backlog.json is read ACROSS items far more often than INTO one; every byte of a
// landed item's prose is paid for by every reader. Past a third of the file, the
// close-out step has been skipped often enough to matter.
{
  const { cold, total, fraction } = A.coldFraction(backlog);
  if (fraction > 0.30) {
    warn(`backlog.json is ${(fraction * 100).toFixed(0)}% frozen history (${(cold / 1024).toFixed(0)} KB of ${(total / 1024).toFixed(0)} KB is prose on landed items) — run tools/session/archive_designs.js.`);
  }
}

// ---- Invariant 5 (heuristic): REFINED.md sections past retention ----
// Policy: REFINED holds only in-flight tasks + at most one one-cycle-retained summary.
// A pile of "COMPLETE" section headers means the drain step was skipped. Heuristic
// only — the "one cycle" window needs human judgment, so this warns, never fails.
try {
  const refined = fs.readFileSync(P('docs/development/REFINED.md'), 'utf8');
  const completeSections = (refined.match(/^##\s+.*\bCOMPLETE\b/gim) || []).length;
  if (completeSections > 2) {
    warn(`REFINED.md carries ${completeSections} COMPLETE sections — policy retains ~1; drain the older ones (their record lives in DEVLOG/requirements).`);
  }
} catch { /* REFINED optional */ }

// ---------------------------------------------------------------------------
// Status drift, both directions (Ben's ruling, 2026-08-22, on NR-514)
// ---------------------------------------------------------------------------
// Two defects, one cause: an item's status disagreeing with the tree.
//
//   FALSE COMPLETE  the item says landed; the code was never written.
//                   BL-377 (the mercenary contract seam, the player's INCOME)
//                   sat like this for ten days after a sweep mistook it for a
//                   sibling. It hides owed work.
//   FALSE OPEN      the item says designed; the work merged long ago. BL-480
//                   was found this way. It hides landed work, which is how the
//                   same thing gets built twice.
//
// Neither is visible from any status view, because status is the thing that is
// wrong. The check is deliberately CONSERVATIVE in both directions — a lint
// that cries wolf gets ignored, and the earlier hand-sweep returned 48
// candidates that were mostly forward references in comments ("BL-315 will
// read this"). So:
//
//   * false-complete needs EVERY named source path to be absent. A rename
//     (src/app.cpp -> src/core/app.cpp) leaves some paths present and is not
//     reported here; those are stale paths, not drift.
//   * false-open needs a commit whose SUBJECT LINE names the id and which is an
//     ancestor of HEAD. Subject, not body, is what drops the forward references
//     — a commit that merely mentions an id in its body is not a landing.
//
// Both are WARNINGS, never fails: git may be unavailable (a fresh clone, a
// tarball), and a legitimately-open item can carry a subject-line commit from a
// partial slice. A warning that names the evidence is the right weight.

// Known-benign never-written paths, seeded 2026-08-22 when the check was added.
// An entry here is a CLAIM WITH A REASON, not a mute: the check is precise about
// what it measures ("this path has no creation commit on any branch") and the
// three reasons that fact can be innocent are all pre-history, so they can be
// enumerated once rather than guessed at every run.
//
//   RENAME BEFORE THE IMPORT — the repo's history begins with one squashed
//   commit, so a file renamed before it has no creation record under its old
//   name. src/app.cpp -> src/core/app.cpp, chart -> charts, globe_preview ->
//   generation_preview, economy_ledger -> balance_ledger, render_ux_questions
//   -> render_question_log.
//   A GAP THAT IS ALREADY RECORDED — src/world/serialisation.cpp has never
//   existed and NR-349 says so; the items that name it landed and skipped the
//   serialiser deliberately. BL-107 owns it.
//   A RETIRED SURFACE — ledger_history.cpp was retired by the item that names it.
//
// Anything NOT in this list is a path an item claims and nobody ever wrote,
// which is exactly the BL-377 defect. Add entries only with the reason.
const NEVER_WRITTEN_OK = new Set([
  'BL-008:src/app.cpp',                             // renamed -> src/core/app.cpp
  'BL-085:src/app.cpp',                             // renamed -> src/core/app.cpp
  'BL-248:src/ui/app.cpp',                          // renamed -> src/core/app.cpp
  'BL-197:src/ui/chart.hpp',                        // renamed -> src/ui/charts.hpp
  'BL-197:src/ui/chart.cpp',                        // renamed -> src/ui/charts.cpp
  'BL-198:src/ui/chart.cpp',                        // renamed -> src/ui/charts.cpp
  'BL-256:src/ui/globe_preview.cpp',                // became src/ui/generation_preview.cpp
  'BL-256:src/ui/globe_preview.hpp',                // became src/ui/generation_preview.hpp
  'BL-260:tools/session/render_ux_questions.js',    // renamed -> render_question_log.js
  'BL-281:src/ui/ledger_history.cpp',               // the view this item RETIRED
  'BL-343:src/ui/economy_ledger.cpp',               // renamed -> src/ui/balance_ledger.cpp
  'BL-398:src/world/corp_blackboard.cpp',           // blackboard folded into corp_ai
  'BL-263:src/world/serialisation.cpp',             // no serialiser exists at all — NR-349, BL-107
  'BL-517:src/world/serialisation.cpp',             // same
  'BL-448:src/world/serialization.cpp',             // same, US spelling
  'BL-594:scripts/verify/ancient_roster.lua',       // verified by a LIVE playthrough (computer-use
                                                     // against the built exe) instead — a scripted
                                                     // capture proves a surface renders, not that a
                                                     // press on it is reachable, and this item's own
                                                     // design names that distinction explicitly
]);

function checkStatusDrift() {
  const { execFileSync } = require('child_process');
  const git = (args) => {
    try {
      return execFileSync('git', args, { cwd: ROOT, encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] });
    } catch (e) {
      return null;
    }
  };
  if (git(['rev-parse', '--git-dir']) === null) return; // no git here; skip silently

  // Expand "src/world/foo.{hpp,cpp}" and strip a trailing "(new)" annotation.
  const expand = (f) => {
    const clean = String(f).replace(/\s*\([^)]*\)\s*$/, '').trim();
    const m = clean.match(/^(.*)\{([^}]*)\}(.*)$/);
    return m ? m[2].split(',').map((x) => m[1] + x.trim() + m[3]) : [clean];
  };
  const isSource = (f) => /^(src|scripts|tools)\//.test(f);

  // A LANDING COMMIT, not merely a commit that mentions an id. Measured against
  // this repo's actual history: the shapes that mean "this item landed" are
  //   "BL-480: ..."            an item commit          (57 instances)
  //   "Merge BL-480 (...)"     a lane/worktree merge   (17)
  // and the shapes that name an id while meaning the OPPOSITE are filings and
  // lifecycle moves — "File BL-521 ...", "Close BL-377 ...", "Pause BL-435 ...".
  // Anything that mentions an id mid-subject ("... (BL-467)") is prose about the
  // work, not a landing, and is deliberately not matched: that looseness is what
  // made the first hand-sweep return 48 candidates, most of them noise.
  const NOT_A_LANDING = /^(file|close|reopen|pause|park|defer|split|rescope|retire|revert)\b/i;
  let subjects = null;
  const subjectLanded = (id) => {
    if (subjects === null) {
      const out = git(['log', '--format=%s', 'HEAD']);
      subjects = out === null ? [] : out.split('\n');
    }
    const re = new RegExp('^(merge\\s+)?' + id + '\\b', 'i');
    return subjects.some((sub) => re.test(sub) && !NOT_A_LANDING.test(sub));
  };

  for (const item of backlog.items) {
    // A SUPERSEDED item is terminal without ever having been built, so neither
    // arm applies: it never intended to write the files it names, and no commit
    // ever landed it. `superseded_by` is the marker (introduced 2026-08-22 with
    // BL-158 and BL-345). Its referential integrity is checked separately below,
    // over every item rather than only those naming source paths — the bug that
    // check's own negative test caught on its first run.
    if (Array.isArray(item.superseded_by) && item.superseded_by.length) continue;

    // A CANCELLED item is the same shape as a superseded one and is exempt for the
    // same reason: it is closed without ever having been built, so the files it
    // names were never going to be written and their absence is the expected
    // outcome, not a false-complete. Without this the 2026-09-01 cancellation pass
    // turned every unbuilt item into a warning — the check firing hardest exactly
    // where it has nothing to say.
    if (A.CANCELLED.has(item.status)) continue;

    const named = (item.files || []).filter((f) => typeof f === 'string')
      .flatMap(expand).filter(isSource);
    if (!named.length) continue;

    if (TERMINAL.has(item.status)) {
      // The precise tell, taken from how BL-377 was actually found: a named
      // path that has NO CREATION COMMIT ANYWHERE IN HISTORY — nobody ever
      // wrote that file, on any branch. That is what separates a never-written
      // file from a RENAME (src/app.cpp -> src/core/app.cpp), where the old
      // path is absent today but has a creation commit behind it. Thirteen of
      // the fifteen absent paths in this backlog are renames; only the real
      // defect has no creation.
      //
      // An earlier draft warned only when EVERY named path was absent. That
      // draft could not have caught BL-377, which named ten files of which
      // seven existed — the mutation test that proved so is why this arm reads
      // per-path instead.
      const neverWritten = named.filter((f) => {
        if (fs.existsSync(P(f))) return false;
        const created = git(['log', '--all', '--diff-filter=A', '--format=%H', '--', f]);
        return created !== null && created.trim() === '';
      });
      // Second piece of evidence, and it is what makes the arm usable. This
      // repo's history begins with one squashed import, so a file renamed
      // BEFORE that import also has no creation commit — "never written" alone
      // fires on fifteen items, thirteen of them ancient renames.
      //
      // The pair that actually identified BL-377 was: a named path nobody ever
      // wrote, AND NOT ONE REFERENCE TO THE ITEM'S ID anywhere in src/, tools/
      // or scripts/. Landed work cites its own id in the code it landed —
      // BL-343 is all over law.cpp, BL-448 all over stance.hpp. Work that was
      // never written cites nothing, because there is nothing to cite.
      const flagged = neverWritten.filter((f) => !NEVER_WRITTEN_OK.has(`${item.id}:${f}`));
      if (flagged.length) {
        warn(`${item.id} (${item.short_name}) is "${item.status}" but named path(s) nobody ever wrote on any branch — possible FALSE COMPLETE, the BL-377 shape: ${flagged.join(', ')}. If this is a rename or a known gap, add it to NEVER_WRITTEN_OK with the reason.`);
      }
    } else if (!item.parked) {
      if (subjectLanded(item.id)) {
        warn(`${item.id} (${item.short_name}) is "${item.status}" but a commit whose SUBJECT names it is an ancestor of HEAD — possible FALSE OPEN, the BL-480 shape. Verify against the code before flipping it — a partial slice can land under an item that is legitimately still open.`);
      }
    }
  }
}
checkStatusDrift();

// A retired item must name a REAL destination. Retiring an item whose strands go
// to an id that does not exist is how a design gets lost while looking filed.
function checkSupersessions() {
  const ids = new Set(backlog.items.map((i) => i.id));
  for (const item of backlog.items) {
    if (!Array.isArray(item.superseded_by) || !item.superseded_by.length) continue;
    if (!TERMINAL.has(item.status)) {
      fail(`${item.id} (${item.short_name}) has superseded_by but status "${item.status}" — a superseded item is terminal.`);
    }
    const missing = item.superseded_by.filter((id) => !ids.has(id));
    if (missing.length) {
      fail(`${item.id} (${item.short_name}) is superseded_by ${missing.join(', ')}, which do(es) not exist — a retired item must name a real destination.`);
    }
    if (!item.resolution) {
      warn(`${item.id} (${item.short_name}) is superseded with no resolution — record WHAT moved WHERE, or the reasoning is lost.`);
    }
  }
}
checkSupersessions();

// An item cannot be scheduled into a minor its own blockers land AFTER. A minor
// cannot cut while part of it waits on a later one, so a version goal that sits
// earlier than a blocker's is a scheduling claim the dependency graph
// contradicts.
//
// Found 2026-08-22 by the hotspot audit, which caught four v0.1.16 items waiting
// on v0.1.19 and v0.1.22 blockers — a quarter of a minor's perf chain. The sweep
// that followed found two more, one of them created the same session by a ruling
// that added a `requires` without moving the version goal to match. That is the
// shape this check exists for: the inversion arrives as a SIDE EFFECT of an
// otherwise-correct edit, and nothing else notices.
//
// WARNING, not a fail: a parked blocker is a legitimate reason to sit inverted
// (the item is waiting on a decision, not a build), and a version goal is a plan
// rather than a contract.
function checkVersionInversion() {
  const ORD = (v) => {
    const m = String(v || '').match(/^v(\d+)\.(\d+)\.(\d+)$/);
    return m ? (+m[1] * 10000 + +m[2] * 100 + +m[3]) : null;
  };
  // Union the cold store: most `requires` targets are LANDED, and since
  // archive_landed.js evicted their rows a hot-only index resolves them to undefined
  // — which this loop treats as "no dependency" and skips. The check would have gone
  // quietly vacuous rather than failing, which is the worse of the two.
  const byId = new Map(A.allItems(backlog, A.ROOT).map((i) => [i.id, i]));
  for (const item of backlog.items) {
    if (TERMINAL.has(item.status) || item.parked) continue;
    const mine = ORD(item.version_goal);
    if (mine === null) continue;
    for (const r of item.requires || []) {
      const dep = byId.get(r);
      if (!dep || TERMINAL.has(dep.status)) continue;
      const theirs = ORD(dep.version_goal);
      if (theirs === null || theirs <= mine) continue;
      const parked = dep.parked ? ' (blocker is PARKED — may be legitimate)' : '';
      warn(`${item.id} (${item.short_name}) targets ${item.version_goal} but requires ${r}, which targets ${dep.version_goal}${parked} — a minor cannot cut while part of it waits on a later one.`);
    }
  }
}
checkVersionInversion();

// ---------------------------------------------------------------------------
// STRUCTURAL INTEGRITY OF THE JSON STORES — the defect no view can show
// ---------------------------------------------------------------------------
// Found 2026-08-23, and it had been live since 2026-08-22: `requirements.json`
// held TWO group objects fused into one, because an append left out the `},{`
// between them. That is still VALID JSON — the second object's keys simply land
// inside the first, and JSON's last-key-wins rule silently discards whichever
// they collide with. The `value-anchor` group (BL-543, four rows) vanished
// entirely: `requirements_query.js value-anchor` answered "nothing matched" and
// every consumer agreed with it, because by the time anything looked, the parse
// had already thrown the group away.
//
// WHAT MAKES IT WORTH A PERMANENT CHECK is what happened next. The store is read
// by parse-and-rewrite scripts, so the first tool to load and re-save the file
// after the fusion made the loss PERMANENT IN THE TEXT — the group had still
// been recoverable from the raw bytes right up to that moment, and then was not.
// A corruption that erases itself on the next write is exactly the class that
// has to be caught by machinery rather than by reading.
//
// Two cheap invariants catch it, and both are checked over the RAW TEXT rather
// than the parsed object, because the parse is the thing under suspicion:
//   1. every `"brief":` / `"id":` key at record depth must survive the parse —
//      a count mismatch IS a fusion;
//   2. a brief and an id must be unique — a duplicate makes every query
//      ambiguous, and `requirements_query.js <brief>` silently answers for one
//      of two different pieces of work.
function checkStoreIntegrity() {
  const stores = [
    { rel: REQUIREMENTS, parsed: reqs, coll: 'groups', key: 'brief', label: 'requirement group' },
    { rel: BACKLOG_JSON, parsed: backlog, coll: 'items', key: 'id', label: 'backlog item' },
  ];

  for (const st of stores) {
    let raw = '';
    try { raw = fs.readFileSync(P(st.rel), 'utf8'); } catch { continue; }
    const records = st.parsed && Array.isArray(st.parsed[st.coll]) ? st.parsed[st.coll] : null;
    if (!records) continue;

    // Count the key at RECORD DEPTH only, with the depth CALIBRATED FROM THE
    // FILE rather than hardcoded: take the indentation of the first occurrence
    // and count every occurrence at exactly that indentation. A nested `"id":`
    // (a requirement row carries one) is deeper and does not match; one inside a
    // prose string is not at line start and does not match either. Calibrating
    // matters because the two stores have been written by different tools at
    // different indent widths, and an anchored pattern goes silently INERT the
    // first time one of them is reformatted — which is the failure mode this
    // whole check exists to not have.
    const first = raw.match(new RegExp(`^([ \t]*)"${st.key}":`, 'm'));
    const seen = first
      ? (raw.match(new RegExp(`^${first[1]}"${st.key}":`, 'gm')) || []).length
      : 0;

    if (seen === 0) {
      warn(`${st.rel}: found no "${st.key}" key at line start — the file's shape has changed, so the fusion check is INERT. Re-check checkStoreIntegrity against the new layout.`);
    } else if (seen !== records.length) {
      fail(`${st.rel}: the raw text carries ${seen} "${st.key}" keys but only ${records.length} ${st.label}(s) survive the parse — ${seen - records.length} record(s) are FUSED into a neighbour by a missing "},{" and are being silently discarded by last-key-wins. Do NOT let a tool re-save this file before repairing it: a parse-and-rewrite makes the loss permanent. Find the join with: grep -n '"${st.key}":' ${st.rel} and compare against the parsed order.`);
    }

    const keys = records.map((r) => r && r[st.key]).filter(Boolean);
    const dupes = [...new Set(keys.filter((k, i) => keys.indexOf(k) !== i))];
    for (const dup of dupes) {
      fail(`${st.rel}: ${st.label} "${dup}" appears ${keys.filter((k) => k === dup).length}× — a duplicate ${st.key} makes every query for it ambiguous. Rename the older one or merge them.`);
    }
  }
}
checkStoreIntegrity();

report();

function report() {
  const line = (m) => `  - ${m}`;
  if (fails.length) {
    console.error(`\nbacklog_lint: ${fails.length} FAIL(s)`);
    fails.forEach((m) => console.error(line(m)));
  }
  if (warns.length) {
    console.error(`\nbacklog_lint: ${warns.length} warning(s)`);
    warns.forEach((m) => console.error(line(m)));
  }
  if (!fails.length && !warns.length) {
    console.log('backlog_lint: clean — backlog.json, requirements.json, BACKLOG.md and REFINED.md agree.');
  } else {
    console.error(`\nbacklog_lint: ${fails.length} fail(s), ${warns.length} warning(s).`);
  }
  process.exit(fails.length ? 1 : 0);
}
