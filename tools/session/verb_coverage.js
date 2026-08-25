#!/usr/bin/env node
// verb_coverage.js — verb reachability coverage tool (BL-444).
//
// Joins four data sources with no external dependencies, in the style of
// backlog_query.js / actions_query.js:
//
//   1. Seam       — the corp_verb enum in src/world/corp_command.hpp. Ground
//                    truth: every verb a corp_command can carry.
//   2. Dictionary — docs/ai/ACTIONS.json's "gameplay" family. Each seam verb
//                    should have a "gameplay.<verb>" entry; a missing one is
//                    a dictionary gap (BL-270's own promise: "where dictionary
//                    and seam disagree, the dictionary is wrong").
//   3. UI press   — the matched entry's `surface` field. Either a real UI
//                    description, or the literal SEAM-ONLY marker text
//                    (gameplay.md's convention: "SEAM-ONLY: this verb has NO
//                    player-facing press").
//   4. AI scorer  — grep of src/world/corp_ai.cpp for `.verb = corp_verb::X`
//                    assignments (not `==` comparisons): which verbs the
//                    deterministic scored-utility layer (corp_ai.cpp) ever
//                    emits as a candidate.
//
// A second, AUTHORED table maps each seam verb to a src/world/ subsystem, so
// a subsystem that owns zero verbs is visible rather than silently absent
// (Ben's 2026-08-17 widening — the proof case was the convoy layer owning
// zero of fifteen verbs before BL-452 gave it two).
//
// EXIT CODE: non-zero ONLY when a seam verb is missing from the dictionary.
// UI-press gaps and AI-candidate gaps are reported but never fail the run —
// some verbs are legitimately SEAM-ONLY (e.g. accept_quote) or legitimately
// absent from the scorer (e.g. set_workforce_auto, a player-only hand-back).
//
// USAGE:
//   node tools/session/verb_coverage.js            both tables, plain text
//   node tools/session/verb_coverage.js --json      machine-readable dump

'use strict';
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..', '..');
const CORP_COMMAND_HPP = path.join(ROOT, 'src', 'world', 'corp_command.hpp');
const ACTIONS_JSON = path.join(ROOT, 'docs', 'ai', 'ACTIONS.json');
const CORP_AI_CPP = path.join(ROOT, 'src', 'world', 'corp_ai.cpp');

const argv = process.argv.slice(2);
const wantJson = argv.includes('--json');

// ---------------------------------------------------------------------------
// 1. Seam — parse `enum class corp_verb` out of corp_command.hpp.
// ---------------------------------------------------------------------------
function parseSeamVerbs() {
    const src = fs.readFileSync(CORP_COMMAND_HPP, 'utf8');
    const enumStart = src.indexOf('enum class corp_verb');
    if (enumStart === -1) throw new Error('corp_verb enum not found in corp_command.hpp');
    const braceOpen = src.indexOf('{', enumStart);
    const braceClose = src.indexOf('};', braceOpen);
    if (braceOpen === -1 || braceClose === -1) throw new Error('could not locate corp_verb enum body');
    const body = src.slice(braceOpen + 1, braceClose);

    const verbs = [];
    for (const rawLine of body.split('\n')) {
        // Strip line comments (// ...) before looking for an enumerator.
        const line = rawLine.replace(/\/\/.*$/, '').trim();
        if (!line) continue;
        const m = line.match(/^([A-Za-z_][A-Za-z0-9_]*)/);
        if (!m) continue;
        verbs.push(m[1]);
    }
    return verbs;
}

// ---------------------------------------------------------------------------
// 2 & 3. Dictionary + UI press — docs/ai/ACTIONS.json's "gameplay" family.
// ---------------------------------------------------------------------------
function loadGameplayEntries() {
    const { entries } = JSON.parse(fs.readFileSync(ACTIONS_JSON, 'utf8'));
    return entries.filter((e) => e.family === 'gameplay');
}

