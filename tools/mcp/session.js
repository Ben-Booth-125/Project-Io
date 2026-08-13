#!/usr/bin/env node
// Play a scripted session against `ProjectIo --serve` and print the transcript.
//
// WHY THIS EXISTS. `smoke.js` asks whether the protocol ANSWERS. This asks what
// happens when someone actually PLAYS — which is a different and richer question,
// and the one Ben named on 2026-08-13: "pushing for AI play will expose more bugs
// and give us actionable improvements now."
//
// The play loop it enables is deliberately batch-shaped rather than interactive,
// and that is a feature of the determinism rather than a limitation. The world is
// rebuilt identically on every run, so appending commands to a script and
// re-running replays the whole session byte-for-byte and then extends it. An
// agent therefore plays by: run the script, read the transcript, decide the next
// move, append it, run again. Every earlier turn is reproduced exactly.
//
// Usage:
//   node tools/mcp/session.js <script-file> [--ticks N] [--quiet]
//   node tools/mcp/session.js -            (read the script from stdin)
//
// Script format: one request per line, exactly the --serve line protocol
// (TICK / CORPS / BODIES / BLACKBOARD / COMMAND / SHUTDOWN). Blank lines and
// lines starting with # are ignored, so a script can carry its own reasoning.
// SHUTDOWN is appended automatically if absent.
//
// Output: the request echoed, then its response, so the transcript reads as a
// dialogue. BLACKBOARD responses are summarised by default (they run to tens of
// thousands of facts); pass --facts to see them in full.

const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const readline = require('readline');

const ROOT = path.resolve(__dirname, '..', '..');
const args = process.argv.slice(2);
const QUIET = args.includes('--quiet');
const FULL_FACTS = args.includes('--facts');
const tickArgIdx = args.indexOf('--ticks');
const WARMUP = tickArgIdx >= 0 ? Number(args[tickArgIdx + 1]) : 8;
const scriptArg = args.find((a) => !a.startsWith('--') && a !== String(WARMUP));

const EXE_CANDIDATES = [
  path.join(ROOT, 'build', 'ProjectIo'),
  path.join(ROOT, 'build', 'ProjectIo.exe'),
  path.join(ROOT, 'build', 'Release', 'ProjectIo.exe'),
  path.join(ROOT, 'build', 'Debug', 'ProjectIo.exe'),
];

function resolveExe() {
  for (const p of EXE_CANDIDATES) if (fs.existsSync(p)) return p;
  throw new Error('ProjectIo not built. cmake --build build --target ProjectIo');
}

function readScript() {
  if (!scriptArg || scriptArg === '-') return fs.readFileSync(0, 'utf8');
  return fs.readFileSync(scriptArg, 'utf8');
}

async function main() {
  const lines = readScript()
    .split('\n')
    .map((l) => l.trim())
    .filter((l) => l.length > 0 && !l.startsWith('#'));
  if (lines[lines.length - 1] !== 'SHUTDOWN') lines.push('SHUTDOWN');

  const child = spawn(resolveExe(), ['--serve', '--ticks', String(WARMUP)], { cwd: ROOT });
  const rl = readline.createInterface({ input: child.stdout });
  let pending = null;

  rl.on('line', (line) => {
    if (line.startsWith('[Lua]')) return;
    if (!pending) return;
    if (line === 'END' || line.startsWith('OK ') || line.startsWith('RESULT ')
        || line.startsWith('ERR ') || line === 'BYE') {
      pending.lines.push(line);
      const p = pending; pending = null; p.resolve(p.lines);
    } else {
      pending.lines.push(line);
    }
  });

  const stderr = [];
  child.stderr.on('data', (d) => stderr.push(String(d)));
  child.on('exit', (code) => {
    if (pending) {
      const p = pending; pending = null;
      p.reject(new Error(`--serve exited (code ${code}). stderr:\n${stderr.join('')}`));
    }
  });

  const send = (line) => new Promise((resolve, reject) => {
    pending = { resolve, reject, lines: [] };
    const t = setTimeout(() => {
      if (pending) { const p = pending; pending = null; p.reject(new Error(`timeout: ${line}`)); }
    }, 180000);
    const wrap = (fn) => (v) => { clearTimeout(t); fn(v); };
    pending.resolve = wrap(resolve); pending.reject = wrap(reject);
    child.stdin.write(line + '\n');
  });

  // Tally of every result code seen, so a session ends with a summary of how the
  // seam answered rather than only what the player intended.
  const results = new Map();
  let tick = WARMUP;

  console.log(`# ProjectIo --serve, ${WARMUP} warm-up ticks, ${lines.length} requests\n`);

  for (const line of lines) {
    let out;
    try {
      out = await send(line);
    } catch (e) {
      console.log(`>>> ${line}\n!!! ${e.message}`);
      break;
    }

    if (line.startsWith('BLACKBOARD') && !FULL_FACTS) {
      const facts = out.filter((l) => l.startsWith('{')).map((l) => JSON.parse(l));
      // Summarise by predicate family — the shape a player reasons about.
      const byPred = new Map();
      for (const f of facts) {
        const k = f.predicate.split(':')[0];
        byPred.set(k, (byPred.get(k) || 0) + 1);
      }
      console.log(`>>> ${line}`);
      console.log(`    ${facts.length} facts: `
        + [...byPred.entries()].map(([k, n]) => `${k}=${n}`).join(' '));
      // Own-asset facts are the ones a player acts on; show them.
      const own = facts.filter((f) => f.provenance === 'own-asset');
      if (own.length && own.length <= 200) {
        for (const f of own) console.log(`    own ${f.subject} ${f.predicate} = ${f.value}`);
      }
      continue;
    }

    console.log(`>>> ${line}`);
    for (const l of out) {
      if (!QUIET || !l.startsWith('{')) console.log(`    ${l}`);
      const m = /result=(\S+)/.exec(l);
      if (m) results.set(m[1], (results.get(m[1]) || 0) + 1);
      const t = /^OK tick=(\d+)/.exec(l);
      if (t) tick = Number(t[1]);
    }
  }

  console.log(`\n# final tick ${tick}`);
  console.log('# result tally: '
    + ([...results.entries()].map(([k, n]) => `${k}=${n}`).join(' ') || '(no COMMANDs issued)'));
  child.kill();
}

main().catch((e) => { console.error(e); process.exit(1); });
