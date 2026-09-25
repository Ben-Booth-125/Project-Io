#!/usr/bin/env node
// seat_pick_check.js -- BL-1076 (the player chooses the corporation they play),
// checked on the REAL interactive tail: (seed, pick) reproduces the seat.
//
//   node tools/verify/seat_pick_check.js [path/to/ProjectIo.exe] [--epoch <year>]
//
// Runs the app headlessly, from the exe's own directory, on the default params:
//
//   1. --autostart               the tail DRAWS (no player to ask) and says so;
//                                its seat X and state hash H are read off the log;
//   2. --autostart --seat X      the same firm, PICKED through corp_verb::take_seat:
//                                must open on H exactly -- a pick of the drawn firm
//                                is the drawn seat, nothing else moves;
//   3. --autostart --seat Y      a different ranked firm, twice: both runs open on
//                                the same hash (reproducible from (seed, pick)),
//                                both seated on Y -- and that hash is NOT the
//                                draw's: world::state_hash folds the seat
//                                (BL-1082), so a different pick is a different
//                                hash, asserted from the hash alone;
//   4. --autostart --seat 1      an id that names no ranked specialist: the run
//                                FAILS and seats nobody (never a silent fall-back
//                                to the draw).
//
// Default exe: build_rel/ProjectIo.exe (Release; a Debug world build is many
// minutes), falling back to build/ProjectIo.exe. Five full world builds.

'use strict';
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..', '..');
const args = process.argv.slice(2);
let exe = null;
const extra = [];
for (let i = 0; i < args.length; ++i) {
  if (args[i] === '--epoch' && i + 1 < args.length) extra.push('--epoch', args[++i]);
  else exe = path.resolve(args[i]);
}
if (!exe) {
  for (const c of ['build_rel/ProjectIo.exe', 'build/ProjectIo.exe']) {
    const p = path.join(root, c);
    if (fs.existsSync(p)) { exe = p; break; }
  }
}
if (!exe || !fs.existsSync(exe)) {
  console.error('seat_pick_check: no ProjectIo.exe found (build it, or pass its path)');
  process.exit(2);
}

function run(more) {
  const t0 = Date.now();
  const r = spawnSync(exe, ['--autostart', ...extra, ...more], {
    cwd: path.dirname(exe), encoding: 'utf8', maxBuffer: 256 * 1024 * 1024,
    timeout: 60 * 60 * 1000,
  });
  const out = (r.stdout || '') + (r.stderr || '');
  const hash = (out.match(/\[autostart\] state_hash=([0-9A-F]{16})/) || [])[1] || null;
  const drawn = (out.match(/\[seat\] drawn (\d+)/) || [])[1] || null;
  const picked = (out.match(/\[seat\] picked (\d+)/) || [])[1] || null;
  return { rc: r.status, out, hash, drawn, picked, secs: (Date.now() - t0) / 1000 };
}

const draw = run([]);
// A second ranked firm, read off the tail's own `[seat] ranked <id>` lines (the
// canvas's order, printed on every seat) -- the last-ranked one, so the check
// also picks a firm the floor may have MARKED.
const ids = [];
for (const m of draw.out.matchAll(/\[seat\] ranked (\d+)/g)) ids.push(m[1]);
const other = ids.slice().reverse().find((id) => id !== draw.drawn) || null;

const same = draw.drawn ? run(['--seat', draw.drawn]) : null;
let y1 = null, y2 = null;
if (other) {
  y1 = run(['--seat', other]);
  y2 = run(['--seat', other]);
}
const bad = run(['--seat', '1']);

const checks = [
  ['draw run exited 0', draw.rc === 0],
  ['draw run says it drew, and why', !!draw.drawn && /\[seat\] drawn \d+ .+no player to ask/.test(draw.out)],
  ['draw run printed a state hash', !!draw.hash],
  ['the ranking is printed (another firm to pick)', !!other],
  ['pick of the drawn firm exited 0', !!same && same.rc === 0],
  ['pick of the drawn firm went through take_seat', !!same && same.picked === draw.drawn],
  ['pick of the drawn firm == the drawn state hash', !!same && same.hash === draw.hash],
  ['a different pick seats that firm (run 1)', !!y1 && y1.rc === 0 && y1.picked === other],
  ['a different pick seats that firm (run 2)', !!y2 && y2.rc === 0 && y2.picked === other],
  ['(seed, pick) reproduces the seat: run 1 hash == run 2 hash', !!y1 && !!y2 && !!y1.hash && y1.hash === y2.hash],
  // BL-1082: world::state_hash folds is_player / player_entity, so a different
  // pick IS a different hash. Asserted from the hash alone; the `[seat] picked`
  // id printed beside it above is a diagnostic, not the assertion.
  ['a different pick is a different seat (from the state hash alone)',
   !!y1 && !!y1.hash && !!draw.hash && y1.hash !== draw.hash],
  ['a pick naming no ranked firm FAILS the run', bad.rc !== 0 && /--seat 1 REJECTED/.test(bad.out)],
  ['a refused pick seats nobody', !/\[seat\] (drawn|picked)/.test(bad.out)],
];

console.log(`seat_pick_check: ${exe}`);
console.log(`  draw : seat ${draw.drawn || '-'}  ${draw.hash || '-'}  (${draw.secs.toFixed(0)} s)`);
if (same) console.log(`  pick X: seat ${same.picked || '-'}  ${same.hash || '-'}  (${same.secs.toFixed(0)} s)`);
if (y1) console.log(`  pick Y: seat ${y1.picked || '-'}  ${y1.hash || '-'}  (${y1.secs.toFixed(0)} s)`);
if (y2) console.log(`  pick Y: seat ${y2.picked || '-'}  ${y2.hash || '-'}  (${y2.secs.toFixed(0)} s)`);
let failed = 0;
for (const [name, ok] of checks) {
  console.log(`  ${ok ? 'PASS' : 'FAIL'}  ${name}`);
  if (!ok) ++failed;
}
if (failed) {
  const tail = (s) => s.split(/\r?\n/).filter((l) => /autostart|seat|FAIL|rror/.test(l)).slice(-20).join('\n');
  console.log('--- draw run (filtered) ---\n' + tail(draw.out));
  console.log('--- refused run (filtered) ---\n' + tail(bad.out));
}
process.exit(failed ? 1 : 0);