// gameplay entries are id'd "gameplay.<verb>" (confirmed by inspection of
// gameplay.build / gameplay.accept_quote) — derive the verb name from the id
// rather than assuming a separate field.
function verbFromEntryId(id) {
    const m = /^gameplay\.(.+)$/.exec(id);
    return m ? m[1] : null;
}

// ---------------------------------------------------------------------------
// 4. AI scorer — grep corp_ai.cpp for verb-assignment lines.
// ---------------------------------------------------------------------------
function parseAiCandidateVerbs() {
    const src = fs.readFileSync(CORP_AI_CPP, 'utf8');
    const emitted = new Set();
    // Matches `<something>.verb = corp_verb::name;` (any whitespace), while
    // excluding `==` comparisons (`is_build = (c.cmd.verb == corp_verb::build)`).
    const re = /\bverb\s*=\s*corp_verb::([A-Za-z_][A-Za-z0-9_]*)/g;
    for (const line of src.split('\n')) {
        if (line.includes('==')) continue; // comparison, not an assignment
        let m;
        re.lastIndex = 0;
        while ((m = re.exec(line)) !== null) {
            emitted.add(m[1]);
        }
    }
    return emitted;
}

// ---------------------------------------------------------------------------
// Table 2 — AUTHORED subsystem ownership map.
//
// Hand-maintained, not derived: reviewed by a human over time as verbs and
// subsystems are added. Every src/world/ subsystem that owns at least one
// verb is listed, PLUS subsystems that legitimately own zero, so the gap is
// visible (the design's proof case: Convoy/Logistics owned zero of fifteen
// verbs until BL-452 gave it dispatch_convoy/hold_convoy).
// ---------------------------------------------------------------------------
const SUBSYSTEM_MAP = [
    {
        subsystem: 'Extraction / Production / Construction',
        files: ['construction.cpp', 'placement_rules.cpp', 'building_profit.cpp'],
        verbs: ['build', 'demolish', 'set_recipe', 'set_workforce', 'set_workforce_auto', 'idle', 'resume'],
    },
    {
        subsystem: 'Roads',
        files: ['road_generation.cpp'],
        verbs: ['place_road'],
    },
    {
        subsystem: 'Survey',
        files: ['survey_system.cpp'],
        verbs: ['survey'],
    },
    {
        subsystem: 'Military / Units',
        files: ['combat.cpp', 'campaign_battle.cpp', 'terrain_combat.cpp', 'unit_roster.cpp'],
        verbs: ['hire_unit', 'march_unit', 'halt_unit', 'disband_unit'],
    },
    {
        subsystem: 'Market / Trade (order book)',
        files: ['order_book.cpp', 'market_clearing.cpp'],
        verbs: ['place_sell_order', 'remove_sell_order'],
    },
    {
        subsystem: 'Procurement',
        files: ['procurement.cpp'],
        verbs: ['request_quote', 'accept_quote', 'cancel_contract'],
    },
    {
        subsystem: 'Convoy / Logistics',
        files: ['supply_system.cpp', 'logistics.cpp'],
        verbs: ['dispatch_convoy', 'hold_convoy'],
    },
    {
        subsystem: 'Diplomacy / Stance',
        files: ['stance.cpp', 'standing.cpp'],
        verbs: ['declare_hostile', 'offer_friendship', 'accept_friendship', 'return_to_neutral'],
    },
    {
        subsystem: 'Battle',
        files: ['battle_system.cpp', 'campaign_battle.cpp'],
        verbs: ['withdraw_from_battle'],
    },
    {
        subsystem: 'Contracts / Mercenary',
        files: ['contracts.cpp', 'mercenary.cpp'],
        verbs: ['accept_offer', 'abandon_contract'],
    },
    {
        subsystem: 'Population / Centres',
        files: ['population_generation.cpp', 'economy_system.cpp'],
        verbs: ['raze_centre'],
    },
    {
        subsystem: 'Research / Tech tree',
        files: ['tech_tree.cpp', 'tech_gate.cpp'],
        verbs: [],
    },
    {
        subsystem: 'Law',
        files: ['law.cpp'],
        verbs: [],
    },
];

