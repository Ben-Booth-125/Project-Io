// GENERATED FILE — DO NOT HAND-EDIT.
// Source: docs/generation/trees/exploration_tree.json
// Regenerate: node tools/session/gen_empire_tree_table.js exploration
// Lint first: node tools/session/tree_lint.js exploration
#pragma once

#include <cstdint>

namespace io::exploration_tree {

inline constexpr int node_count = 31;

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
inline constexpr const char* branch_names[branch_count] = { "SP", "HL", "PT", "WY", "GD" };

inline constexpr int rim_node_index = 5; ///< EX-SP-3m, "The Long Reckoning"

inline constexpr node nodes[node_count] = {
    { "EX-SP-1a", node_kind::major, 1, 0, gate_atom::none, -1, true, 34082882ULL, 0ULL, -1, -1 }, // 0: Consolidated Stores
    { "EX-SP-1m", node_kind::milestone, 1, 0, gate_atom::none, -1, false, 5ULL, 528384ULL, -1, -1 }, // 1: The Common Purse
    { "EX-SP-2a", node_kind::major, 2, 0, gate_atom::none, -1, false, 10ULL, 0ULL, -1, -1 }, // 2: The Chartered Company
    { "EX-SP-2m", node_kind::milestone, 2, 0, gate_atom::none, -1, false, 20ULL, 134234112ULL, 9, 10 }, // 3: The Standing Charter
    { "EX-SP-3a", node_kind::major, 3, 0, gate_atom::none, -1, false, 40ULL, 0ULL, -1, -1 }, // 4: Double-Entry Reckoning
    { "EX-SP-3m", node_kind::milestone, 3, 0, gate_atom::none, -1, false, 16ULL, 536872960ULL, 22, 23 }, // 5: The Long Reckoning
    { "EX-HL-1a", node_kind::major, 1, 1, gate_atom::coastal, -1, false, 8577ULL, 0ULL, -1, -1 }, // 6: Decked Coaster
    { "EX-HL-1b", node_kind::minor, 1, 1, gate_atom::none, -1, false, 33554496ULL, 0ULL, -1, -1 }, // 7: Sounding & Chart
    { "EX-HL-2c", node_kind::minor, 2, 1, gate_atom::none, -1, false, 1600ULL, 0ULL, -1, -1 }, // 8: Careening & Refit
    { "EX-HL-2a", node_kind::major, 2, 1, gate_atom::coastal, 10, false, 35072ULL, 0ULL, -1, -1 }, // 9: Ocean Carrack
    { "EX-HL-2b", node_kind::major, 2, 1, gate_atom::coastal, 9, false, 256ULL, 0ULL, -1, -1 }, // 10: Fleet of the Line
    { "EX-HL-3a", node_kind::major, 3, 1, gate_atom::coastal, -1, false, 1073742336ULL, 0ULL, -1, -1 }, // 11: Oceanic Navigation
    { "EX-PT-1a", node_kind::major, 1, 2, gate_atom::coastal, -1, false, 24577ULL, 0ULL, -1, -1 }, // 12: Harbour Works
    { "EX-PT-1b", node_kind::minor, 1, 2, gate_atom::none, -1, false, 4160ULL, 0ULL, -1, -1 }, // 13: Pilot & Lighthouse
    { "EX-PT-2a", node_kind::major, 2, 2, gate_atom::none, -1, false, 102400ULL, 0ULL, -1, -1 }, // 14: Standing Garrison
    { "EX-PT-2b", node_kind::minor, 2, 2, gate_atom::none, -1, false, 16896ULL, 0ULL, -1, -1 }, // 15: Naval Stores
    { "EX-PT-3c", node_kind::minor, 3, 2, gate_atom::none, -1, false, 409600ULL, 0ULL, -1, -1 }, // 16: Dry Dock
    { "EX-PT-3a", node_kind::major, 3, 2, gate_atom::coastal, 18, false, 16842752ULL, 0ULL, -1, -1 }, // 17: Blue-Water Squadron
    { "EX-PT-3b", node_kind::major, 3, 2, gate_atom::coastal, 17, false, 65536ULL, 0ULL, -1, -1 }, // 18: Fortified Roadstead
    { "EX-WY-1a", node_kind::major, 1, 3, gate_atom::none, -1, false, 68157441ULL, 0ULL, -1, -1 }, // 19: Post Roads
    { "EX-WY-2a", node_kind::major, 2, 3, gate_atom::none, -1, false, 271056896ULL, 0ULL, -1, -1 }, // 20: Bonded Warehouse
    { "EX-WY-3c", node_kind::minor, 3, 3, gate_atom::none, -1, false, 13631488ULL, 0ULL, -1, -1 }, // 21: Wayhouse Relay
    { "EX-WY-3a", node_kind::major, 3, 3, gate_atom::none, 23, false, 18874368ULL, 0ULL, -1, -1 }, // 22: Trunk Highway
    { "EX-WY-3b", node_kind::major, 3, 3, gate_atom::arable, 22, false, 2097152ULL, 0ULL, -1, -1 }, // 23: Canal Cut
    { "EX-WY-3d", node_kind::minor, 3, 3, gate_atom::none, -1, false, 4325376ULL, 0ULL, -1, -1 }, // 24: Toll & Escort
    { "EX-GD-1a", node_kind::major, 1, 4, gate_atom::none, -1, false, 201326721ULL, 0ULL, -1, -1 }, // 25: Named Staples
    { "EX-GD-1b", node_kind::minor, 1, 4, gate_atom::none, -1, false, 34078720ULL, 0ULL, -1, -1 }, // 26: Weights & Tally
    { "EX-GD-2a", node_kind::major, 2, 4, gate_atom::none, -1, false, 838860800ULL, 0ULL, -1, -1 }, // 27: Quayside Market
    { "EX-GD-2b", node_kind::minor, 2, 4, gate_atom::none, -1, false, 135266304ULL, 0ULL, -1, -1 }, // 28: Bill of Lading
    { "EX-GD-3a", node_kind::major, 3, 4, gate_atom::none, -1, false, 1207959552ULL, 0ULL, -1, -1 }, // 29: Standing Preference
    { "EX-GD-3b", node_kind::minor, 3, 4, gate_atom::none, -1, false, 536872960ULL, 0ULL, -1, -1 }, // 30: Factor's Ledger
};

inline constexpr int term_count = 12;
inline constexpr const char* term_names[term_count] = { "stores_low", "spire", "purse_low", "surplus", "coastal_holdings", "reach_bound", "wants_unmet", "threatened", "ground_port", "subject_held", "throughput_bound", "known" };

enum class scorer_term : uint8_t {
    stores_low = 0,
    spire = 1,
    purse_low = 2,
    surplus = 3,
    coastal_holdings = 4,
    reach_bound = 5,
    wants_unmet = 6,
    threatened = 7,
    ground_port = 8,
    subject_held = 9,
    throughput_bound = 10,
    known = 11,
};

inline constexpr scorer_term node_term[node_count] = {
    scorer_term::stores_low,
    scorer_term::spire,
    scorer_term::purse_low,
    scorer_term::spire,
    scorer_term::surplus,
    scorer_term::spire,
    scorer_term::coastal_holdings,
    scorer_term::coastal_holdings,
    scorer_term::reach_bound,
    scorer_term::wants_unmet,
    scorer_term::threatened,
    scorer_term::wants_unmet,
    scorer_term::ground_port,
    scorer_term::ground_port,
    scorer_term::threatened,
    scorer_term::stores_low,
    scorer_term::ground_port,
    scorer_term::subject_held,
    scorer_term::threatened,
    scorer_term::reach_bound,
    scorer_term::throughput_bound,
    scorer_term::throughput_bound,
    scorer_term::reach_bound,
    scorer_term::throughput_bound,
    scorer_term::throughput_bound,
    scorer_term::wants_unmet,
    scorer_term::stores_low,
    scorer_term::wants_unmet,
    scorer_term::throughput_bound,
    scorer_term::surplus,
    scorer_term::known,
};

} // namespace io::exploration_tree
