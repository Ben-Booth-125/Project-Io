// GENERATED FILE — DO NOT HAND-EDIT.
// Source: docs/generation/trees/industry_tree.json
// Regenerate: node tools/session/gen_empire_tree_table.js industry
// Lint first: node tools/session/tree_lint.js industry
#pragma once

#include <cstdint>
#include "tree_effect.hpp" // the shared effect vocabulary (BL-973)

namespace io::industry_tree {

inline constexpr int node_count = 45;

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

inline constexpr int branch_count = 6;
inline constexpr const char* branch_names[branch_count] = { "SP", "MT", "MV", "LD", "CH", "HL" };

inline constexpr int rim_node_index = 5; ///< IN-SP-3m, "The Renewed Line" — the node carrying the open-next-tree effect

inline constexpr int effect_count = 86;
inline constexpr io::tree_effect effects[effect_count] = {
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 150, io::tree_effect_key::none, 0, false, "industrial" }, // 0: IN-SP-1a
    { io::tree_effect_kind::upgrade, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Water Mill" }, // 1: IN-SP-1a
    { io::tree_effect_kind::open, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 2, false, "ring 2" }, // 2: IN-SP-1m
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 120, io::tree_effect_key::none, 0, false, "industrial" }, // 3: IN-SP-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 60, io::tree_effect_key::none, 0, false, "reach" }, // 4: IN-SP-2a
    { io::tree_effect_kind::open, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 3, false, "ring 3" }, // 5: IN-SP-2m
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 200, io::tree_effect_key::none, 0, false, "industrial" }, // 6: IN-SP-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 80, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 7: IN-SP-3a
    { io::tree_effect_kind::open, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, true, "campaign tree" }, // 8: IN-SP-3m
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 30, io::tree_effect_key::none, 0, false, "industrial" }, // 9: IN-MT-1e
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Blast Works" }, // 10: IN-MT-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 120, io::tree_effect_key::none, 0, false, "industrial" }, // 11: IN-MT-1a
    { io::tree_effect_kind::retire, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "the charcoal route" }, // 12: IN-MT-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 60, io::tree_effect_key::none, 0, false, "industrial" }, // 13: IN-MT-1b
    { io::tree_effect_kind::upgrade, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Ore Pits" }, // 14: IN-MT-1b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::defence, 40, io::tree_effect_key::none, 0, false, "defence" }, // 15: IN-MT-1b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 100, io::tree_effect_key::none, 0, false, "industrial" }, // 16: IN-MT-1c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 40, io::tree_effect_key::none, 0, false, "research" }, // 17: IN-MT-1c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::muster_cost, -40, io::tree_effect_key::none, 0, false, "muster_cost" }, // 18: IN-MT-1d
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 150, io::tree_effect_key::none, 0, false, "industrial" }, // 19: IN-MT-2a
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Arsenal" }, // 20: IN-MT-2a
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "unit roster: industrial" }, // 21: IN-MT-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 80, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 22: IN-MT-2b
    { io::tree_effect_kind::access, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "steeper landforms" }, // 23: IN-MT-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 100, io::tree_effect_key::none, 0, false, "industrial" }, // 24: IN-MT-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 60, io::tree_effect_key::none, 0, false, "research" }, // 25: IN-MT-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 150, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 26: IN-MT-3b
    { io::tree_effect_kind::resource, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "fixed nitrogen as a bulk good" }, // 27: IN-MT-3b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::plague, 30, io::tree_effect_key::none, 0, false, "plague" }, // 28: IN-MT-3c
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Rail Head" }, // 29: IN-MV-1a
    { io::tree_effect_kind::reach, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "interior haul cost" }, // 30: IN-MV-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 120, io::tree_effect_key::none, 0, false, "reach" }, // 31: IN-MV-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 30, io::tree_effect_key::none, 0, false, "reach" }, // 32: IN-MV-1b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 30, io::tree_effect_key::none, 0, false, "reach" }, // 33: IN-MV-1c
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Coaling Station" }, // 34: IN-MV-2a
    { io::tree_effect_kind::access, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "scheduled ocean leg" }, // 35: IN-MV-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 100, io::tree_effect_key::none, 0, false, "reach" }, // 36: IN-MV-2a
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "Signal Line" }, // 37: IN-MV-2b
    { io::tree_effect_kind::intel, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "activity-fog freshness" }, // 38: IN-MV-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::cohesion, 60, io::tree_effect_key::none, 0, false, "cohesion" }, // 39: IN-MV-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 100, io::tree_effect_key::none, 0, false, "industrial" }, // 40: IN-MV-3a
    { io::tree_effect_kind::resource, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "oil as a bulk fuel" }, // 41: IN-MV-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::reach, 80, io::tree_effect_key::none, 0, false, "reach" }, // 42: IN-MV-3b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::forage, 40, io::tree_effect_key::none, 0, false, "forage" }, // 43: IN-MV-3b
    { io::tree_effect_kind::intel, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "aerial reconnaissance" }, // 44: IN-MV-3c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::defence, 60, io::tree_effect_key::none, 0, false, "defence" }, // 45: IN-MV-3c
    { io::tree_effect_kind::unlock, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "unit roster: machine age" }, // 46: IN-MV-3c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 20, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 47: IN-LD-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::manpower, 120, io::tree_effect_key::none, 0, false, "manpower" }, // 48: IN-LD-1b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 80, io::tree_effect_key::none, 0, false, "industrial" }, // 49: IN-LD-1b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::cohesion, -60, io::tree_effect_key::none, 0, false, "cohesion" }, // 50: IN-LD-1b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::cohesion, 60, io::tree_effect_key::none, 0, false, "cohesion" }, // 51: IN-LD-1c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 80, io::tree_effect_key::none, 0, false, "stores" }, // 52: IN-LD-1c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, -40, io::tree_effect_key::none, 0, false, "industrial" }, // 53: IN-LD-1c
    { io::tree_effect_kind::modifier, io::tree_modifier_term::manpower, 100, io::tree_effect_key::none, 0, false, "manpower" }, // 54: IN-LD-1d
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 60, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 55: IN-LD-1d
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 30, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 56: IN-LD-1e
    { io::tree_effect_kind::resource, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "fertiliser as a traded good" }, // 57: IN-LD-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 100, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 58: IN-LD-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 30, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 59: IN-LD-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::manpower, 150, io::tree_effect_key::none, 0, false, "manpower" }, // 60: IN-LD-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 100, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 61: IN-LD-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::forage, 30, io::tree_effect_key::none, 0, false, "forage" }, // 62: IN-LD-3b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 100, io::tree_effect_key::none, 0, false, "research" }, // 63: IN-CH-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 60, io::tree_effect_key::none, 0, false, "research" }, // 64: IN-CH-1b
    { io::tree_effect_kind::institution, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "property in invention" }, // 65: IN-CH-1b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 30, io::tree_effect_key::none, 0, false, "stores" }, // 66: IN-CH-1c
    { io::tree_effect_kind::institution, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "chartered corporate form by registration, not by grant" }, // 67: IN-CH-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 80, io::tree_effect_key::none, 0, false, "research" }, // 68: IN-CH-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::assimilation, 80, io::tree_effect_key::none, 0, false, "assimilation" }, // 69: IN-CH-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 40, io::tree_effect_key::none, 0, false, "industrial" }, // 70: IN-CH-2b
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 20, io::tree_effect_key::none, 0, false, "industrial" }, // 71: IN-CH-2c
    { io::tree_effect_kind::institution, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "heavy plant nation-owned; corporations lease" }, // 72: IN-CH-2d
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 120, io::tree_effect_key::none, 0, false, "industrial" }, // 73: IN-CH-2d
    { io::tree_effect_kind::modifier, io::tree_modifier_term::muster_cost, -40, io::tree_effect_key::none, 0, false, "muster_cost" }, // 74: IN-CH-2d
    { io::tree_effect_kind::institution, io::tree_modifier_term::none, 0, io::tree_effect_key::none, 0, false, "corporations own processing capacity" }, // 75: IN-CH-2e
    { io::tree_effect_kind::modifier, io::tree_modifier_term::industrial, 60, io::tree_effect_key::none, 0, false, "industrial" }, // 76: IN-CH-2e
    { io::tree_effect_kind::modifier, io::tree_modifier_term::research, 60, io::tree_effect_key::none, 0, false, "research" }, // 77: IN-CH-2e
    { io::tree_effect_kind::modifier, io::tree_modifier_term::stores, 40, io::tree_effect_key::none, 0, false, "stores" }, // 78: IN-CH-2e
    { io::tree_effect_kind::modifier, io::tree_modifier_term::cohesion, 100, io::tree_effect_key::none, 0, false, "cohesion" }, // 79: IN-CH-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::assimilation, 60, io::tree_effect_key::none, 0, false, "assimilation" }, // 80: IN-CH-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::plague, 20, io::tree_effect_key::none, 0, false, "plague" }, // 81: IN-HL-1a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::plague, 150, io::tree_effect_key::none, 0, false, "plague" }, // 82: IN-HL-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::carrying_capacity, 60, io::tree_effect_key::none, 0, false, "carrying_capacity" }, // 83: IN-HL-2a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::plague, 200, io::tree_effect_key::none, 0, false, "plague" }, // 84: IN-HL-3a
    { io::tree_effect_kind::modifier, io::tree_modifier_term::manpower, 60, io::tree_effect_key::none, 0, false, "manpower" }, // 85: IN-HL-3a
};

