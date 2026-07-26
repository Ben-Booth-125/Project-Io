#!/usr/bin/env node
/*
 * backlog_view.js - render docs/development/backlog.json as an HTML dashboard.
 *
 * The at-a-glance companion to tools/status.ps1: where status.ps1 answers in the
 * terminal, this renders a self-contained HTML page - top actionable priorities up
 * front, the full open list grouped by priority, blocked items flagged with what
 * they wait on, and each item expandable to its summary + design prose.
 *
 * Zero dependencies. Output goes to out/ (gitignored) and opens in the default
 * browser unless told otherwise.
 *
 * Usage:
 *   node tools/backlog_view.js               # write out/backlog_view.html + open it
 *   node tools/backlog_view.js --no-open     # write only
 *   node tools/backlog_view.js --out <path>  # custom output path
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { execFile } = require('child_process');

// ---- CLI ----
const argv = process.argv.slice(2);
const noOpen = argv.includes('--no-open');
const outArg = argv.indexOf('--out');
const repo = path.resolve(__dirname, '..');
const outPath = outArg >= 0 && argv[outArg + 1]
  ? path.resolve(argv[outArg + 1])
  : path.join(repo, 'out', 'backlog_view.html');

// ---- Load + derive ----
const backlogPath = path.join(repo, 'docs', 'development', 'backlog.json');
const items = JSON.parse(fs.readFileSync(backlogPath, 'utf8')).items;

const TERMINAL = new Set(['complete', 'shipped', 'delivered']);
const PRI_ORDER = ['SSS', 'S', 'A', 'B', 'C', 'F'];
const priRank = p => { const i = PRI_ORDER.indexOf(p); return i < 0 ? PRI_ORDER.length : i; };
const idNum = id => parseInt(String(id).replace(/\D/g, ''), 10) || 0;

const doneIds = new Set(items.filter(i => TERMINAL.has(i.status)).map(i => i.id));
const blockersOf = it => [
  ...(it.requires || []).filter(r => r && !doneIds.has(r)),
  ...(it.blocked_on || []).filter(Boolean),
];

const enriched = items.map(it => ({
  ...it,
  done: TERMINAL.has(it.status),
  blockers: TERMINAL.has(it.status) ? [] : blockersOf(it),
}));

const open = enriched.filter(i => !i.done);
const done = enriched.filter(i => i.done);
const actionable = open
  .filter(i => !i.parked && i.blockers.length === 0)
  .sort((a, b) => priRank(a.priority) - priRank(b.priority) || idNum(a.id) - idNum(b.id));

// ---- HTML ----
const esc = s => String(s ?? '')
  .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');

// Minimal markdown for summary/design prose: escape first, then bold / code / paragraphs.
const md = s => esc(s)
  .replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>')
  .replace(/`([^`]+)`/g, '<code>$1</code>')
  .split(/\n{2,}/).map(p => `<p>${p.replace(/\n/g, '<br>')}</p>`).join('');

const statusMark = s => s === 'designed' ? '&#10003;' : s === 'design-owed' ? '~' : '&bull;';
const diffDots = d => d ? '&#9679;'.repeat(d) + '&#9675;'.repeat(Math.max(0, 5 - d)) : '';

function itemRow(it) {
  const blocked = it.blockers.length > 0;
  const cls = ['item', blocked && 'blocked', it.parked && 'parked', it.done && 'done']
    .filter(Boolean).join(' ');
  const chips = [
    it.version_goal && `<span class="chip goal">${esc(it.version_goal)}</span>`,
    `<span class="chip cat">${esc(it.category)}</span>`,
    it.difficulty != null && `<span class="chip diff" title="difficulty ${it.difficulty}/5">${diffDots(it.difficulty)}</span>`,
    blocked && `<span class="chip wait">waiting on ${it.blockers.map(esc).join(', ')}</span>`,
    it.parked && `<span class="chip parkchip">parked</span>`,
    it.done && `<span class="chip donechip">${esc(it.resolved || it.status)}</span>`,
  ].filter(Boolean).join('');
  return `<details class="${cls}" data-search="${esc([it.id, it.short_name, it.title, it.category, it.version_goal].join(' ').toLowerCase())}" data-cat="${esc(it.category)}">
  <summary>
    <span class="mark" title="${esc(it.status)}">${statusMark(it.status)}</span>
    <span class="id">${esc(it.id)}</span>
    <span class="title">${esc(it.title)}</span>
    <span class="chips">${chips}</span>
  </summary>
  <div class="body">
    <div class="meta">${esc(it.short_name)} &middot; status: ${esc(it.status)} &middot; written ${esc(it.written)}${it.authority_doc ? ` &middot; authority: <code>${esc(it.authority_doc)}</code>` : ''}</div>
    ${it.summary ? `<div class="prose">${md(it.summary)}</div>` : ''}
    ${it.design ? `<details class="design"><summary>design prose</summary><div class="prose">${md(it.design)}</div></details>` : ''}
    ${(it.files || []).length ? `<div class="files">${it.files.map(f => `<code>${esc(f)}</code>`).join(' ')}</div>` : ''}
  </div>
</details>`;
}

function topCard(it, rank) {
  const firstSentence = String(it.summary || '').split(/(?<=\.)\s/)[0] || '';
  return `<a class="card pri-${esc(it.priority)}" href="#i-${esc(it.id)}" onclick="reveal('${esc(it.id)}')">
  <div class="card-top"><span class="pri">${esc(it.priority)}</span><span class="rank">#${rank}</span></div>
  <div class="card-id">${esc(it.id)} ${esc(it.short_name)}</div>
  <div class="card-title">${esc(it.title)}</div>
  <div class="card-sum">${esc(firstSentence)}</div>
  <div class="card-foot"><span title="difficulty">${diffDots(it.difficulty)}</span>${it.version_goal ? `<span class="chip goal">${esc(it.version_goal)}</span>` : ''}</div>
</a>`;
}

const priSections = PRI_ORDER.map(p => {
  const inPri = open.filter(i => i.priority === p)
    .sort((a, b) => (a.blockers.length > 0) - (b.blockers.length > 0) || idNum(a.id) - idNum(b.id));
  if (!inPri.length) return '';
  return `<section class="pri-section">
  <h2><span class="pri pri-${p}">${p}</span> <span class="count">${inPri.length} open</span></h2>
  ${inPri.map(i => `<div id="i-${esc(i.id)}">${itemRow(i)}</div>`).join('\n')}
</section>`;
}).join('\n');

const doneSection = `<section class="pri-section" id="done-section" hidden>
  <h2><span class="pri done-pri">DONE</span> <span class="count">${done.length} delivered</span></h2>
  ${done.slice().sort((a, b) => String(b.resolved || '').localeCompare(String(a.resolved || ''))).map(itemRow).join('\n')}
</section>`;

const cats = [...new Set(open.map(i => i.category))].sort();
const byPri = PRI_ORDER.map(p => [p, open.filter(i => i.priority === p).length]).filter(([, n]) => n);
const pct = Math.round(100 * done.length / items.length);
const now = new Date();
const pad = n => String(n).padStart(2, '0');
const generated = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())} ${pad(now.getHours())}:${pad(now.getMinutes())}`;

const html = `<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Project Io &mdash; Backlog</title>
<style>
  :root {
    --bg: #12151a; --panel: #1a1f27; --panel2: #212835; --text: #d7dce4; --dim: #8892a0;
    --line: #2b3342; --accent: #5aa9e6;
    --sss: #e65a5a; --s: #e6a05a; --a: #e6d05a; --b: #7dc87d; --c: #5aa9e6; --f: #666f7d;
  }
  * { box-sizing: border-box; }
  body { margin: 0; font: 14px/1.5 "Segoe UI", system-ui, sans-serif; background: var(--bg); color: var(--text); }
  header { padding: 18px 24px 10px; border-bottom: 1px solid var(--line); position: sticky; top: 0; background: var(--bg); z-index: 5; }
  h1 { margin: 0 0 4px; font-size: 18px; font-weight: 600; }
  h1 small { color: var(--dim); font-weight: 400; margin-left: 8px; }
  .stats { color: var(--dim); font-size: 12.5px; display: flex; gap: 14px; flex-wrap: wrap; align-items: center; }
  .stats b { color: var(--text); }
  .controls { display: flex; gap: 10px; margin-top: 10px; flex-wrap: wrap; align-items: center; }
  input[type=search], select { background: var(--panel); color: var(--text); border: 1px solid var(--line); border-radius: 6px; padding: 5px 10px; font-size: 13px; }
  input[type=search] { width: 240px; }
  label.tog { color: var(--dim); font-size: 12.5px; display: inline-flex; gap: 5px; align-items: center; cursor: pointer; }
  main { padding: 18px 24px 60px; max-width: 1100px; margin: 0 auto; }
  .cards { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 12px; margin-bottom: 26px; }
  .card { background: var(--panel); border: 1px solid var(--line); border-left: 3px solid var(--accent); border-radius: 8px; padding: 12px 14px; text-decoration: none; color: inherit; display: block; transition: background .12s; }
  .card:hover { background: var(--panel2); }
  .card-top { display: flex; justify-content: space-between; margin-bottom: 6px; }
  .rank { color: var(--dim); font-size: 12px; }
  .card-id { font-size: 12px; color: var(--dim); font-family: Consolas, monospace; }
  .card-title { font-weight: 600; margin: 3px 0 6px; }
  .card-sum { font-size: 12.5px; color: var(--dim); }
  .card-foot { margin-top: 8px; display: flex; gap: 10px; align-items: center; font-size: 11px; color: var(--dim); letter-spacing: 2px; }
  .pri { font-weight: 700; font-size: 12px; padding: 1px 8px; border-radius: 10px; background: var(--panel2); }
  .pri-SSS { border-left-color: var(--sss); } .pri-SSS .pri, .pri-section .pri-SSS { color: var(--sss); }
  .pri-S { border-left-color: var(--s); } .pri-S .pri, .pri-section .pri-S { color: var(--s); }
  .pri-A { border-left-color: var(--a); } .pri-A .pri, .pri-section .pri-A { color: var(--a); }
  .pri-B { border-left-color: var(--b); } .pri-B .pri, .pri-section .pri-B { color: var(--b); }
  .pri-C { border-left-color: var(--c); } .pri-C .pri, .pri-section .pri-C { color: var(--c); }
  .pri-F { border-left-color: var(--f); } .pri-F .pri, .pri-section .pri-F { color: var(--f); }
  .done-pri { color: var(--b); }
  section.pri-section { margin-bottom: 22px; }
  section.pri-section h2 { font-size: 14px; border-bottom: 1px solid var(--line); padding-bottom: 6px; display: flex; gap: 10px; align-items: baseline; }
  .count { color: var(--dim); font-size: 12px; font-weight: 400; }
  .item { border: 1px solid transparent; border-radius: 6px; }
  .item > summary { list-style: none; cursor: pointer; display: flex; gap: 10px; align-items: baseline; padding: 5px 8px; border-radius: 6px; }
  .item > summary::-webkit-details-marker { display: none; }
  .item > summary:hover { background: var(--panel); }
  .item[open] { background: var(--panel); border-color: var(--line); margin: 4px 0; }
  .mark { width: 14px; text-align: center; color: var(--accent); flex-shrink: 0; }
  .id { font-family: Consolas, monospace; font-size: 12px; color: var(--dim); flex-shrink: 0; }
  .title { flex: 1 1 auto; min-width: 200px; }
  .chips { display: flex; gap: 6px; flex-wrap: wrap; justify-content: flex-end; }
  .chip { font-size: 10.5px; padding: 1px 7px; border-radius: 9px; background: var(--panel2); color: var(--dim); white-space: nowrap; }
  .chip.goal { color: var(--accent); }
  .chip.wait { color: var(--s); }
  .chip.diff { letter-spacing: 1.5px; font-size: 8px; }
  .chip.donechip { color: var(--b); }
  .item.blocked > summary .title, .item.parked > summary .title { color: var(--dim); }
  .body { padding: 4px 12px 12px 32px; font-size: 13px; }
  .meta { color: var(--dim); font-size: 12px; margin-bottom: 8px; }
  .prose p { margin: 0 0 8px; }
  .prose code, .meta code, .files code { background: var(--panel2); padding: 0 4px; border-radius: 3px; font-size: 12px; font-family: Consolas, monospace; }
  details.design { margin: 6px 0; }
  details.design > summary { cursor: pointer; color: var(--accent); font-size: 12px; }
  .files { margin-top: 8px; display: flex; flex-wrap: wrap; gap: 5px; }
  .hidden { display: none !important; }
  h2.sect { font-size: 13px; color: var(--dim); text-transform: uppercase; letter-spacing: 1px; margin: 0 0 10px; }
  @media (prefers-color-scheme: light) {
    :root { --bg: #f5f6f8; --panel: #ffffff; --panel2: #eceef2; --text: #232830; --dim: #68707d; --line: #d8dce3; }
  }
</style>
</head><body>
<header>
  <h1>Project Io &mdash; Backlog <small>generated ${generated}</small></h1>
  <div class="stats">
    <span><b>${open.length}</b> open</span><span><b>${done.length}</b> done</span>
    <span><b>${pct}%</b> delivered</span>
    <span>open by priority: ${byPri.map(([p, n]) => `<b>${p}</b>&thinsp;${n}`).join(' &middot; ')}</span>
  </div>
  <div class="controls">
    <input type="search" id="q" placeholder="filter&hellip; (id, title, category)">
    <select id="cat"><option value="">all categories</option>${cats.map(c => `<option>${esc(c)}</option>`).join('')}</select>
    <label class="tog"><input type="checkbox" id="show-blocked" checked> blocked</label>
    <label class="tog"><input type="checkbox" id="show-parked"> parked</label>
    <label class="tog"><input type="checkbox" id="show-done"> done</label>
  </div>
</header>
<main>
  <h2 class="sect">Top priorities &mdash; actionable now (unblocked, not parked)</h2>
  <div class="cards">${actionable.slice(0, 6).map((it, i) => topCard(it, i + 1)).join('\n')}</div>
  ${priSections}
  ${doneSection}
</main>
<script>
  const q = document.getElementById('q'), cat = document.getElementById('cat');
  const showBlocked = document.getElementById('show-blocked');
  const showParked = document.getElementById('show-parked');
  const showDone = document.getElementById('show-done');
  function apply() {
    const term = q.value.trim().toLowerCase(), c = cat.value;
    document.getElementById('done-section').hidden = !showDone.checked;
    document.querySelectorAll('.item').forEach(el => {
      let hide = false;
      if (term && !el.dataset.search.includes(term)) hide = true;
      if (c && el.dataset.cat !== c) hide = true;
      if (!showBlocked.checked && el.classList.contains('blocked')) hide = true;
      if (!showParked.checked && el.classList.contains('parked')) hide = true;
      el.classList.toggle('hidden', hide);
    });
    document.querySelectorAll('section.pri-section').forEach(s => {
      if (s.id === 'done-section') return;
      s.classList.toggle('hidden', ![...s.querySelectorAll('.item')].some(i => !i.classList.contains('hidden')));
    });
  }
  [q, cat, showBlocked, showParked, showDone].forEach(el => el.addEventListener('input', apply));
  function reveal(id) {
    const host = document.getElementById('i-' + id);
    if (host) { const d = host.querySelector('.item'); d.classList.remove('hidden'); d.open = true; }
  }
  apply();
</script>
</body></html>`;

// ---- Write + open ----
fs.mkdirSync(path.dirname(outPath), { recursive: true });
fs.writeFileSync(outPath, html, 'utf8');
console.log(`backlog_view: ${open.length} open / ${done.length} done -> ${outPath}`);

if (!noOpen) {
  const opener = process.platform === 'win32'
    ? ['powershell.exe', ['-NoProfile', '-Command', `Start-Process '${outPath.replace(/'/g, "''")}'`]]
    : process.platform === 'darwin' ? ['open', [outPath]] : ['xdg-open', [outPath]];
  execFile(opener[0], opener[1], err => {
    if (err) console.error(`(could not auto-open: ${err.message}; open the file manually)`);
  });
}
