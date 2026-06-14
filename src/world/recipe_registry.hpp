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
    /// placement to author building_component.recipe from a stable name.
    uint16_t recipe_id(const std::string& name) const;

    /// Economy constants for a building type.
    const building_economics& economics(building_type type) const
    {
        return m_building_econ[static_cast<std::size_t>(type)];
    }

    float t_full() const { return m_t_full; }
    float t_idle() const { return m_t_idle; }

    std::size_t recipe_count() const { return m_recipes.size(); }

    // --- direct construction for tests (headless harness builds these by hand) ---
    void set_thresholds(float t_full, float t_idle) { m_t_full = t_full; m_t_idle = t_idle; }
    void set_economics(building_type type, const building_economics& e)
    {
        m_building_econ[static_cast<std::size_t>(type)] = e;
    }
    uint16_t add_recipe(const recipe& r)
    {
        m_recipes.push_back(r);
        return static_cast<uint16_t>(m_recipes.size() - 1);
    }

private:
    std::vector<recipe> m_recipes;

    /// Indexed by building_type (none / extraction_site / processing_facility / port).
    std::array<building_economics, 4> m_building_econ = {};

    float m_t_full = 1.0f;
    float m_t_idle = 0.2f;
};
