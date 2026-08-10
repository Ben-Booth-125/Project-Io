#pragma once

#include "condition_set.hpp" // BL-344: tech_node::condition is a real predicate

#include <string>
#include <vector>

// Forward-declared so this header carries no sol2/Lua dependency (same pattern as
// recipe_registry.hpp). Only tech_tree.cpp pulls in the Lua state, and that TU is
// excluded from the SDL/Lua-free headless world superset (CMakeLists + build.yml).
class lua_state;

/// One quest record from scripts/tech_tree.lua — a chain of techs whose capstone
/// opens further quest trees (capstones open QUEST TREES, not Eras — the
/// 2026-07-08 BL-087 Era reframe). Quest fields are kept as strings
/// deliberately — the quest schema is still design-fluid and the only consumer
/// is the read-only F9 viewer panel. (Its sibling `tech_node` is no longer
/// mock-only: BL-344 gave that one a real predicate — see below.)
struct tech_quest
{
    std::string id;       ///< e.g. "E1-PROPLOOP".
    std::string name;     ///< Display name.
    int         era = 0;  ///< Era the quest belongs to (0 = Terrestrial, 1 = Early Space).
    std::string type;     ///< "gate" (capstone opens quest trees) or "standing" (deepens forever).
    std::string thread;   ///< Research-landscape thread the quest realises.
    std::string thesis;   ///< One-line statement of what completing the quest proves.
    std::string capstone; ///< Tech id whose completion IS the quest goal ("" for standing lines).
    std::vector<std::string> opens; ///< Quest ids the capstone unlocks.
    std::string status;   ///< "sketch" | "stub" | "reserved".
};

/// One tech record from scripts/tech_tree.lua. See the schema note in that file
/// and docs/research/ERA1_TECH_LANDSCAPE.md § Itemisation schema.
struct tech_node
{
    std::string id;         ///< e.g. "E1-PL-09".
    std::string name;       ///< Display name.
    std::string short_name; ///< On-canvas label (BL-310 round 3) — 1-2 words,
                             ///< "so players do not have to work out a
                             ///< dictionary in their mind for each tech" (Ben,
                             ///< 2026-08-06). Empty for most nodes today
                             ///< (authoring 130+ by hand is its own pass); the
                             ///< renderer falls back to a truncated `name`
                             ///< rather than the bare id either way.
    std::string quest;      ///< Owning quest id.
    std::string kind;      ///< "invention" | "tier" | "capstone".
    int         tier = 0;  ///< 2/3 for tier techs, 0 otherwise.
    std::vector<std::string> prereqs; ///< Tech ids; may cross quests (sparingly, marked).
    std::string cost;      ///< Cost model B: "S" | "M" | "L" | "XL".
    std::string payoff;    ///< gate | resource | building | recipe | efficiency | enabling.

    /// The tech's REAL gate (BL-344). Until 2026-08-10 this was
    /// `std::string condition` — one of six descriptive labels (research |
    /// structure | stockpile | market | surplus | era). A label describes what
    /// a gate would be about; it cannot resolve true or false, so no tech could
    /// ever be earned and the tree was a picture of a system rather than the
    /// system. Promoted to BL-342's `condition_set`.
    ///
    /// Populated by copying the predicate out of `prototype_tech_gates()`
    /// (tech_gate.hpp) when this node's id appears there. `tech_gate.cpp` is the
    /// ONE authority for what a gate requires, because a gate that gates
    /// construction has to be linkable from the Lua-free world superset;
    /// `scripts/tech_tree.lua` authors identity, topology and prose, never a
    /// predicate. Empty when `earnable` is false — and note that an empty
    /// `condition_set` means ALWAYS TRUE (BL-342 property 2), which is why
    /// "not earnable" is carried by its own flag rather than by an empty set.
    condition_set condition;

    /// Whether this tech has an authored gate at all. False for the great
    /// majority: the antiquity nodes are history-sim descriptions and the Era-1
    /// nodes are sketches, and inventing predicates for 130+ of them would be
    /// authoring where no evidence exists. A non-earnable tech renders honestly
    /// in the F9 viewer as "no gate authored" rather than as unlocked.
    bool earnable = false;

    /// The original descriptive label, preserved verbatim (research | structure
    /// | stockpile | market | surplus | era | endowment | diffusion | creed).
    /// It is still the best one-word summary of what a node is ABOUT, the F9
    /// viewer has always shown it, and the history-sim's derived nodes carry
    /// labels (`endowment`, `diffusion`) that were never in the six-label set
    /// and never became predicates. Kept as a label, now honestly named one.
    std::string condition_label;

    std::string unlocks;   ///< What completing the tech gives, in prose.
    std::string status;    ///< "sketch" | "stub" | "parked".
};

/// Startup-loaded, read-only store of the tech/quest tree. Pure data once built;
/// populated from scripts/tech_tree.lua (protected calls only) and consumed by
/// ui::draw_tech_tree_panel.
///
/// The simulation does NOT read this registry — it reads `tech_gate.hpp`'s
/// Lua-free gate table, which this registry copies its predicates from. That
/// split is not a duplicate authority but a deliberate one-way dependency: the
/// gate must be linkable by the SDL/Lua-free world superset, and the viewer must
/// show exactly what the gate requires (BL-344; tech_gate.hpp explains why).
class tech_tree_registry
{
public:
    /// Populate the registry from scripts/tech_tree.lua via the embedded Lua state.
    ///
    /// @param lua A lua_state that has already run scripts/tech_tree.lua.
    /// @throws std::runtime_error on a Lua error or malformed data.
    void load_from_lua(lua_state& lua);

    /// All quests, in authored order.
    const std::vector<tech_quest>& quests() const { return m_quests; }

    /// All techs, in authored order (the viewer filters by quest id).
    const std::vector<tech_node>& techs() const { return m_techs; }

    /// Bumped by every `load_from_lua`. A UI cache that retains `const tech_node*`
    /// into `m_techs` must stamp on THIS, not on the registry address or the entry
    /// counts: `verify.new_world` reloads the same file into the same member, so
    /// address and counts are both unchanged while every pointer is dangling.
    std::uint32_t generation() const { return m_generation; }

private:
    std::vector<tech_quest> m_quests;
    std::vector<tech_node>  m_techs;
    std::uint32_t           m_generation = 0;
};