// ---------------------------------------------------------------------------
// Build Table 1.
// ---------------------------------------------------------------------------
function buildReachabilityTable() {
    const seamVerbs = parseSeamVerbs();
    const gameplayEntries = loadGameplayEntries();

    const byVerb = new Map();
    for (const e of gameplayEntries) {
        const v = verbFromEntryId(e.id);
        if (v) byVerb.set(v, e);
    }

    const aiVerbs = parseAiCandidateVerbs();

    const rows = seamVerbs.map((verb) => {
        const entry = byVerb.get(verb);
        const inDictionary = !!entry;
        let uiPress = 'N/A (dictionary gap)';
        if (entry) {
            const surface = entry.surface || '';
            uiPress = /^\s*SEAM-ONLY/i.test(surface) || /\bSEAM-ONLY\b/.test(surface.slice(0, 20))
                ? 'SEAM-ONLY'
                : 'UI';
        }
        const aiCandidate = aiVerbs.has(verb);
        return { verb, inDictionary, uiPress, aiCandidate };
    });

    return { seamVerbs, rows };
}

// ---------------------------------------------------------------------------
// Build Table 2.
// ---------------------------------------------------------------------------
function buildSubsystemTable(seamVerbs) {
    const owned = new Set();
    for (const row of SUBSYSTEM_MAP) for (const v of row.verbs) owned.add(v);
    const unmapped = seamVerbs.filter((v) => !owned.has(v));
    return { rows: SUBSYSTEM_MAP, unmapped };
}

// ---------------------------------------------------------------------------
// Render.
// ---------------------------------------------------------------------------
function pad(s, w) { return String(s).padEnd(w); }

function renderTable1(rows) {
    const cols = ['verb', 'dictionary', 'UI press', 'AI candidate'];
    const cells = rows.map((r) => [
        r.verb,
        r.inDictionary ? 'yes' : 'MISSING',
        r.uiPress,
        r.aiCandidate ? 'yes' : 'no',
    ]);
    const widths = cols.map((c, i) => Math.max(c.length, ...cells.map((row) => row[i].length)));
    const line = (vals) => vals.map((v, i) => pad(v, widths[i])).join('  ');
    const out = [];
    out.push(line(cols));
    out.push(widths.map((w) => '-'.repeat(w)).join('  '));
    for (const row of cells) out.push(line(row));
    return out.join('\n');
}

function renderTable2(rows, seamVerbCount) {
    const cols = ['subsystem', 'files', 'verb count', 'verbs'];
    const cells = rows.map((r) => [
        r.subsystem,
        r.files.join(', '),
        String(r.verbs.length),
        r.verbs.join(', ') || '(none)',
    ]);
    const widths = cols.map((c, i) => Math.max(c.length, ...cells.map((row) => Math.min(row[i].length, 60))));
    const trim = (s, w) => (s.length > w ? s.slice(0, w - 1) + '…' : s);
    const line = (vals) => vals.map((v, i) => pad(trim(v, widths[i]), widths[i])).join('  ');
    const out = [];
    out.push(line(cols));
    out.push(widths.map((w) => '-'.repeat(w)).join('  '));
    for (const row of cells) out.push(line(row));
    const totalMapped = rows.reduce((n, r) => n + r.verbs.length, 0);
    out.push('');
    out.push(`${totalMapped} of ${seamVerbCount} seam verb(s) mapped to a subsystem.`);
    return out.join('\n');
}

