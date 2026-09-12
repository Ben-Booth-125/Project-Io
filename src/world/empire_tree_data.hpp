// GENERATED FILE — DO NOT HAND-EDIT.
// Source: docs/generation/trees/empire_tree.json
// Regenerate: node tools/session/gen_empire_tree_table.js empire
// Lint first: node tools/session/tree_lint.js empire
#pragma once

#include <cstdint>

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
};

inline constexpr int branch_count = 5;
inline constexpr const char* branch_names[branch_count] = { "SP", "RD", "AR", "HA", "PE" };

inline constexpr int rim_node_index = 7; ///< EM-SP-4m, "The Enforceable Promise"

inline constexpr node nodes[node_count] = {
    { "EM-SP-1a", node_kind::major, 1, 0, gate_atom::none, -1, true, 2200097521922ULL, 0ULL, -1, -1 }, // 0: Codified Law & Census
    { "EM-SP-1m", node_kind::milestone, 1, 0, gate_atom::none, -1, false, 5ULL, 1073742080ULL, -1, -1 }, // 1: The Written Ledger
    { "EM-SP-2a", node_kind::major, 2, 0, gate_atom::ore_q, -1, false, 17179870218ULL, 0ULL, -1, -1 }, // 2: Coinage at Scale
    { "EM-SP-2m", node_kind::milestone, 2, 0, gate_atom::none, -1, false, 20ULL, 8796093023232ULL, 32, 33 }, // 3: The Sworn Province
    { "EM-SP-3a", node_kind::major, 3, 0, gate_atom::none, -1, false, 281474976710696ULL, 0ULL, -1, -1 }, // 4: Endowed Scholarship
    { "EM-SP-3m", node_kind::milestone, 3, 0, gate_atom::none, -1, false, 80ULL, 140806207832064ULL, -1, -1 }, // 5: The Lettered Court
    { "EM-SP-4a", node_kind::major, 4, 0, gate_atom::none, -1, false, 562949953421472ULL, 0ULL, -1, -1 }, // 6: Credit Instruments & Double Entry
    { "EM-SP-4m", node_kind::milestone, 4, 0, gate_atom::none, -1, false, 64ULL, 562949953552384ULL, -1, -1 }, // 7: The Enforceable Promise
    { "EM-RD-1a", node_kind::major, 1, 1, gate_atom::none, -1, false, 513ULL, 0ULL, -1, -1 }, // 8: Engineered Road Network
    { "EM-RD-1b", node_kind::minor, 1, 1, gate_atom::none, -1, false, 3328ULL, 0ULL, -1, -1 }, // 9: Road Surveys
    { "EM-RD-2a", node_kind::major, 2, 1, gate_atom::none, -1, false, 4612ULL, 0ULL, -1, -1 }, // 10: Way Stations & Post
    { "EM-RD-2b", node_kind::major, 2, 1, gate_atom::coastal, -1, false, 16896ULL, 0ULL, -1, -1 }, // 11: Deep-Hull Sail
    { "EM-RD-2c", node_kind::minor, 2, 1, gate_atom::none, -1, false, 4203520ULL, 0ULL, -1, -1 }, // 12: Span Bridges
    { "EM-RD-3a", node_kind::major, 3, 1, gate_atom::none, -1, false, 36864ULL, 0ULL, -1, -1 }, // 13: Passes & Causeways
    { "EM-RD-3b", node_kind::major, 3, 1, gate_atom::coastal, -1, false, 67584ULL, 0ULL, -1, -1 }, // 14: Lateen & Long-Range Rig
    { "EM-RD-3c", node_kind::minor, 3, 1, gate_atom::none, -1, false, 139264ULL, 0ULL, -1, -1 }, // 15: Caravan Halts
    { "EM-RD-4a", node_kind::major, 4, 1, gate_atom::coastal, -1, false, 16384ULL, 0ULL, -1, -1 }, // 16: Ocean-Rated Hulls
    { "EM-RD-4b", node_kind::major, 4, 1, gate_atom::none, -1, false, 294912ULL, 0ULL, -1, -1 }, // 17: Imperial Cartography
    { "EM-RD-4c", node_kind::minor, 4, 1, gate_atom::none, -1, false, 562949953552384ULL, 0ULL, -1, -1 }, // 18: Toll Stations
    { "EM-AR-1a", node_kind::major, 1, 2, gate_atom::ore_q, -1, false, 1048577ULL, 0ULL, -1, -1 }, // 19: Bloomery Iron
    { "EM-AR-1b", node_kind::minor, 1, 2, gate_atom::none, -1, false, 6815744ULL, 0ULL, -1, -1 }, // 20: Drill & Levy Rolls
    { "EM-AR-2a", node_kind::major, 2, 2, gate_atom::none, -1, false, 9437184ULL, 0ULL, -1, -1 }, // 21: Siegecraft
    { "EM-AR-2b", node_kind::major, 2, 2, gate_atom::none, -1, false, 68161536ULL, 0ULL, -1, -1 }, // 22: Wall Circuit
    { "EM-AR-2c", node_kind::minor, 2, 2, gate_atom::none, -1, false, 18874368ULL, 0ULL, -1, -1 }, // 23: Quartermasters
    { "EM-AR-3a", node_kind::major, 3, 2, gate_atom::grassland, -1, false, 8388608ULL, 0ULL, -1, -1 }, // 24: Stirrup Cavalry
    { "EM-AR-3b", node_kind::major, 3, 2, gate_atom::ore_q, -1, false, 603979776ULL, 0ULL, -1, -1 }, // 25: Pattern-Forged Steel
    { "EM-AR-3c", node_kind::minor, 3, 2, gate_atom::none, -1, false, 34531704832ULL, 0ULL, -1, -1 }, // 26: Armourers' Guild
    { "EM-AR-4a", node_kind::major, 4, 2, gate_atom::none, -1, false, 67108864ULL, 0ULL, -1, -1 }, // 27: Stone Fortress
    { "EM-AR-4c", node_kind::major, 4, 2, gate_atom::fuel, -1, false, 536870912ULL, 0ULL, -1, -1 }, // 28: Blast Furnace
    { "EM-AR-4d", node_kind::minor, 4, 2, gate_atom::none, -1, false, 301989888ULL, 0ULL, -1, -1 }, // 29: Charcoal Burners
    { "EM-HA-1a", node_kind::major, 1, 3, gate_atom::none, -1, false, 19327352833ULL, 0ULL, -1, -1 }, // 30: Iron-Shod Plough
    { "EM-HA-1c", node_kind::major, 1, 3, gate_atom::arable, -1, false, 35433480192ULL, 0ULL, -1, -1 }, // 31: Irrigation Works
    { "EM-HA-2a", node_kind::major, 2, 3, gate_atom::none, 33, false, 292057776128ULL, 0ULL, -1, -1 }, // 32: Temple Stores
    { "EM-HA-2b", node_kind::major, 2, 3, gate_atom::none, 32, false, 292057776128ULL, 0ULL, -1, -1 }, // 33: Open Granaries
    { "EM-HA-2c", node_kind::minor, 2, 3, gate_atom::none, -1, false, 13958643716ULL, 0ULL, -1, -1 }, // 34: Harvest Tallies
    { "EM-HA-2d", node_kind::major, 2, 3, gate_atom::arable, -1, false, 139653545984ULL, 0ULL, -1, -1 }, // 35: Water Wheel
    { "EM-HA-3a", node_kind::major, 3, 3, gate_atom::none, -1, false, 824633720832ULL, 0ULL, -1, -1 }, // 36: Three-Field Rotation
    { "EM-HA-3b", node_kind::major, 3, 3, gate_atom::none, -1, false, 1133871366144ULL, 0ULL, -1, -1 }, // 37: Windmill
    { "EM-HA-3c", node_kind::minor, 3, 3, gate_atom::none, -1, false, 81604378624ULL, 0ULL, -1, -1 }, // 38: Manuring & Fallow
    { "EM-HA-4a", node_kind::major, 4, 3, gate_atom::arable, -1, false, 68719476736ULL, 0ULL, -1, -1 }, // 39: Land Reclamation
    { "EM-HA-4b", node_kind::major, 4, 3, gate_atom::arable, -1, false, 137438953472ULL, 0ULL, -1, -1 }, // 40: Mill Networks
    { "EM-PE-1a", node_kind::major, 1, 4, gate_atom::none, -1, false, 4398046511105ULL, 0ULL, -1, -1 }, // 41: Physicians' Canon
    { "EM-PE-1b", node_kind::minor, 1, 4, gate_atom::none, -1, false, 28587302322176ULL, 0ULL, -1, -1 }, // 42: Shared Measures
    { "EM-PE-2a", node_kind::major, 2, 4, gate_atom::none, -1, false, 39582418599936ULL, 0ULL, -1, -1 }, // 43: Provincial Governors
    { "EM-PE-2b", node_kind::major, 2, 4, gate_atom::none, -1, false, 145135534866432ULL, 0ULL, -1, -1 }, // 44: Paper
    { "EM-PE-2c", node_kind::minor, 2, 4, gate_atom::none, -1, false, 79164837199872ULL, 0ULL, -1, -1 }, // 45: Founded Infirmaries
    { "EM-PE-3a", node_kind::major, 3, 4, gate_atom::none, -1, false, 2286984185774080ULL, 0ULL, -1, -1 }, // 46: Quarantine Doctrine
    { "EM-PE-3b", node_kind::major, 3, 4, gate_atom::none, -1, false, 299067162755072ULL, 0ULL, -1, -1 }, // 47: Positional Arithmetic
    { "EM-PE-3c", node_kind::minor, 3, 4, gate_atom::none, -1, false, 703687441776656ULL, 0ULL, -1, -1 }, // 48: Temple Schools
    { "EM-PE-4a", node_kind::major, 4, 4, gate_atom::none, -1, false, 281474976972864ULL, 0ULL, -1, -1 }, // 49: Chartered Companies
    { "EM-PE-4b", node_kind::major, 4, 4, gate_atom::none, -1, false, 2251799813685248ULL, 0ULL, -1, -1 }, // 50: Syncretic Rites
    { "EM-PE-4c", node_kind::minor, 4, 4, gate_atom::none, -1, false, 1196268651020288ULL, 0ULL, -1, -1 }, // 51: Pilgrim Roads
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
