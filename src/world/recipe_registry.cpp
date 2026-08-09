#include "recipe_registry.hpp"

#include "scripting/lua_state.hpp"

#include <sol/sol.hpp>

#include <stdexcept>
#include <unordered_map>

namespace {

/// Map a Lua resource name (matching the resource_type enum identifiers) to the
/// enum value. Covers the full enum so recipes outside the prototype subset load
/// without a retrofit. Returns false in `ok` for an unknown name.
resource_type resource_from_name(const std::string& name, bool& ok)
{
    static const std::unordered_map<std::string, resource_type> table = {
        { "iron_ore",              resource_type::iron_ore },
        { "coal",                  resource_type::coal },
        { "petroleum",             resource_type::petroleum },
        { "silica",                resource_type::silica },
        { "copper_ore",            resource_type::copper_ore },
        { "rare_earth_ore",        resource_type::rare_earth_ore },
        { "agricultural_produce",  resource_type::agricultural_produce },
        { "water",                 resource_type::water },
        { "iron_nickel_ore",       resource_type::iron_nickel_ore },
        { "platinum_group_metals", resource_type::platinum_group_metals },
        { "regolith",              resource_type::regolith },
        { "stone",                 resource_type::stone },
        { "timber",                resource_type::timber },
        { "sand",                  resource_type::sand },
        { "clay",                  resource_type::clay },
        { "peat",                  resource_type::peat },
        { "tobacco",               resource_type::tobacco },
        { "spices",                resource_type::spices },
        { "coffee",                resource_type::coffee },
        { "furs",                  resource_type::furs },
        { "steel",                 resource_type::steel },
        { "refined_fuel",          resource_type::refined_fuel },
        { "food_rations",          resource_type::food_rations },
        { "propellant",            resource_type::propellant },
    };

    const auto it = table.find(name);
    ok = (it != table.end());
    return ok ? it->second : resource_type::iron_ore;
}

/// Read a Lua {resource_name = qty} table into a resource-indexed array.
void read_resource_map(const sol::table& src, std::array<float, resource_count>& dst,
                       const std::string& context)
{
    for (const auto& kv : src)
    {
        const std::string rname = kv.first.as<std::string>();
        bool ok = false;
        const resource_type r = resource_from_name(rname, ok);
        if (!ok)
            throw std::runtime_error("Unknown resource '" + rname + "' in " + context);
        dst[static_cast<std::size_t>(r)] = kv.second.as<float>();
    }
}

} // namespace

// --- load_from_lua -----------------------------------------------------------
// (recipe_count / recipe_at are now inline in the header so the SDL/Lua-free world
//  superset links without this translation unit — see recipe_registry.hpp.)

