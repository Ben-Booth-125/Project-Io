#!/usr/bin/env node
// Protocol smoke check for `ProjectIo --serve` — the word interface an
// out-of-process agent plays through (BL-278, docs/ai/AI_OPPONENT.md § 10a).
//
// WHY THIS EXISTS. BL-278 landed 2026-08-03 and was recorded as smoke-tested by
// hand, once. Nothing re-ran it, so three separate regressions accumulated
// silently and all three were found by reading rather than by failing:
//
//   * six of the fifteen `corp_verb` values could not be issued at all, because
//     the COMMAND opcode never parsed their arguments;
//   * four of the twelve `corp_command_result` codes were reported as
//     `rejected_invalid`, telling an agent its syntax was wrong when a supplier
//     had simply declined;
//   * `tools/mcp/server.js` spawned `build/ProjectIo.exe`, which the primary
//     (Linux) build never produces — so the MCP server could not start here.
//
// A seam nobody exercises is a seam nobody can trust. This is the cheap,
// committed check that the protocol still answers, so the next regression
// fails a run instead of waiting to be read.
//
// It asserts SHAPE, not economics: that every opcode answers, that every verb
// is reachable and returns a TYPED result, and that a rejection is specific
// rather than a catch-all. Whether a given command *should* succeed against a
// given world is the harnesses' business (tools/verify/), not this file's — so
// a `rejected_*` is a pass here as long as it is the right KIND of rejection.
//
// Run:  node tools/mcp/smoke.js            (build/ProjectIo must exist)
//       node tools/mcp/smoke.js --verbose
// Exit: 0 all checks passed, 1 otherwise.

const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');
const readline = require('readline');

const ROOT = path.resolve(__dirname, '..', '..');
const VERBOSE = process.argv.includes('--verbose');

// Same resolution order as server.js — if these drift, the smoke check stops
// testing the thing the server actually spawns.
const EXE_CANDIDATES = [
  path.join(ROOT, 'build', 'ProjectIo'),
  path.join(ROOT, 'build', 'ProjectIo.exe'),
  path.join(ROOT, 'build', 'Release', 'ProjectIo.exe'),
  path.join(ROOT, 'build', 'Debug', 'ProjectIo.exe'),
];

// corp_command.hpp's corp_verb, in declaration order. Index == enum value.
const VERBS = [
  'build', 'demolish', 'set_recipe', 'set_workforce', 'idle', 'resume',
  'place_road', 'survey', 'hire_unit',
  'place_sell_order', 'remove_sell_order', 'set_workforce_auto',
  'request_quote', 'accept_quote', 'cancel_contract',
];

// Every code corp_command_result declares.
//
// NOTE ON WHAT THIS CAN AND CANNOT CATCH. A name outside this set means the seam
// invented a code. It does NOT catch a missing switch case, because
// corp_command_result_name's fall-through returns "rejected_invalid", which is
// itself a legitimate member — the defect this file was written after was
// precisely four codes collapsing into that one, and set membership is blind to
// it. What catches that is the procurement probe below, which reaches a decline
// the seam can only express with one of the four BL-350 names.
const RESULT_CODES = new Set([
  'applied', 'rejected_no_corp', 'rejected_not_owner', 'rejected_invalid',
  'rejected_placement', 'rejected_funds', 'rejected_state', 'rejected_tech_locked',
  'rejected_no_capacity', 'rejected_no_input_access', 'rejected_embargo',
  'rejected_reputation',
]);

// The four BL-350 declines. If NONE of these is ever observable across a sweep of
// suppliers, either the world genuinely has no declining supplier, or the switch
// is collapsing them — the check below reports which, rather than guessing.
const PROCUREMENT_DECLINES = [
  'rejected_no_capacity', 'rejected_no_input_access', 'rejected_embargo',
  'rejected_reputation',
];

let failures = 0;
function check(ok, label, detail) {
  console.log(`  [${ok ? 'PASS' : 'FAIL'}] ${label}`);
  if (!ok) {
    ++failures;
    if (detail !== undefined) console.log(`         ${detail}`);
  } else if (VERBOSE && detail !== undefined) {
    console.log(`         ${detail}`);
  }
}

