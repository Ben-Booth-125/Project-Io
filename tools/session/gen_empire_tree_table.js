#!/usr/bin/env node
// gen_empire_tree_table.js — generates a Lua-free, static C++ node-table header for a
// tree store (docs/generation/trees/<tree>_tree.json), the same pattern render_actions.js
// uses for ACTIONS.json -> ACTIONS.md.
//
// GENERALISED (BL-930) to serve every tree rather than forking a twin script per tree —
// the empire (BL-912) and exploration (BL-930) wirings both regenerate through this file,
// and colonisation/industry can join the same way when their turn comes. The one-arg
// default is unchanged: no argument means "empire", so every existing call site (docs,
// comments, muscle memory) keeps working exactly as it did before this item.
//
// Why generated rather than parsed at runtime: src/world has no JSON parser today (the
// grant register and TREES.md both call for a Lua-free translation unit), and hand-writing
// dozens of node rows would drift from the store the moment either changed. Run the lint
// (node tools/session/tree_lint.js <tree>) after any edit to a tree's JSON, then
// regenerate its header here, then rebuild.
//
// The table builder is EXPORTED (BL-974) so tree_lint.js can regenerate a store's table in
// memory and compare it byte for byte with the header on disk — a stale header is a lint
// failure, not a silent old tree. Keep the CLI and the export on the one code path.
//
// Run: node tools/session/gen_empire_tree_table.js [tree]
//   tree defaults to "empire"; pass "exploration" for the Exploration tree, etc.

const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, '..', '..');

function storePath(tree) { return path.join(ROOT, 'docs', 'generation', 'trees', `${tree}_tree.json`); }
function headerPath(tree) { return path.join(ROOT, 'src', 'world', `${tree}_tree_data.hpp`); }

function cppStr(s) { return JSON.stringify(s); }

