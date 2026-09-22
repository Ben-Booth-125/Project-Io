// GENERATED FILE — DO NOT HAND-EDIT.
// Source: docs/generation/trees/empire_tree.json
// Regenerate: node tools/session/gen_empire_tree_table.js empire
// Lint first: node tools/session/tree_lint.js empire
#pragma once

#include <cstdint>
#include "tree_effect.hpp" // the shared effect vocabulary (BL-973)

namespace io::empire_tree {

inline constexpr int node_count = 52;

enum class node_kind : uint8_t { minor = 0, major = 1, milestone = 2 };
enum class gate_atom : int8_t { none = -1, ore_q = 0, fuel = 1, arable = 2, coastal = 3, grassland = 4 };

struct node
{
    const char* id;
    node_kind   kind;
    uint8_t     ring;
    uint8_t     branch;      ///< index into branch_count
    gate_atom   gate;
    int8_t      excludes;    ///< fork partner index, or -1
    bool        is_root;     ///< OWN declared `links` was empty in the store -- the
                             ///< tree's true entry point. NOT the same as "no
                             ///< undirected neighbour": every other ring-1 major names
                             ///< it as ITS prerequisite, which would otherwise make the
                             ///< root look like it needs a neighbour held -- an
                             ///< unlockable tree. Rule 2's OR-availability is skipped
                             ///< entirely for a root.
    uint64_t    neighbours_mask; ///< undirected link set (rule 2, OR-availability)
    uint64_t    requires_mask;   ///< milestones: AND set (rule 4)
    int8_t      requires_fork_a; ///< milestones: fork pair, EITHER satisfies (-1 if none)
    int8_t      requires_fork_b;
    uint8_t     effects_begin;   ///< first row of this node's run in `effects[]` (BL-973)
    uint8_t     effects_n;       ///< rows in that run (never 0: the lint refuses an effectless node)
};

inline constexpr int branch_count = 5;
inline constexpr const char* branch_names[branch_count] = { "SP", "RD", "AR", "HA", "PE" };

inline constexpr int rim_node_index = 7; ///< EM-SP-4m, "The Enforceable Promise" — the node carrying the open-next-tree effect

inline constexpr int effect_count = 88;
inline constexpr io::tree_effect effects[effect_count] = {
    { io::tree_effect_kind::institution, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "the sovereign counts" }, // 0: EM-SP-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::manpower, 40, io::tree_effect_key::none, 0, false, "manpower" }, // 1: EM-SP-1a
    { io::tree_effect_kind::open, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 2, false, "ring 2" }, // 2: EM-SP-1m
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 80, io::tree_effect_key::none, 0, false, "stores" }, // 3: EM-SP-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 40, io::tree_effect_key::none, 0, false, "research" }, // 4: EM-SP-2a
    { io::tree_effect_kind::open, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 3, false, "ring 3" }, // 5: EM-SP-2m
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 120, io::tree_effect_key::none, 0, false, "research" }, // 6: EM-SP-3a
    { io::tree_effect_kind::institution, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "the record survives the dynasty" }, // 7: EM-SP-3a
    { io::tree_effect_kind::open, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 4, false, "ring 4" }, // 8: EM-SP-3m
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 100, io::tree_effect_key::none, 0, false, "stores" }, // 9: EM-SP-4a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 60, io::tree_effect_key::none, 0, false, "research" }, // 10: EM-SP-4a
    { io::tree_effect_kind::open, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, true, "exploration tree" }, // 11: EM-SP-4m
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 100, io::tree_effect_key::none, 0, false, "reach" }, // 12: EM-RD-1a
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Way Station" }, // 13: EM-RD-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 30, io::tree_effect_key::none, 0, false, "reach" }, // 14: EM-RD-1b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 80, io::tree_effect_key::none, 0, false, "reach" }, // 15: EM-RD-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::cohesion, 30, io::tree_effect_key::none, 0, false, "cohesion" }, // 16: EM-RD-2a
    { io::tree_effect_kind::access, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "strait leg — campaign and settle across a narrow water" }, // 17: EM-RD-2b
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Harbour Mole" }, // 18: EM-RD-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 40, io::tree_effect_key::none, 0, false, "reach" }, // 19: EM-RD-2c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 100, io::tree_effect_key::none, 0, false, "reach" }, // 20: EM-RD-3a
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Span Bridge" }, // 21: EM-RD-3a
    { io::tree_effect_kind::access, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "open-sea leg — staging across a sea rather than a strait" }, // 22: EM-RD-3b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 60, io::tree_effect_key::none, 0, false, "reach" }, // 23: EM-RD-3b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::forage, 40, io::tree_effect_key::none, 0, false, "forage" }, // 24: EM-RD-3c
    { io::tree_effect_kind::access, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "far leg — the long crossing pass 2 needs for colonisation across water" }, // 25: EM-RD-4a
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Deepwater Wharf" }, // 26: EM-RD-4a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 60, io::tree_effect_key::none, 0, false, "reach" }, // 27: EM-RD-4b
    { io::tree_effect_kind::intel, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "neighbours' holdings and stacks are visible to the scorer" }, // 28: EM-RD-4b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 40, io::tree_effect_key::none, 0, false, "stores" }, // 29: EM-RD-4c
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Ore Pits" }, // 30: EM-AR-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::defence, 40, io::tree_effect_key::none, 0, false, "defence" }, // 31: EM-AR-1a
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "unit roster: classical" }, // 32: EM-AR-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::manpower, 40, io::tree_effect_key::none, 0, false, "manpower" }, // 33: EM-AR-1b
    { io::tree_effect_kind::upgrade, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "unit roster: siege train" }, // 34: EM-AR-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::muster_cost, -30, io::tree_effect_key::none, 0, false, "muster_cost" }, // 35: EM-AR-2a
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Wall Circuit" }, // 36: EM-AR-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::defence, 100, io::tree_effect_key::none, 0, false, "defence" }, // 37: EM-AR-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::forage, 40, io::tree_effect_key::none, 0, false, "forage" }, // 38: EM-AR-2c
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "unit roster: medieval — heavy horse" }, // 39: EM-AR-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 40, io::tree_effect_key::none, 0, false, "reach" }, // 40: EM-AR-3a
    { io::tree_effect_kind::upgrade, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "unit roster: arms and armour" }, // 41: EM-AR-3b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::defence, 60, io::tree_effect_key::none, 0, false, "defence" }, // 42: EM-AR-3b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::muster_cost, -30, io::tree_effect_key::none, 0, false, "muster_cost" }, // 43: EM-AR-3c
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Stone Fortress" }, // 44: EM-AR-4a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::defence, 150, io::tree_effect_key::none, 0, false, "defence" }, // 45: EM-AR-4a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 150, io::tree_effect_key::none, 0, false, "industrial" }, // 46: EM-AR-4c
    { io::tree_effect_kind::upgrade, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "unit roster: cast iron" }, // 47: EM-AR-4c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 40, io::tree_effect_key::none, 0, false, "industrial" }, // 48: EM-AR-4d
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 80, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 49: EM-HA-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 120, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 50: EM-HA-1c
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Channel Works" }, // 51: EM-HA-1c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 150, io::tree_effect_key::none, 0, false, "stores" }, // 52: EM-HA-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::cohesion, 40, io::tree_effect_key::none, 0, false, "cohesion" }, // 53: EM-HA-2a
    { io::tree_effect_kind::doctrine, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Granary Doctrine — the surplus is gathered and redistributed by command" }, // 54: EM-HA-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 60, io::tree_effect_key::none, 0, false, "stores" }, // 55: EM-HA-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 60, io::tree_effect_key::none, 0, false, "research" }, // 56: EM-HA-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 40, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 57: EM-HA-2b
    { io::tree_effect_kind::doctrine, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Granary Doctrine — the surplus is priced and traded" }, // 58: EM-HA-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 40, io::tree_effect_key::none, 0, false, "stores" }, // 59: EM-HA-2c
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Water Mill" }, // 60: EM-HA-2d
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 60, io::tree_effect_key::none, 0, false, "industrial" }, // 61: EM-HA-2d
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 40, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 62: EM-HA-2d
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 150, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 63: EM-HA-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 60, io::tree_effect_key::none, 0, false, "industrial" }, // 64: EM-HA-3b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 30, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 65: EM-HA-3b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 40, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 66: EM-HA-3c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 150, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 67: EM-HA-4a
    { io::tree_effect_kind::access, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "wetland ground becomes arable" }, // 68: EM-HA-4a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 120, io::tree_effect_key::none, 0, false, "industrial" }, // 69: EM-HA-4b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 40, io::tree_effect_key::none, 0, false, "stores" }, // 70: EM-HA-4b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::plague, 60, io::tree_effect_key::none, 0, false, "plague" }, // 71: EM-PE-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::assimilation, 30, io::tree_effect_key::none, 0, false, "assimilation" }, // 72: EM-PE-1b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::assimilation, 80, io::tree_effect_key::none, 0, false, "assimilation" }, // 73: EM-PE-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::cohesion, 40, io::tree_effect_key::none, 0, false, "cohesion" }, // 74: EM-PE-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 30, io::tree_effect_key::none, 0, false, "reach" }, // 75: EM-PE-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 80, io::tree_effect_key::none, 0, false, "research" }, // 76: EM-PE-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 30, io::tree_effect_key::none, 0, false, "stores" }, // 77: EM-PE-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::plague, 40, io::tree_effect_key::none, 0, false, "plague" }, // 78: EM-PE-2c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::plague, 120, io::tree_effect_key::none, 0, false, "plague" }, // 79: EM-PE-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 80, io::tree_effect_key::none, 0, false, "research" }, // 80: EM-PE-3b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 40, io::tree_effect_key::none, 0, false, "stores" }, // 81: EM-PE-3b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::assimilation, 40, io::tree_effect_key::none, 0, false, "assimilation" }, // 82: EM-PE-3c
    { io::tree_effect_kind::institution, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "an entity that outlives its members can own" }, // 83: EM-PE-4a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 80, io::tree_effect_key::none, 0, false, "stores" }, // 84: EM-PE-4a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::assimilation, 150, io::tree_effect_key::none, 0, false, "assimilation" }, // 85: EM-PE-4b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::cohesion, 60, io::tree_effect_key::none, 0, false, "cohesion" }, // 86: EM-PE-4b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::assimilation, 30, io::tree_effect_key::none, 0, false, "assimilation" }, // 87: EM-PE-4c
};

