#!/usr/bin/env node
// Protocol smoke check for `ProjectIo --serve` — the word interface an
// out-of-process agent plays through (BL-278, docs/ai/AI_OPPONENT.md § 10a).
//
// WHY THIS EXISTS. BL-278 landed 2026-08-03 and was recorded as smoke-tested by
// hand, once. Nothing re-ran it, so three separate regressions accumulated
// silently and all three were found by reading rather than by failing:
//
//   * five of the fifteen `corp_verb` values could not be issued at all, because
//     the COMMAND opcode never parsed their arguments (and a sixth, hire_unit,
//     could only ever raise roster row 0);
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
// it. The procurement probe below is the best available substitute: it sweeps
// suppliers looking for a decline only the four BL-350 names can express. It
// REPORTS rather than fails when it finds none, because "no supplier on this
// world declines for these reasons" is a legitimate state of the economy and
// failing on it would turn this file into a fixture test. So: observing one
// proves the switch is exhaustive; observing none proves nothing either way,
// and says so.
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

function makeClient(exe, ticks, extraArgs = []) {
  const child = spawn(exe, ['--serve', '--ticks', String(ticks), ...extraArgs], { cwd: ROOT });
  const rl = readline.createInterface({ input: child.stdout });
  let pending = null;

  rl.on('line', (line) => {
    if (line.startsWith('[Lua]')) return; // world-build banner, not a response
    if (!pending) return;
    pending.lines.push(line);
    // BLACKBOARD / CORPS / BODIES stream rows then END, and since BL-397 a
    // refused BLACKBOARD is `ERR ... END` — so END alone terminates them.
    // Resolving on the ERR line would leave the trailing END to be read as
    // the NEXT request's response. Every other op answers exactly one line.
    if (pending.multi ? line === 'END' : true) {
      const p = pending;
      pending = null;
      p.resolve(p.lines);
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
      pending = { resolve, reject, lines: [],
                  multi: /^(BLACKBOARD|CORPS|BODIES)\b/.test(line) };
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

    // BL-387: this server was spawned with no --as, so its session actor is
    // the player corp — every command below must be issued as it, or the
    // authority gate (correctly) answers rejected_not_owner before the seam
    // is ever reached. Acting as a rival now needs `--as`, tested below.
    check(!!player, 'a player corp exists (the default session actor)');
    const subject = player;
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
    const bodyId = bodyRows[0] && bodyRows[0].id;
    check(bodyId !== undefined, 'at least one body is nameable on the seam', String(bodyId));

    // Find a body whose market prices the good we intend to list — the seam
    // rejects an unpriced resource before it ever looks at quantity, so picking
    // blindly would test the wrong rejection.
    //
    // The candidate resources are READ OFF THE BLACKBOARD (`price:<n>` facts)
    // rather than swept over a hard-coded count. A literal here would be a
    // second definition of `resource_type` living in a file whose whole purpose
    // is catching consumers that lag the enum they transcribe — and it had
    // already gone stale at 31 against a resource_count of 42.
    const pricedResources = [...new Set(
      facts.filter((f) => f.predicate.startsWith('price:'))
           .map((f) => Number(f.predicate.slice('price:'.length)))
           .filter((n) => Number.isInteger(n)))].sort((a, b) => a - b);
    check(pricedResources.length > 0,
          'the blackboard names priced resources to probe with',
          `${pricedResources.length} distinct`);

    let listBody, listRes;
    outer:
    for (const b of bodyRows) {
      for (const r of pricedResources) {
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

      // A malformed float must be REFUSED, not silently replaced by the default.
      // This is the regression an adversarial review caught in the first cut of
      // the non-finite guard: `floor_price` defaults to 0, which the seam reads
      // as "accept the market price", so substituting it turned "sell only above
      // this floor" into "sell at market every tick" and answered `applied`. The
      // caller would have had no way to know its order was not the one it placed.
      for (const bad_price of ['nan', 'inf', '1e400']) {
        const r = resultOf(await io.send(
          `COMMAND corp=${subject.id} verb=9 subject=${listBody} target=${listRes} `
          + `quantity=5 floor_price=${bad_price}`));
        check(r === 'rejected_invalid',
              `place_sell_order with floor_price=${bad_price} is refused, not defaulted`, r);
      }
      // Same for quantity, where the default happens to be rejected downstream
      // anyway — asserted so the two keys cannot drift apart silently.
      const badQty = resultOf(await io.send(
        `COMMAND corp=${subject.id} verb=9 subject=${listBody} target=${listRes} quantity=nan`));
      check(badQty === 'rejected_invalid',
            'place_sell_order with quantity=nan is refused', badQty);
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

    // --- BL-387: the session actor gate -----------------------------------
    // The predecessor of this block only asserted that a command naming the
    // player corp got a well-formed reply — an overclaim it had to disclaim in
    // its own header, because the seam had NO actor authority at all: any
    // caller could command any rival by name. Now the protocol layer refuses
    // it, and refusal must PERFORM NOTHING — that is the invariant the whole
    // hardening batch defends, so it is asserted with a state snapshot, not
    // just a result string.
    console.log('\n  -- BL-387: the seam acts only as the session actor --');
    const snapshot = async () =>
      (await io.send('CORPS')).join('\n') + '\n'
      + (await io.send(`BLACKBOARD corp=${subject.id}`)).join('\n');
    const rival = ai[0];
    {
      const beforeAuth = await snapshot();
      const crossCorp = resultOf(await io.send(
        `COMMAND corp=${rival.id} verb=7 subject=${bodyId ?? 0}`));
      check(crossCorp === 'rejected_not_owner',
            'a COMMAND naming a corp that is not the session actor is refused as rejected_not_owner',
            crossCorp);
      check(await snapshot() === beforeAuth,
            'the refused cross-corp command performed nothing (state snapshot identical)');
    }

    // --- BL-396: out-of-domain fields are refused whole, world unchanged ----
    // Each of these used to pass the parser by narrowing: verb=256 truncated
    // to 0 and BUILT A BUILDING answering `applied`; type=200 indexed the
    // economics array out of bounds and SEGFAULTED the process;
    // workforce=4294967396 wrapped to a legal 100 and applied; quantity=1e300
    // is a finite double that overflows to +inf when narrowed to float. All
    // must now answer rejected_invalid, and — the batch's invariant — a
    // refusal performs NOTHING, asserted by snapshot after every case.
    console.log('\n  -- BL-396: out-of-domain fields are refused, world unchanged --');
    {
      const beforeRange = await snapshot();
      const rangeCases = [
        ['verb=256 (truncates to build)',        'verb=256 tile=0'],
        ['verb=265 (truncates to hire_unit)',    'verb=265 tile=0'],
        ['verb=-1',                              'verb=-1'],
        ['type=200 (economics array OOB)',       'verb=0 tile=0 type=200'],
        ['road_tier=255',                        'verb=6 tile=0 road_tier=255'],
        ['workforce=4294967396 (wraps to 100)',  'verb=3 subject=0 workforce=4294967396'],
        ['unit_type=70000 (beyond uint16)',      'verb=8 tile=0 unit_type=70000'],
        ['quantity=1e300 (overflows to +inf as float)',
         `verb=9 subject=${bodyId ?? 0} target=0 quantity=1e300`],
        ['floor_price=-1',
         `verb=9 subject=${bodyId ?? 0} target=0 quantity=5 floor_price=-1`],
      ];
      for (const [label, args] of rangeCases) {
        const r = resultOf(await io.send(`COMMAND corp=${subject.id} ${args}`));
        check(r === 'rejected_invalid', `${label} -> rejected_invalid`, r);
        check(await snapshot() === beforeRange,
              `${label} performed nothing (state snapshot identical)`);
      }
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

    // --- BL-387: `--as any` lifts the gate, and only on request ------------
    // A second server, spawned permissive. The SAME cross-corp command the
    // default server refused above must reach the seam here — bot-vs-bot
    // corpus generation is the one caller that legitimately plays every corp,
    // and it has to ask for that mode explicitly.
    // --- BL-397: reads serve only the session actor's private view ----------
    // The blackboard is a corp's PRIVATE view (cash, pools, recipes); the
    // opcode used to export whichever corp the request line named. The refusal
    // must keep the lines-then-END shape or a streaming client's framing
    // breaks — asserted literally.
    console.log("\n  -- BL-397: reads serve only the session actor's private view --");
    {
      const bbRival = await io.send(`BLACKBOARD corp=${rival.id}`);
      check(bbRival.length === 2 && bbRival[0] === 'ERR result=rejected_not_owner'
            && bbRival[1] === 'END',
            'BLACKBOARD for a non-actor corp answers ERR result=rejected_not_owner then END',
            bbRival.join(' | '));
    }

    console.log('\n  -- BL-387: --as any restores cross-corp reach (explicit opt-in) --');
    const io2 = makeClient(exe, 8, ['--as', 'any']);
    try {
      const anyCross = resultOf(await io2.send(
        `COMMAND corp=${rival.id} verb=7 subject=${bodyId ?? 0}`));
      check(anyCross !== null && RESULT_CODES.has(anyCross) && anyCross !== 'rejected_not_owner',
            'under --as any the same cross-corp command is not refused for actor reasons',
            anyCross);

      // Under --as any the same rival read serves.
      const bbAny = await io2.send(`BLACKBOARD corp=${rival.id}`);
      check(bbAny.filter((l) => l.startsWith('{')).length > 0
            && !bbAny.some((l) => l.startsWith('ERR ')),
            'under --as any a non-actor BLACKBOARD serves facts rather than refusing');

      // --- BL-397: remove_sell_order no longer distinguishes foreign from
      // nonexistent. It answered `rejected_not_owner` for someone else's order
      // and `rejected_invalid` for no order — a perfect oracle: order ids are
      // one global monotonic sequence, so sweeping the id space mapped the
      // whole book. Build the two cases with KNOWN ids: place as the player
      // (its id found by sweeping to the `applied` removal — the sweep itself
      // must never surface rejected_not_owner), then place as a rival, whose
      // order takes exactly the next id. Runs on the --as any server because
      // constructing a foreign order requires acting as two corps.
      if (listBody !== undefined) {
        const placed = resultOf(await io2.send(
          `COMMAND corp=${subject.id} verb=9 subject=${listBody} target=${listRes} `
          + `quantity=5 floor_price=0.5`));
        check(placed === 'applied', 'the player can place the probe order on the --as any server',
              placed);

        let foundId = null;
        let sweepLeaked = false;
        for (let id = 1; id <= 4096 && foundId === null; ++id) {
          const r = resultOf(await io2.send(
            `COMMAND corp=${subject.id} verb=10 order=${id}`));
          if (r === 'applied') foundId = id;
          else if (r === 'rejected_not_owner') sweepLeaked = true;
        }
        check(!sweepLeaked,
              'sweeping the order-id space never surfaces rejected_not_owner (the oracle is gone)');
        check(foundId !== null, 'the sweep finds and removes the player\'s own order',
              String(foundId));

        if (foundId !== null) {
          const rivalPlaced = resultOf(await io2.send(
            `COMMAND corp=${rival.id} verb=9 subject=${listBody} target=${listRes} `
            + `quantity=5 floor_price=0.5`));
          check(rivalPlaced === 'applied', 'a rival order exists to probe against', rivalPlaced);
          const foreignId = foundId + 1; // next allocation of the monotonic counter

          const foreign = resultOf(await io2.send(
            `COMMAND corp=${subject.id} verb=10 order=${foreignId}`));
          const nonexistent = resultOf(await io2.send(
            `COMMAND corp=${subject.id} verb=10 order=4000000000`));
          check(foreign === 'rejected_invalid' && nonexistent === 'rejected_invalid'
                && foreign === nonexistent,
                'a foreign order id and a nonexistent order id answer the SAME result string',
                `foreign=${foreign} nonexistent=${nonexistent}`);
        }
      } else {
        console.log('         (no priced (body, resource) pair found earlier; '
                    + 'order-oracle check skipped and says so)');
      }
    } finally {
      io2.close();
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
