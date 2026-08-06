#pragma once

#include <string>
#include <vector>

// Forward-declared so this header carries no sol2/Lua dependency (same pattern as
// recipe_registry.hpp). Only tech_tree.cpp pulls in the Lua state, and that TU is
// excluded from the SDL/Lua-free headless world superset (CMakeLists + build.yml).
class lua_state;

/// One quest record from scripts/tech_tree.lua — a chain of techs whose capstone
/// opens further quest trees (capstones open QUEST TREES, not Eras — the
/// 2026-07-08 BL-087 Era reframe). Mock data only: the tech system is
/// post-prototype; nothing in the simulation reads this. Fields are kept as
/// strings deliberately — the schema is still design-fluid and the only consumer
/// is the read-only F9 viewer panel.
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
    std::string condition; ///< research | structure | stockpile | market | surplus | era.
    std::string unlocks;   ///< What completing the tech gives, in prose.
    std::string status;    ///< "sketch" | "stub" | "parked".
};

/// Startup-loaded, read-only store of the mock tech/quest tree. Pure data once
/// built; populated from scripts/tech_tree.lua (protected calls only) and consumed
/// solely by ui::draw_tech_tree_panel. No simulation system reads it (BL-087
/// resolution 6: the tech system is post-prototype).
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

private:
    std::vector<tech_quest> m_quests;
    std::vector<tech_node>  m_techs;
};