// Build the header text for one tree from its store. Pure: reads the store, returns the
// text and a little summary, writes nothing. Throws on a malformed store.
function generate(tree)
{
    const STORE = storePath(tree);
    const NAMESPACE = `io::${tree}_tree`;

    const doc = JSON.parse(fs.readFileSync(STORE, 'utf8'));
    if (doc.tree !== tree) throw new Error(`expected tree: "${tree}" in ${STORE}`);

    const nodes = doc.nodes;
    const idIndex = new Map(nodes.map((n, i) => [n.id, i]));
    const branchIndex = new Map(doc.branches.map((b, i) => [b.key, i]));

    function idxOrMinus1(id) { return id == null ? -1 : idIndex.get(id); }

    // links are stored undirected (once, on the node farther from the root) in the store;
    // the sim needs the full undirected neighbour set per node to test rule 2
    // (OR-availability), so expand both directions here.
    const neighbours = nodes.map(() => new Set());
    for (let i = 0; i < nodes.length; ++i)
    {
        for (const linkId of nodes[i].links || [])
        {
            const j = idIndex.get(linkId);
            if (j == null) throw new Error(`unknown link target ${linkId} from ${nodes[i].id}`);
            neighbours[i].add(j);
            neighbours[j].add(i);
        }
    }

    let out = '';
    out += '// GENERATED FILE — DO NOT HAND-EDIT.\n';
    out += `// Source: docs/generation/trees/${tree}_tree.json\n`;
    out += `// Regenerate: node tools/session/gen_empire_tree_table.js ${tree}\n`;
    out += `// Lint first: node tools/session/tree_lint.js ${tree}\n`;
    out += '#pragma once\n\n';
    out += '#include <cstdint>\n';
    out += '#include "tree_effect.hpp" // the shared effect vocabulary (BL-973)\n\n';
    out += `namespace ${NAMESPACE} {\n\n`;
    out += `inline constexpr int node_count = ${nodes.length};\n\n`;
    out += 'enum class node_kind : uint8_t { minor = 0, major = 1, milestone = 2 };\n';
    out += 'enum class gate_atom : int8_t { none = -1, ore_q = 0, fuel = 1, arable = 2, coastal = 3, grassland = 4 };\n\n';
    out += 'struct node\n{\n';
    out += '    const char* id;\n';
    out += '    node_kind   kind;\n';
    out += '    uint8_t     ring;\n';
    out += '    uint8_t     branch;      ///< index into branch_count\n';
    out += '    gate_atom   gate;\n';
    out += '    int8_t      excludes;    ///< fork partner index, or -1\n';
    out += '    bool        is_root;     ///< OWN declared `links` was empty in the store -- the\n';
    out += '                             ///< tree\'s true entry point. NOT the same as "no\n';
    out += '                             ///< undirected neighbour": every other ring-1 major names\n';
    out += '                             ///< it as ITS prerequisite, which would otherwise make the\n';
    out += '                             ///< root look like it needs a neighbour held -- an\n';
    out += '                             ///< unlockable tree. Rule 2\'s OR-availability is skipped\n';
    out += '                             ///< entirely for a root.\n';
    out += '    uint64_t    neighbours_mask; ///< undirected link set (rule 2, OR-availability)\n';
    out += '    uint64_t    requires_mask;   ///< milestones: AND set (rule 4)\n';
    out += '    int8_t      requires_fork_a; ///< milestones: fork pair, EITHER satisfies (-1 if none)\n';
    out += '    int8_t      requires_fork_b;\n';
    out += '    uint8_t     effects_begin;   ///< first row of this node\'s run in `effects[]` (BL-973)\n';
    out += '    uint8_t     effects_n;       ///< rows in that run (never 0: the lint refuses an effectless node)\n';
    out += '};\n\n';

    out += `inline constexpr int branch_count = ${doc.branches.length};\n`;
    out += `inline constexpr const char* branch_names[branch_count] = { ${doc.branches.map(b => cppStr(b.key)).join(', ')} };\n\n`;

    // THE EFFECTS TABLE (BL-973): every node's effects, flattened into one
    // contiguous array in node order, each node keeping (begin, n) into it.
    // The vocabulary is io::tree_effect_kind / tree_modifier_term /
    // tree_effect_key (src/world/tree_effect.hpp), the same closed sets
    // tree_lint.js validates the store against — a store value outside them
    // is a lint failure before it is a generator one. Change the three lists
    // here, in the lint, and in tree_effect.hpp together.
    const EFFECT_KINDS = ['unlock', 'upgrade', 'retire', 'modifier', 'access', 'reach', 'intel',
        'institution', 'doctrine', 'resource', 'open'];
    const MOD_TERMS = ['reach', 'carrying_capacity', 'manpower', 'defence', 'industrial', 'stores',
        'cohesion', 'assimilation', 'plague', 'research', 'forage', 'muster_cost'];
    const EFFECT_KEYS = ['none', 'sea_legs', 'post_roads'];
    const effectRows = [];   // { cpp, comment }
    const effectRange = [];  // per node: [begin, n]
    let rimIndex = -1;
    for (let i = 0; i < nodes.length; ++i)
    {
        const n = nodes[i];
        const begin = effectRows.length;
        for (const e of n.effects || [])
        {
            if (!EFFECT_KINDS.includes(e.kind)) throw new Error(`${n.id}: effect kind ${e.kind} not in vocabulary`);
            let term = 'none', perMille = 0, key = 'none', openRing = 0, openTree = false;
            if (e.kind === 'modifier')
            {
                if (!MOD_TERMS.includes(e.target)) throw new Error(`${n.id}: modifier target ${e.target} not a sim term`);
                term = e.target;
                perMille = e.per_mille | 0;
            }
            if (e.key != null)
            {
                if (e.kind === 'modifier') throw new Error(`${n.id}: a modifier carries no key`);
                if (!EFFECT_KEYS.includes(e.key) || e.key === 'none')
                    throw new Error(`${n.id}: effect key ${e.key} not in vocabulary`);
                key = e.key;
            }
            if (e.kind === 'open')
            {
                const ring = /^ring (\d+)$/.exec(e.target);
                if (ring) openRing = parseInt(ring[1], 10);
                else if (/^[a-z]+ tree$/.test(e.target))
                {
                    if (rimIndex >= 0)
                        throw new Error(`${n.id}: a second node opens the next tree (first: ${nodes[rimIndex].id})`);
                    openTree = true;
                    rimIndex = i;
                }
                else throw new Error(`${n.id}: open target "${e.target}" is neither "ring N" nor "<tree> tree"`);
            }
            effectRows.push({
                cpp: `{ io::tree_effect_kind::${e.kind}, io::tree_modifier_term::${term}, ${perMille}, `
                   + `io::tree_effect_key::${key}, ${openRing}, ${openTree ? 'true' : 'false'}, ${cppStr(e.target)} }`,
                comment: `${effectRows.length}: ${n.id}`,
            });
        }
        effectRange.push([begin, effectRows.length - begin]);
    }
    if (effectRows.length > 255) throw new Error(`${STORE}: more than 255 effect rows (uint8_t ranges)`);

    // THE RIM, READ OFF THE STORE'S OWN EFFECT rather than counted: the one
    // node whose `open` effect targets the NEXT TREE ("exploration tree",
    // "industry tree"). TREES.md sec Milestones: "the last milestone unlocks
    // the next tree" — so it must also be the highest-ring spire milestone,
    // and the generator refuses a store where the two disagree. Before
    // BL-973 this was found structurally (highest SP milestone) and the
    // open effect itself was emitted nowhere.
    if (rimIndex < 0) throw new Error(`no node opens the next tree in ${STORE}`);
    const rim = { n: nodes[rimIndex], i: rimIndex };
    {
        const spireMilestones = nodes
            .map((n, i) => ({ n, i }))
            .filter(({ n }) => n.kind === 'milestone' && n.branch === 'SP');
        if (spireMilestones.length === 0) throw new Error(`no spire (SP) milestone found in ${STORE}`);
        const top = spireMilestones.reduce((a, b) => (b.n.ring > a.n.ring ? b : a));
        if (top.i !== rimIndex)
            throw new Error(`${rim.n.id} opens the next tree but ${top.n.id} is the highest spire milestone`);
    }
    out += `inline constexpr int rim_node_index = ${rim.i}; ///< ${rim.n.id}, "${rim.n.name}" — the node carrying the open-next-tree effect\n\n`;

    out += `inline constexpr int effect_count = ${effectRows.length};\n`;
    out += 'inline constexpr io::tree_effect effects[effect_count] = {\n';
    for (const r of effectRows) out += `    ${r.cpp}, // ${r.comment}\n`;
    out += '};\n\n';

    out += 'inline constexpr node nodes[node_count] = {\n';
    for (let i = 0; i < nodes.length; ++i)
    {
        const n = nodes[i];
        const kindName = { minor: 'minor', major: 'major', milestone: 'milestone' }[n.kind];
        const gateName = n.gate == null ? 'none' : n.gate;
        const excludes = idxOrMinus1(n.excludes);

        let neighMask = 0n;
        for (const j of neighbours[i]) neighMask |= (1n << BigInt(j));

        let reqMask = 0n;
        for (const rid of n.requires || [])
        {
            const j = idIndex.get(rid);
            if (j == null) throw new Error(`unknown requires target ${rid} from ${n.id}`);
            reqMask |= (1n << BigInt(j));
        }

        let forkA = -1, forkB = -1;
        if (n.requires_fork)
        {
            forkA = idIndex.get(n.requires_fork[0]);
            forkB = idIndex.get(n.requires_fork[1]);
        }

        const isRoot = isRootNode(n);
        out += `    { ${cppStr(n.id)}, node_kind::${kindName}, ${n.ring}, ${branchIndex.get(n.branch)}, `
             + `gate_atom::${gateName}, ${excludes}, ${isRoot ? 'true' : 'false'}, ${neighMask}ULL, `
             + `${reqMask}ULL, ${forkA}, ${forkB}, ${effectRange[i][0]}, ${effectRange[i][1]} }, // ${i}: ${n.name}\n`;
    }
    out += '};\n\n';

    // The scorer term per node (TREES.md § The scorer; pursued_when is the per-node spec).
    const TERMS = Array.from(new Set(nodes.map(n => n.pursued_when.term)));
    out += `inline constexpr int term_count = ${TERMS.length};\n`;
    out += `inline constexpr const char* term_names[term_count] = { ${TERMS.map(cppStr).join(', ')} };\n\n`;
    out += 'enum class scorer_term : uint8_t {\n';
    TERMS.forEach((t, i) => { out += `    ${t} = ${i},\n`; });
    out += '};\n\n';
    out += 'inline constexpr scorer_term node_term[node_count] = {\n';
    for (const n of nodes)
        out += `    scorer_term::${n.pursued_when.term},\n`;
    out += '};\n\n';

    out += `} // namespace ${NAMESPACE}\n`;

    return { text: out, nodeCount: nodes.length, branchCount: doc.branches.length, rimId: rim.n.id,
             effectCount: effectRows.length };
}

// THE ROOT DERIVATION, in one place: a node is the tree's root iff its OWN declared `links`
// is empty. The store convention that makes this hold is "each undirected edge declared
// once, on the node farther from the root" — so every non-root node declares at least the
// edge toward the root, and the root declares nothing. tree_lint.js asserts exactly one
// node satisfies this per tree, using this same function.
function isRootNode(n) { return (n.links || []).length === 0; }

module.exports = { generate, isRootNode, storePath, headerPath };

if (require.main === module)
{
    const TREE = process.argv[2] || 'empire';
    const OUT = headerPath(TREE);
    const r = generate(TREE);
    fs.writeFileSync(OUT, r.text);
    console.log(`wrote ${OUT} (${r.nodeCount} nodes, ${r.branchCount} branches, ${r.effectCount} effects, rim=${r.rimId})`);
}
