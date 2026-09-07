#!/usr/bin/env node
// header_graph.js — a check over the doc headers and the citations that cross them.
//
// Every authority doc named in CLAUDE.md § 3 opens with a header block — Settles /
// Proposes, Not here, Confused with (DEVELOPMENT_PRACTICES.md § The doc header — an
// index of questions). That block is an index of QUESTIONS, so the set of headers is
// also a map of where each boundary between two docs is drawn. This tool reads that
// map, and the corpus's own cross-references, and reports four things.
//
//   (a) DANGLING CITATIONS   every "DOC.md § Heading" reference resolved against the
//                            target document's real headings. OBJECTIVE. It FAILS.
//                            PREFIX hits — a citation naming only the OPENING of a
//                            real heading — are counted and printed SEPARATELY,
//                            grouped by cited form: an exact-match check cannot see
//                            them, and a human resolves each at a glance.
//   (b) THE HEADER GRAPH     mutual pairs and ONE-WAY edges from the Confused with /
//                            Not here lines. JUDGEMENT. It only PRINTS. The one-way
//                            half is first-class: a sweep scoped off mutual pairs
//                            alone is blind to the larger set, which is how a
//                            one-way sprawl survives a boundary pass.
//   (c) COVERAGE             every doc in CLAUDE.md § 3 carries a header; every doc
//                            carrying a header is in § 3. Orphans BOTH ways, which is
//                            the router's staleness detector — exactly as
//                            ui_coverage.js's orphan checks are the surface
//                            catalogue's. PRINTS.
//   (d) STATE-INDEPENDENCE   a header saying landed / built / pending / shipped, or
//                            carrying a BL- id at all. OBJECTIVE. It FAILS.
//
// WHAT IT DELIBERATELY DOES NOT DO: it never tries to decide that two docs assert the
// same SUBJECT. That is judgement, and a tool that guesses at it produces a list
// nobody reads. The graph hands a human the candidate set; the hard failure is
// reserved for the half that is a fact.
//
// SCOPE. Citations are swept from docs/, src/, scripts/, tools/ AND .claude/. A
// citation in a header comment, a Lua script, a harness README or a skill file rots
// exactly like one in prose, and the misses outside docs/ are the ones a doc-only
// sweep — or a hand-grep — never reaches.
//
// USAGE:  node tools/session/header_graph.js [options]
//   (no args)      run all four checks, summary first
//   --dangling     only the citation check, every hit listed
//   --graph        only the header graph
//   --coverage     only the § 3 / header cross-check
//   --state        only the state-independence check
//   --doc <NAME>   everything known about one doc: its header, its edges, citations
//                  INTO it that failed, and the citations it makes
//   --strict       treat PREFIX citations as failures too (default: they print loud
//                  and exit 0 — a resolvable citation should not hold a gate red,
//                  or the gate stops being read)
//   --json         machine-readable dump of all four checks
//   --quiet        suppress the per-hit listings; counts only
//   --self-test    run the parser against a fixture of a dozen headings — every
//                  citation and header SHAPE the corpus contains, pinned. No corpus,
//                  no filesystem. A check-tool needs a check on itself.
// EXIT:   1 if a dangling citation or a state-independence violation is found by a
//         check that ACTUALLY RAN. --graph and --coverage print judgement and never
//         fail; asked for on their own, they exit 0.
//
// Zero dependencies (fs only). Companion to ui_coverage.js and backlog_lint.js.

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const P = (rel) => path.join(ROOT, rel);
const rel = (abs) => path.relative(ROOT, abs).split(path.sep).join('/');

const SCAN_DIRS = ['docs', 'src', 'scripts', 'tools', '.claude'];
const SCAN_EXT = new Set(['.md', '.cpp', '.hpp', '.h', '.lua', '.js', '.json', '.bat', '.ps1', '.txt']);
const SKIP_DIRS = new Set(['node_modules', '.git', 'build', 'build_gen', 'build_rel', 'build_live', 'screenshots', 'golden']);

// A HISTORICAL RECORD IS NOT SWEPT. An archived backlog row, a devlog entry, a
// retired file: each states what was true on the day it was written, and repairing
// its citation to satisfy a checker would falsify the record rather than fix a
// reference. Nothing reads them for routing. Live stores (backlog.json,
// NEEDS_REVIEW.json, requirements.json) ARE swept — an open item's citation is a
// live pointer somebody is about to follow.
const HISTORICAL = [
    /(^|\/)archive\//,
    /^docs\/development\/DEVLOG\.md$/,
    /^docs\/development\/BACKLOG\.md$/,       // retired, per CLAUDE.md § 3
    /^KNOWN_BUGS\.md$/, /^REVIEW_LOG\.md$/,   // retired
];
const isHistorical = (r) => HISTORICAL.some((re) => re.test(r));

const argv = process.argv.slice(2);
const has = (f) => argv.includes(f);
const val = (f) => { const i = argv.indexOf(f); return i >= 0 ? argv[i + 1] : null; };

