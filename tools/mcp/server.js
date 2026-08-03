#!/usr/bin/env node
// tools/mcp/server.js — Io MCP server (BL-278, docs/ai/AI_OPPONENT.md § 10).
//
// Exposes the three legs of the word interface — the blackboard export
// (BL-206), the action dictionary (BL-270), and the corp-command seam
// (src/world/corp_command.hpp) — over the Model Context Protocol, so any MCP
// client (Claude Code/Desktop, a local-model agent loop, a bespoke harness)
// can drive Io. Ships no HTTP client and no API key inside ProjectIo.exe: this
// is a separate local process that spawns `ProjectIo --serve` as a child and
// speaks its line protocol (see run_serve in src/main.cpp) on its stdin/
// stdout. With nothing attached, the game is unaffected.
//
// Wire protocol here is MCP-over-stdio: newline-delimited JSON-RPC 2.0 on
// this process's own stdin/stdout. No SDK dependency (none is installed in
// this repo) — the surface is small enough to hand-roll: initialize,
// tools/list, tools/call, resources/list, resources/read.
//
// Design calls this seam settled (BL-278's `design` field):
//   - push state, don't make the model pull it -> get_blackboard always
//     returns the full current-tick blackboard, never a diff the model must
//     reconstruct.
//   - mask to legal primitives -> issue_command's verbs are exactly
//     corp_command's; a rejection is a typed, loggable result, not a silent
//     no-op.
//   - never ask the model to carry world state across turns -> every tool
//     call is stateless from the caller's side; state lives in the spawned
//     ProjectIo process, not in the conversation.

const { spawn } = require('child_process');
const path = require('path');
const readline = require('readline');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..', '..');
const EXE = path.join(ROOT, 'build', 'ProjectIo.exe');
const ACTIONS_QUERY = path.join(ROOT, 'tools', 'session', 'actions_query.js');

// corp_command.hpp's corp_verb, in declaration order — the tool schema names
// these; the wire protocol to ProjectIo --serve still sends the integer.
const VERBS = ['build', 'demolish', 'set_recipe', 'set_workforce', 'idle', 'resume', 'place_road', 'survey'];

// ---------------------------------------------------------------------------
// The ProjectIo --serve child process
// ---------------------------------------------------------------------------

let child = null;
let childRl = null;
let pending = null; // {resolve, reject, lines: string[]} for the in-flight request

function ensureChild() {
  if (child) return;
  child = spawn(EXE, ['--serve', '--ticks', '12'], { cwd: ROOT });
  childRl = readline.createInterface({ input: child.stdout });
  child.on('exit', () => { child = null; childRl = null; });
  childRl.on('line', (line) => {
    if (line.startsWith('[Lua]')) return; // world-build startup banner, not a response
    if (!pending) return;
    if (line === 'END' || line.startsWith('OK ') || line.startsWith('RESULT ')
        || line.startsWith('ERR ') || line === 'BYE') {
      pending.lines.push(line);
      pending.resolve(pending.lines);
      pending = null;
    } else {
      pending.lines.push(line);
    }
  });
}

/// Send one request line to the --serve process; resolve with its response
/// lines (the terminator line included).
function sendRequest(line) {
  ensureChild();
  return new Promise((resolve, reject) => {
    if (pending) { reject(new Error('a request is already in flight')); return; }
    pending = { resolve, reject, lines: [] };
    child.stdin.write(line + '\n');
  });
}

function kv(obj) {
  return Object.entries(obj)
    .filter(([, v]) => v !== undefined && v !== null)
    .map(([k, v]) => `${k}=${v}`)
    .join(' ');
}

// ---------------------------------------------------------------------------
// Tools
// ---------------------------------------------------------------------------

const TOOLS = [
  {
    name: 'get_blackboard',
    description: 'Read a corporation\'s visibility-honest blackboard (BL-206): every fact it '
      + 'is entitled to know, pushed in full rather than requiring follow-up queries.',
    inputSchema: {
      type: 'object',
      properties: {
        corp: { type: 'integer', description: 'Corporation entity id.' },
        ticks: { type: 'integer', description: 'Tick to read at (defaults to the server\'s current tick).' },
      },
      required: ['corp'],
    },
  },
  {
    name: 'issue_command',
    description: 'Apply one corp_command through the player-grade validation seam '
      + '(src/world/corp_command.hpp — no bypass). Returns the typed result: applied, or one '
      + 'of the rejected_* reasons.',
    inputSchema: {
      type: 'object',
      properties: {
        corp: { type: 'integer', description: 'Acting corporation entity id.' },
        verb: { type: 'string', enum: VERBS },
        subject: { type: 'integer', description: 'Building (demolish/set_recipe/set_workforce/idle/resume) or body (survey) entity id.' },
        tile: { type: 'integer', description: 'Target tile entity id (build / place_road).' },
        type: { type: 'integer', description: 'building_type (build only): 0 none, 1 extraction_site, 2 processing_facility, 3 port, 4 launchpad, 5 inland_logistics_hub.' },
        target: { type: 'integer', description: 'resource_type index (build/extraction only) — see docs/economy/RESOURCES.md.' },
        recipe: { type: 'integer', description: 'Recipe id (set_recipe, and build seed).' },
        workforce: { type: 'integer', description: 'set_workforce value, 0-200.' },
        road_tier: { type: 'integer', description: 'place_road tier, 1-3.' },
      },
      required: ['corp', 'verb'],
    },
  },
  {
    name: 'advance_tick',
    description: 'Advance the running world by one tick (dispatch/advance convoys -> '
      + 'run_economy_step -> clear_markets -> apply_budget -> credit convoys), the same '
      + 'sequence the interactive app runs.',
    inputSchema: { type: 'object', properties: {} },
  },
  {
    name: 'lookup_action',
    description: 'Fetch one or more full entries from the action dictionary (BL-270) by exact '
      + 'id, family (gameplay|canvas|lens|ledger|chrome), or keyword — the `reason_to_select` '
      + 'field is the design-intent prior for choosing between controls.',
    inputSchema: {
      type: 'object',
      properties: { query: { type: 'string' } },
      required: ['query'],
    },
  },
  {
    name: 'list_actions',
    description: 'The compact [id, surface] index of every control in the game '
      + '(ACTIONS_INDEX.json) — hold this in context, fetch full entries with lookup_action.',
    inputSchema: { type: 'object', properties: {} },
  },
];

