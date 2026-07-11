#pragma once

#include "components.hpp"

#include <array>
#include <cstdint>
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
    /// Per-resource material cost of construction (BL-044). Indexed by
    /// static_cast<std::size_t>(resource_type). Bought from the tile's local
    /// market at its prevailing price, folded into the credit cost
    /// (construction.cpp). Zero = no requirement for that good.
    std::array<float, resource_count> resource_build_cost = {};

    /// Economy ticks the building spends under construction before it starts
    /// producing (playtest patch, 2026-07-06). 0 = instant (pre-existing
    /// behaviour), authored in scripts/economy.lua as `build_duration_ticks`.
    float build_duration_ticks = 0.0f;
};

/// BL-078 elastic nation-substrate model tunables, authored in scripts/economy.lua
/// under `economy.substrate`. Applied at tick time by inject_substrate_demand so
/// the whole demand/supply model is retunable without regenerating the world.
/// See docs/economy/PRODUCTION.md (on landing) and economy.lua for the full model.
struct substrate_params
{
    /// Per-capita aggregate demand weight per resource (population + background
    /// industry pull). Indexed by static_cast<std::size_t>(resource_type).
    std::array<float, resource_count> demand_basket = {};
    float capacity_scale       = 2.0f;  ///< deposit-derived capacity → supply ceiling scale.
    float clearing_fraction    = 0.90f; ///< abstract supply clears this fraction of demand (leaves the margin).
    float demand_elasticity    = 0.80f; ///< exponent on (base_price / price).
    float elasticity_min       = 0.30f; ///< clamp lo on the elasticity factor.
    float elasticity_max       = 2.50f; ///< clamp hi on the elasticity factor.
    float demand_scale         = 1.00f; ///< global population → demand scale.
    float growth_met_threshold = 0.50f; ///< basket met-supply ratio a centre needs to grow.
};

/// BL-095 construction-gate tunables, authored in scripts/economy.lua under
/// `economy.construction`. Read by run_economy_step's construction step, which
/// paces each build against the local market's recent supply of its materials.
struct construction_params
{
    float max_stretch = 10.0f; ///< longest a material-starved build stretches to (×base duration); below 1/max_stretch it pauses.
};

/// BL-148/149 logistics-node discount tunables, authored in scripts/economy.lua under
/// the top-level `logistics.node_discount`. A convoy's intra-body haul cost is discounted
/// for each population-centre (BL-148) and inland_logistics_hub (BL-149) tile its A* path
/// crosses, so the world's cities and the player's hubs form a cheap logistics network the
/// player plugs into. See docs/economy/SUPPLY.md (on landing).
struct logistics_node_params
{
    float city_discount_per_scale = 0.04f; ///< discount fraction per population-centre `scale` point (1–5) on the path.
    float hub_discount            = 0.12f; ///< flat discount fraction per inland_logistics_hub tile on the path.
    float discount_cap            = 0.50f; ///< ceiling on the summed node discount (fraction of the haul cost).
};

/// Player road-placement cost for a single tier (BL-147 core, BL-172 ladder), authored in
/// scripts/economy.lua under `economy.roads.{track,road,highway}`. A placed road tile costs a
/// flat credit sum plus per-resource materials bought from the local market — the same cost
/// shape as building construction, but paid up front (roads are instant, per-tile, not
/// durative). The registry holds one of these per tier; see `recipe_registry::road_econ(tier)`.
/// See docs/economy/SUPPLY.md / docs/ui/PLANETARY.md.
struct road_economics
{
    float build_cost = 40.0f; ///< flat credit cost of a road tile of this tier.
    /// Per-resource material cost, indexed by static_cast<std::size_t>(resource_type); bought
    /// from the tile's local market at its prevailing price. Zero = no requirement.
    std::array<float, resource_count> resource_build_cost = {};
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

    /// BL-078 elastic-substrate model tunables (economy.substrate in Lua).
    const substrate_params& substrate() const { return m_substrate; }

    /// BL-095 construction-gate tunables (economy.construction in Lua).
    const construction_params& construction() const { return m_construction; }

    /// Base logistics cost per unit distance per unit cargo for the given convoy mode.
    float logistics_cost(convoy_mode m) const
    {
        return m_logistics_costs[static_cast<std::size_t>(m)];
    }

    /// BL-148/149 logistics-node discount tunables (logistics.node_discount in Lua).
    const logistics_node_params& logistics_nodes() const { return m_logistics_nodes; }