// ---------------------------------------------------------------------------
// Main.
// ---------------------------------------------------------------------------
function main() {
    const { seamVerbs, rows } = buildReachabilityTable();
    const { rows: subsystemRows, unmapped } = buildSubsystemTable(seamVerbs);

    const dictionaryGaps = rows.filter((r) => !r.inDictionary).map((r) => r.verb);
    const uiGaps = rows.filter((r) => r.inDictionary && r.uiPress === 'SEAM-ONLY').map((r) => r.verb);
    const aiGaps = rows.filter((r) => !r.aiCandidate).map((r) => r.verb);
    const zeroVerbSubsystems = subsystemRows.filter((r) => r.verbs.length === 0).map((r) => r.subsystem);

    if (wantJson) {
        console.log(JSON.stringify({
            seam_verb_count: seamVerbs.length,
            reachability: rows,
            subsystems: subsystemRows,
            unmapped_verbs: unmapped,
            dictionary_gaps: dictionaryGaps,
            ui_press_gaps: uiGaps,
            ai_candidate_gaps: aiGaps,
            zero_verb_subsystems: zeroVerbSubsystems,
        }, null, 1));
    } else {
        console.log(`Table 1 — per-verb reachability (${seamVerbs.length} seam verbs)\n`);
        console.log(renderTable1(rows));
        console.log('');
        console.log(`  dictionary: ${seamVerbs.length - dictionaryGaps.length}/${seamVerbs.length} covered` +
            (dictionaryGaps.length ? `  -- GAP: ${dictionaryGaps.join(', ')}` : ''));
        console.log(`  UI press:   ${rows.length - uiGaps.length}/${rows.length} have a human press` +
            (uiGaps.length ? `  (SEAM-ONLY: ${uiGaps.join(', ')})` : ''));
        console.log(`  AI scorer:  ${rows.length - aiGaps.length}/${rows.length} emitted by corp_ai.cpp` +
            (aiGaps.length ? `  (not emitted: ${aiGaps.join(', ')})` : ''));

        console.log('\n\nTable 2 — per-subsystem verb ownership (authored map, not derived)\n');
        console.log(renderTable2(subsystemRows, seamVerbs.length));
        if (zeroVerbSubsystems.length) {
            console.log(`\n  zero-verb subsystem(s): ${zeroVerbSubsystems.join(', ')}`);
        }
        if (unmapped.length) {
            console.log(`  UNMAPPED seam verb(s) (missing from SUBSYSTEM_MAP): ${unmapped.join(', ')}`);
        }
    }

    if (dictionaryGaps.length) {
        console.error(`\nverb_coverage: ${dictionaryGaps.length} seam verb(s) missing from the ACTIONS.json dictionary: ${dictionaryGaps.join(', ')}`);
        process.exit(1);
    }

    // MCP drift guard (2026-08-25, NR-639 — a deliberate widening of the exit
    // contract): tools/mcp/server.js's VERBS array is index-is-value against
    // the seam enum, and it silently fell TEN verbs behind. The wire seam is
    // an AI-facing boundary (io-standing-rules.md), so divergence fails the
    // run exactly like a dictionary gap: length AND order must match.
    {
        const serverSrc = fs.readFileSync(path.join(ROOT, 'tools', 'mcp', 'server.js'), 'utf8');
        const m = serverSrc.match(/const VERBS = \[([\s\S]*?)\];/);
        const mcpVerbs = m ? [...m[1].matchAll(/'([a-z_]+)'/g)].map((x) => x[1]) : [];
        const mismatch = mcpVerbs.length !== seamVerbs.length
            || mcpVerbs.some((v, i) => v !== seamVerbs[i]);
        if (mismatch) {
            console.error(`\nverb_coverage: tools/mcp/server.js VERBS diverges from the seam ` +
                `(${mcpVerbs.length} vs ${seamVerbs.length} verbs` +
                `${mcpVerbs.length === seamVerbs.length ? ', order differs' : ''}) — ` +
                `append the enum tail in order; index-is-value.`);
            process.exit(1);
        }
        console.log(`\n  MCP server:  VERBS in lockstep with the seam (${mcpVerbs.length} verbs).`);
    }
    process.exit(0);
}

main();