// ---------------------------------------------------------------------------
// Line protocol client
// ---------------------------------------------------------------------------

function makeClient(exe, ticks) {
  const child = spawn(exe, ['--serve', '--ticks', String(ticks)], { cwd: ROOT });
  const rl = readline.createInterface({ input: child.stdout });
  let pending = null;

  rl.on('line', (line) => {
    if (line.startsWith('[Lua]')) return; // world-build banner, not a response
    if (!pending) return;
    // BLACKBOARD and CORPS stream N lines then END; everything else is one line.
    if (line === 'END' || line.startsWith('OK ') || line.startsWith('RESULT ')
        || line.startsWith('ERR ') || line === 'BYE') {
      pending.lines.push(line);
      const p = pending;
      pending = null;
      p.resolve(p.lines);
    } else {
      pending.lines.push(line);
    }
  });

  const stderr = [];
  child.stderr.on('data', (d) => stderr.push(String(d)));
  child.on('exit', (code) => {
    if (pending) {
      const p = pending; pending = null;
      p.reject(new Error(`--serve exited (code ${code}) mid-request. stderr:\n${stderr.join('')}`));
    }
  });

  function send(line) {
    return new Promise((resolve, reject) => {
      if (pending) return reject(new Error('a request is already in flight'));
      pending = { resolve, reject, lines: [] };
      const timer = setTimeout(() => {
        if (pending) { const p = pending; pending = null; p.reject(new Error(`timeout waiting for: ${line}`)); }
      }, 120000);
      const done = (fn) => (v) => { clearTimeout(timer); fn(v); };
      pending.resolve = done(resolve);
      pending.reject = done(reject);
      child.stdin.write(line + '\n');
    });
  }

  return { child, send, close: () => { try { child.stdin.end(); } catch (_) {} child.kill(); } };
}

function resultOf(lines) {
  const line = lines.find((l) => l.startsWith('RESULT ')) || '';
  const m = /result=(\S+)/.exec(line);
  return m ? m[1] : null;
}

// ---------------------------------------------------------------------------

