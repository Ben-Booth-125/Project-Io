#!/usr/bin/env node
// sound_cues.js — read scripts/sounds.lua (the cue table, docs/ui/SOUND.md) without a Lua
// interpreter, check it, and build the listening page.
//
//   node tools/session/sound_cues.js            list every cue: family, name, length, when
//   node tools/session/sound_cues.js --check    validate fields; non-zero exit on a fault
//   node tools/session/sound_cues.js --json     print the table as JSON
//   node tools/session/sound_cues.js --page <out.html>
//                                              write the listening page (template:
//                                              tools/session/sound_page.html)
//
// The table is restricted to plain literals (tables, strings, numbers, booleans, `--`
// comments) so this parser and the game read the same thing. The synthesis the page runs
// mirrors src/audio/synth.cpp; change both together.
'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const LUA = path.join(ROOT, 'scripts', 'sounds.lua');
const TEMPLATE = path.join(__dirname, 'sound_page.html');

function parseLua(src) {
  let i = 0;
  const err = (m) => { throw new Error(`sounds.lua: ${m} at offset ${i}`); };
  const ws = () => {
    for (;;) {
      while (i < src.length && /\s/.test(src[i])) i++;
      if (src.startsWith('--', i)) { while (i < src.length && src[i] !== '\n') i++; continue; }
      return;
    }
  };
  const value = () => {
    ws();
    const c = src[i];
    if (c === '{') return table();
    if (c === '"' || c === "'") {
      const q = c; i++; let s = '';
      while (src[i] !== q) { if (i >= src.length) err('unterminated string'); if (src[i] === '\\') { i++; } s += src[i++]; }
      i++; return s;
    }
    const m = /^-?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?/.exec(src.slice(i));
    if (m) { i += m[0].length; return Number(m[0]); }
    if (src.startsWith('true', i)) { i += 4; return true; }
    if (src.startsWith('false', i)) { i += 5; return false; }
    err('unexpected value');
  };
  const table = () => {
    i++; // {
    const obj = {}; const arr = []; let isArr = null;
    for (;;) {
      ws();
      if (src[i] === '}') { i++; break; }
      const km = /^([A-Za-z_][A-Za-z0-9_]*)\s*=/.exec(src.slice(i));
      if (km) {
        if (isArr === true) err('mixed table');
        isArr = false; i += km[0].length; obj[km[1]] = value();
      } else {
        if (isArr === false) err('mixed table');
        isArr = true; arr.push(value());
      }
      ws();
      if (src[i] === ',' || src[i] === ';') i++;
    }
    return isArr ? arr : obj;
  };
  ws();
  if (!src.startsWith('return', i)) err('expected `return {`');
  i += 6;
  const v = value();
  ws();
  if (i < src.length) err('trailing content');
  return v;
}

const FAMILY = (name) =>
  name.startsWith('ui_') ? 'UI presses' :
  name.startsWith('alert_') ? 'Alerts' :
  name.startsWith('construction_') ? 'Construction' :
  /^(war_|battle_)/.test(name) ? 'Conflict' :
  name.startsWith('research_') ? 'Research' :
  name.startsWith('time_') ? 'Time' :
  name.startsWith('space_') ? 'Space' : 'Other';

const cueLength = (c) => Math.max(...c.notes.map((n) => (n.at || 0) + n.a + (n.h || 0) + n.r));

function check(t) {
  const faults = [];
  const num = (v, lo, hi) => typeof v === 'number' && v >= lo && v <= hi;
  for (const [name, c] of Object.entries(t.cues || {})) {
    if (!['ui', 'event'].includes(c.group)) faults.push(`${name}: group must be ui|event`);
    if (!num(c.priority, 1, 9)) faults.push(`${name}: priority 1..9`);
    if (!num(c.gain, 0, 1)) faults.push(`${name}: gain 0..1`);
    if (typeof c.when !== 'string' || !c.when) faults.push(`${name}: when missing`);
    if (!Array.isArray(c.notes) || !c.notes.length) { faults.push(`${name}: no notes`); continue; }
    c.notes.forEach((n, k) => {
      const at = `${name}[${k + 1}]`;
      if (!['sine', 'tri', 'square', 'noise'].includes(n.wave)) faults.push(`${at}: wave`);
      if (n.wave !== 'noise' && !num(n.f, 20, 20000)) faults.push(`${at}: f 20..20000`);
      if (n.f1 !== undefined && !num(n.f1, 20, 20000)) faults.push(`${at}: f1 20..20000`);
      if (!num(n.a, 0.0005, 5)) faults.push(`${at}: a 0.0005..5`);
      if (!num(n.r, 0.005, 10)) faults.push(`${at}: r 0.005..10`);
      if (n.h !== undefined && !num(n.h, 0, 10)) faults.push(`${at}: h 0..10`);
      if (n.at !== undefined && !num(n.at, 0, 10)) faults.push(`${at}: at 0..10`);
      if (!num(n.g, 0, 1)) faults.push(`${at}: g 0..1`);
      if (n.lp !== undefined && !num(n.lp, 20, 20000)) faults.push(`${at}: lp 20..20000`);
      if (n.lp1 !== undefined && (n.lp === undefined || !num(n.lp1, 20, 20000))) faults.push(`${at}: lp1 needs lp, 20..20000`);
    });
    if (cueLength(c) > 3) faults.push(`${name}: longer than 3 s`);
  }
  const b = t.bed;
  if (!b || !num(b.gain, 0, 1) || !Array.isArray(b.partials)) faults.push('bed: gain and partials required');
  return faults;
}

function main() {
  const args = process.argv.slice(2);
  const t = parseLua(fs.readFileSync(LUA, 'utf8'));
  if (args.includes('--json')) { process.stdout.write(JSON.stringify(t, null, 1) + '\n'); return; }
  if (args.includes('--check')) {
    const f = check(t);
    if (f.length) { console.log(`sound_cues: ${f.length} fault(s)`); f.forEach((x) => console.log('  ' + x)); process.exit(1); }
    console.log(`sound_cues: ${Object.keys(t.cues).length} cues, bed OK, 0 faults`);
    return;
  }
  const pi = args.indexOf('--page');
  if (pi >= 0) {
    const out = args[pi + 1];
    if (!out) { console.error('--page needs an output path'); process.exit(2); }
    const f = check(t);
    if (f.length) { console.error('fix the table first (--check)'); process.exit(1); }
    const cues = Object.entries(t.cues).map(([name, c]) => ({ name, family: FAMILY(name), ...c }));
    const html = fs.readFileSync(TEMPLATE, 'utf8')
      .replace('/*CUE_DATA*/null', JSON.stringify({ cues, bed: t.bed }));
    fs.writeFileSync(out, html);
    console.log(`sound_cues: wrote ${out} (${cues.length} cues)`);
    return;
  }
  for (const [name, c] of Object.entries(t.cues))
    console.log(`${FAMILY(name).padEnd(13)} ${name.padEnd(22)} ${cueLength(c).toFixed(2)} s  ${c.when}`);
}

main();
