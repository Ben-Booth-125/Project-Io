#!/usr/bin/env node
// begin_adopts_check.js -- BL-1073 (Begin adopts the wizard's world), checked.
//
//   node tools/verify/begin_adopts_check.js [path/to/ProjectIo.exe] [--epoch <year>]
//
// Runs the app twice, headlessly, from the exe's own directory (it reads
// scripts/*.lua relative to it), on the same default params:
//
//   --autostart-adopt   the wizard's last round builds the world on its own
//                       worker, an invalidation is checked to drop the cache,
//                       then Begin ADOPTS the held world and the tail (prelude,
//                       validation run, seat) runs to play;
//   --autostart         Begin builds the world COLD, as it did before the cache.
//
// PASS when: the adopt run cached the world, both drop checks passed, Begin
// printed `[begin] adopted` and the adopt run never printed `[gen budget]`
// (the line poll_worldgen prints after Begin's own build -- so its absence is
// the log proof that Begin never called make_hard_coded_world), the cold run
// DID print it, and the two opening state hashes are EQUAL.
//
// Default exe: build_rel/ProjectIo.exe (Release; a Debug world build is many
// minutes), falling back to build/ProjectIo.exe.

'use strict';
const { spawnSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..', '..');
const args = process.argv.slice(2);
let exe = null;
const extra = [];
for (let i = 0; i < args.length; ++i) {
  if (args[i] === '--epoch' && i + 1 < args.length) { extra.push('--epoch', args[++i]); }
  else exe = path.resolve(args[i]);
}
if (!exe) {
  for (const c of ['build_rel/ProjectIo.exe', 'build/ProjectIo.exe']) {
    const p = path.join(root, c);
    if (fs.existsSync(p)) { exe = p; break; }
  }
}
if (!exe || !fs.existsSync(exe)) {
  console.error('begin_adopts_check: no ProjectIo.exe found (build it, or pass its path)');
  process.exit(2);
}

function run(flag) {
  const t0 = Date.now();
  const r = spawnSync(exe, [flag, ...extra], {
    cwd: path.dirname(exe), encoding: 'utf8', maxBuffer: 256 * 1024 * 1024,
    timeout: 60 * 60 * 1000,
  });
  const out = (r.stdout || '') + (r.stderr || '');
  const m = out.match(/state_hash=([0-9A-F]{16})/);
  return { flag, rc: r.status, out, hash: m ? m[1] : null, secs: (Date.now() - t0) / 1000 };
}

const adopt = run('--autostart-adopt');
const cold  = run('--autostart');

const checks = [
  ['adopt run exited 0', adopt.rc === 0],
  ['round 6 cached its world', /\[autostart-adopt\] CACHED/.test(adopt.out)],
  ['a reroll above round 6 drops the cache', /PASS  a reroll above round 6 drops the cache/.test(adopt.out)],
  ['a planetology move drops the cache', /PASS  a planetology move drops the cache/.test(adopt.out)],
  ['Begin adopted (logged)', /\[begin\] adopted the wizard's world/.test(adopt.out)],
  ['Begin never built: no [gen budget] in the adopt run', !/\[gen budget\]/.test(adopt.out)],
  ['cold run exited 0', cold.rc === 0],
  ['cold run built (logged)', /\[begin\] no wizard world to adopt/.test(cold.out) && /\[gen budget\]/.test(cold.out)],
  ['both runs printed a state hash', !!adopt.hash && !!cold.hash],
  ['adopted == cold state hash', !!adopt.hash && adopt.hash === cold.hash],
];

console.log(`begin_adopts_check: ${exe}`);
console.log(`  adopt: ${adopt.hash || '-'}  (${adopt.secs.toFixed(0)} s)`);
console.log(`  cold : ${cold.hash || '-'}  (${cold.secs.toFixed(0)} s)`);
let failed = 0;
for (const [name, ok] of checks) {
  console.log(`  ${ok ? 'PASS' : 'FAIL'}  ${name}`);
  if (!ok) ++failed;
}
if (failed) {
  const tail = (s) => s.split(/\r?\n/).filter((l) => /autostart|begin|wizard world|FAIL|rror/.test(l)).slice(-20).join('\n');
  console.log('--- adopt run (filtered) ---\n' + tail(adopt.out));
  console.log('--- cold run (filtered) ---\n' + tail(cold.out));
}
process.exit(failed ? 1 : 0);