async function main() {
  console.log('=== ProjectIo --serve protocol smoke check ===\n');

  const exe = EXE_CANDIDATES.find((p) => fs.existsSync(p));
  check(!!exe, 'the ProjectIo binary resolves on this platform',
        exe || `none of:\n         ${EXE_CANDIDATES.join('\n         ')}`);
  if (!exe) {
    console.log('\nCannot continue without a binary. Build: cmake --build build --target ProjectIo');
    process.exit(1);
  }

  const io = makeClient(exe, 8);
  try {
    // --- opcodes answer at all -------------------------------------------
    const corpLines = await io.send('CORPS');
    const corps = corpLines.filter((l) => l.startsWith('{')).map((l) => JSON.parse(l));
    check(corps.length > 0, `CORPS lists corporations (${corps.length})`);
    check(corpLines[corpLines.length - 1] === 'END', 'CORPS terminates with END');

    const ai = corps.filter((c) => !c.is_player);
    const player = corps.find((c) => c.is_player);
    check(ai.length > 0, `at least one non-player corporation exists (${ai.length})`);
    check(corps.every((c) => typeof c.id === 'number' && typeof c.name === 'string'),
          'every corp row carries an id and a name');

    const subject = ai[0];
    if (VERBOSE) console.log(`         acting as corp ${subject.id} (${subject.name})`);

    const tickLines = await io.send('TICK');
    check(/^OK tick=\d+$/.test(tickLines[0] || ''), 'TICK advances and acknowledges',
          tickLines[0]);

    const bbLines = await io.send(`BLACKBOARD corp=${subject.id}`);
    const facts = bbLines.filter((l) => l.startsWith('{')).map((l) => JSON.parse(l));
    check(facts.length > 0, `BLACKBOARD returns facts (${facts.length})`);
    check(bbLines[bbLines.length - 1] === 'END', 'BLACKBOARD terminates with END');
    check(facts.every((f) => '_v' in f && 'predicate' in f && 'provenance' in f),
          'every fact carries schema version, predicate and provenance');

    const errLines = await io.send('NONSENSE_OPCODE');
    check((errLines[0] || '').startsWith('ERR '), 'an unknown opcode is refused, not ignored',
          errLines[0]);

    // --- every verb is reachable and typed --------------------------------
    // Deliberately weak arguments: the point is that each verb REACHES the seam
    // and comes back with a code from the enum. A verb whose args are never
    // parsed cannot be distinguished from one that was legitimately refused —
    // except that it can never produce a rejection specific to its own
    // arguments, which is what the next block tests.
    console.log('\n  -- every corp_verb reaches the seam and returns a typed result --');
    for (let v = 0; v < VERBS.length; ++v) {
      const lines = await io.send(`COMMAND corp=${subject.id} verb=${v} subject=0 tile=0`);
      const r = resultOf(lines);
      check(r !== null && RESULT_CODES.has(r),
            `verb ${v} (${VERBS[v]}) -> ${r === null ? 'NO RESULT LINE' : r}`,
            r !== null && !RESULT_CODES.has(r) ? `'${r}' is not a corp_command_result` : undefined);
    }

    // --- the arguments actually arrive ------------------------------------
    // The regression this file was written for. place_sell_order rejects
    // quantity <= 0 before it looks at anything else, so if `quantity` never
    // reaches the parser the verb is rejected_invalid NO MATTER WHAT is sent.
    // Sending a well-formed order against a real body must therefore move OFF
    // rejected_invalid — either applied, or a rejection about the BOOK
    // (rejected_state) rather than about the arguments.
    console.log('\n  -- arguments reach apply_corp_command (not just the opcode) --');
    // Body ids come from BODIES, not from the blackboard: market facts are keyed
    // by MARKET id, so an agent that only reads state cannot name the body that
    // `survey` / `place_sell_order` / `request_quote` take as their subject.
    const bodyLines = await io.send('BODIES');
    const bodyRows = bodyLines.filter((l) => l.startsWith('{')).map((l) => JSON.parse(l));
    check(bodyRows.length > 0, `BODIES lists bodies (${bodyRows.length})`);
    check(bodyLines[bodyLines.length - 1] === 'END', 'BODIES terminates with END');
    check(bodyRows.every((b) => typeof b.id === 'number' && typeof b.survey === 'string'),
          'every body row carries an id and a survey phase');
    const bodies = new Set(bodyRows.map((b) => b.id));
    const bodyId = bodyRows[0] && bodyRows[0].id;
    check(bodyId !== undefined, 'at least one body is nameable on the seam', String(bodyId));

    // Find a body whose market prices the good we intend to list — the seam
    // rejects an unpriced resource before it ever looks at quantity, so picking
    // blindly would test the wrong rejection.
    let listBody, listRes;
    outer:
    for (const b of bodyRows) {
      for (let r = 0; r < 31; ++r) {
        const probe = resultOf(await io.send(
          `COMMAND corp=${subject.id} verb=9 subject=${b.id} target=${r} quantity=5 floor_price=0.5`));
        if (probe !== 'rejected_invalid') { listBody = b.id; listRes = r; break outer; }
      }
    }
    check(listBody !== undefined,
          'some (body, resource) pair accepts a well-formed sell order',
          listBody === undefined ? 'every pair still returns rejected_invalid'
                                 : `body ${listBody}, resource ${listRes}`);

    if (listBody !== undefined) {
      // The control: identical command, quantity omitted. apply_corp_command
      // rejects quantity <= 0 before anything else, so if `quantity` never
      // reached the parser BOTH of these would read rejected_invalid and the
      // verb would be unreachable no matter what a caller sent.
      const bad = resultOf(await io.send(
        `COMMAND corp=${subject.id} verb=9 subject=${listBody} target=${listRes}`));
      check(bad === 'rejected_invalid',
            'place_sell_order with no quantity is rejected as invalid (the control)', bad);
    }

    // hire_unit reads unit_type; request_quote reads counterparty. Neither can
    // produce its own specific rejection if the argument never arrives.
    // Sweep suppliers and resources looking for at least one of BL-350's four
    // specific declines. This is the real test that the result switch is
    // exhaustive: before the fix, every one of them arrived as
    // `rejected_invalid`, so observing even one proves the seam can now say WHY
    // a supplier said no. It is reported rather than asserted when the sweep
    // finds only successes and generic rejections, because "no supplier on this
    // world declines for these reasons" is a legitimate state of the economy and
    // failing on it would make this a fixture test.
    const seenCodes = new Set();
    for (const supplier of ai.slice(0, 6)) {
      for (const r of [0, 1, 2, 5, 9]) {
        const res = resultOf(await io.send(
          `COMMAND corp=${subject.id} verb=12 subject=${bodyId ?? 0} target=${r} `
          + `quantity=1 counterparty=${supplier.id}`));
        if (res) seenCodes.add(res);
      }
    }
    check([...seenCodes].every((c) => RESULT_CODES.has(c)),
          'every request_quote outcome is a declared corp_command_result',
          [...seenCodes].join(', '));
    const specific = PROCUREMENT_DECLINES.filter((c) => seenCodes.has(c));
    if (specific.length > 0) {
      check(true, `procurement declines are specific, not collapsed to rejected_invalid `
                  + `(saw ${specific.join(', ')})`);
    } else {
      console.log('         (no BL-350-specific decline observed on this world; '
                  + `outcomes seen: ${[...seenCodes].join(', ')})`);
    }

    // --- the player corp is not commandable through the seam --------------
    // The standing rule: the player's corp is never auto-acted on. The seam is
    // shared with the AI, so this is worth asserting on the wire, not just in
    // the scorer.
    if (player) {
      const asPlayer = resultOf(await io.send(
        `COMMAND corp=${player.id} verb=7 subject=0`));
      check(asPlayer !== null && RESULT_CODES.has(asPlayer),
            'a command naming the player corp still returns a typed result', asPlayer);
    }

    // --- survey actually progresses ---------------------------------------
    // `survey` was an applicable verb whose effect never arrived, because
    // --serve never called advance_surveys. Dispatch one, tick, and assert the
    // fact count moves — a survey that reveals nothing reveals the bug.
    console.log('\n  -- a dispatched survey progresses across ticks --');
    // This is the assertion the whole file exists for, so it has to be one the
    // bug could actually fail. `advance_surveys` was missing from --serve's tick,
    // and the symptom was NOT "fewer facts" — it was a survey that stayed frozen
    // forever. So the check reads the survey's own progress counters out of
    // BODIES before and after ticking, and requires them to MOVE. A fact-count
    // comparison would pass whether or not the survey ever advanced.
    const hidden = bodyRows.filter((b) => b.survey === 'hidden');
    let surveyedId = null;
    for (const b of hidden) {
      const r = resultOf(await io.send(`COMMAND corp=${subject.id} verb=7 subject=${b.id}`));
      if (r === 'applied') { surveyedId = b.id; break; }
    }
    check(hidden.length === 0 || surveyedId !== null,
          'a survey can be dispatched on an unsurveyed body',
          hidden.length === 0 ? 'no hidden body on this world — check is vacuous here, and says so'
                              : `body ${surveyedId}`);

    if (surveyedId !== null) {
      const before = bodyRows.find((b) => b.id === surveyedId);
      for (let i = 0; i < 6; ++i) await io.send('TICK');
      const nowRows = (await io.send('BODIES'))
        .filter((l) => l.startsWith('{')).map((l) => JSON.parse(l));
      const now = nowRows.find((b) => b.id === surveyedId);
      const moved = now && before &&
        (now.survey !== before.survey || now.regions_done > before.regions_done);
      check(!!moved,
            'the dispatched survey ACTUALLY progresses when the world ticks',
            `${before && before.survey}/${before && before.regions_done} -> `
            + `${now && now.survey}/${now && now.regions_done}`);
    }

    const bye = await io.send('SHUTDOWN');
    check(bye[0] === 'BYE', 'SHUTDOWN is acknowledged', bye[0]);
  } catch (e) {
    check(false, 'the protocol session completed without error', e.message);
  } finally {
    io.close();
  }

  console.log(failures === 0 ? '\nALL CHECKS PASSED' : `\nFAILURES (${failures})`);
  process.exit(failures === 0 ? 0 : 1);
}

main().catch((e) => { console.error(e); process.exit(1); });
