#!/usr/bin/env node
// build_harness.js — build ONE tools/verify/<name>.cpp against the world superset,
// with no CMake configure and no network.
//
//   node tools/verify/build_harness.js <name> [--run] [--debug] [--jobs N] [--clean]
//
// THE WORLD SET IS CACHED (BL-960): the 66 src/world TUs compile once per
// configuration into build_gen/verify/_world/<release|debug>/ and every harness
// links the cached objects. A TU recompiles when its .cpp or any header under
// src/ is newer than its object, or when the flags change; --clean drops the
// cache. See the comment at the cache for why objects are linked directly
// rather than archived.
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

// THE DEPENDENCY CACHE IS NOT ALWAYS UNDER ROOT, and assuming it was made this
// builder unusable in exactly the place it is needed most. A git WORKTREE - which
// is how every sub-agent runs - has no _deps_cache of its own, so the sol2 and
// Lua include paths below pointed at nothing and every build died on
// `Cannot open include file: 'sol/sol.hpp'`. That reads precisely like the
// wrong-builder symptom this file refuses harnesses to prevent, except here the
// builder is right and the headers are simply absent. Reported by a worktree
// agent, 2026-09-06.
//
// Resolution order mirrors build_lua_harness.sh: an explicit IO_DEPS_CACHE, then
// this checkout's own, then the MAIN checkout's - which a worktree finds through
// git's common dir. CMakeLists honours the same env override.
function resolveDepsCache() {
  if (process.env.IO_DEPS_CACHE) return process.env.IO_DEPS_CACHE;
  const local = path.join(ROOT, '_deps_cache');
  if (fs.existsSync(local)) return local;
  try {
    const r = spawnSync('git', ['rev-parse', '--path-format=absolute', '--git-common-dir'],
                        { cwd: ROOT, encoding: 'utf8' });
    if (r.status === 0) {
      const main = path.dirname(r.stdout.trim());
      const shared = path.join(main, '_deps_cache');
      if (fs.existsSync(shared)) return shared;
    }
  } catch { /* fall through to the local path and let the compiler say so */ }
  return local;
}
const DEPS = resolveDepsCache();

/// Quote one argument for the cl command line.
///
/// NOT JSON.stringify, which is the trap this replaced. JSON escaping DOUBLES
/// backslashes, so an absolute Windows path came out as
/// `"C:\\Users\\benbo\\..."` and named a directory that does not exist. It
/// survived unnoticed for the source-file arguments only because cl tolerates it
/// there; on an `/I` include path it fails outright, and it fails as C1083 on
/// sol/sol.hpp — indistinguishable from the wrong-builder symptom this file
/// exists to diagnose. Shell quoting is not string escaping.
function q(p) { return '"' + p + '"'; }
const WORLD = path.join(ROOT, 'src', 'world');

// The five sol2/Lua TUs io_world_obj excludes. Mirrored from CMakeLists
// (line ~474: the IO_WORLD_SOURCES list(FILTER ... EXCLUDE) regex — keep the
// two in lockstep). contract_template joined the exclusion with BL-570 and
// this mirror missed it until 2026-08-25, which is exactly the rot the header
// comment warns about.
const LUA_TUS = new Set(['recipe_registry', 'works_registry', 'tech_tree', 'world_gen_config']);

