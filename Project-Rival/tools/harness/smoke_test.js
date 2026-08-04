#!/usr/bin/env node
// smoke_test.js — BL-306 conformance smoke test for the 0 A.D. RL interface.
//
// Gates every text-mode campaign session: proves the seam is alive, the state
// parses, and steps advance, before any agent plays through it.
//
// Protocol (from source/tools/rlclient/python/zero_ad/api.py, pinned R28):
//   POST /reset?playerID=1   body = scenario config JSON  -> game state JSON
//   POST /step               body = "{player};{json}" lines -> game state JSON
//   POST /evaluate           body = JS source              -> JSON result
//
// Usage: node smoke_test.js [--exe <path-to-pyrogenesis.exe>] [--port 6000]
//        If --exe is given, the engine is spawned headless and killed at exit;
//        otherwise an already-running engine on the port is used.

const http = require('http');
const fs = require('fs');
const path = require('path');
const { spawn } = require('child_process');

const args = process.argv.slice(2);
const opt = (name, dflt) => {
    const i = args.indexOf('--' + name);
    return i >= 0 ? args[i + 1] : dflt;
};
const PORT = parseInt(opt('port', '6000'), 10);
const EXE = opt('exe', null);

function post(pathname, body) {
    return new Promise((resolve, reject) => {
        const req = http.request(
            { host: '127.0.0.1', port: PORT, path: pathname, method: 'POST' },
            (res) => {
                const chunks = [];
                res.on('data', (c) => chunks.push(c));
                res.on('end', () => resolve({ status: res.statusCode, body: Buffer.concat(chunks).toString('utf8') }));
            }
        );
        req.on('error', reject);
        req.setHeader('Content-Type', 'text/plain');
        req.end(Buffer.from(body, 'utf8'));
    });
}

async function waitForPort(timeoutMs) {
    const start = Date.now();
    for (;;) {
        try {
            await post('templates', '');
            return;
        } catch (e) {
            if (Date.now() - start > timeoutMs) throw new Error('engine never opened port ' + PORT);
            await new Promise((r) => setTimeout(r, 1000));
        }
    }
}

const checks = [];
function check(name, ok, detail) {
    checks.push({ name, ok, detail: detail || '' });
    console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? '  — ' + detail : ''}`);
}

async function main() {
    let engine = null;
    if (EXE) {
        engine = spawn(EXE, ['--rl-interface=127.0.0.1:' + PORT, '-autostart-nonvisual'], {
            stdio: 'ignore',
            detached: false,
        });
        engine.on('exit', (code) => { if (code) console.log('engine exited with code', code); });
    }
    try {
        await waitForPort(90_000);
        check('R28-1 port open', true, `RL interface answering on :${PORT}`);

        const config = fs.readFileSync(path.join(__dirname, 'smoke_config.json'), 'utf8');
        const reset = await post('reset?playerID=1', config);
        check('R28-2 reset accepted', reset.status === 200, `HTTP ${reset.status}`);

        let state;
        try { state = JSON.parse(reset.body); } catch (e) { state = null; }
        check('R28-3 state parses as JSON', !!state, state ? `${reset.body.length} bytes` : reset.body.slice(0, 200));

        const ents = state && state.entities ? Object.keys(state.entities).length : 0;
        check('R28-4 entities present', ents > 0, `${ents} entities`);

        let last = state;
        for (let i = 0; i < 10; i++) {
            const step = await post('step', '');
            last = JSON.parse(step.body);
        }
        const t0 = state ? JSON.stringify(state).length : 0;
        check('R28-5 ten steps advance', !!last, `state ${t0} -> ${JSON.stringify(last).length} bytes`);

        const evalRes = await post('evaluate', '1 + 1');
        check('R28-6 evaluate seam', evalRes.status === 200 && evalRes.body.trim() === '2', `got ${evalRes.body.trim().slice(0, 40)}`);

        const failed = checks.filter((c) => !c.ok).length;
        console.log(`\n${failed === 0 ? 'SMOKE PASS' : 'SMOKE FAIL'}: ${checks.length - failed}/${checks.length} checks passed`);
        process.exitCode = failed === 0 ? 0 : 1;
    } finally {
        if (engine) engine.kill();
    }
}

main().catch((e) => { console.error('SMOKE FAIL:', e.message); process.exitCode = 1; });
