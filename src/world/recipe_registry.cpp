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
        { "steel",                 resource_type::steel },
        { "refined_fuel",          resource_type::refined_fuel },
        { "food_rations",          resource_type::food_rations },
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

// --- recipe_count / recipe_at ------------------------------------------------

int recipe_registry::recipe_count(building_type bt) const
{
    // Only processing facilities use recipes. Extraction sites and ports do not.
    if (bt != building_type::processing_facility)
        return 0;
    return static_cast<int>(m_recipes.size());
}

namespace {
// Dummy recipe returned when the building type carries no recipes.
const recipe& empty_recipe()
{
    static const recipe r{};
    return r;
}
} // namespace

const recipe& recipe_registry::recipe_at(building_type bt, int i) const
{
    const int n = recipe_count(bt);
    if (n == 0)
        return empty_recipe();
    const int clamped = (i < 0) ? 0 : (i >= n ? n - 1 : i);
    return m_recipes[static_cast<std::size_t>(clamped)];
}

// --- load_from_lua -----------------------------------------------------------

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

    sol::optional<sol::table> buildings = (*econ)["buildings"];
    if (buildings)
    {
        struct named_type { const char* key; building_type type; };
        const named_type types[] = {
            { "extraction_site",     building_type::extraction_site },
            { "processing_facility", building_type::processing_facility },
            { "port",                building_type::port },
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
            m_building_econ[static_cast<std::size_t>(nt.type)] = e;
        }
    }
}