// WHICH HARNESSES NEED THE LUA BUILDER IS DERIVED, NOT LISTED (BL-774, closing
// NR-767). This used to be a hand-written four-entry map. The real set is 23 —
// measured, not estimated — so the list was wrong by a factor of five and every
// harness it missed failed on sol/sol.hpp instead of being refused with a reason.
// That error reads exactly like broken code, which is why it kept costing time.
//
// THE PREDICATE IS A LINK QUESTION, NOT AN INCLUDE ONE, and getting that
// backwards is the obvious mistake — it was made and measured before this was
// written. Reaching scripting/lua_state.hpp through the include graph proves
// nothing, because the sol2 and Lua headers are ON the include path above (a
// src/world TU pulls them in for every harness). 54 of 138 harnesses reach that
// header; only ~20 fail to build. world_determinism reaches it and links fine.
//
// What actually fails is the LINK: this builder omits the four sol2/Lua TUs, so
// a harness dies with LNK2019 iff it references a symbol DEFINED in one of them
// — in practice recipe_registry::load_from_lua and friends, reached by naming
// `load_from_lua`, or lua_state itself (defined in src/scripting/lua_state.cpp,
// which is likewise not compiled here).
//
// So the test is: does the harness's own translation unit REFERENCE either
// symbol in live code? Comments do not count — works_roster_harness mentions
// load_from_lua in a comment, links perfectly well, and was misrouted by a naive
// grep. Verified against real link probes in both directions (2026-09-06).
// load_from_lua / lua_state cover the recipe_registry-and-friends route;
// persona_pack is the third sol2 TU (src/scripting/persona_pack.cpp), which
// persona_counsel_harness reaches without ever naming a lua_state.
const LUA_SYMBOLS = /\b(load_from_lua|lua_state|persona_pack)\b/;

/// Source with comments blanked, so a symbol named only in prose does not read
/// as a reference.
///
/// SINGLE PASS, NOT TWO REGEXES. The obvious two-substitution version (block
/// comments, then line comments) is wrong on this codebase and silently so:
/// pregame_balance_harness has a `//` line that quotes a `/*`, so the block pass
/// matched that opener and blanked ~100 lines of real code — including the very
/// `lua_state lua;` this is looking for. It read as clean and routed a
/// definitely-Lua harness to the wrong builder. String literals are skipped for
/// the same class of reason: a "//" inside one starts no comment.
function stripComments(text) {
  let out = '';
  for (let i = 0; i < text.length; i++) {
    const c = text[i], d = text[i + 1];
    if (c === '/' && d === '*') {
      const end = text.indexOf('*/', i + 2);
      out += ' ';
      i = end < 0 ? text.length : end + 1;
    } else if (c === '/' && d === '/') {
      const end = text.indexOf('\n', i);
      out += ' ';
      i = end < 0 ? text.length : end - 1;
    } else if (c === '"') {
      out += ' ';
      for (i++; i < text.length; i++) {
        if (text[i] === '\\') { i++; continue; }
        if (text[i] === '"' || text[i] === '\n') break;
      }
    } else if (c === "'" && /[0-9A-Za-z_]/.test(text[i - 1] || '') && /[0-9A-Za-z_]/.test(d || '')) {
      // A DIGIT SEPARATOR, NOT A CHARACTER LITERAL. `1'000'000` and
      // `0b0000'0001` are live in this tree (river_generation_harness,
      // demography_harness). Treating that apostrophe as a quote blanks the
      // rest of the line, which is a FALSE NEGATIVE — the dangerous direction,
      // because it routes a Lua harness to the plain builder and the reader
      // gets an unexplained LNK2019. An apostrophe directly after an
      // identifier character is a separator; a character literal never is.
      out += ' ';
    } else if (c === "'") {
      out += ' ';
      for (i++; i < text.length; i++) {
        if (text[i] === '\\') { i++; continue; }
        if (text[i] === "'" || text[i] === '\n') break;
      }
    } else {
      out += c;
    }
  }
  return out;
}

// An include that is ITSELF the signal. scripting/persona_pack.hpp is the third
// sol2 TU's header and carries no inline-only surface, so including it means
// linking it — persona_counsel_harness includes it and never names a lua_state.
//
// scripting/lua_state.hpp is deliberately NOT here, and the asymmetry is
// measured rather than assumed: condition_set_harness and recipe_switch_harness
// both include it, use only the inline helper surface, and link clean on the
// world path. Include-implies-link is true for one of these headers and false
// for the other.
const LUA_INCLUDES = /^\s*#\s*include\s+"[^"]*scripting\/persona_pack\.hpp"/m;

