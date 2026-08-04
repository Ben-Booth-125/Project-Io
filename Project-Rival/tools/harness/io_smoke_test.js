#!/usr/bin/env node
// io_smoke_test.js — BL-306 conformance smoke test for Io's word interface.
//
// The arena is Project Io itself (NR-060): this gates every text-mode Rival
// session the way the retired RTS smoke gated the RTS bench. It drives the Io
// MCP server (tools/mcp/server.js, BL-278) over MCP's newline-delimited
// JSON-RPC, exercising all three legs of the word interface:
//   read    — get_blackboard returns the full current-tick blackboard
//   meaning — lookup_action resolves a dictionary entry
//   write   — issue_command accepts a legal verb and REJECTS an illegal one
//             as a typed result (not a silent no-op, not a crash)
//
// Usage: node io_smoke_test.js   (from anywhere; paths resolved from repo root)

const { spawn } = require('child_process');
const path = require('path');
const readline = require('readline');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const SERVER = path.join(ROOT, 'tools', 'mcp', 'server.js');

const server = spawn('node', [SERVER], { cwd: ROOT, stdio: ['pipe', 'pipe', 'inherit'] });
const rl = readline.createInterface({ input: server.stdout });

let nextId = 1;
const inflight = new Map();

rl.on('line', (line) => {
  let msg;
  try { msg = JSON.parse(line); } catch (e) { return; }
  if (msg.id !== undefined && inflight.has(msg.id)) {
    inflight.get(msg.id)(msg);
    inflight.delete(msg.id);
  }
});

function rpc(method, params, timeoutMs) {
  const id = nextId++;
  const req = { jsonrpc: '2.0', id, method, params: params || {} };
  return new Promise((resolve, reject) => {
    const t = setTimeout(() => reject(new Error(`timeout on ${method}`)), timeoutMs || 120000);
    inflight.set(id, (msg) => { clearTimeout(t); resolve(msg); });
    server.stdin.write(JSON.stringify(req) + '\n');
  });
}

const checks = [];
function check(name, ok, detail) {
  checks.push({ name, ok });
  console.log(`${ok ? 'PASS' : 'FAIL'}  ${name}${detail ? '  — ' + detail : ''}`);
}

function toolText(msg) {
  const c = msg.result && msg.result.content;
  return c && c[0] && c[0].text ? c[0].text : '';
}

async function main() {
  const init = await rpc('initialize', {
    protocolVersion: '2025-06-18',
    capabilities: {},
    clientInfo: { name: 'io-smoke', version: '0.1' },
  });
  check('IO-1 initialize', !!(init.result && init.result.serverInfo),
    init.result && init.result.serverInfo && init.result.serverInfo.name);

  const tools = await rpc('tools/list');
  const names = ((tools.result && tools.result.tools) || []).map((t) => t.name);
  const expected = ['get_blackboard', 'issue_command', 'advance_tick', 'lookup_action', 'list_actions'];
  check('IO-2 five tools listed', expected.every((n) => names.includes(n)), names.join(','));

  // Who am I? — list_corps answers the agent's first question (NR-061).
  const corpsMsg = await rpc('tools/call', { name: 'list_corps', arguments: {} });
  let corps = [];
  try { corps = JSON.parse(toolText(corpsMsg)).corps || []; } catch (e) { /* leave empty */ }
  const player = corps.find((c) => c.is_player) || corps[0];
  const corpId = player ? player.id : null;
  check('IO-3a corps enumerate', corps.length > 0 && !!player,
    player ? `${corps.length} corps; player = ${player.name} (id ${player.id})` : toolText(corpsMsg).slice(0, 80));

  let bbText = '';
  let facts = 0;
  if (corpId !== null) {
    const bb = await rpc('tools/call', { name: 'get_blackboard', arguments: { corp: corpId } });
    bbText = toolText(bb);
    try { facts = JSON.parse(bbText).facts.length; } catch (e) { /* leave 0 */ }
  }
  check('IO-3b blackboard reads', facts > 0, `corp ${corpId}: ${bbText.length} bytes, ${facts} facts`);

  const tick = await rpc('tools/call', { name: 'advance_tick', arguments: {} });
  check('IO-4 ticks advance', !tick.error && !(tick.result && tick.result.isError), toolText(tick).slice(0, 80));

  const look = await rpc('tools/call', { name: 'lookup_action', arguments: { query: 'build' } });
  const lookText = toolText(look);
  check('IO-5 dictionary resolves', lookText.length > 0 && !(look.result && look.result.isError),
    lookText.slice(0, 60).replace(/\n/g, ' '));

  // The legality check: a verb with nonsense args must come back as a typed
  // rejection — an error RESULT, not a hang, not a crash, not silent success.
  const bad = await rpc('tools/call', {
    name: 'issue_command',
    arguments: { corp: corpId === null ? 1 : corpId, verb: 'demolish', subject: 999999999 },
  });
  const badText = toolText(bad);
  const rejected = (bad.result && bad.result.isError) || /ERR|reject|invalid|unknown|fail/i.test(badText);
  check('IO-6 illegal command rejected (typed)', rejected, badText.slice(0, 80).replace(/\n/g, ' '));

  const failed = checks.filter((c) => !c.ok).length;
  console.log(`\n${failed === 0 ? 'SMOKE PASS' : 'SMOKE FAIL'}: ${checks.length - failed}/${checks.length} checks passed`);
  process.exitCode = failed === 0 ? 0 : 1;
  server.kill();
}

main().catch((e) => { console.error('SMOKE FAIL:', e.message); server.kill(); process.exitCode = 1; });
