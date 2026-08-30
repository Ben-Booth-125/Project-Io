#!/usr/bin/env node
// no_backlog_ids — no backlog id may reach the player's screen (BL-690).
//
// WHY THIS IS A SOURCE SCAN AND NOT A RUNTIME CHECK. The obvious form — capture
// some frames and search the drawn text — can only ever see the surfaces a script
// happened to open. It would report green while "BL-087 design mock" sat on the
// Research panel simply because no capture opened it, which is the same vacuous
// greenness that let four of these ship (and that NR-663 records for
// `expect_no_clipping` on this very class of surface). The property wanted here
// is static and total: NO string literal that can be drawn contains a backlog id.
// A scan of the source proves that; a scan of one run's pixels cannot.
//
// It also catches the pattern being COPIED, which is the actual concern — the
// fourth instance landed the day before the sweep that found them.
//
// Run: node tools/verify/no_backlog_ids.js
// Exit: 0 clean, 1 on any finding, 2 on a usage error.

const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const SCAN_DIRS = ['src'];
const BL_ID = /\bBL-\d{2,4}\b/;

// Strip // and /* */ comments so a BL id in a code comment — which is legitimate
// and encouraged, it is how a design decision cites its owner — is not reported.
// Handles string literals and char literals so a "//" INSIDE a string is not
// mistaken for a comment start.
function stripComments(src) {
  let out = '';
  let i = 0;
  const n = src.length;
  while (i < n) {
    const c = src[i];
    const d = src[i + 1];
    if (c === '/' && d === '/') {
      while (i < n && src[i] !== '\n') { out += ' '; i++; }
    } else if (c === '/' && d === '*') {
      out += '  '; i += 2;
      while (i < n && !(src[i] === '*' && src[i + 1] === '/')) {
        out += (src[i] === '\n') ? '\n' : ' ';
        i++;
      }
      out += '  '; i += 2;
    } else if (c === '"') {
      out += c; i++;
      while (i < n) {
        if (src[i] === '\\') { out += src[i] + (src[i + 1] || ''); i += 2; continue; }
        out += src[i];
        if (src[i] === '"') { i++; break; }
        i++;
      }
    } else if (c === "'") {
      out += c; i++;
      while (i < n) {
        if (src[i] === '\\') { out += src[i] + (src[i + 1] || ''); i += 2; continue; }
        out += src[i];
        if (src[i] === "'") { i++; break; }
        i++;
      }
    } else {
      out += c; i++;
    }
  }
  return out;
}

// Every double-quoted literal, with the line it starts on.
function stringLiterals(src) {
  const found = [];
  let line = 1;
  let i = 0;
  const n = src.length;
  while (i < n) {
    if (src[i] === '\n') { line++; i++; continue; }
    if (src[i] !== '"') { i++; continue; }
    const startLine = line;
    let lit = '';
    i++;
    while (i < n) {
      if (src[i] === '\\') { lit += src[i + 1] || ''; i += 2; continue; }
      if (src[i] === '"') { i++; break; }
      if (src[i] === '\n') line++;
      lit += src[i];
      i++;
    }
    found.push({ line: startLine, text: lit });
  }
  return found;
}

function walk(dir, acc) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) walk(p, acc);
    else if (/\.(cpp|hpp|h|cc)$/.test(e.name)) acc.push(p);
  }
  return acc;
}

// An #include path may legitimately carry an id-looking token; nothing else may.
function isIncludeLine(rawLines, lineNo) {
  const l = rawLines[lineNo - 1] || '';
  return /^\s*#\s*include/.test(l);
}

let findings = [];
let scanned = 0;

for (const d of SCAN_DIRS) {
  const abs = path.join(ROOT, d);
  if (!fs.existsSync(abs)) continue;
  for (const file of walk(abs, [])) {
    const raw = fs.readFileSync(file, 'utf8');
    const rawLines = raw.split('\n');
    scanned++;
    for (const lit of stringLiterals(stripComments(raw))) {
      if (!BL_ID.test(lit.text)) continue;
      if (isIncludeLine(rawLines, lit.line)) continue;
      findings.push({
        file: path.relative(ROOT, file).replace(/\\/g, '/'),
        line: lit.line,
        text: lit.text.length > 90 ? lit.text.slice(0, 90) + '...' : lit.text,
      });
    }
  }
}

console.log('=== no_backlog_ids (BL-690) ===');
console.log(`scanned ${scanned} source files under ${SCAN_DIRS.join(', ')}\n`);

if (findings.length === 0) {
  console.log('  [PASS] no backlog id appears in any string literal');
  console.log('\n=== no_backlog_ids: 0 finding(s) ===');
  process.exit(0);
}

for (const f of findings) {
  console.log(`  [FAIL] ${f.file}:${f.line}  "${f.text}"`);
}
console.log(
  '\nA backlog id is internal bookkeeping and means nothing to a player. Keep what\n' +
  'the line is honestly saying — that a control is not wired, that a surface is a\n' +
  'mock — and drop the reference.');
console.log(`\n=== no_backlog_ids: ${findings.length} finding(s) ===`);
process.exit(1);