/// Does @p entry reference a symbol only the omitted Lua TUs define?
/// Returns the matched symbol, or null.
///
/// SCOPE IS THE HARNESS'S OWN .cpp, AND THAT IS DELIBERATE — following its
/// includes makes this wrong in both directions. tools/verify/harness_params.hpp
/// includes scripting/lua_state.hpp and defines `parsed_gen_config(lua_state&)`,
/// but that helper is **inline**: including the header costs nothing, and only
/// CALLING it pulls the definition. Half the harnesses include it and link fine.
/// A harness that does call it necessarily names lua_state in its own source to
/// construct the argument, so the direct scan catches it anyway.
function luaSymbolSite(entry) {
  let text;
  try { text = fs.readFileSync(entry, 'utf8'); } catch { return null; }
  const code = stripComments(text);
  const m = code.match(LUA_SYMBOLS);
  if (m) return m[1];
  // Include paths are string literals, which stripComments blanks — so this
  // reads the raw text, with comments removed only line-wise to avoid matching
  // an include quoted inside prose.
  const uncommented = text.replace(/^\s*\/\/[^\n]*$/gm, '');
  return LUA_INCLUDES.test(uncommented) ? 'scripting/persona_pack.hpp' : null;
}

function die(msg, code = 1) { console.error('build_harness: ' + msg); process.exit(code); }

const argv = process.argv.slice(2);
const flags = new Set(argv.filter(a => a.startsWith('--')));
const jobsArg = argv.findIndex(a => a === '--jobs');
const jobs = jobsArg >= 0 ? parseInt(argv[jobsArg + 1], 10) : Math.max(1, os.cpus().length - 1);
const positional = argv.filter((a, i) => !a.startsWith('--') && !(jobsArg >= 0 && i === jobsArg + 1));
const name = positional[0];

if (!name) die('usage: node tools/verify/build_harness.js <name> [--run] [--debug] [--jobs N] [--clean]');
if (!/^[A-Za-z0-9_]+$/.test(name)) die(`"${name}" is not a harness name (letters, digits and underscore only)`);

const src = path.join(ROOT, 'tools', 'verify', name + '.cpp');
if (!fs.existsSync(src)) die(`tools/verify/${name}.cpp does not exist`);
// A THIRD MISROUTE CLASS, named rather than left to be rediscovered. Neither
// builder compiles src/core/save_game.cpp, and CMake links it against imgui
// (save_game.hpp reaches src/ui/plot_history.hpp -> imgui.h). So a save-envelope
// harness fails here with an unexplained LNK2019 and fails the Lua builder with
// C1083 on imgui.h. Refusing with the reason costs nothing; discovering it costs
// a compile and a wrong hypothesis.
if (/^\s*#\s*include\s+"[^"]*core\/save_game\.hpp"/m.test(fs.readFileSync(src, 'utf8'))) {
  die(`${name} includes core/save_game.hpp, which links imgui — neither headless builder\n` +
      '  compiles src/core/save_game.cpp. Build it through CMake:\n' +
      `    cmake --build build --target ${name}       (from a Developer Prompt / after vcvars)`);
}

