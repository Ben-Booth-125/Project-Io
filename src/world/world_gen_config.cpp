#include "world_gen_config.hpp"

#include "resource_names.hpp"
#include "scripting/lua_state.hpp"

#include <sol/sol.hpp>

#include <stdexcept>
#include <string>

void world_gen_config::load_from_lua(lua_state& lua)
{
    sol::state& s = lua.state();

    sol::optional<sol::table> wg = s["world_gen"];
    if (!wg)
        return; // no world_gen table authored — keep the built-in defaults,
                // and stay marked `is_fallback`: nothing was parsed.

    // Parsed from here on, so this is no longer the built-in fallback. Set
    // BEFORE the field reads below so an early return from a malformed table
    // cannot leave a half-parsed config claiming to be the fallback.
    is_fallback = false;

    if (sol::optional<sol::table> ds = (*wg)["deposit_scalar"])
    {
        deposit_scalar[0] = ds->get_or("sparse",   deposit_scalar[0]);
        deposit_scalar[1] = ds->get_or("lean",     deposit_scalar[1]);
        deposit_scalar[2] = ds->get_or("standard", deposit_scalar[2]);
    }

    // BL-1200: the lake size cap. Rejected, never clamped -- a non-integer or
    // out-of-domain value is an authoring error, and a silently nudged cap
    // would move every world's coastline without saying so.
    {
        const sol::object cap = (*wg)["lake_size_cap"];
        if (cap.valid() && cap.get_type() != sol::type::lua_nil)
        {
            if (cap.get_type() != sol::type::number)
                throw std::runtime_error("world_gen.lake_size_cap must be a number");
            const double v = cap.as<double>();
            if (!(v >= lake_size_cap_min && v <= lake_size_cap_max)
                || v != static_cast<double>(static_cast<int>(v)))
                throw std::runtime_error(
                    "world_gen.lake_size_cap must be an integer in ["
                    + std::to_string(lake_size_cap_min) + ", "
                    + std::to_string(lake_size_cap_max) + "]");
            lake_size_cap = static_cast<int>(v);
        }
    }

    if (sol::optional<sol::table> km = (*wg)["kepler_market"])
    {
        if (sol::optional<sol::table> bp = (*km)["base_price"])
        {
            for (const auto& kv : *bp)
            {
                const std::string rname = kv.first.as<std::string>();
                bool ok = false;
                const resource_type r = resource_names::resource_from_name(rname, ok);
                if (!ok)
                    throw std::runtime_error(
                        "Unknown resource '" + rname + "' in world_gen.kepler_market.base_price");
                kepler_base_price[static_cast<std::size_t>(r)] = kv.second.as<float>();
            }
        }

        if (sol::optional<sol::table> cv = (*km)["carving"])
        {
            market_carving.rich_factor        = cv->get_or("rich_factor",        market_carving.rich_factor);
            market_carving.barren_factor      = cv->get_or("barren_factor",      market_carving.barren_factor);
            market_carving.corp_presence_gain = cv->get_or("corp_presence_gain", market_carving.corp_presence_gain);
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