inline constexpr node nodes[node_count] = {
    { "EM-SP-1a", node_kind::major, 1, 0, gate_atom::none, -1, true, 2200097521922ULL, 0ULL, -1, -1, 0, 2 }, // 0: Codified Law & Census
    { "EM-SP-1m", node_kind::milestone, 1, 0, gate_atom::none, -1, false, 5ULL, 1073742080ULL, -1, -1, 2, 1 }, // 1: The Written Ledger
    { "EM-SP-2a", node_kind::major, 2, 0, gate_atom::ore_q, -1, false, 17179870218ULL, 0ULL, -1, -1, 3, 2 }, // 2: Coinage at Scale
    { "EM-SP-2m", node_kind::milestone, 2, 0, gate_atom::none, -1, false, 20ULL, 8796093023232ULL, 32, 33, 5, 1 }, // 3: The Sworn Province
    { "EM-SP-3a", node_kind::major, 3, 0, gate_atom::none, -1, false, 281474976710696ULL, 0ULL, -1, -1, 6, 2 }, // 4: Endowed Scholarship
    { "EM-SP-3m", node_kind::milestone, 3, 0, gate_atom::none, -1, false, 80ULL, 140806207832064ULL, -1, -1, 8, 1 }, // 5: The Lettered Court
    { "EM-SP-4a", node_kind::major, 4, 0, gate_atom::none, -1, false, 562949953421472ULL, 0ULL, -1, -1, 9, 2 }, // 6: Credit Instruments & Double Entry
    { "EM-SP-4m", node_kind::milestone, 4, 0, gate_atom::none, -1, false, 64ULL, 562949953552384ULL, -1, -1, 11, 1 }, // 7: The Enforceable Promise
    { "EM-RD-1a", node_kind::major, 1, 1, gate_atom::none, -1, false, 513ULL, 0ULL, -1, -1, 12, 2 }, // 8: Engineered Road Network
    { "EM-RD-1b", node_kind::minor, 1, 1, gate_atom::none, -1, false, 3328ULL, 0ULL, -1, -1, 14, 1 }, // 9: Road Surveys
    { "EM-RD-2a", node_kind::major, 2, 1, gate_atom::none, -1, false, 4612ULL, 0ULL, -1, -1, 15, 2 }, // 10: Way Stations & Post
    { "EM-RD-2b", node_kind::major, 2, 1, gate_atom::coastal, -1, false, 16896ULL, 0ULL, -1, -1, 17, 2 }, // 11: Deep-Hull Sail
    { "EM-RD-2c", node_kind::minor, 2, 1, gate_atom::none, -1, false, 4203520ULL, 0ULL, -1, -1, 19, 1 }, // 12: Span Bridges
    { "EM-RD-3a", node_kind::major, 3, 1, gate_atom::none, -1, false, 36864ULL, 0ULL, -1, -1, 20, 2 }, // 13: Passes & Causeways
    { "EM-RD-3b", node_kind::major, 3, 1, gate_atom::coastal, -1, false, 67584ULL, 0ULL, -1, -1, 22, 2 }, // 14: Lateen & Long-Range Rig
    { "EM-RD-3c", node_kind::minor, 3, 1, gate_atom::none, -1, false, 139264ULL, 0ULL, -1, -1, 24, 1 }, // 15: Caravan Halts
    { "EM-RD-4a", node_kind::major, 4, 1, gate_atom::coastal, -1, false, 16384ULL, 0ULL, -1, -1, 25, 2 }, // 16: Ocean-Rated Hulls
    { "EM-RD-4b", node_kind::major, 4, 1, gate_atom::none, -1, false, 294912ULL, 0ULL, -1, -1, 27, 2 }, // 17: Imperial Cartography
    { "EM-RD-4c", node_kind::minor, 4, 1, gate_atom::none, -1, false, 562949953552384ULL, 0ULL, -1, -1, 29, 1 }, // 18: Toll Stations
    { "EM-AR-1a", node_kind::major, 1, 2, gate_atom::ore_q, -1, false, 1048577ULL, 0ULL, -1, -1, 30, 3 }, // 19: Bloomery Iron
    { "EM-AR-1b", node_kind::minor, 1, 2, gate_atom::none, -1, false, 6815744ULL, 0ULL, -1, -1, 33, 1 }, // 20: Drill & Levy Rolls
    { "EM-AR-2a", node_kind::major, 2, 2, gate_atom::none, -1, false, 9437184ULL, 0ULL, -1, -1, 34, 2 }, // 21: Siegecraft
    { "EM-AR-2b", node_kind::major, 2, 2, gate_atom::none, -1, false, 68161536ULL, 0ULL, -1, -1, 36, 2 }, // 22: Wall Circuit
    { "EM-AR-2c", node_kind::minor, 2, 2, gate_atom::none, -1, false, 18874368ULL, 0ULL, -1, -1, 38, 1 }, // 23: Quartermasters
    { "EM-AR-3a", node_kind::major, 3, 2, gate_atom::grassland, -1, false, 8388608ULL, 0ULL, -1, -1, 39, 2 }, // 24: Stirrup Cavalry
    { "EM-AR-3b", node_kind::major, 3, 2, gate_atom::ore_q, -1, false, 603979776ULL, 0ULL, -1, -1, 41, 2 }, // 25: Pattern-Forged Steel
    { "EM-AR-3c", node_kind::minor, 3, 2, gate_atom::none, -1, false, 34531704832ULL, 0ULL, -1, -1, 43, 1 }, // 26: Armourers' Guild
    { "EM-AR-4a", node_kind::major, 4, 2, gate_atom::none, -1, false, 67108864ULL, 0ULL, -1, -1, 44, 2 }, // 27: Stone Fortress
    { "EM-AR-4c", node_kind::major, 4, 2, gate_atom::fuel, -1, false, 536870912ULL, 0ULL, -1, -1, 46, 2 }, // 28: Blast Furnace
    { "EM-AR-4d", node_kind::minor, 4, 2, gate_atom::none, -1, false, 301989888ULL, 0ULL, -1, -1, 48, 1 }, // 29: Charcoal Burners
    { "EM-HA-1a", node_kind::major, 1, 3, gate_atom::none, -1, false, 19327352833ULL, 0ULL, -1, -1, 49, 1 }, // 30: Iron-Shod Plough
    { "EM-HA-1c", node_kind::major, 1, 3, gate_atom::arable, -1, false, 35433480192ULL, 0ULL, -1, -1, 50, 2 }, // 31: Irrigation Works
    { "EM-HA-2a", node_kind::major, 2, 3, gate_atom::none, 33, false, 292057776128ULL, 0ULL, -1, -1, 52, 3 }, // 32: Temple Stores
    { "EM-HA-2b", node_kind::major, 2, 3, gate_atom::none, 32, false, 292057776128ULL, 0ULL, -1, -1, 55, 4 }, // 33: Open Granaries
    { "EM-HA-2c", node_kind::minor, 2, 3, gate_atom::none, -1, false, 13958643716ULL, 0ULL, -1, -1, 59, 1 }, // 34: Harvest Tallies
    { "EM-HA-2d", node_kind::major, 2, 3, gate_atom::arable, -1, false, 139653545984ULL, 0ULL, -1, -1, 60, 3 }, // 35: Water Wheel
    { "EM-HA-3a", node_kind::major, 3, 3, gate_atom::none, -1, false, 824633720832ULL, 0ULL, -1, -1, 63, 1 }, // 36: Three-Field Rotation
    { "EM-HA-3b", node_kind::major, 3, 3, gate_atom::none, -1, false, 1133871366144ULL, 0ULL, -1, -1, 64, 2 }, // 37: Windmill
    { "EM-HA-3c", node_kind::minor, 3, 3, gate_atom::none, -1, false, 81604378624ULL, 0ULL, -1, -1, 66, 1 }, // 38: Manuring & Fallow
    { "EM-HA-4a", node_kind::major, 4, 3, gate_atom::arable, -1, false, 68719476736ULL, 0ULL, -1, -1, 67, 2 }, // 39: Land Reclamation
    { "EM-HA-4b", node_kind::major, 4, 3, gate_atom::arable, -1, false, 137438953472ULL, 0ULL, -1, -1, 69, 2 }, // 40: Mill Networks
    { "EM-PE-1a", node_kind::major, 1, 4, gate_atom::none, -1, false, 4398046511105ULL, 0ULL, -1, -1, 71, 1 }, // 41: Physicians' Canon
    { "EM-PE-1b", node_kind::minor, 1, 4, gate_atom::none, -1, false, 28587302322176ULL, 0ULL, -1, -1, 72, 1 }, // 42: Shared Measures
    { "EM-PE-2a", node_kind::major, 2, 4, gate_atom::none, -1, false, 39582418599936ULL, 0ULL, -1, -1, 73, 3 }, // 43: Provincial Governors
    { "EM-PE-2b", node_kind::major, 2, 4, gate_atom::none, -1, false, 145135534866432ULL, 0ULL, -1, -1, 76, 2 }, // 44: Paper
    { "EM-PE-2c", node_kind::minor, 2, 4, gate_atom::none, -1, false, 79164837199872ULL, 0ULL, -1, -1, 78, 1 }, // 45: Founded Infirmaries
    { "EM-PE-3a", node_kind::major, 3, 4, gate_atom::none, -1, false, 2286984185774080ULL, 0ULL, -1, -1, 79, 1 }, // 46: Quarantine Doctrine
    { "EM-PE-3b", node_kind::major, 3, 4, gate_atom::none, -1, false, 299067162755072ULL, 0ULL, -1, -1, 80, 2 }, // 47: Positional Arithmetic
    { "EM-PE-3c", node_kind::minor, 3, 4, gate_atom::none, -1, false, 703687441776656ULL, 0ULL, -1, -1, 82, 1 }, // 48: Temple Schools
    { "EM-PE-4a", node_kind::major, 4, 4, gate_atom::none, -1, false, 281474976972864ULL, 0ULL, -1, -1, 83, 2 }, // 49: Chartered Companies
    { "EM-PE-4b", node_kind::major, 4, 4, gate_atom::none, -1, false, 2251799813685248ULL, 0ULL, -1, -1, 85, 2 }, // 50: Syncretic Rites
    { "EM-PE-4c", node_kind::minor, 4, 4, gate_atom::none, -1, false, 1196268651020288ULL, 0ULL, -1, -1, 87, 1 }, // 51: Pilgrim Roads
};