const luaSite = luaSymbolSite(src);
if (luaSite) {
  die(`${name} needs a live Lua state — it references \`${luaSite}\`, ` +
      'defined in a sol2/Lua TU this builder omits.\n' +
      '  Build it with the Lua builder instead, which links sol2 + Lua:\n' +
      `    bash tools/verify/build_lua_harness.sh ${name}       (bash — and what a worktree agent uses)\n` +
      `    cmd /c tools\\verify\\build_lua_harness.bat ${name}   (cmd)\n` +
      '  A harness that fails on sol/sol.hpp or LNK2019 here is the WRONG BUILDER, not broken code.');
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
// Since BL-960 it holds only the harness's OWN object; the world set lives in
// the shared cache below.
const objDir = path.join(outDir, name + '.obj');
fs.mkdirSync(objDir, { recursive: true });
// Sweep the world objects an earlier build of this harness left here, so the
// 3.3 GB of duplicated objects drains as harnesses are rebuilt rather than
// sitting beside the cache forever. Only this harness's own object survives.
for (const f of fs.readdirSync(objDir)) {
  if (/\.(obj|o)$/.test(f) && path.basename(f, path.extname(f)) !== name) fs.unlinkSync(path.join(objDir, f));
}

const exe = path.join(outDir, name + (isWindows ? '.exe' : ''));
// A stale binary must never be mistaken for a fresh one if the compile fails.
if (fs.existsSync(exe) && fs.statSync(exe).isFile()) fs.unlinkSync(exe);

const debug = flags.has('--debug');

// THE WORLD SET IS COMPILED ONCE AND SHARED (BL-960). Before this, every
// harness recompiled all 66 src/world TUs into its own object dir, with no
// up-to-date check: build_gen/verify held 3.3 GB of the same objects 59 times
// over, and a one-line edit to history_sim.cpp cost a clean build. CMakeLists
// already compiles the set once (io_world_obj); this is the same discipline
// for the CMake-free path every worktree agent uses.
//
// Objects live in build_gen/verify/_world/<release|debug>/, one dir per
// configuration because the flags differ (/Zi and asan objects must never
// meet /O2 ones at link time). A TU is rebuilt when its object is missing,
// older than its .cpp, older than the NEWEST HEADER under src/, or when the
// flag stamp changed. The header rule is deliberately coarse: a false rebuild
// costs seconds, a missed one costs a wrong answer, and a real dependency scan
// is what CMake is for.
//
// The cached objects are LINKED DIRECTLY, not archived into a static library.
// A .lib pulls in only the objects a symbol reference reaches, which would
// silently drop any world TU whose contribution is a static initialiser; the
// old path linked every object, and this keeps that meaning exactly. The
// object list goes through a response file so the command line cannot
// overflow cmd's limit as the set grows.
const cfgName = debug ? 'debug' : 'release';
const worldDir = path.join(outDir, '_world', cfgName);
const objExt = isWindows ? '.obj' : '.o';
if (flags.has('--clean')) {
  fs.rmSync(worldDir, { recursive: true, force: true });
  console.log(`build_harness: cleaned ${path.relative(ROOT, worldDir)}`);
}
fs.mkdirSync(worldDir, { recursive: true });

/// Newest mtime of any header under @p dir (recursive). The coarse dependency
/// rule described above.
function newestHeaderMtime(dir) {
  let newest = 0;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, entry.name);
    if (entry.isDirectory()) newest = Math.max(newest, newestHeaderMtime(p));
    else if (/\.(hpp|h|inl|ipp)$/.test(entry.name)) newest = Math.max(newest, fs.statSync(p).mtimeMs);
  }
  return newest;
}
const headerMtime = newestHeaderMtime(path.join(ROOT, 'src'));

const worldObj = (s) => path.join(worldDir, path.basename(s, '.cpp') + objExt);
function objIsFresh(s) {
  const o = worldObj(s);
  if (!fs.existsSync(o)) return false;
  const om = fs.statSync(o).mtimeMs;
  return om >= fs.statSync(s).mtimeMs && om >= headerMtime;
}

let cmd, args;
// The world-set compile and the harness compile+link are separate steps; each
// branch fills these. `worldCmd` is null when nothing in the cache is stale.
let worldCmd = null, worldArgs;
let stale;