if (has('--help') || has('-h')) {
    // Read to the END OF THE COMMENT, not to a hard-coded line number — a line added to
    // the usage block above used to be silently cut off the bottom of --help.
    const head = [];
    for (const l of fs.readFileSync(__filename, 'utf8').split('\n').slice(1)) {
        if (!/^\/\//.test(l.trim()) && l.trim() !== '') break;
        head.push(l.replace(/^\s*\/\/ ?/, ''));
    }
    console.log(head.join('\n').trimEnd());
    process.exit(0);
}

// --- text normalisation ----------------------------------------------------

// A heading and a citation of it are the same string written by two hands, months
// apart. Everything that is presentation rather than identity comes off both sides:
// emphasis, code ticks, link syntax, the three dash characters, smart quotes.
const demarkup = (s) => s
    .replace(/\[([^\]]*)\]\([^)]*\)/g, '$1')      // [text](link) -> text
    .replace(/[*~`]/g, '')
    // AN UNDERSCORE INSIDE A WORD IS PART OF A FILENAME, NOT EMPHASIS. Stripping it
    // unconditionally turned `NATION_GENERATION.md` into `NATIONGENERATION.md`, which
    // resolves to nothing — so NO doc whose filename carries an underscore could ever be
    // an edge TARGET, and a third of the corpus was invisible to the graph. An underscore
    // flanked by alphanumerics on BOTH sides is an identifier; any other one is emphasis.
    .replace(/_+/g, (m, i, whole) =>
        (/[A-Za-z0-9]/.test(whole[i - 1] || '') && /[A-Za-z0-9]/.test(whole[i + m.length] || '') ? m : ''))
    .replace(/[‐-―−]/g, '-')       // – — ‒ − -> -
    .replace(/[‘’]/g, "'")
    .replace(/[“”]/g, '"')
    .replace(/\s+/g, ' ')
    .trim();

// Token form is what the two sides are actually compared in: lowercase words, all
// punctuation gone. "10c. What the research says" and "10c — what the research says"
// tokenise identically, and neither is more correct than the other.
const tokens = (s) => demarkup(s).toLowerCase().replace(/[^a-z0-9]+/g, ' ').trim().split(' ').filter(Boolean);

const startsWith = (hay, needle) => needle.length > 0 && needle.length <= hay.length
    && needle.every((t, i) => hay[i] === t);

// --- the document index ----------------------------------------------------

function walk(dir, out) {
    let entries;
    try { entries = fs.readdirSync(dir, { withFileTypes: true }); } catch { return out; }
    for (const e of entries) {
        if (e.name.startsWith('.') && e.name !== '.claude') continue;
        const full = path.join(dir, e.name);
        if (e.isDirectory()) { if (!SKIP_DIRS.has(e.name)) walk(full, out); }
        else out.push(full);
    }
    return out;
}

const allFiles = [];
for (const d of SCAN_DIRS) walk(P(d), allFiles);
for (const f of fs.readdirSync(ROOT)) {
    const full = path.join(ROOT, f);
    if (fs.statSync(full).isFile() && f.endsWith('.md')) allFiles.push(full);
}
// Project-Rival is a separate discipline with its own CLAUDE.md; its docs are
// resolvable as citation TARGETS (Io cites them) but are never swept, graphed, or
// held to § 3.
walk(P('Project-Rival'), allFiles);

const mdFiles = allFiles.filter((f) => f.endsWith('.md'));

// WHAT COUNTS AS A HEADING. Not only a `#` line. This corpus names a section three
// ways and cites all three: a `#` heading; a bold lead-in opening a line or a
// sentence (GLOSSARY.md's every term, every rule in io-standing-rules.md); and a
// table row's first cell (PLANETARY.md's layers, LENSES.md's roster, ICONS.md's
// glyphs). Indexing only `#` lines calls the other two dangling — 51 citations in
// this corpus — which is the noisy direction the tool cannot afford. What is NOT an
// anchor is a bold run in the middle of a sentence: that is emphasis.
function headingsOf(text) {
    const out = [];
    let fence = false;
    for (const line of text.split('\n')) {
        if (/^\s*(```|~~~)/.test(line)) { fence = !fence; continue; }
        if (fence) continue;
        const hm = line.match(/^(#{1,6})\s+(.+?)\s*#*\s*$/);
        const found = [];
        if (hm) found.push(hm[2]);
        else {
            // A bold run that OPENS a line or a sentence. io-standing-rules.md
            // keeps whole rules as sentence-initial bold inside a bullet's
            // continuation, and the corpus cites them by that name.
            for (const b of line.matchAll(/(^|[.!?)]\s+|^\s*(?:[-*+]|\d+\.)\s+)\*\*([^*]{3,90})\*\*/g)) {
                found.push(b[2].replace(/[.:,;]\s*$/, ''));
            }
            // A TABLE ROW'S FIRST CELL IS AN ANCHOR TOO. PLANETARY.md's layer list,
            // LENSES.md's roster and ICONS.md's glyph table each name their entries
            // in a leading cell and nowhere else, and the corpus cites those names.
            const row = line.match(/^\s*\|\s*([^|]{3,60}?)\s*\|/);
            if (row && !/^[-:\s]+$/.test(row[1])) found.push(row[1]);
        }
        for (const f of found) addHeading(out, hm ? hm[1].length : 7, f);
    }
    return out;
}

// One heading, plus the aliases a legitimate citation is allowed to use: a trailing
// attribution dropped ("Pass 7 — Starting treasury *(Ben, 2026-08-24)*" cited as
// "Pass 7 - Starting treasury"), a leading section number dropped ("1. SOTA map"
// cited as "SOTA map"). Getting this wrong in the noisy direction is what makes a
// checker unusable, so the aliases are generous and the misses are few.
function addHeading(out, level, text) {
    const raw = demarkup(text);
    if (!raw) return;
    const aliases = new Set([raw]);
    let cut = raw;
    for (let i = 0; i < 3; i++) {
        const c = cut.replace(/\s*\([^()]*\)\s*$/, '').trim();
        if (c === cut || !c) break;
        cut = c; aliases.add(cut);
    }
    const noNum = raw.replace(/^\d+[a-z]?[.)]?\s+/i, '').trim();
    if (noNum && noNum !== raw) aliases.add(noNum);
    out.push({ level, text: raw, bold: level === 7, aliases: [...aliases].map((a) => ({ text: a, tok: tokens(a) })) });
}

// The header block: the FIRST blockquote under the H1 that opens Settles or Proposes.
// A doc may carry other blockquotes (a status note, a pending-review flag); those are
// not the header and must not be parsed as one.
function headerOf(text) {
    const lines = text.split('\n');
    let block = null;
    for (let i = 0; i < Math.min(lines.length, 60); i++) {
        if (!/^\s*>/.test(lines[i])) continue;
        const buf = [];
        let j = i;
        while (j < lines.length && /^\s*>/.test(lines[j])) buf.push(lines[j].replace(/^\s*>\s?/, '')), j++;
        const joined = buf.join(' ');
        if (/\*\*(Settles|Proposes):\*\*/.test(joined)) { block = { text: joined, line: i + 1, raw: buf }; break; }
        i = j;
    }
    if (!block) return null;
    const field = (name) => {
        const re = new RegExp(`\\*\\*${name}:\\*\\*([\\s\\S]*?)(?=\\*\\*(?:Settles|Proposes|Not here|Confused with):\\*\\*|$)`, 'i');
        const m = block.text.match(re);
        return m ? m[1].trim() : null;
    };
    return {
        line: block.line,
        text: block.text,
        kind: /\*\*Proposes:\*\*/.test(block.text) ? 'Proposes' : 'Settles',
        settles: field('Settles') || field('Proposes'),
        notHere: field('Not here'),
        confusedWith: field('Confused with'),
    };
}

const docs = new Map();     // repo-relative path -> record
for (const f of mdFiles) {
    const text = fs.readFileSync(f, 'utf8');
    const r = rel(f);
    docs.set(r, {
        path: r,
        base: path.basename(r),
        stem: path.basename(r, '.md'),
        rival: r.startsWith('Project-Rival/'),
        headings: headingsOf(text),
        header: headerOf(text),
    });
}

// basename -> paths, for the many citations that name a doc without its directory.
const byBase = new Map();
for (const d of docs.values()) {
    if (!byBase.has(d.base)) byBase.set(d.base, []);
    byBase.get(d.base).push(d.path);
}

// Resolving a bare doc name is the one genuinely ambiguous step: five basenames are
// duplicated in this repo. Nearest-by-directory to the citing file wins, then docs/,
// then anything that is not Project-Rival. Still tied -> AMBIGUOUS, reported, never
// silently guessed.
function resolveDoc(name, fromFile) {
    let n = String(name).trim().replace(/^[.\/]*/, '');
    if (!n) return { miss: 'empty' };
    if (!n.endsWith('.md')) n += '.md';
    const direct = n.split('/').join('/');
    if (docs.has(direct)) return { path: direct };
    const base = path.basename(direct);
    let cands = byBase.get(base) || [];
    if (!cands.length) return { miss: 'nofile', name: direct };
    if (cands.length > 1 && direct.includes('/')) {
        // a partial path ("ui/LENSES.md", "../politics/NATIONS.md") narrows it
        const tail = direct.replace(/^(\.\.\/)+/, '');
        const narrowed = cands.filter((c) => c === tail || c.endsWith('/' + tail));
        if (narrowed.length) cands = narrowed;
    }
    if (cands.length === 1) return { path: cands[0] };
    const fromDir = fromFile ? path.dirname(rel(fromFile)) : '';
    const score = (c) => {
        const a = path.dirname(c).split('/'), b = fromDir.split('/');
        let k = 0; while (k < a.length && k < b.length && a[k] === b[k]) k++;
        return k * 100 + (c.startsWith('docs/') ? 10 : 0) + (c.startsWith('Project-Rival/') ? -20 : 0);
    };
    const ranked = [...cands].sort((x, y) => score(y) - score(x));
    if (score(ranked[0]) === score(ranked[1])) return { miss: 'ambiguous', name: base, cands: ranked };
    return { path: ranked[0] };
}

// --- (a) citations ---------------------------------------------------------

// Citation shapes actually present in this corpus, all of which must parse:
//   `docs/economy/FINANCE.md` § The quarterly return
//   [LENSES.md](LENSES.md) § The strip rotates with the rung
//   (../politics/NATIONS.md § 2. The treasury - a balance with both halves
//   LOGISTICS.md § Logistic Points
//   docs/SYSTEMS.md § Trade / § Supply          (the second § chains to the first doc)
//   AI_OPPONENT.md § 10i                        (a section number, not a phrase)
//   io-standing-rules § Terms & docs            (no extension)
//   docs/ui/SELECTION.md (§ Polymorphism)       (the § is inside its own parenthesis)
// A § whose target is a SOURCE file (world.hpp §, economy.lua §) is a code-section
// marker, not a doc citation, and is skipped rather than reported.
//
// THE TRAILING `[(\[]?` IS LOAD-BEARING. `X.md (§ Heading)` puts an OPENING bracket
// between the doc name and the §; without it the anchor failed, the whole shape went
// unswept — 13 citations, 5 of them dangling, including two in src/ui/ — and the tool
// reported the SAME citation when it was written without the parenthesis.
const DOC_REF = /(?:\[[^\]]*\]\()?([A-Za-z0-9_][A-Za-z0-9_.\/-]*|(?:\.\.\/)+[A-Za-z0-9_][A-Za-z0-9_.\/-]*)(?:\.md)?['"`)\]]{0,3}\)?\s*([(\[])?\s*$/;
const SRC_EXT = /\.(cpp|hpp|h|lua|js|json|bat|ps1|py|txt|png)$/i;
// Across the OPENING bracket the name has to LOOK like a doc. "own corporation (§ Always
// open)" is prose, but `corporation` is also the basename of a ledger doc, so admitting a
// bare lowercase word across the bracket invents a citation out of an ordinary sentence.
// A real one carries an extension, a directory, or a capital.
const DOC_SHAPED = (n) => /\.md$/i.test(n) || n.includes('/') || /[A-Z]/.test(n);
// A CHAIN IS SHORT. "SYSTEMS.md § Trade / § Supply" puts the second § a few characters
// after the first; a backlog record is one 4000-character JSON line on which a doc named
// at the start would otherwise adopt every stray § in the paragraph.
const CHAIN_SPAN = 40;

// A CITATION WRAPS. An 80-column comment splits one reference across two lines, and
// reading only the first leaves the cited heading as the word "The" — a false miss,
// in the direction that matters most. So a short candidate that runs to the end of
// its line borrows the next line's opening, once.
//
// The wrap must be in the SAME comment, though. A comment's last line followed by the
// code it describes is not a wrap, and joining them appends a statement to the cited
// heading — so a line carrying a comment marker only ever borrows from a line
// carrying the same one.
const MARKER = /^\s*(\/\/\/?|--+|\*|>+|#{1,6}(?=\s))/;
function continuation(lines, ln) {
    const next = lines[ln + 1];
    if (next === undefined) return '';
    const mine = (lines[ln].match(MARKER) || [, ''])[1];
    const theirs = (next.match(MARKER) || [, ''])[1];
    if (mine && theirs !== mine) return '';
    const t = next.replace(MARKER, '').trim();
    if (!t || /^[-*+|=]/.test(t) || /^[{}[\]]/.test(t)) return '';
    if (/^\d+[.)]\s/.test(t)) return '';              // an ordered-list item is not a wrap
    // A DIGIT OPENS A WRAP AS READILY AS A LETTER. "`COLLAPSE.md` § The\n4000-year
    // problem" is one citation split by an 80-column rule; refusing the borrow left the
    // cited heading as the word "The" and reported a false MISS against a heading the
    // tool resolves correctly wherever the same citation happens to fit on one line.
    if (!/^[a-z(0-9]|^[A-Z][a-z]/.test(t)) return '';  // a new sentence in caps is not a wrap
    return ' ' + t.slice(0, 90);
}

// The candidate heading text runs on into prose. It does not need a precise end for
// the RESOLVE direction — the test there is "does a real heading start this text" —
// so only hard delimiters cut it. The PREFIX direction gets the tighter trim below.
function candidateAfter(line, at) {
    // \r first: this tree is CRLF, lines are split on \n, and a regex `.` does not match
    // the carriage return left behind — a `$`-anchored match silently never fires.
    let s = line.slice(at + 1).replace(/\r/g, '').replace(/^[\s:]+/, '');
    // A QUOTED HEADING IS SELF-DELIMITING, NOT A DEAD END. `§ "The goal"` used to hit the
    // double quote as a hard delimiter, leave an empty candidate, and get dropped before
    // it was ever classified — a whole shape swept and silently discarded, one instance of
    // it a citation pointing at the wrong document entirely. The backslash is for a
    // citation living inside a JSON string (backlog.json, requirements.json).
    const q = s.match(/^\\?["“](.*)$/);
    if (q) {
        const close = q[1].search(/\\?["”]/);
        return (close >= 0 ? q[1].slice(0, close) : q[1]).trim();
    }
    const hard = /[`"|\]\n]|\\n|§/;
    const m = s.match(hard);
    if (m) s = s.slice(0, m.index);
    // an unbalanced ) closes the enclosing parenthesis, not the heading
    let depth = 0;
    for (let i = 0; i < s.length; i++) {
        if (s[i] === '(') depth++;
        else if (s[i] === ')') { if (depth === 0) { s = s.slice(0, i); break; } depth--; }
    }
    return s.replace(/^[\s:]+/, '').trim();
}
const trimTight = (s) => demarkup(s).split(/[,;:.]|\s-\s|\s—\s|\s\(/)[0].trim();

// The candidate, with the wrap borrowed only when the line genuinely ran out mid-
// citation: nothing closed it, and what it holds so far is too short to be a heading.
function citedAt(lines, ln, at) {
    const line = lines[ln];
    const first = candidateAfter(line, at);
    const uncut = line.slice(at + 1).replace(/^[\s:]+/, '').trimEnd() === first;
    if (!uncut || tokens(first).length >= 6) return first;
    const joined = candidateAfter(line + continuation(lines, ln), at);
    return joined.length > first.length ? joined : first;
}

// A BOLD LEAD-IN OR A TABLE CELL IS A WEAK ANCHOR. It is one row of a table or the
// opening of a sentence, not a section — and a weak anchor of a SINGLE COMMON WORD
// certifies almost anything: "SPRINTS.md § Sprint 16" passed as OK against the word
// "Sprint", though SPRINTS.md has no Sprint 16 heading; "LENSES.md § rung table" passed
// against the cell "Rung"; "RESOURCES.md § Mercantile value track" against "Mercantile".
// That is the most dangerous failure this tool has, because a false PASS prints nowhere.
// So a one-token weak anchor certifies a citation only when the citation IS that token.
// A one-token `#` heading is trusted: an H2 named "Overview" really is a section.
const weakOneToken = (head, aTok, tightLen) => head.bold && aTok.length < 2 && tightLen > 1;

// The section-id shape: "10i", "5", "3a" — and "Q1", the question ids ERA1_TECH_LANDSCAPE
// and the ledger Q&As number their sections with. An id is distinctive enough to match on
// its own, which is what a phrase-length floor cannot do for a one-token name.
const SECTION_ID = /^[a-z]{0,2}[0-9]+[a-z]?$/;

// Classify one cited phrase against one document's headings. Pure — it takes the two
// strings and nothing else, which is what makes --self-test possible.
function classifyCitation(cited, headings) {
    const ctok = tokens(cited);
    if (!ctok.length) return { cls: 'EMPTY' };
    const tight = tokens(trimTight(cited));

    // 1. a real heading (or one of its aliases) opens the cited text
    let best = null, weak = null;
    for (const head of headings) {
        for (const a of head.aliases) {
            if (!startsWith(ctok, a.tok)) continue;
            if (weakOneToken(head, a.tok, tight.length)) { weak = weak || { head }; continue; }
            if (!best || a.tok.length > best.tok.length) best = { head, tok: a.tok };
        }
    }
    if (best) return { cls: 'OK', matched: best.head.text };

    // 2. a bare section id: "§ 10i", "§ 10c.6-7", "§ 5", "§ Q1"
    if (SECTION_ID.test(ctok[0])) {
        const n = headings.find((x) => tokens(x.text)[0] === ctok[0] && SECTION_ID.test(tokens(x.text)[0]));
        if (n) return { cls: 'OK', matched: n.text };
    }

    // 3. THE CITATION IS SHORTER THAN THE HEADING IT NAMES — the province.hpp
    //    class: it names "The partition" where PROVINCES.md's heading reads
    //    "The partition - grown from settlement, stopped by terrain".
    //    Resolvable by a human,
    //    invisible to an exact-match check, and so reported in its own block
    //    rather than folded into either the passes or the misses.
    //
    //    The cited text runs on into prose far more often than it stops
    //    cleanly, so the search walks the citation's opening words from longest
    //    to shortest and takes the first opening a real heading begins with. A
    //    hit is marked `truncated` when the whole cited phrase is inside the
    //    heading (a clean short form) and `diverges` when the citation carries
    //    on past the part that matched — which is either run-on prose or a tail
    //    that is simply wrong, and only a human can say which. Two tokens is
    //    the floor, or one of at least four characters; below that a short
    //    citation would match half the file.
    let pre = null, took = 0;
    for (let k = Math.min(ctok.length, 12); k >= 1; k--) {
        const open = ctok.slice(0, k);
        if (k === 1 && open[0].length < 4) break;
        let best2 = null;
        for (const head of headings) {
            for (const a of head.aliases) {
                if (weakOneToken(head, a.tok, tight.length)) continue;
                if (startsWith(a.tok, open) && (!best2 || a.tok.length < best2.tok.length)) best2 = { head, tok: a.tok };
            }
        }
        if (best2) { pre = best2; took = k; break; }
    }
    if (pre) {
        return {
            cls: 'PREFIX', matched: pre.head.text, tight: trimTight(cited),
            shape: took >= tight.length ? 'truncated' : 'diverges',
        };
    }
    // A weak one-token anchor may not CERTIFY a citation, but it is not nothing either:
    // "ICONS.md § hq glyph" names the glyph table's `HQ` cell and one word more. Demoting
    // it to MISS would trade one false pass for one false failure, so it lands in the
    // block that exists for exactly this — resolvable by a human, invisible to a match.
    if (weak) return { cls: 'PREFIX', matched: weak.head.text, tight: trimTight(cited), shape: 'diverges' };
    return { cls: 'MISS' };
}

function scanCitations() {
    const hits = [];
    for (const f of allFiles) {
        if (!SCAN_EXT.has(path.extname(f))) continue;
        const r = rel(f);
        if (r.startsWith('Project-Rival/') || isHistorical(r)) continue;
        // THIS FILE SPELLS THE CITATION SHAPE BY EXAMPLE, over and over, and an example
        // is not a citation. Sweeping itself made the tool report its own documentation.
        if (r === 'tools/session/header_graph.js') continue;
        let text;
        try { text = fs.readFileSync(f, 'utf8'); } catch { continue; }
        if (!text.includes('§')) continue;
        const lines = text.split('\n');
        for (let ln = 0; ln < lines.length; ln++) {
            const line = lines[ln];
            let at = -1, lastDoc = null, lastAt = -1;
            const chained = () => lastDoc && at - lastAt <= CHAIN_SPAN;
            while ((at = line.indexOf('§', at + 1)) >= 0) {
                const before = line.slice(0, at).replace(/\s*$/, '');
                let m = before.match(DOC_REF);
                if (m && m[2] && !DOC_SHAPED(m[1])) m = null;   // prose across a "(§" bracket
                let target = null, named = null;
                if (m) {
                    named = m[1];
                    if (SRC_EXT.test(named)) { lastDoc = null; continue; }
                    // "DOC.md § Heading" is how the corpus SPELLS the citation
                    // shape when it describes it, this file included. Not a citation.
                    if (/^DOC\.md$/i.test(named)) { lastDoc = null; continue; }
                    const res = resolveDoc(named, f);
                    if (res.path) { target = res.path; lastDoc = res.path; lastAt = at; }
                    else if (res.miss === 'ambiguous') {
                        hits.push({ file: r, line: ln + 1, named, cls: 'AMBIGUOUS', cands: res.cands, cited: citedAt(lines, ln, at) });
                        lastDoc = null; continue;
                    } else if (/\.md$/i.test(named) || named.toUpperCase() === named) {
                        // Looks like a doc and is not one. Real: a doc that was renamed
                        // or never existed. Anything lowercase with no extension is
                        // prose ("the ladder §") and is dropped.
                        if (/\.md$/i.test(named)) hits.push({ file: r, line: ln + 1, named, cls: 'NODOC', cited: citedAt(lines, ln, at) });
                        lastDoc = null; continue;
                    } else if (chained()) {
                        // A PROSE WORD DOES NOT BREAK THE CHAIN. "FINANCE.md § Disclosure
                        // and § Whole-firm acquisition" puts the word "and" before the
                        // second §; treating that as a failed doc dropped the second
                        // section, which is the chaining the shape exists to express.
                        target = lastDoc; named = null;
                    } else { lastDoc = null; continue; }
                } else if (chained()) {
                    target = lastDoc;                 // "SYSTEMS.md § Trade / § Supply"
                } else continue;

                const cited = citedAt(lines, ln, at);
                lastAt = at;                          // a chain walks: § A / § B / § C
                if (!cited) continue;
                hits.push({ file: r, line: ln + 1, named: named || path.basename(target), target, cited });
            }
        }
    }

    // classify
    for (const h of hits) {
        if (h.cls) continue;
        Object.assign(h, classifyCitation(h.cited, docs.get(h.target).headings));
    }
    return hits;
}

const citations = scanCitations();
const cite = (c) => citations.filter((h) => h.cls === c);
const misses = cite('MISS');
const prefixes = cite('PREFIX');
const nodocs = cite('NODOC');
const ambiguous = cite('AMBIGUOUS');
const empties = cite('EMPTY');

// --- (b) the header graph --------------------------------------------------

// Confused with: a plain list of doc names, each of which MAY be followed by prose —
//                "`../military/MILITARY.md` above all — the two resolvers are constantly
//                mistaken for one another; also ../lore/HISTORY.md".
// Not here:      questions, each naming its owner(s) in parentheses — "the money loop
//                (FINANCE)", "(AI_OPPONENT, the authority)", "what any one panel says
//                (HEADER, PROFILE, TIME_CONTROLS, CHAT)". EVERY name in the parenthetical
//                is an owner, not just the first: a question with four owners draws four
//                boundaries, and reading only the first hid three of them.
//
// A doc name is one unspaced token, optionally relative-pathed. A RELATIVE PATH IS THE
// CORPUS'S NORMAL CROSS-DIRECTORY FORM (`../military/MILITARY`), so it must be admitted;
// requiring a leading letter rejected every cross-directory owner there is. Prose words
// are lowercase and resolve to nothing, so the case test is what keeps the parser honest.
const DOC_TOKEN = /^(?:\.{1,2}\/)*[A-Za-z0-9_][A-Za-z0-9_.\/-]*$/;
function docTokensIn(chunk) {
    const out = [];
    for (const w of demarkup(chunk).split(/\s+/)) {
        const t = w.replace(/^[("'[]+/, '').replace(/[.,;:)"'\]]+$/, '');
        if (t.length < 3 || !DOC_TOKEN.test(t)) continue;
        if (!/[A-Z]/.test(t) && !/\.md$/i.test(t)) continue;   // prose is lowercase
        out.push(t);
    }
    return out;
}
// The first token of a chunk that is a real document. Scanning rather than anchoring is
// what lets "also ../lore/HISTORY.md" and "`MILITARY.md` above all — prose" both resolve.
function firstDocIn(chunk, fromFile) {
    for (const t of docTokensIn(chunk)) {
        const r = resolveDoc(t, fromFile);
        if (r.path) return r.path;
    }
    return null;
}
const confusedChunks = (s) => String(s || '').split(/[,·;]/);
const notHereChunks = (s) => [...String(s || '').matchAll(/\(([^)]*)\)/g)].flatMap((m) => m[1].split(/[,;·]/));
function edgesOf(d) {
    const out = [];
    if (!d.header) return out;
    const from = P(d.path);
    for (const chunk of confusedChunks(d.header.confusedWith)) {
        const to = firstDocIn(chunk, from);
        if (to && to !== d.path) out.push({ to, kind: 'confused' });
    }
    for (const part of notHereChunks(d.header.notHere)) {
        const to = firstDocIn(part, from);
        if (to && to !== d.path) out.push({ to, kind: 'nothere' });
    }
    return out;
}

// --- self-test -------------------------------------------------------------

// A CHECK-TOOL WITH NO CHECK ON ITSELF IS THE THING THE PROJECT'S RULES WARN ABOUT. Two
// cold reviews found eleven defects in this file, every one of them a citation or header
// SHAPE the corpus contains and the parser did not handle. So each shape is pinned here
// against a fixture of a dozen headings — no corpus, no filesystem, nothing to keep in
// step — and a regression in any of them fails loudly instead of quietly narrowing the
// sweep. A tool that is quietly incomplete applies a correct rule to a partial write set,
// which is the exact failure this tool exists to stop.
if (has('--self-test')) {
    const FIXTURE = [
        '# Fixture', '',
        '## Q1 — quest shape: mostly a binary tree, some dead-end leaves',
        '## The 4000-year problem — making the run affordable',
        '## Rung applicability',
        '### Water kinds: lake, coast and ocean',
        '## Design state — the two open states',
        '## 10i. The wire protocol',
        '## Mercantile — endemic trade goods',
        '## Pass 7 — Starting treasury *(Ben, 2026-08-24)*', '',
        '| Rung | On the strip |', '| --- | --- |',
        '| Sprint | a weak anchor |', '| HQ | the glyph |', '',
        '- **Design** — design depth only', '',
    ].join('\n');
    const H = headingsOf(FIXTURE);
    const cls = (s) => classifyCitation(s, H).cls;
    const shape = (s) => classifyCitation(s, H).shape;
    let pass = 0; const fail = [];
    const ok = (name, got, want) => {
        if (JSON.stringify(got) === JSON.stringify(want)) pass++;
        else fail.push(`${name}\n      got  ${JSON.stringify(got)}\n      want ${JSON.stringify(want)}`);
    };
    const line = (s) => { const a = s.indexOf('§'); return citedAt([s, ''], 0, a); };

    // (1) a filename's underscore survives demarkup; an emphasis underscore does not
    ok('1  demarkup keeps NATION_GENERATION', demarkup('`../generation/NATION_GENERATION.md`'), '../generation/NATION_GENERATION.md');
    ok('1b demarkup strips _emphasis_', demarkup('the _emphatic_ word'), 'the emphatic word');
    ok('1c demarkup keeps DEVELOPMENT_PRACTICES', demarkup('**DEVELOPMENT_PRACTICES.md**'), 'DEVELOPMENT_PRACTICES.md');

    // (3) EVERY owner in a Not-here parenthetical, not just the first
    ok('3  four owners in one parenthetical',
        notHereChunks('what any one panel says (HEADER, PROFILE, TIME_CONTROLS, CHAT) · x (MENU)').map((c) => docTokensIn(c)[0]),
        ['HEADER', 'PROFILE', 'TIME_CONTROLS', 'CHAT', 'MENU']);
    ok('3b a parenthetical naming no doc yields none',
        notHereChunks('the money loop (AI_OPPONENT, the authority)').map((c) => docTokensIn(c)[0] || null),
        ['AI_OPPONENT', null]);

    // (4) a relative path is the corpus's normal cross-directory owner
    ok('4  relative-path owner admitted',
        notHereChunks('a battle in one costs (../military/MILITARY) · x (../economy/RESOURCES, ../economy/TILES)')
            .map((c) => docTokensIn(c)[0] || null),
        ['../military/MILITARY', '../economy/RESOURCES', '../economy/TILES']);

    // (5) a Confused-with entry followed by prose still names its doc
    ok('5  doc name ahead of prose',
        confusedChunks('`../military/MILITARY.md` above all — the two resolvers are mistaken for one another; also ../lore/HISTORY.md, ../lore/COLLAPSE.md.')
            .map((c) => docTokensIn(c)[0] || null),
        ['../military/MILITARY.md', '../lore/HISTORY.md', '../lore/COLLAPSE.md']);

    // (7) a one-token section id followed by run-on prose
    ok('7  Q1 + run-on prose resolves', cls('Q1. The motive behind Q1 survives: the'), 'OK');
    ok('7b a numeric section id still resolves', cls('10i'), 'OK');

    // (8) a wrap whose continuation opens with a digit
    ok('8  digit opens a wrap', continuation(['// `COLLAPSE.md` § The', '// 4000-year problem. A run'], 0).trim(), '4000-year problem. A run');
    ok('8b an ordered-list item is not a wrap', continuation(['// x § The', '// 1. a list item'], 0), '');
    ok('8c the borrowed citation then resolves', cls('The 4000-year problem. A settle-dominated run'), 'PREFIX');

    // (9) an opening paren between the doc name and the §
    const named = (before) => (before.match(DOC_REF) || [])[1] || null;
    ok('9  "(§" keeps the doc', named('docs/ui/SELECTION.md ('), 'docs/ui/SELECTION.md');
    ok('9b the plain shape is unaffected', named('`docs/ui/SELECTION.md`'), 'docs/ui/SELECTION.md');
    ok('9c a bare prose word across "(§" is not a doc', DOC_SHAPED(named('own corporation (') || ''), false);

    // (10) a quoted heading is self-delimiting, not a dead end
    ok('10  quoted heading survives', line('X.md § "Availability is cash-free; spending is not").'), 'Availability is cash-free; spending is not');
    ok('10b quoted heading inside a JSON string', line('X.md § \\"Travel time - distance costs time\\", and'), 'Travel time - distance costs time');

    // (11) a one-token WEAK anchor may not certify a citation
    ok('11  Sprint 16 is not OK', cls('Sprint 16 is open. The item is closed inside'), 'PREFIX');
    ok('11b rung table is not OK', cls('rung table'), 'PREFIX');
    ok('11c Mercantile value track is not OK', cls('Mercantile value track'), 'PREFIX');
    ok('11d a weak anchor still demotes to PREFIX, never to MISS', cls('hq glyph'), 'PREFIX');
    ok('11e the citation that IS the weak anchor stays OK', cls('HQ'), 'OK');
    ok('11f a one-token "#" heading is trusted', cls('Rung applicability and what follows'), 'OK');

    // the behaviour the reviews confirmed was already right, pinned so it stays right
    ok('R1 truncated vs diverges', [shape('Water kinds'), shape('Design state'), shape('rung table')],
        ['truncated', 'truncated', 'diverges']);
    ok('R2 a genuinely absent heading still MISSes', cls('Polymorphism'), 'MISS');
    ok('R3 an attribution tail is an alias', cls('Pass 7 - Starting treasury'), 'OK');
    ok('R4 a citation that tokenises to nothing is EMPTY, not OK', cls('  '), 'EMPTY');

    console.log(`\nheader_graph --self-test: ${pass} passed, ${fail.length} failed`);
    for (const f of fail) console.log(`  FAIL  ${f}`);
    process.exit(fail.length ? 1 : 0);
}

const headed = [...docs.values()].filter((d) => d.header && !d.rival);
const graph = new Map();      // from -> Map(to -> Set(kind))
for (const d of headed) {
    const m = new Map();
    for (const e of edgesOf(d)) {
        if (!m.has(e.to)) m.set(e.to, new Set());
        m.get(e.to).add(e.kind);
    }
    graph.set(d.path, m);
}
const edgeTo = (a, b) => graph.has(a) && graph.get(a).has(b);

const mutual = [], oneWay = [];
for (const [from, tos] of graph) {
    for (const [to, kinds] of tos) {
        if (edgeTo(to, from)) { if (from < to) mutual.push({ a: from, b: to, kinds: [...kinds], back: [...graph.get(to).get(from)] }); }
        else oneWay.push({ from, to, kinds: [...kinds], headed: graph.has(to) });
    }
}
// A doc many others point at while it points back at none is the shape a one-way
// sprawl takes: everyone knows the boundary is there, the owner has not written it
// down. Ranking by that in-degree is what lets a sweep be scoped off the graph.
const inDeg = new Map();
for (const e of oneWay) inDeg.set(e.to, (inDeg.get(e.to) || 0) + 1);
const sprawl = [...inDeg.entries()].sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0]));

// --- (c) coverage ----------------------------------------------------------

// CLAUDE.md § 3 is a set of tables of backticked paths. A row may name a directory
// once and its siblings bare ("`docs/ui/MENU.md`, `HEADER.md`, ..."), and may carry a
// glob ("`docs/ui/ledgers/*.md`"), so a row is resolved left to right with the last
// directory seen as the context for a bare name.
function section3() {
    const text = fs.readFileSync(P('CLAUDE.md'), 'utf8');
    const start = text.indexOf('## 3.');
    const end = text.indexOf('## 4.', start);
    const body = text.slice(start, end < 0 ? undefined : end);
    const named = new Set();
    for (const line of body.split('\n')) {
        let dir = null;
        for (const m of line.matchAll(/`([^`]+)`/g)) {
            const t = m[1].trim();
            if (!/\.md$/.test(t) && !/\*\.md$/.test(t)) continue;
            if (t.includes('*')) {
                const d = path.dirname(t);
                for (const p of docs.keys()) if (path.dirname(p) === d) named.add(p);
                dir = d; continue;
            }
            if (t.includes('/')) { dir = path.dirname(t); if (docs.has(t)) named.add(t); continue; }
            const guess = dir ? dir + '/' + t : null;
            if (guess && docs.has(guess)) named.add(guess);
            else { const r = resolveDoc(t, P('CLAUDE.md')); if (r.path) named.add(r.path); }
        }
    }
    return named;
}
const routed = section3();
const headedSet = new Set(headed.map((d) => d.path));
const routedNoHeader = [...routed].filter((p) => !headedSet.has(p)).sort();
// NO ALLOW-LIST. See the note printed with this block.
const headerNotRouted = [...headedSet].filter((p) => !routed.has(p)).sort();

// --- (d) state-independence ------------------------------------------------

// The spec forbids a header saying landed / built / pending / shipped and forbids a
// BL- id outright — "never carries a BL- id", in DEVELOPMENT_PRACTICES.md
// § The doc header - an index of questions. So the BL- test is unconditional here, with
// no attempt to judge which ROLE the id is playing: the spec left it no legitimate role.
const STATE_WORDS = [
    [/\blanded\b/i, 'landed'], [/\bshipped\b/i, 'shipped'], [/\bnot yet\b/i, 'not yet'],
    [/\bpending\b/i, 'pending'], [/\bunbuilt\b/i, 'unbuilt'], [/\bin progress\b/i, 'in progress'],
    // "built" and "delivered" are the two words the ban NEEDS and the two a routing
    // line uses innocently — "the substrate an event is built from (META_LAYER)",
    // "how an item is delivered (DELIVERY)" are both questions, not build claims. So
    // they only trip with a temporal qualifier in front, which is what turns the word
    // into a state.
    [/\b(?:already|now|not yet|never|still not|is not|isn't)\s+(?:built|delivered|implemented)\b/i, 'build state'],
    [/\bBL-\d+/, 'BL- id'],
];
const stateHits = [];
for (const d of headed) {
    for (const [re, label] of STATE_WORDS) {
        const m = d.header.text.match(re);
        if (m) stateHits.push({ doc: d.path, line: d.header.line, word: label, at: m[0], ctx: d.header.text.slice(Math.max(0, m.index - 50), m.index + 60).trim() });
    }
}

// --- output ----------------------------------------------------------------

const quiet = has('--quiet');
const only = ['--dangling', '--graph', '--coverage', '--state'].filter(has);
const show = (k) => !only.length || has('--' + k);
const cap = (arr, n) => (quiet ? [] : arr.slice(0, n));

function blockDangling() {
    console.log(`\n=== (a) DANGLING CITATIONS — ${citations.length} doc-section references swept from ${SCAN_DIRS.join('/ ')}/ ===`);
    console.log(`  OK        ${String(cite('OK').length).padStart(5)}   the cited heading exists`);
    console.log(`  PREFIX    ${String(prefixes.length).padStart(5)}   names a PREFIX of a longer real heading — resolvable, invisible to exact match`);
    console.log(`  MISS      ${String(misses.length).padStart(5)}   no heading in the target doc begins with the cited text  ** FAIL **`);
    console.log(`  NODOC     ${String(nodocs.length).padStart(5)}   names a .md that does not exist`);
    if (ambiguous.length) console.log(`  AMBIGUOUS ${String(ambiguous.length).padStart(5)}   a duplicated basename, not resolvable to one file`);
    // THE ARITHMETIC MUST CLOSE. A citation that tokenises to nothing used to be neither
    // passed nor reported nor counted, so the buckets quietly summed two short of the
    // total — a class that fails silently is the one defect this tool cannot carry.
    if (empties.length) console.log(`  EMPTY     ${String(empties.length).padStart(5)}   the cited text tokenises to nothing — not classifiable, listed here so the tally closes`);
    if (empties.length && !quiet) for (const h of empties) console.log(`      ${h.file}:${h.line}  -> ${h.target}`);

    if (misses.length && !quiet) {
        console.log(`\n  MISSES (${misses.length}) — the heading is not there:`);
        const byTarget = {};
        for (const h of misses) (byTarget[h.target] = byTarget[h.target] || []).push(h);
        for (const t of Object.keys(byTarget).sort()) {
            console.log(`\n    -> ${t}`);
            for (const h of byTarget[t]) console.log(`       ${h.file}:${h.line}  § ${h.cited}`);
        }
    }
    if (prefixes.length && !quiet) {
        // One distinct cited-form per row, with the sites folded in behind a count:
        // 313 hits are 150-odd forms, and the form is what gets repaired.
        // `diverges` first — there the citation carries words the heading does not,
        // so it is either wrapped prose or a tail that is simply wrong, and only
        // those two are worth a human's eye. `truncated` is the established short
        // form, correct but exact-match-invisible.
        const g = new Map();
        for (const h of prefixes) {
            const k = [h.target, h.tight, h.matched, h.shape].join('|');
            if (!g.has(k)) g.set(k, { t: h.target, c: h.tight, m: h.matched, sh: h.shape, sites: [] });
            g.get(k).sites.push(h.file + ':' + h.line);
        }
        const rows = [...g.values()]
            .sort((a, b) => (a.sh === b.sh ? 0 : a.sh === 'diverges' ? -1 : 1) || b.sites.length - a.sites.length || a.t.localeCompare(b.t));
        console.log(`\n  PREFIX HITS — ${prefixes.length} citations in ${rows.length} distinct forms.`);
        console.log(`  The citation names an OPENING of a real heading, not the whole of it.`);
        for (const r of rows.filter((x) => x.sh === 'diverges')) {
            console.log(`
    [diverges] x${r.sites.length}  ${r.t}`);
            console.log(`       cites § ${r.c}`);
            console.log(`       real  § ${r.m}`);
            console.log(`       ${r.sites.slice(0, 6).join('  ')}${r.sites.length > 6 ? `  (+${r.sites.length - 6})` : ''}`);
        }
        // The truncated forms are the corpus's established short-hand and read as
        // correct; they fold to one line each, and --json carries their sites.
        const tr = rows.filter((x) => x.sh === 'truncated');
        console.log(`
    TRUNCATED (${tr.length} forms) — the cited text stops inside the heading: correct short-hand, exact-match-invisible.`);
        for (const r of tr) console.log(`      x${String(r.sites.length).padStart(2)}  ${r.t} § ${r.c}   ->   § ${r.m}`);
    }
    if (nodocs.length && !quiet) {
        console.log(`\n  NO SUCH DOC (${nodocs.length}):`);
        for (const h of nodocs) console.log(`    ${h.file}:${h.line}  ${h.named} § ${h.cited}`);
    }
    if (ambiguous.length && !quiet) {
        console.log(`\n  AMBIGUOUS DOC NAME (${ambiguous.length}) — a duplicated basename:`);
        for (const h of ambiguous) console.log(`    ${h.file}:${h.line}  ${h.named} -> ${h.cands.join(' | ')}`);
    }
}

function blockGraph() {
    console.log(`\n=== (b) THE HEADER GRAPH — ${headed.length} docs carry a header ===`);
    const edges = mutual.length * 2 + oneWay.length;
    console.log(`  ${edges} edges: ${mutual.length} MUTUAL pairs (${mutual.length * 2} edges) + ${oneWay.length} ONE-WAY edges.`);
    console.log(`  A boundary sweep scoped off the mutual pairs alone is blind to ${oneWay.length} edges —`);
    console.log(`  which is where a one-way sprawl lives. Both halves are printed.\n`);

    console.log(`  ONE-WAY, ranked by how many docs point at a target that points back at none.`);
    console.log(`  A high count is a doc whose neighbours all know the boundary and whose own`);
    console.log(`  header does not: the first place to scope a sweep.\n`);
    for (const [to, n] of sprawl.slice(0, quiet ? 0 : 15)) {
        const from = oneWay.filter((e) => e.to === to);
        console.log(`    ${String(n).padStart(2)}x  <- ${to}${graph.has(to) ? '' : '   (target carries NO header)'}`);
        for (const e of from) console.log(`          ${e.from}  [${e.kinds.join('+')}]`);
    }
    if (!quiet && sprawl.length > 15) console.log(`    ... and ${sprawl.length - 15} more targets with one-way edges (--json for all).`);

    if (!quiet) {
        console.log(`\n  MUTUAL PAIRS (${mutual.length}) — both docs name the other; a genuine shared border:`);
        for (const p of mutual) console.log(`    ${p.a}  <->  ${p.b}   [${p.kinds.join('+')} / ${p.back.join('+')}]`);
    }
}

function blockCoverage() {
    console.log(`\n=== (c) COVERAGE — CLAUDE.md § 3 vs the docs that carry a header ===`);
    console.log(`  ${routed.size} docs named in § 3.  ${headed.length} docs carry a header.`);
    console.log(`  ${routedNoHeader.length} routed with NO header.  ${headerNotRouted.length} headed but NOT routed.`);
    console.log(`\n  NO ALLOW-LIST is carried here, deliberately. An allow-list is a second`);
    console.log(`  catalogue to keep current, and the orphan list is precisely the router's`);
    console.log(`  staleness detector — an entry suppressed is the detector switched off for`);
    console.log(`  that doc. Ledger Q&As, design notes and mockdata will appear below and are`);
    console.log(`  expected to; a human reads the list, the tool does not judge it.`);
    if (!quiet) {
        if (routedNoHeader.length) {
            console.log(`\n  ROUTED IN § 3, NO HEADER (${routedNoHeader.length}) — the router promises an index the doc does not have:`);
            for (const p of routedNoHeader) console.log(`    ${p}`);
        }
        if (headerNotRouted.length) {
            console.log(`\n  CARRIES A HEADER, NOT IN § 3 (${headerNotRouted.length}) — either the router is behind, or the doc is not an authority:`);
            for (const p of headerNotRouted) console.log(`    ${p}`);
        }
    }
}

function blockState() {
    console.log(`\n=== (d) STATE-INDEPENDENCE — a header never says landed/built/pending/shipped, and never carries a BL- id ===`);
    console.log(`  ${stateHits.length} violation(s) across ${headed.length} headers.` + (stateHits.length ? '  ** FAIL **' : ''));
    if (!quiet) for (const s of stateHits) console.log(`    ${s.doc}:${s.line}  "${s.at}"  — ...${s.ctx}...`);
}

// --doc <NAME>: one doc, every angle.
const one = val('--doc');
if (one) {
    const r = resolveDoc(one, P('CLAUDE.md'));
    if (!r.path) { console.error(`header_graph: cannot resolve doc "${one}"`); process.exit(0); }
    const d = docs.get(r.path);
    console.log(`${d.path}   ${d.headings.length} headings`);
    if (!d.header) console.log(`  NO HEADER BLOCK.`);
    else {
        console.log(`  header line ${d.header.line}, opens with ${d.header.kind}:`);
        for (const l of ['settles', 'notHere', 'confusedWith']) if (d.header[l]) console.log(`    ${l}: ${d.header[l]}`);
    }
    console.log(`  in CLAUDE.md § 3: ${routed.has(d.path) ? 'yes' : 'NO'}`);
    const out = [...(graph.get(d.path) || new Map()).entries()];
    console.log(`  names ${out.length} doc(s): ${out.map(([t, k]) => `${t} [${[...k].join('+')}]${edgeTo(t, d.path) ? ' <->' : ' ->'}`).join(', ') || '(none)'}`);
    const inbound = [...graph.entries()].filter(([f, m]) => m.has(d.path)).map(([f]) => f);
    console.log(`  named by ${inbound.length}: ${inbound.join(', ') || '(none)'}`);
    const bad = citations.filter((h) => h.target === d.path && (h.cls === 'MISS' || h.cls === 'PREFIX'));
    console.log(`  broken citations INTO it: ${bad.length}`);
    for (const h of bad) console.log(`    [${h.cls}] ${h.file}:${h.line}  § ${h.cited}`);
    const mine = citations.filter((h) => h.file === d.path && (h.cls === 'MISS' || h.cls === 'PREFIX'));
    console.log(`  broken citations it MAKES: ${mine.length}`);
    for (const h of mine) console.log(`    [${h.cls}] ${h.target} § ${h.cited}`);
    process.exit(0);
}

if (has('--json')) {
    console.log(JSON.stringify({
        swept: SCAN_DIRS, citations: citations.length,
        tally: { ok: cite('OK').length, prefix: prefixes.length, miss: misses.length, nodoc: nodocs.length, ambiguous: ambiguous.length },
        misses, prefixes, nodocs, ambiguous,
        graph: { headed: headed.length, mutual, oneWay, sprawl },
        coverage: { routed: [...routed].sort(), headed: [...headedSet].sort(), routedNoHeader, headerNotRouted },
        state: stateHits,
    }, null, 1));
    process.exit(misses.length || stateHits.length ? 1 : 0);
}

if (show('dangling')) blockDangling();
if (show('graph')) blockGraph();
if (show('coverage')) blockCoverage();
if (show('state')) blockState();

// THE EXIT CODE ANSWERS THE QUESTION THAT WAS ASKED. `--graph` and `--coverage` print a
// judgement and report no problem, but the code was computed over every check regardless
// of which one ran, so both exited 1 for a reason that view never showed — and anything
// wiring them to a gate got a red light with no finding behind it.
const failed = (show('dangling') ? misses.length + (has('--strict') ? prefixes.length : 0) : 0)
    + (show('state') ? stateHits.length : 0);
console.log(`\nheader_graph: ${citations.length} citations · ${headed.length} headers · ${mutual.length} mutual · ${oneWay.length} one-way`);
console.log(`  ${misses.length} dangling, ${prefixes.length} prefix, ${stateHits.length} state-dependent -> ${failed ? 'FAIL' : 'pass'}`);
process.exit(failed ? 1 : 0);
