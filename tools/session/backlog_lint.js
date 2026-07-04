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

const ROOT = path.resolve(__dirname, '..', '..');
const P = (rel) => path.join(ROOT, rel);
const BACKLOG_JSON = 'docs/development/backlog.json';
const REQUIREMENTS = 'docs/development/req/requirements.json';
const BACKLOG_MD = 'docs/development/BACKLOG.md';

// Terminal statuses — an item in one of these has landed; its bookkeeping must be closed.
const TERMINAL = new Set(['complete', 'shipped']);
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