if (isWindows) {
  // cl is only on PATH inside a Developer Prompt; vcvars64 puts it there.
  const vcvars = 'C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat';
  // PRECEDENCE FIX 2026-08-31: this read `!spawnSync(...).status === 0`, which parses
  // as `(!status) === 0` - a boolean compared to a number, so the whole condition was
  // ALWAYS FALSE and the guard never fired. A missing cl then failed later and
  // confusingly instead of here with the message that names the fix.
  if (!fs.existsSync(vcvars) && spawnSync('where', ['cl'], { shell: true }).status !== 0)
    die('cl not found and vcvars64.bat is not at its default path — open a Developer Prompt, or build through CMake');
  const clFlags = ['cl', '/nologo', '/std:c++20', '/EHsc', '/MP',
    debug ? '/Od /Zi' : '/O2', '/I', 'src', '/I', 'tools\\verify',
    // sol2 + Lua headers. NOT because a harness wants Lua - none do - but because
    // a src/world TU in the superset now includes scripting/lua_state.hpp, whose
    // first line is <sol/sol.hpp>. Without these every world-superset harness dies
    // on C1083 before a single assertion runs, which is how main sat with the whole
    // verifier-headless tier unbuildable (2026-08-31). Third rot of this arg list;
    // this file's own header comment already warned about the first two.
    // RESOLVED AND PROPERLY QUOTED, and it took two goes plus two agents to get
    // both halves right. These were the literal cwd-relative strings
    // `_deps_cache\sol2_src\include` and `_deps_cache\lua_src`, so
    // `resolveDepsCache()` above — added to let a worktree find the MAIN
    // checkout's cache — never reached the Windows branch at all, and every
    // Windows worktree agent still died on C1083 'sol/sol.hpp'. Two agents
    // reported it independently.
    //
    // The second half is why this uses q() and not JSON.stringify: JSON escaping
    // DOUBLES backslashes, so the now-absolute path came out as
    // `"C:\\Users\\benbo\\..."` and named a directory that does not exist.
    // That survived on the source-file arguments because cl tolerates it there,
    // and failed outright on an /I path — as C1083 again, indistinguishable from
    // the wrong-builder symptom this file exists to diagnose. Shell quoting is
    // not string escaping. Verified from a worktree with IO_DEPS_CACHE unset.
    // Fifth rot of this arg list; the header comment warned about the first two.
    '/I', q(path.join(DEPS, 'sol2_src', 'include')), '/I', q(path.join(DEPS, 'lua_src'))];

  // The flag stamp: a change to the compile flags above invalidates the whole
  // cache, since an object carries no record of the flags that made it.
  stale = staleWorldSet(clFlags.join(' '));

  if (stale.length) {
    // Compile only the stale TUs, objects into the shared dir. /c with
    // /Fo:<dir>\ names each object after its source.
    worldCmd = `call "${vcvars}" >nul 2>&1 && ` + [...clFlags, '/c',
      ...stale.map(s => q(path.relative(ROOT, s))),
      `/Fo:${path.relative(ROOT, worldDir)}\\`].join(' ');
    worldArgs = undefined;
  }
  // The harness's own TU, then the cached objects by response file. cl hands
  // .obj arguments straight to the linker, so no separate link step.
  const rsp = writeResponseFile();
  const cl = [...clFlags,
    q(path.relative(ROOT, src)),
    `/Fo:${path.relative(ROOT, objDir)}\\`, `/Fe:${path.relative(ROOT, exe)}`,
    `@${path.relative(ROOT, rsp)}`].join(' ');
  // ONE command string, and argv0 is NOT 'cmd'. It used to be
  // spawnSync('cmd', ['/c', ...], { shell: true }), which is a DOUBLE WRAP:
  // node runs `cmd /d /s /c "cmd /c call "<vcvars>" >nul 2>&1 && cl ..."`, the
  // nested quotes split the vcvars path at its first space, `call` fails, and
  // `>nul 2>&1` swallows the error — so the only symptom was `'cl' is not
  // recognized` from a builder that had never initialised the environment.
  // Measured 2026-09-06: this failed for EVERY caller, from bash and cmd alike,
  // so the whole non-Lua verifier-headless tier was unbuildable. Fourth rot of
  // this invocation; the header comment warns about the earlier three.
  cmd = `call "${vcvars}" >nul 2>&1 && ${cl}`; args = undefined;
} else {
  const gxxFlags = ['-std=c++20', debug ? '-O0' : '-O2', '-g',
    '-I', path.join(ROOT, 'src'), '-I', path.join(ROOT, 'tools', 'verify'),
    // See the MSVC branch above: a world TU reaches scripting/lua_state.hpp -> sol/sol.hpp.
    '-I', path.join(DEPS, 'sol2_src', 'include'),
    '-I', path.join(DEPS, 'lua_src'),
    ...(debug ? ['-fsanitize=address,undefined'] : [])];
  stale = staleWorldSet(gxxFlags.join(' '));
  if (stale.length) {
    // `g++ -c a.cpp b.cpp` writes a.o and b.o into the cwd, so the world dir
    // is the cwd for this step. Sequential; --jobs is the MSVC /MP knob.
    worldCmd = 'g++'; worldArgs = [...gxxFlags, '-c', ...stale];
  }
  const rsp = writeResponseFile();
  cmd = 'g++';
  args = [...gxxFlags, src, `@${rsp}`, '-o', exe];
  void jobs;
}

