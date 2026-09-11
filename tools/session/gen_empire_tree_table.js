#!/usr/bin/env node
// gen_empire_tree_table.js — generates src/world/empire_tree_data.hpp, a Lua-free, static
// C++ table twin of docs/generation/trees/empire_tree.json (BL-912).
//
// Why generated rather than parsed at runtime: src/world has no JSON parser today (the
// grant register and TREES.md both call for a Lua-free translation unit), and hand-writing
// 52 node rows would drift from the store the moment either changed. This script is the
// sync point, the same pattern render_actions.js uses for ACTIONS.json -> ACTIONS.md: run
// the lint (node tools/session/tree_lint.js empire) after any edit to the JSON, then
// regenerate this header, then rebuild.
//
// Run: node tools/session/gen_empire_tree_table.js

const fs = require('fs');
const path = require('path');

const STORE = path.join(__dirname, '..', '..', 'docs', 'generation', 'trees', 'empire_tree.json');
const OUT = path.join(__dirname, '..', '..', 'src', 'world', 'empire_tree_data.hpp');

const doc = JSON.parse(fs.readFileSync(STORE, 'utf8'));
if (doc.tree !== 'empire') throw new Error('expected tree: "empire"');

const nodes = doc.nodes;
const idIndex = new Map(nodes.map((n, i) => [n.id, i]));
const branchIndex = new Map(doc.branches.map((b, i) => [b.key, i]));

function idxOrMinus1(id) { return id == null ? -1 : idIndex.get(id); }

function cppStr(s) { return JSON.stringify(s); }

// links are stored undirected (once, from the lower id) in the store; the sim needs the
// full undirected neighbour set per node to test rule 2 (OR-availability), so expand both
// directions here.
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
out += '// Source: docs/generation/trees/empire_tree.json\n';
out += '// Regenerate: node tools/session/gen_empire_tree_table.js\n';
out += '// Lint first: node tools/session/tree_lint.js empire\n';
out += '#pragma once\n\n';
out += '#include <cstdint>\n\n';
out += 'namespace io::empire_tree {\n\n';
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
out += '                             ///< tree\'s true entry point (EM-SP-1a). NOT the same as\n';
out += '                             ///< "no undirected neighbour": every other ring-1 major\n';
out += '                             ///< names it as ITS prerequisite, which would otherwise\n';
out += '                             ///< make the root look like it needs a neighbour held --\n';
out += '                             ///< an unlockable tree. Rule 2\'s OR-availability is skipped\n';
out += '                             ///< entirely for a root.\n';
out += '    uint64_t    neighbours_mask; ///< undirected link set (rule 2, OR-availability)\n';
out += '    uint64_t    requires_mask;   ///< milestones: AND set (rule 4)\n';
out += '    int8_t      requires_fork_a; ///< milestones: fork pair, EITHER satisfies (-1 if none)\n';
out += '    int8_t      requires_fork_b;\n';
out += '};\n\n';

out += `inline constexpr int branch_count = ${doc.branches.length};\n`;
out += `inline constexpr const char* branch_names[branch_count] = { ${doc.branches.map(b => cppStr(b.key)).join(', ')} };\n\n`;

// The rim milestone (BL-912): the item names EM-SP-4m, requiring EM-PE-4a and EM-RD-4b.
// Confirmed against the store rather than assumed.
const rimId = 'EM-SP-4m';
if (!idIndex.has(rimId)) throw new Error('rim milestone EM-SP-4m not found in store');
out += `inline constexpr int rim_node_index = ${idIndex.get(rimId)}; ///< ${rimId}, "${nodes[idIndex.get(rimId)].name}"\n\n`;

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

    const isRoot = (n.links || []).length === 0;
    out += `    { ${cppStr(n.id)}, node_kind::${kindName}, ${n.ring}, ${branchIndex.get(n.branch)}, `
         + `gate_atom::${gateName}, ${excludes}, ${isRoot ? 'true' : 'false'}, ${neighMask}ULL, `
         + `${reqMask}ULL, ${forkA}, ${forkB} }, // ${i}: ${n.name}\n`;
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

out += '} // namespace io::empire_tree\n';

fs.writeFileSync(OUT, out);
console.log(`wrote ${OUT} (${nodes.length} nodes, ${doc.branches.length} branches, rim=${rimId})`);