inline constexpr node nodes[node_count] = {
    { "IN-SP-1a", node_kind::major, 1, 0, gate_atom::none, -1, true, 66114ULL, 0ULL, -1, -1, 0, 2 }, // 0: Atmospheric & Rotative Steam
    { "IN-SP-1m", node_kind::milestone, 1, 0, gate_atom::none, -1, false, 5ULL, 65536ULL, 7, 8, 2, 1 }, // 1: The Cheap Ton
    { "IN-SP-2a", node_kind::major, 2, 0, gate_atom::fuel, -1, false, 526346ULL, 0ULL, -1, -1, 3, 2 }, // 2: High-Pressure & Compound Engines
    { "IN-SP-2m", node_kind::milestone, 2, 0, gate_atom::none, -1, false, 20ULL, 1572864ULL, 39, 40, 5, 1 }, // 3: The Scheduled World
    { "IN-SP-3a", node_kind::major, 3, 0, gate_atom::fuel, -1, false, 2199025352744ULL, 0ULL, -1, -1, 6, 2 }, // 4: Electrification
    { "IN-SP-3m", node_kind::milestone, 3, 0, gate_atom::none, -1, false, 16ULL, 17592188149760ULL, -1, -1, 8, 1 }, // 5: The Renewed Line
    { "IN-MT-1e", node_kind::minor, 1, 1, gate_atom::none, -1, false, 385ULL, 0ULL, -1, -1, 9, 1 }, // 6: Furnace Practice
    { "IN-MT-1a", node_kind::major, 1, 1, gate_atom::fuel, 8, false, 133184ULL, 0ULL, -1, -1, 10, 3 }, // 7: Coke Smelting
    { "IN-MT-1b", node_kind::major, 1, 1, gate_atom::none, 7, false, 64ULL, 0ULL, -1, -1, 13, 3 }, // 8: Charcoal Iron
    { "IN-MT-1c", node_kind::major, 1, 1, gate_atom::ore_q, -1, false, 268436481ULL, 0ULL, -1, -1, 16, 2 }, // 9: Machine Tools
    { "IN-MT-1d", node_kind::minor, 1, 1, gate_atom::none, -1, false, 2560ULL, 0ULL, -1, -1, 18, 1 }, // 10: Interchangeable Parts
    { "IN-MT-2a", node_kind::major, 2, 1, gate_atom::ore_q, -1, false, 274877920388ULL, 0ULL, -1, -1, 19, 3 }, // 11: Converter Steel
    { "IN-MT-2b", node_kind::major, 2, 1, gate_atom::none, -1, false, 2048ULL, 0ULL, -1, -1, 22, 2 }, // 12: Framed Construction & Cement
    { "IN-MT-3a", node_kind::major, 3, 1, gate_atom::fuel, -1, false, 51200ULL, 0ULL, -1, -1, 24, 2 }, // 13: Synthetic Chemistry
    { "IN-MT-3b", node_kind::major, 3, 1, gate_atom::fuel, -1, false, 4294975488ULL, 0ULL, -1, -1, 26, 2 }, // 14: Fixed-Nitrogen Synthesis
    { "IN-MT-3c", node_kind::minor, 3, 1, gate_atom::none, -1, false, 17592186052608ULL, 0ULL, -1, -1, 28, 1 }, // 15: Coal-Tar Distillates
    { "IN-MV-1a", node_kind::major, 1, 2, gate_atom::fuel, -1, false, 34360918017ULL, 0ULL, -1, -1, 29, 3 }, // 16: Railway
    { "IN-MV-1b", node_kind::minor, 1, 2, gate_atom::none, -1, false, 65664ULL, 0ULL, -1, -1, 32, 1 }, // 17: Iron Rails
    { "IN-MV-1c", node_kind::minor, 1, 2, gate_atom::none, -1, false, 524288ULL, 0ULL, -1, -1, 33, 1 }, // 18: Packet Service
    { "IN-MV-2a", node_kind::major, 2, 2, gate_atom::coastal, -1, false, 1076101124ULL, 0ULL, -1, -1, 34, 3 }, // 19: Steamship
    { "IN-MV-2b", node_kind::major, 2, 2, gate_atom::none, -1, false, 65536ULL, 0ULL, -1, -1, 37, 3 }, // 20: Telegraph
    { "IN-MV-3a", node_kind::major, 3, 2, gate_atom::fuel, -1, false, 4308074512ULL, 0ULL, -1, -1, 40, 2 }, // 21: Internal Combustion & Oil
    { "IN-MV-3b", node_kind::major, 3, 2, gate_atom::none, -1, false, 2097152ULL, 0ULL, -1, -1, 42, 2 }, // 22: Automotive Transport
    { "IN-MV-3c", node_kind::major, 3, 2, gate_atom::none, -1, false, 2097152ULL, 0ULL, -1, -1, 44, 3 }, // 23: Flight
    { "IN-LD-1a", node_kind::minor, 1, 3, gate_atom::none, -1, false, 234881024ULL, 0ULL, -1, -1, 47, 1 }, // 24: Field Survey
    { "IN-LD-1b", node_kind::major, 1, 3, gate_atom::arable, 26, false, 16777216ULL, 0ULL, -1, -1, 48, 3 }, // 25: Cleared Holdings
    { "IN-LD-1c", node_kind::major, 1, 3, gate_atom::arable, 25, false, 16777216ULL, 0ULL, -1, -1, 51, 3 }, // 26: Smallholder Tenure
    { "IN-LD-1d", node_kind::major, 1, 3, gate_atom::arable, -1, false, 822083584ULL, 0ULL, -1, -1, 54, 2 }, // 27: Farm Mechanisation
    { "IN-LD-1e", node_kind::minor, 1, 3, gate_atom::none, -1, false, 134218240ULL, 0ULL, -1, -1, 56, 1 }, // 28: Iron Implements
    { "IN-LD-2a", node_kind::major, 2, 3, gate_atom::none, -1, false, 3355443200ULL, 0ULL, -1, -1, 57, 2 }, // 29: Soil Chemistry & Fertiliser Trade
    { "IN-LD-2b", node_kind::minor, 2, 3, gate_atom::none, -1, false, 537395200ULL, 0ULL, -1, -1, 59, 1 }, // 30: Fertiliser Cargo
    { "IN-LD-3a", node_kind::major, 3, 3, gate_atom::arable, -1, false, 4831838208ULL, 0ULL, -1, -1, 60, 2 }, // 31: Tractor & Fertiliser Package
    { "IN-LD-3b", node_kind::minor, 3, 3, gate_atom::none, -1, false, 2149597184ULL, 0ULL, -1, -1, 62, 1 }, // 32: Fuel & Fertiliser Depot
    { "IN-CH-1a", node_kind::major, 1, 4, gate_atom::none, -1, false, 4552665333760ULL, 0ULL, -1, -1, 63, 1 }, // 33: Empirical Method
    { "IN-CH-1b", node_kind::major, 1, 4, gate_atom::none, -1, false, 8589934592ULL, 0ULL, -1, -1, 64, 2 }, // 34: Patent Grants
    { "IN-CH-1c", node_kind::minor, 1, 4, gate_atom::none, -1, false, 68719542272ULL, 0ULL, -1, -1, 66, 1 }, // 35: Bond Market
    { "IN-CH-2a", node_kind::major, 2, 4, gate_atom::none, -1, false, 309237645312ULL, 0ULL, -1, -1, 67, 1 }, // 36: General Incorporation
    { "IN-CH-2b", node_kind::major, 2, 4, gate_atom::none, -1, false, 2207613190144ULL, 0ULL, -1, -1, 68, 3 }, // 37: Mass Schooling
    { "IN-CH-2c", node_kind::minor, 2, 4, gate_atom::none, -1, false, 1717986920448ULL, 0ULL, -1, -1, 71, 1 }, // 38: Plant Registry
    { "IN-CH-2d", node_kind::major, 2, 4, gate_atom::none, 40, false, 274877906944ULL, 0ULL, -1, -1, 72, 3 }, // 39: State Arsenal
    { "IN-CH-2e", node_kind::major, 2, 4, gate_atom::none, 39, false, 274877906944ULL, 0ULL, -1, -1, 75, 4 }, // 40: Private Works
    { "IN-CH-3a", node_kind::major, 3, 4, gate_atom::none, -1, false, 137438953488ULL, 0ULL, -1, -1, 79, 2 }, // 41: Broadcast
    { "IN-HL-1a", node_kind::minor, 1, 5, gate_atom::none, -1, false, 8804682956800ULL, 0ULL, -1, -1, 81, 1 }, // 42: Microscopy
    { "IN-HL-2a", node_kind::major, 2, 5, gate_atom::none, -1, false, 21990232555520ULL, 0ULL, -1, -1, 82, 2 }, // 43: Germ Theory & Sanitation
    { "IN-HL-3a", node_kind::major, 3, 5, gate_atom::none, -1, false, 8796093054976ULL, 0ULL, -1, -1, 84, 2 }, // 44: Antibiotics & Mass Vaccination
};