/// Which world TUs need compiling, honouring the flag stamp.
function staleWorldSet(flagString) {
  const stampPath = path.join(worldDir, 'flags.txt');
  let previous = null;
  try { previous = fs.readFileSync(stampPath, 'utf8'); } catch { /* first build */ }
  if (previous !== flagString) {
    // Flags changed (or first build): every object is suspect. Remove them so
    // a failed compile cannot leave a mixed-flag cache behind.
    for (const s of sources) { try { fs.unlinkSync(worldObj(s)); } catch { /* absent */ } }
    fs.writeFileSync(stampPath, flagString, 'utf8');
    return sources.slice();
  }
  return sources.filter(s => !objIsFresh(s));
}

/// One object path per line, in the same sorted order the old command line
/// used, so link order stays deterministic.
function writeResponseFile() {
  const rsp = path.join(worldDir, 'objects.rsp');
  fs.writeFileSync(rsp, sources.map(s => q(isWindows ? path.relative(ROOT, worldObj(s)) : worldObj(s))).join('\n') + '\n', 'utf8');
  return rsp;
}

const label = isWindows ? 'cl' : 'g++';
console.log(`build_harness: ${name} <- ${sources.length} world TUs, ${stale.length} to compile, ${sources.length - stale.length} cached (${label}, ${debug ? 'debug+asan' : 'release'})`);
const t0 = Date.now();

if (worldCmd !== null) {
  const w = worldArgs === undefined
    ? spawnSync(worldCmd, { cwd: ROOT, stdio: 'inherit', shell: true })
    : spawnSync(worldCmd, worldArgs, { cwd: worldDir, stdio: 'inherit' });
  const missing = stale.filter(s => !fs.existsSync(worldObj(s)));
  if (w.status !== 0 || missing.length) {
    // Never leave a half-fresh object behind: a TU that failed has no object,
    // and one that compiled before the failure is fine to keep.
    console.error(`build_harness: COMPILE_FAILED in the world set (${((Date.now() - t0) / 1000).toFixed(1)}s)` +
      (missing.length ? `; no object for ${missing.map(m => path.basename(m)).join(', ')}` : ''));
    process.exit(1);
  }
  console.log(`build_harness: world set ready (${((Date.now() - t0) / 1000).toFixed(1)}s)`);
}

const t1 = Date.now();
const r = args === undefined
  ? spawnSync(cmd, { cwd: ROOT, stdio: 'inherit', shell: true })
  : spawnSync(cmd, args, { cwd: ROOT, stdio: 'inherit', shell: isWindows });
const secs = ((Date.now() - t0) / 1000).toFixed(1);
const linkSecs = ((Date.now() - t1) / 1000).toFixed(1);

if (r.status !== 0 || !fs.existsSync(exe)) {
  console.error(`build_harness: COMPILE_FAILED (${secs}s)`);
  process.exit(1);
}
console.log(`build_harness: COMPILE_OK  ${path.relative(ROOT, exe)}  (${secs}s total, ${linkSecs}s harness+link)`);

if (flags.has('--run')) {
  // Run from the repo root: a script-rooted harness resolves scripts/ relative to cwd.
  const run = spawnSync(exe, [], { cwd: ROOT, stdio: 'inherit' });
  process.exit(run.status === null ? 1 : run.status);
}