inline constexpr int term_count = 15;
inline constexpr const char* term_names[term_count] = { "spire", "stores_low", "surplus", "reach_bound", "coastal_holdings", "threatened", "ground_ore", "manpower_bound", "ground_grass", "cohesion_low", "ground_fuel", "food_bound", "ground_farm", "plague_struck", "many_peoples" };

enum class scorer_term : uint8_t {
    spire = 0,
    stores_low = 1,
    surplus = 2,
    reach_bound = 3,
    coastal_holdings = 4,
    threatened = 5,
    ground_ore = 6,
    manpower_bound = 7,
    ground_grass = 8,
    cohesion_low = 9,
    ground_fuel = 10,
    food_bound = 11,
    ground_farm = 12,
    plague_struck = 13,
    many_peoples = 14,
};

inline constexpr scorer_term node_term[node_count] = {
    scorer_term::spire,
    scorer_term::spire,
    scorer_term::stores_low,
    scorer_term::spire,
    scorer_term::surplus,
    scorer_term::spire,
    scorer_term::stores_low,
    scorer_term::spire,
    scorer_term::reach_bound,
    scorer_term::reach_bound,
    scorer_term::reach_bound,
    scorer_term::coastal_holdings,
    scorer_term::reach_bound,
    scorer_term::reach_bound,
    scorer_term::coastal_holdings,
    scorer_term::reach_bound,
    scorer_term::coastal_holdings,
    scorer_term::threatened,
    scorer_term::stores_low,
    scorer_term::ground_ore,
    scorer_term::manpower_bound,
    scorer_term::threatened,
    scorer_term::threatened,
    scorer_term::reach_bound,
    scorer_term::ground_grass,
    scorer_term::ground_ore,
    scorer_term::manpower_bound,
    scorer_term::cohesion_low,
    scorer_term::ground_fuel,
    scorer_term::ground_fuel,
    scorer_term::food_bound,
    scorer_term::ground_farm,
    scorer_term::stores_low,
    scorer_term::surplus,
    scorer_term::stores_low,
    scorer_term::manpower_bound,
    scorer_term::food_bound,
    scorer_term::manpower_bound,
    scorer_term::food_bound,
    scorer_term::food_bound,
    scorer_term::ground_fuel,
    scorer_term::plague_struck,
    scorer_term::many_peoples,
    scorer_term::many_peoples,
    scorer_term::surplus,
    scorer_term::plague_struck,
    scorer_term::plague_struck,
    scorer_term::surplus,
    scorer_term::many_peoples,
    scorer_term::surplus,
    scorer_term::many_peoples,
    scorer_term::many_peoples,
};

} // namespace io::empire_tree
