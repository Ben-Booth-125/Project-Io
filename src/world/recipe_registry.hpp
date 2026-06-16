#pragma once

#include "components.hpp"

#include <array>
#include <string>
#include <vector>

// Forward-declared so this header carries no sol2/Lua dependency — the economy
// systems (world/*, SDL- and Lua-free) include it and stay headlessly buildable.
// Only recipe_registry.cpp pulls in the Lua state to populate the tables.
class lua_state;

/// A processing recipe: per-batch input and output quantities, indexed by
/// resource_type. Reagents are simply inputs with no matching output. Authored
/// in scripts/recipes.lua; the recipe's id is its index in recipe_registry::recipes.
struct recipe
{
    std::string                       name;
    std::array<float, resource_count> inputs  = {};
    std::array<float, resource_count> outputs = {};
};

/// Per-building-type economic constants, authored in scripts/economy.lua.
struct building_economics
{
    float base_rate   = 0.0f; ///< Extraction units/tick at richness 1, workforce 1; or processing batches/tick at workforce 1.
    float maintenance = 0.0f; ///< Flat per-tick upkeep charged to the owning corp.
    float base_wage   = 0.0f; ///< Wage per unit workforce per tick.
    float build_cost  = 0.0f; ///< One-off construction cost (Layer 4 build UI).
};

/// Startup-loaded registry of processing recipes and economy constants. Pure
/// data once built; constructed either from Lua (load_from_lua) in the real
/// build or by hand in a headless test harness.
class recipe_registry
{
public:
    /// Populate the registry from scripts/recipes.lua and scripts/economy.lua via
    /// the embedded Lua state (protected calls only).
    ///
    /// @param lua A loaded lua_state.
    /// @throws std::runtime_error on a Lua error or malformed data.
    void load_from_lua(lua_state& lua);

    /// Look up a recipe by its id (index). Returns nullptr for `no_recipe` or any
    /// out-of-range id.
    const recipe* get_recipe(uint16_t id) const
    {
        if (id == no_recipe || id >= m_recipes.size())
            return nullptr;
        return &m_recipes[id];
    }

    /// Recipe id for a recipe name, or `no_recipe` if none matches. Used at
    /// placement to author building_component.recipe from a stable name. Inline
    /// (pure data) so player-construction logic stays headless-buildable without
    /// linking the Lua-bound translation unit.
    uint16_t recipe_id(const std::string& name) const
    {
        for (std::size_t i = 0; i < m_recipes.size(); ++i)
            if (m_recipes[i].name == name)
                return static_cast<uint16_t>(i);
        return no_recipe;
    }

    /// Economy constants for a building type.
    const building_economics& economics(building_type type) const
    {
        return m_building_econ[static_cast<std::size_t>(type)];
    }

    float t_full() const { return m_t_full; }
    float t_idle() const { return m_t_idle; }

    /// Base logistics cost per unit distance per unit cargo for the given convoy mode.
    float logistics_cost(convoy_mode m) const
    {
        return m_logistics_costs[static_cast<std::size_t>(m)];
    }

    std::size_t recipe_count() const { return m_recipes.size(); }

    /// Returns the number of available recipes for the given building type.
    /// Only processing_facility has recipes; all other types return 0.
    int recipe_count(building_type bt) const;

    /// Returns the recipe at index @p i for building type @p bt.
    /// The index is clamped to [0, recipe_count(bt) - 1]; returns a dummy empty
    /// recipe if the type has no recipes.
    const recipe& recipe_at(building_type bt, int i) const;

    // --- direct construction for tests (headless harness builds these by hand) ---
    void set_thresholds(float t_full, float t_idle) { m_t_full = t_full; m_t_idle = t_idle; }
    void set_economics(building_type type, const building_economics& e)
    {
        m_building_econ[static_cast<std::size_t>(type)] = e;
    }
    void set_logistics_cost(convoy_mode m, float v)
    {
        m_logistics_costs[static_cast<std::size_t>(m)] = v;
    }
    uint16_t add_recipe(const recipe& r)
    {
        m_recipes.push_back(r);
        return static_cast<uint16_t>(m_recipes.size() - 1);
    }

private:
    std::vector<recipe> m_recipes;

    /// Indexed by building_type (none / extraction_site / processing_facility / port / launchpad).
    std::array<building_economics, 5> m_building_econ = {};

    float m_t_full = 1.0f;
    float m_t_idle = 0.2f;

    /// Logistics base cost per unit distance per unit cargo, indexed by convoy_mode
    /// (land=0, sea=1, air=2, space=3). Defaults match economy.lua values.
    std::array<float, 4> m_logistics_costs = { 0.02f, 0.05f, 0.15f, 1.00f };
};