async function callTool(name, args) {
  if (name === 'get_blackboard') {
    const lines = await sendRequest(`BLACKBOARD ${kv({ corp: args.corp, ticks: args.ticks })}`);
    const facts = lines.filter((l) => l !== 'END').map((l) => JSON.parse(l));
    return { facts };
  }
  if (name === 'issue_command') {
    const verbIdx = VERBS.indexOf(args.verb);
    if (verbIdx === -1) throw new Error(`unknown verb '${args.verb}' — expected one of ${VERBS.join(', ')}`);
    const lines = await sendRequest(`COMMAND ${kv({ ...args, verb: verbIdx })}`);
    const resultLine = lines.find((l) => l.startsWith('RESULT '));
    const m = /result=(\S+) building=(-?\d+)/.exec(resultLine || '');
    return { result: m ? m[1] : 'unknown', building: m ? Number(m[2]) : -1 };
  }
  if (name === 'advance_tick') {
    const lines = await sendRequest('TICK');
    const m = /tick=(\d+)/.exec(lines[0] || '');
    return { tick: m ? Number(m[1]) : null };
  }
  if (name === 'lookup_action') {
    const out = execFileSync('node', [ACTIONS_QUERY, args.query], { cwd: ROOT, encoding: 'utf8' });
    return JSON.parse(out);
  }
  if (name === 'list_actions') {
    const out = execFileSync('node', [ACTIONS_QUERY, '--index'], { cwd: ROOT, encoding: 'utf8' });
    return JSON.parse(out);
  }
  throw new Error(`unknown tool '${name}'`);
}

// ---------------------------------------------------------------------------
// Resources — the blackboard leg, addressed by URI template
// ---------------------------------------------------------------------------

const RESOURCE_TEMPLATE = { uriTemplate: 'blackboard://{corp}', name: 'corp_blackboard', description: TOOLS[0].description, mimeType: 'application/json' };

async function readResource(uri) {
  const m = /^blackboard:\/\/(\d+)$/.exec(uri);
  if (!m) throw new Error(`unrecognised resource uri '${uri}' — expected blackboard://<corp>`);
  const result = await callTool('get_blackboard', { corp: Number(m[1]) });
  return { contents: [{ uri, mimeType: 'application/json', text: JSON.stringify(result) }] };
}

// ---------------------------------------------------------------------------
// MCP JSON-RPC 2.0 over this process's own stdio
// ---------------------------------------------------------------------------

const rl = readline.createInterface({ input: process.stdin });

function send(msg) {
  process.stdout.write(JSON.stringify(msg) + '\n');
}

function reply(id, result) {
  send({ jsonrpc: '2.0', id, result });
}

function replyError(id, code, message) {
  send({ jsonrpc: '2.0', id, error: { code, message } });
}

rl.on('line', async (raw) => {
  let req;
  try { req = JSON.parse(raw); } catch { return; }
  const { id, method, params } = req;

  try {
    if (method === 'initialize') {
      reply(id, {
        protocolVersion: '2024-11-05',
        capabilities: { tools: {}, resources: {} },
        serverInfo: { name: 'io-mcp-server', version: '0.1.0' },
      });
    } else if (method === 'tools/list') {
      reply(id, { tools: TOOLS });
    } else if (method === 'tools/call') {
      const result = await callTool(params.name, params.arguments || {});
      reply(id, { content: [{ type: 'text', text: JSON.stringify(result) }] });
    } else if (method === 'resources/templates/list') {
      reply(id, { resourceTemplates: [RESOURCE_TEMPLATE] });
    } else if (method === 'resources/read') {
      reply(id, await readResource(params.uri));
    } else if (method === 'shutdown') {
      if (child) { child.stdin.write('SHUTDOWN\n'); }
      reply(id, {});
    } else if (id !== undefined) {
      replyError(id, -32601, `method not found: ${method}`);
    }
    // notifications (no id) with unhandled methods are silently ignored.
  } catch (e) {
    if (id !== undefined) replyError(id, -32000, e.message);
  }
});

process.on('exit', () => { if (child) child.kill(); });