inline constexpr int term_count = 22;
inline constexpr const char* term_names[term_count] = { "spire", "reach_bound", "furnace_lit", "ground_ore", "ground_fuel", "ground_forest", "tariff_pressure", "threatened", "food_bound", "plague_struck", "coastal_holdings", "colonial_reach", "cohesion_low", "fuel_bound", "labour_bound", "manpower_bound", "ground_farm", "ground_port", "surplus", "known", "credit_bound", "many_peoples" };

enum class scorer_term : uint8_t {
    spire = 0,
    reach_bound = 1,
    furnace_lit = 2,
    ground_ore = 3,
    ground_fuel = 4,
    ground_forest = 5,
    tariff_pressure = 6,
    threatened = 7,
    food_bound = 8,
    plague_struck = 9,
    coastal_holdings = 10,
    colonial_reach = 11,
    cohesion_low = 12,
    fuel_bound = 13,
    labour_bound = 14,
    manpower_bound = 15,
    ground_farm = 16,
    ground_port = 17,
    surplus = 18,
    known = 19,
    credit_bound = 20,
    many_peoples = 21,
};

inline constexpr scorer_term node_term[node_count] = {
    scorer_term::spire,
    scorer_term::spire,
    scorer_term::reach_bound,
    scorer_term::spire,
    scorer_term::furnace_lit,
    scorer_term::spire,
    scorer_term::ground_ore,
    scorer_term::ground_fuel,
    scorer_term::ground_forest,
    scorer_term::tariff_pressure,
    scorer_term::threatened,
    scorer_term::ground_ore,
    scorer_term::food_bound,
    scorer_term::furnace_lit,
    scorer_term::food_bound,
    scorer_term::plague_struck,
    scorer_term::reach_bound,
    scorer_term::ground_ore,
    scorer_term::coastal_holdings,
    scorer_term::colonial_reach,
    scorer_term::cohesion_low,
    scorer_term::fuel_bound,
    scorer_term::reach_bound,
    scorer_term::threatened,
    scorer_term::food_bound,
    scorer_term::labour_bound,
    scorer_term::cohesion_low,
    scorer_term::manpower_bound,
    scorer_term::ground_farm,
    scorer_term::food_bound,
    scorer_term::ground_port,
    scorer_term::manpower_bound,
    scorer_term::ground_farm,
    scorer_term::surplus,
    scorer_term::known,
    scorer_term::credit_bound,
    scorer_term::surplus,
    scorer_term::many_peoples,
    scorer_term::furnace_lit,
    scorer_term::threatened,
    scorer_term::surplus,
    scorer_term::cohesion_low,
    scorer_term::plague_struck,
    scorer_term::plague_struck,
    scorer_term::plague_struck,
};

} // namespace io::industry_tree