void recipe_registry::load_from_lua(lua_state& lua)
{
    sol::state& s = lua.state();

    // --- recipes (scripts/recipes.lua must have run) ---
    sol::optional<sol::table> recipes_tbl = s["recipes"];
    if (!recipes_tbl)
        throw std::runtime_error("recipe_registry: global 'recipes' table not found "
                                 "(was scripts/recipes.lua loaded?)");

    m_recipes.clear();
    for (std::size_t i = 1; i <= recipes_tbl->size(); ++i)
    {
        sol::optional<sol::table> entry = (*recipes_tbl)[i];
        if (!entry)
            throw std::runtime_error("recipe_registry: recipe entry " + std::to_string(i)
                                     + " is not a table");

        recipe r;
        r.name = entry->get_or<std::string>("name", "");

        sol::optional<sol::table> inputs  = (*entry)["inputs"];
        sol::optional<sol::table> outputs = (*entry)["outputs"];
        if (inputs)  read_resource_map(*inputs,  r.inputs,  "recipe '" + r.name + "' inputs");
        if (outputs) read_resource_map(*outputs, r.outputs, "recipe '" + r.name + "' outputs");

        m_recipes.push_back(std::move(r));
    }

    // --- economy constants (scripts/economy.lua must have run) ---
    sol::optional<sol::table> econ = s["economy"];
    if (!econ)
        throw std::runtime_error("recipe_registry: global 'economy' table not found "
                                 "(was scripts/economy.lua loaded?)");

    sol::optional<sol::table> thr = (*econ)["thresholds"];
    if (thr)
    {
        m_t_full = thr->get_or("t_full", 1.0f);
        m_t_idle = thr->get_or("t_idle", 0.2f);
    }

    // BL-078 elastic-substrate model (economy.substrate). Scalars fall back to the
    // struct defaults (which mirror economy.lua) so a partial table still loads.
    sol::optional<sol::table> substrate = (*econ)["substrate"];
    if (substrate)
    {
        substrate_params sp;
        sol::optional<sol::table> basket = (*substrate)["demand_basket"];
        if (basket)
            read_resource_map(*basket, sp.demand_basket, "economy.substrate.demand_basket");
        sp.capacity_scale       = substrate->get_or("capacity_scale",       sp.capacity_scale);
        sp.clearing_fraction    = substrate->get_or("clearing_fraction",    sp.clearing_fraction);
        sp.demand_elasticity    = substrate->get_or("demand_elasticity",    sp.demand_elasticity);
        sp.elasticity_min       = substrate->get_or("elasticity_min",       sp.elasticity_min);
        sp.elasticity_max       = substrate->get_or("elasticity_max",       sp.elasticity_max);
        sp.demand_scale         = substrate->get_or("demand_scale",         sp.demand_scale);
        sp.growth_met_threshold = substrate->get_or("growth_met_threshold", sp.growth_met_threshold);
        m_substrate = sp;
    }

    // BL-095 construction-gate (economy.construction).
    sol::optional<sol::table> construction = (*econ)["construction"];
    if (construction)
    {
        construction_params cp;
        cp.max_stretch = construction->get_or("max_stretch", cp.max_stretch);
        m_construction = cp;
    }

    sol::optional<sol::table> buildings = (*econ)["buildings"];
    if (buildings)
    {
        struct named_type { const char* key; building_type type; };
        const named_type types[] = {
            { "extraction_site",      building_type::extraction_site },
            { "processing_facility",  building_type::processing_facility },
            { "port",                 building_type::port },
            { "launchpad",            building_type::launchpad },
            { "inland_logistics_hub", building_type::inland_logistics_hub }, // BL-149
        };
        for (const named_type& nt : types)
        {
            sol::optional<sol::table> b = (*buildings)[nt.key];
            if (!b)
                continue;
            building_economics e;
            e.base_rate   = b->get_or("base_rate",   0.0f);
            e.maintenance = b->get_or("maintenance", 0.0f);
            e.base_wage   = b->get_or("base_wage",   0.0f);
            e.build_cost  = b->get_or("build_cost",  0.0f);
            e.build_duration_ticks = b->get_or("build_duration_ticks", 0.0f);
            // Optional resource material cost (BL-044).
            sol::optional<sol::table> rcosts = (*b)["resource_costs"];
            if (rcosts)
                read_resource_map(*rcosts, e.resource_build_cost,
                                  std::string("buildings.") + nt.key + ".resource_costs");
            m_building_econ[static_cast<std::size_t>(nt.type)] = e;
        }
    }

    // Road-placement cost per tier (economy.roads.{track,road,highway}, BL-172; BL-147 shipped a
    // single `local` tier). Same shape as a building's cost (flat credits + market-bought
    // materials) but paid up front at placement. Tier index 0..2 = Track/Road/Highway.
    sol::optional<sol::table> roads = (*econ)["roads"];
    if (roads)
    {
        static constexpr const char* kTierKey[3] = { "track", "road", "highway" };
        for (std::uint8_t tier = 1; tier <= 3; ++tier)
        {
            sol::optional<sol::table> t = (*roads)[kTierKey[tier - 1]];
            if (!t)
                continue;
            road_economics re = m_road_econ[tier - 1]; // keep the struct default as the fallback
            re.build_cost = t->get_or("build_cost", re.build_cost);
            sol::optional<sol::table> rcosts = (*t)["resource_costs"];
            if (rcosts)
                read_resource_map(*rcosts, re.resource_build_cost,
                                  std::string("roads.") + kTierKey[tier - 1] + ".resource_costs");
            m_road_econ[tier - 1] = re;
        }
    }

    // --- logistics costs (scripts/economy.lua global 'logistics') ---
    sol::optional<sol::table> logistics = s["logistics"];
    if (logistics)
    {
        sol::optional<sol::table> costs = (*logistics)["base_cost_per_unit_distance"];
        if (costs)
        {
            struct named_mode { const char* key; convoy_mode mode; };
            const named_mode modes[] = {
                { "land",  convoy_mode::land  },
                { "sea",   convoy_mode::sea   },
                { "air",   convoy_mode::air   },
                { "space", convoy_mode::space },
            };
            for (const named_mode& nm : modes)
            {
                sol::optional<float> v = (*costs)[nm.key];
                if (v)
                    m_logistics_costs[static_cast<std::size_t>(nm.mode)] = *v;
            }
        }

        // BL-148/149 logistics-node discount (logistics.node_discount). Partial table falls
        // back to the struct defaults (which mirror economy.lua).
        sol::optional<sol::table> nd = (*logistics)["node_discount"];
        if (nd)
        {
            logistics_node_params p;
            p.city_discount_per_scale = nd->get_or("city_per_scale", p.city_discount_per_scale);
            p.hub_discount            = nd->get_or("hub",            p.hub_discount);
            p.discount_cap            = nd->get_or("cap",            p.discount_cap);
            m_logistics_nodes = p;
        }
    }
}