    /// Player road-placement cost for a tier (BL-172): 1=Track, 2=Road, 3=Highway; clamped to
    /// [1,3]. Authored in economy.roads.{track,road,highway}. Default arg keeps BL-147 callers
    /// (Track) unchanged.
    const road_economics& road_econ(std::uint8_t tier = 1) const
    {
        const std::size_t i = (tier < 1u ? 1u : (tier > 3u ? 3u : tier)) - 1u;
        return m_road_econ[i];
    }

    std::size_t recipe_count() const { return m_recipes.size(); }

    /// Returns the number of available recipes for the given building type.
    /// Only processing_facility has recipes; all other types return 0. Inline (pure
    /// data, no Lua) so the SDL/Lua-free world superset — and the headless harnesses
    /// that exclude recipe_registry.cpp — link without it (BL-079 uses this).
    int recipe_count(building_type bt) const
    {
        if (bt != building_type::processing_facility)
            return 0;
        return static_cast<int>(m_recipes.size());
    }

    /// Returns the recipe at index @p i for building type @p bt. The index is
    /// clamped to [0, recipe_count(bt) - 1]; returns a dummy empty recipe if the
    /// type has no recipes. Inline for the same headless-link reason as above.
    const recipe& recipe_at(building_type bt, int i) const
    {
        static const recipe empty{};
        const int n = recipe_count(bt);
        if (n == 0)
            return empty;
        const int clamped = (i < 0) ? 0 : (i >= n ? n - 1 : i);
        return m_recipes[static_cast<std::size_t>(clamped)];
    }

    // --- direct construction for tests (headless harness builds these by hand) ---
    void set_thresholds(float t_full, float t_idle) { m_t_full = t_full; m_t_idle = t_idle; }
    void set_substrate(const substrate_params& s) { m_substrate = s; }
    void set_construction(const construction_params& c) { m_construction = c; }
    void set_economics(building_type type, const building_economics& e)
    {
        m_building_econ[static_cast<std::size_t>(type)] = e;
    }
    void set_logistics_cost(convoy_mode m, float v)
    {
        m_logistics_costs[static_cast<std::size_t>(m)] = v;
    }
    void set_logistics_nodes(const logistics_node_params& p) { m_logistics_nodes = p; }
    void set_road_econ(std::uint8_t tier, const road_economics& r)
    {
        const std::size_t i = (tier < 1u ? 1u : (tier > 3u ? 3u : tier)) - 1u;
        m_road_econ[i] = r;
    }
    uint16_t add_recipe(const recipe& r)
    {
        m_recipes.push_back(r);
        return static_cast<uint16_t>(m_recipes.size() - 1);
    }

private:
    std::vector<recipe> m_recipes;

    /// Indexed by building_type (none / extraction_site / processing_facility / port /
    /// launchpad / inland_logistics_hub — BL-149 bumped the count 5 → 6).
    std::array<building_economics, 6> m_building_econ = {};

    float m_t_full = 1.0f;
    float m_t_idle = 0.2f;

    /// BL-078 elastic-substrate model tunables (economy.substrate). Defaults match
    /// economy.lua so a hand-built harness registry behaves sensibly without Lua.
    substrate_params m_substrate = {};

    /// BL-095 construction-gate tunables (economy.construction). Defaults match
    /// economy.lua so a hand-built harness registry paces builds sensibly.
    construction_params m_construction = {};

    /// Logistics base cost per unit distance per unit cargo, indexed by convoy_mode
    /// (land=0, sea=1, air=2, space=3). Defaults match economy.lua values.
    std::array<float, 4> m_logistics_costs = { 0.02f, 0.05f, 0.15f, 1.00f };

    /// BL-148/149 node-discount tunables (logistics.node_discount). Defaults match economy.lua
    /// so a hand-built harness registry discounts city/hub routes sensibly without Lua.
    logistics_node_params m_logistics_nodes = {};

    /// Road-placement cost per tier (BL-172): index 0..2 = Track/Road/Highway (road_level 1/2/3).
    /// Credit defaults are used by the Lua-free harnesses; the material line is seeded from Lua
    /// (a harness that skips Lua pays credits only).
    std::array<road_economics, 3> m_road_econ = { {
        road_economics{ 25.0f, {} }, // Track   (road_level 1)
        road_economics{ 45.0f, {} }, // Road    (road_level 2)
        road_economics{ 90.0f, {} }, // Highway (road_level 3)
    } };
};
