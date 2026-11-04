#include "world_gen_config.hpp"

#include "placement_rules.hpp" // is_extractable -- the opening kit's slots (BL-1473)
#include "resource_names.hpp"
#include "scripting/lua_state.hpp"

#include <sol/sol.hpp>

#include <initializer_list>
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

    // BL-1268: the retrofit's rates (TRADE.md § Trade in generation).
    if (sol::optional<sol::table> tr = (*wg)["trade_retrofit"])
    {
        trade_retrofit.flow_points_per_1000 =
            tr->get_or("flow_points_per_1000", trade_retrofit.flow_points_per_1000);
        trade_retrofit.relation_points_per_year =
            tr->get_or("relation_points_per_year", trade_retrofit.relation_points_per_year);
        trade_retrofit.points_per_marketplace =
            tr->get_or("points_per_marketplace", trade_retrofit.points_per_marketplace);
        trade_retrofit.max_per_market = tr->get_or("max_per_market", trade_retrofit.max_per_market);
        if (trade_retrofit.flow_points_per_1000 < 0 || trade_retrofit.relation_points_per_year < 0
            || trade_retrofit.max_per_market < 0)
            throw std::runtime_error("world_gen.trade_retrofit rates must be >= 0");
    }

    if (sol::optional<sol::table> corp = (*wg)["corporations"])
        corporation_count = corp->get_or("count", corporation_count);

    // BL-1473: the major opening kit (CORPORATION_GENERATION.md § A major's opening
    // kit). Every name is resolved HERE and an unknown one throws -- a typo must not
    // silently drop a slot. Recipe names are the one exception: the registry is not
    // loaded yet, so they are kept as strings and resolved, as loudly, when the kit
    // is laid (opening_kit.cpp). An absent table is "no kit".
    opening_kit = opening_kit_table{};
    if (sol::optional<sol::table> ok = (*wg)["opening_kit"])
    {
        opening_kit.active = ok->get<sol::optional<std::string>>("active").value_or(std::string{});
        if (sol::optional<sol::table> cfgs = (*ok)["configs"])
        {
            const auto resource_of = [](const std::string& kit, const std::string& n) {
                bool found = false;
                const resource_type r = resource_names::resource_from_name(n, found);
                if (!found)
                    throw std::runtime_error("world_gen.opening_kit." + kit + ": unknown resource '"
                                             + n + "'");
                return r;
            };
            for (const auto& kv : *cfgs)
            {
                const std::string kit = kv.first.as<std::string>();
                if (kv.second.get_type() != sol::type::table)
                    throw std::runtime_error("world_gen.opening_kit.configs." + kit + " must be a table");
                const sol::table t = kv.second.as<sol::table>();
                // An unknown key is an authoring error (a typo would silently drop a
                // whole category), and so is a list with a non-index key.
                const auto keys_in = [&](const sol::table& tb, std::initializer_list<const char*> ok,
                                         const std::string& where) {
                    for (const auto& e : tb)
                    {
                        if (e.first.get_type() != sol::type::string)
                            throw std::runtime_error(where + ": unexpected non-name key");
                        const std::string k = e.first.as<std::string>();
                        bool known = false;
                        for (const char* o : ok)
                            known = known || k == o;
                        if (!known)
                            throw std::runtime_error(where + ": unknown key '" + k + "'");
                    }
                };
                const auto list_only = [&](const sol::table& tb, const std::string& where) {
                    const std::size_t n = tb.size();
                    for (const auto& e : tb)
                    {
                        const bool index = e.first.get_type() == sol::type::number
                                        && e.first.as<double>() >= 1.0
                                        && e.first.as<double>() <= static_cast<double>(n)
                                        && e.first.as<double>()
                                               == static_cast<double>(static_cast<std::size_t>(e.first.as<double>()));
                        if (!index)
                            throw std::runtime_error(where + " must be a plain list");
                    }
                };
                const std::string where = "world_gen.opening_kit.configs." + kit;
                keys_in(t, { "treasury", "logistics", "extraction", "processing", "civic", "army" }, where);
                opening_kit_config c;
                c.name = kit;
                {
                    const sol::object tr = t["treasury"];
                    if (!tr.valid() || tr.get_type() != sol::type::number)
                        throw std::runtime_error(where + ".treasury is required (a number of credits)");
                    c.treasury = tr.as<float>();
                }
                if (!(c.treasury >= 0.0f))
                    throw std::runtime_error("world_gen.opening_kit." + kit + ".treasury must be >= 0");
                if (sol::optional<sol::table> ex = t["extraction"]) list_only(*ex, where + ".extraction");
                if (sol::optional<sol::table> pr = t["processing"]) list_only(*pr, where + ".processing");
                if (sol::optional<sol::table> cv = t["civic"])      list_only(*cv, where + ".civic");
                // Arrays read by index 1..n: authored order is the slot order.
                if (sol::optional<sol::table> ex = t["extraction"])
                    for (std::size_t i = 1; i <= ex->size(); ++i)
                    {
                        opening_kit_slot s;
                        s.category = kit_slot_category::extraction;
                        s.label    = (*ex)[i].get<std::string>();
                        if (s.label == "flex")
                            s.flex = true;
                        else
                        {
                            s.good = resource_of(kit, s.label);
                            if (!placement_rules::is_extractable(s.good))
                                throw std::runtime_error("world_gen.opening_kit." + kit
                                                         + ": extraction slot '" + s.label
                                                         + "' is not an extractable raw");
                        }
                        c.slots.push_back(std::move(s));
                    }
                if (sol::optional<sol::table> pr = t["processing"])
                    for (std::size_t i = 1; i <= pr->size(); ++i)
                    {
                        const sol::object o = (*pr)[i];
                        if (o.get_type() != sol::type::table)
                            throw std::runtime_error("world_gen.opening_kit." + kit
                                                     + ".processing entries are { good = ..., recipes = {...} }");
                        const sol::table e = o.as<sol::table>();
                        keys_in(e, { "good", "recipes" }, where + ".processing[" + std::to_string(i) + "]");
                        if (sol::optional<sol::table> rs = e["recipes"])
                            list_only(*rs, where + ".processing[" + std::to_string(i) + "].recipes");
                        opening_kit_slot s;
                        s.category = kit_slot_category::processing;
                        s.label    = e.get<sol::optional<std::string>>("good").value_or(std::string{});
                        s.good     = resource_of(kit, s.label);
                        if (sol::optional<sol::table> rs = e["recipes"])
                            for (std::size_t j = 1; j <= rs->size(); ++j)
                                s.recipes.push_back((*rs)[j].get<std::string>());
                        c.slots.push_back(std::move(s));
                    }
                if (sol::optional<sol::table> cv = t["civic"])
                    for (std::size_t i = 1; i <= cv->size(); ++i)
                    {
                        opening_kit_slot s;
                        s.category = kit_slot_category::civic;
                        s.label    = (*cv)[i].get<std::string>();
                        if (s.label == "planetary_marketplace")
                            s.building = building_type::planetary_marketplace;
                        else if (s.label == "research_institute")
                            s.building = building_type::research_institute;
                        else if (s.label == "schooling")
                            s.building = building_type::schooling;
                        else if (s.label == "university")
                            s.building = building_type::university;
                        else
                            throw std::runtime_error("world_gen.opening_kit." + kit
                                                     + ": unknown civic building '" + s.label + "'");
                        c.slots.push_back(std::move(s));
                    }
                if (sol::optional<sol::table> lg = t["logistics"])
                {
                    for (const auto& lkv : *lg)
                    {
                        const std::string n = lkv.first.as<std::string>();
                        if (lkv.second.get_type() != sol::type::number)
                            throw std::runtime_error("world_gen.opening_kit." + kit + ".logistics."
                                                     + n + " must be a count");
                        const double v = lkv.second.as<double>();
                        if (!(v >= 0.0 && v <= 64.0) || v != static_cast<double>(static_cast<int>(v)))
                            throw std::runtime_error("world_gen.opening_kit." + kit + ".logistics."
                                                     + n + " must be an integer in [0, 64]");
                        if (n == "inland_logistics_hub")  c.hubs     = static_cast<int>(v);
                        else if (n == "railway_station")  c.stations = static_cast<int>(v);
                        else if (n == "port")             c.ports    = static_cast<int>(v);
                        else
                            throw std::runtime_error("world_gen.opening_kit." + kit
                                                     + ": unknown logistics node '" + n + "'");
                    }
                    if (!c.logistics_capped())
                        throw std::runtime_error("world_gen.opening_kit." + kit
                                                 + ".logistics must name inland_logistics_hub, "
                                                   "railway_station and port");
                }
                if (sol::optional<sol::table> ar = t["army"])
                {
                    // BL-1474 (MILITARY.md § Nation garrisons): the fixed army.
                    keys_in(*ar, { "capital", "border", "men", "land_recruits" }, where + ".army");
                    const auto classes = [&](const char* key, std::vector<std::uint8_t>& out) {
                        sol::optional<sol::table> ls = (*ar)[key];
                        if (!ls)
                            return;
                        list_only(*ls, where + ".army." + key);
                        static const char* const names[] = { "infantry", "motorised", "mechanised",
                                                             "armour", "light_armour", "artillery",
                                                             "air_defence" };
                        for (std::size_t i = 1; i <= ls->size(); ++i)
                        {
                            const std::string n = (*ls)[i].get<std::string>();
                            std::uint8_t k = 0;
                            while (k < 7 && n != names[k])
                                ++k;
                            if (k == 7)
                                throw std::runtime_error(where + ".army." + key
                                                         + ": unknown land class '" + n + "'");
                            out.push_back(k); // the land_class enumerator's value
                        }
                    };
                    classes("capital", c.army_capital);
                    classes("border", c.army_border);
                    const auto count = [&](const char* key, int lo, int hi, int& out) {
                        const sol::object o = (*ar)[key];
                        if (!o.valid())
                            return;
                        const double v = o.get_type() == sol::type::number ? o.as<double>() : -1.0;
                        if (!(v >= lo && v <= hi) || v != static_cast<double>(static_cast<int>(v)))
                            throw std::runtime_error(where + ".army." + key + " must be an integer in ["
                                                     + std::to_string(lo) + ", " + std::to_string(hi) + "]");
                        out = static_cast<int>(v);
                    };
                    count("men", 1, 10000, c.army_men);
                    count("land_recruits", 0, 1000000, c.land_recruits);
                }
                opening_kit.configs.emplace(kit, std::move(c));
            }
        }
        if (!opening_kit.active.empty() && opening_kit.active != "none"
            && opening_kit.configs.count(opening_kit.active) == 0)
            throw std::runtime_error("world_gen.opening_kit.active names no configuration: '"
                                     + opening_kit.active + "'");
    }
}
