#include "world_gen_config.hpp"

#include "scripting/lua_state.hpp"

#include <sol/sol.hpp>

#include <stdexcept>
#include <unordered_map>

namespace {

/// Map a Lua resource name (matching the resource_type enum identifiers) to the
/// enum value. Returns false in `ok` for an unknown name.
///
/// COVERS THE WHOLE ENUM, deliberately. This table used to hold only "the
/// prototype's tradeable subset", on the reasoning that world_gen.lua authors
/// Kepler's starting market rather than the full economy table. That reasoning
/// made the table a hand-synced duplicate of the one in recipe_registry.cpp, and
/// on 2026-08-12 it cost a startup crash: BL-368 priced the habitability tranche
/// (clean_water, consumer_goods, medical_supplies) in world_gen.lua and added it
/// to recipe_registry's table but not this one, so load_from_lua threw
/// "Unknown resource 'medical_supplies'" and the app died right after generation.
/// A subset is only safe while nobody authors outside it, which is not a property
/// anyone can check at the point of authoring. So: every enum value resolves here,
/// and a new resource is added to this table as a matter of course.
resource_type resource_from_name(const std::string& name, bool& ok)
{
    static const std::unordered_map<std::string, resource_type> table = {
        { "iron_ore",             resource_type::iron_ore },
        { "petroleum",            resource_type::petroleum },
        { "water",                resource_type::water },
        { "agricultural_produce", resource_type::agricultural_produce },
        { "steel",                resource_type::steel },
        { "refined_fuel",         resource_type::refined_fuel },
        { "food_rations",         resource_type::food_rations },
        { "grain",                resource_type::grain },
        { "fodder",               resource_type::fodder },
        { "salt",                 resource_type::salt },
        { "transport_capacity",   resource_type::transport_capacity },
        { "charcoal",             resource_type::charcoal },
        { "iron_blooms",          resource_type::iron_blooms },
        { "bullion",              resource_type::bullion },
        { "trade_goods_misc",     resource_type::trade_goods_misc },
        { "propellant",           resource_type::propellant },
        // BL-340 (2026-08-11): closes the minable-but-unsellable asymmetry
        // (raws) and the processing-chain roster (new goods).
        { "coal",                  resource_type::coal },
        { "silica",                resource_type::silica },
        { "copper_ore",            resource_type::copper_ore },
        { "rare_earth_ore",        resource_type::rare_earth_ore },
        { "iron_nickel_ore",       resource_type::iron_nickel_ore },
        { "platinum_group_metals", resource_type::platinum_group_metals },
        { "silicon",               resource_type::silicon },
        { "refined_copper",        resource_type::refined_copper },
        { "ree_alloy",             resource_type::ree_alloy },
        { "machinery",             resource_type::machinery },
        { "alloys",                resource_type::alloys },
        { "electronics",           resource_type::electronics },
        { "spacecraft_components", resource_type::spacecraft_components },
        // BL-368 (2026-08-11): the habitability tranche. Priced in world_gen.lua
        // from the day it landed; missing here until 2026-08-12, which is the
        // crash described above.
        { "clean_water",           resource_type::clean_water },
        { "consumer_goods",        resource_type::consumer_goods },
        { "medical_supplies",      resource_type::medical_supplies },
        // The remaining enum values, so this table is complete rather than a
        // subset that happens to cover what is authored today. Endemic and
        // ambient goods (BL-286/BL-191): unpriced in world_gen.lua right now,
        // but authoring a price for one must not be a crash.
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
    };

    const auto it = table.find(name);
    ok = (it != table.end());
    return ok ? it->second : resource_type::iron_ore;
}

} // namespace

void world_gen_config::load_from_lua(lua_state& lua)
{
    sol::state& s = lua.state();

    sol::optional<sol::table> wg = s["world_gen"];
    if (!wg)
        return; // no world_gen table authored — keep the built-in defaults.

    if (sol::optional<sol::table> ds = (*wg)["deposit_scalar"])
    {
        deposit_scalar[0] = ds->get_or("sparse",   deposit_scalar[0]);
        deposit_scalar[1] = ds->get_or("lean",     deposit_scalar[1]);
        deposit_scalar[2] = ds->get_or("standard", deposit_scalar[2]);
    }

    if (sol::optional<sol::table> km = (*wg)["kepler_market"])
    {
        if (sol::optional<sol::table> bp = (*km)["base_price"])
        {
            for (const auto& kv : *bp)
            {
                const std::string rname = kv.first.as<std::string>();
                bool ok = false;
                const resource_type r = resource_from_name(rname, ok);
                if (!ok)
                    throw std::runtime_error(
                        "Unknown resource '" + rname + "' in world_gen.kepler_market.base_price");
                kepler_base_price[static_cast<std::size_t>(r)] = kv.second.as<float>();
            }
        }

        if (sol::optional<sol::table> cv = (*km)["carving"])
        {
            market_carving.rich_factor   = cv->get_or("rich_factor",   market_carving.rich_factor);
            market_carving.barren_factor = cv->get_or("barren_factor", market_carving.barren_factor);
        }

        if (sol::optional<sol::table> en = (*km)["endemic"])
        {
            endemic.source_price  = en->get_or("source_price",  endemic.source_price);
            endemic.distance_gain = en->get_or("distance_gain", endemic.distance_gain);
        }
    }

    if (sol::optional<sol::table> corp = (*wg)["corporations"])
        corporation_count = corp->get_or("count", corporation_count);
}
