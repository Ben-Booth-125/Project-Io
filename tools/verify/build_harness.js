#!/usr/bin/env node
// build_harness.js — build ONE tools/verify/<name>.cpp against the world superset,
// with no CMake configure and no network.
//
//   node tools/verify/build_harness.js <name> [--run] [--debug] [--jobs N]
//
// WHY THIS EXISTS (NR-392, Ben's ruling 2026-08-23). A fresh worktree has no
// configured build tree, and `cmake -B build` pulls SDL3, Lua, sol2 and ImGui over
// FetchContent — refused outright by some session network policies (NR-240, NR-313),
// and not worth paying for even when it works if all you want is one headless check.
// This replaces two ad-hoc recipes that said the same thing in two dialects: the
// agent-authored build_gen_harness.bat, and the Linux lib-then-link recipe NR-264
// established. One script, both toolchains, committed rather than rediscovered.
//
// THE SOURCE SET CANNOT DRIFT. It is every src/world/*.cpp MINUS the four sol2/Lua
// translation units, which is exactly how CMakeLists builds io_world_obj. A glob
// rather than a list, for the reason CMakeLists gives in its own comment: the
// hand-picked list rotted twice, and the second time it failed to link.
//
// WHAT IT CANNOT BUILD. A harness needing a live Lua state — pregame_balance_harness,
// persona_counsel_harness, and any harness loading scripts/*.lua through sol2 — is
// refused by name, with the reason. That set is the standing gap NR-558 records.
// Anything touching SDL, ImGui or src/ui is out of scope by construction.
//
// Output: build_gen/verify/<name>[.exe], the one path-scoped build target the
// permission rules allow. %TEMP% is never a target (an unsigned exe there is
// indistinguishable from a dropper).
'use strict';
const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');
const os = require('os');

const ROOT = path.resolve(__dirname, '..', '..');
const WORLD = path.join(ROOT, 'src', 'world');

// The five sol2/Lua TUs io_world_obj excludes. Mirrored from CMakeLists
// (line ~474: the IO_WORLD_SOURCES list(FILTER ... EXCLUDE) regex — keep the
// two in lockstep). contract_template joined the exclusion with BL-570 and
// this mirror missed it until 2026-08-25, which is exactly the rot the header
// comment warns about.
const LUA_TUS = new Set(['recipe_registry', 'works_registry', 'tech_tree', 'world_gen_config',
                         'contract_template']);

// Harnesses that genuinely need a live Lua state, and so cannot be built here.
// CMakeLists declares each of these explicitly with lua54 linked; see its comments.
const NEEDS_LUA = new Map([
  ['pregame_balance_harness', 'constructs a lua_state and calls recipe_registry::load_from_lua'],
  ['persona_counsel_harness', 'loads scripts/personas/*.lua through src/scripting/persona_pack.cpp'],
]);

function die(msg, code = 1) { console.error('build_harness: ' + msg); process.exit(code); }

const argv = process.argv.slice(2);
const flags = new Set(argv.filter(a => a.startsWith('--')));
const jobsArg = argv.findIndex(a => a === '--jobs');
const jobs = jobsArg >= 0 ? parseInt(argv[jobsArg + 1], 10) : Math.max(1, os.cpus().length - 1);
const positional = argv.filter((a, i) => !a.startsWith('--') && !(jobsArg >= 0 && i === jobsArg + 1));
const name = positional[0];

if (!name) die('usage: node tools/verify/build_harness.js <name> [--run] [--debug] [--jobs N]');
if (!/^[A-Za-z0-9_]+$/.test(name)) die(`"${name}" is not a harness name (letters, digits and underscore only)`);

const src = path.join(ROOT, 'tools', 'verify', name + '.cpp');
if (!fs.existsSync(src)) die(`tools/verify/${name}.cpp does not exist`);
if (NEEDS_LUA.has(name)) {
  die(`${name} needs a live Lua state — it ${NEEDS_LUA.get(name)}.\n` +
      '  Build it through CMake instead. See NR-558 for why the headless path cannot reach it.');
}

const sources = fs.readdirSync(WORLD)
  .filter(f => f.endsWith('.cpp'))
  .filter(f => !LUA_TUS.has(path.basename(f, '.cpp')))
  .sort()                                   // deterministic link order
  .map(f => path.join(WORLD, f));

const isWindows = process.platform === 'win32';
const outDir = path.join(ROOT, 'build_gen', 'verify');
// The object dir carries a suffix deliberately: on Linux the executable has no
// extension, so a bare <name> directory would collide with the binary itself.
const objDir = path.join(outDir, name + '.obj');
fs.mkdirSync(isWindows ? objDir : outDir, { recursive: true });

const exe = path.join(outDir, name + (isWindows ? '.exe' : ''));
// A stale binary must never be mistaken for a fresh one if the compile fails.
if (fs.existsSync(exe) && fs.statSync(exe).isFile()) fs.unlinkSync(exe);

const debug = flags.has('--debug');
let cmd, args;

if (isWindows) {
  // cl is only on PATH inside a Developer Prompt; vcvars64 puts it there.
  const vcvars = 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat';
  if (!fs.existsSync(vcvars) && !spawnSync('where', ['cl'], { shell: true }).status === 0)
    die('cl not found and vcvars64.bat is not at its default path — open a Developer Prompt, or build through CMake');
  const cl = ['cl', '/nologo', '/std:c++20', '/EHsc', '/MP',
    debug ? '/Od /Zi' : '/O2', '/I', 'src', '/I', 'tools\\verify',
    JSON.stringify(path.relative(ROOT, src)),
    ...sources.map(s => JSON.stringify(path.relative(ROOT, s))),
    `/Fo:${path.relative(ROOT, objDir)}\\`, `/Fe:${path.relative(ROOT, exe)}`].join(' ');
  cmd = 'cmd'; args = ['/c', `call "${vcvars}" >nul 2>&1 && ${cl}`];
} else {
  cmd = 'g++';
  args = ['-std=c++20', debug ? '-O0' : '-O2', '-g',
    '-I', path.join(ROOT, 'src'), '-I', path.join(ROOT, 'tools', 'verify'),
    ...(debug ? ['-fsanitize=address,undefined'] : []),
    src, ...sources, '-o', exe];
  void jobs; // g++ compiles the TU set in one invocation; --jobs is the MSVC /MP knob
}

const label = isWindows ? 'cl' : 'g++';
console.log(`build_harness: ${name} <- ${sources.length} world TUs (${label}, ${debug ? 'debug+asan' : 'release'})`);
const t0 = Date.now();
const r = spawnSync(cmd, args, { cwd: ROOT, stdio: 'inherit', shell: isWindows });
const secs = ((Date.now() - t0) / 1000).toFixed(1);

if (r.status !== 0 || !fs.existsSync(exe)) {
  console.error(`build_harness: COMPILE_FAILED (${secs}s)`);
  process.exit(1);
}
console.log(`build_harness: COMPILE_OK  ${path.relative(ROOT, exe)}  (${secs}s)`);

if (flags.has('--run')) {
  // Run from the repo root: a script-rooted harness resolves scripts/ relative to cwd.
  const run = spawnSync(exe, [], { cwd: ROOT, stdio: 'inherit' });
  process.exit(run.status === null ? 1 : run.status);
}
