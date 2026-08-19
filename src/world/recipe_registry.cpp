#include "recipe_registry.hpp"

#include "resource_names.hpp"
#include "scripting/lua_state.hpp"

#include <sol/sol.hpp>

#include <stdexcept>

namespace {

/// Read a Lua {resource_name = qty} table into a resource-indexed array.
void read_resource_map(const sol::table& src, std::array<float, resource_count>& dst,
                       const std::string& context)
{
    for (const auto& kv : src)
    {
        const std::string rname = kv.first.as<std::string>();
        bool ok = false;
        const resource_type r = resource_names::resource_from_name(rname, ok);
        if (!ok)
            throw std::runtime_error("Unknown resource '" + rname + "' in " + context);
        dst[static_cast<std::size_t>(r)] = kv.second.as<float>();
    }
}

/// BL-433: read an optional `era = "any"|"ancient"|"industrial"` field.
///
/// An UNKNOWN string throws rather than falling back to `any`. That is the point:
/// a silent fallback would let a typo ("industial") quietly re-admit a space-era
/// entry to the ancient roster, which is the exact defect this gate exists to
/// close. Absent is fine and means `any` — most entries are shared.
era_band read_era(const sol::table& entry, const std::string& context)
{
    sol::optional<std::string> e = entry["era"];
    if (!e)
        return era_band::any;
    if (*e == "any")        return era_band::any;
    if (*e == "ancient")    return era_band::ancient;
    if (*e == "industrial") return era_band::industrial;
    throw std::runtime_error("Unknown era '" + *e + "' in " + context
                             + " (expected any, ancient or industrial)");
}

/// BL-429: "food_rations_milled" -> "Food Rations Milled" — the pre-existing
/// Build-door title-casing (formerly `pretty_recipe` in selection_panel.cpp),
/// now the DEFAULT a recipe's `display_name` falls back to when none is
/// authored, so every recipe has a legible label without requiring one.
std::string title_case(const std::string& raw)
{
    std::string out = raw;
    bool at_start = true;
    for (char& ch : out)
    {
        if (ch == '_')
        {
            ch = ' ';
            at_start = true;
            continue;
        }
        if (at_start && ch >= 'a' && ch <= 'z')
            ch = static_cast<char>(ch - 'a' + 'A');
        at_start = false;
    }
    return out;
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

        r.era = read_era(*entry, "recipe '" + r.name + "'"); // BL-433

        // BL-429: named-building identity. Falls back to a title-cased `name`
        // when the author didn't give the recipe a building name of its own.
        r.display_name = entry->get_or<std::string>("display_name", "");
        if (r.display_name.empty())
            r.display_name = title_case(r.name);

        // BL-434: sub-facility kind. Falls back to the struct default "General"
        // (not "") when absent — see recipe_registry.hpp's field comment for why
        // an empty string is not the fallback.
        r.group = entry->get_or<std::string>("group", "General");

        m_recipes.push_back(std::move(r));
    }

    // BL-433: a fresh load starts band-agnostic. The caller (app::load_economy)
    // sets the campaign's band immediately after; a harness that never sets one
    // keeps the full roster, which is what R3 asserts.
    m_era = era_band::any;
    rebuild_allowed();

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

    // BL-365 population-growth gate (economy.population_growth). Scalars fall
    // back to the struct defaults so a partial table still loads. This is the
    // surviving remnant of the old BL-078 substrate model (see growth_params).
    sol::optional<sol::table> growth = (*econ)["population_growth"];
    if (growth)
    {
        growth_params gp;
        sol::optional<sol::table> basket = (*growth)["demand_basket"];
        if (basket)
            read_resource_map(*basket, gp.demand_basket, "economy.population_growth.demand_basket");
        gp.growth_met_threshold = growth->get_or("growth_met_threshold", gp.growth_met_threshold);
        m_growth = gp;
    }

    // BL-368 population-demand model (economy.population_demand). Scalars fall
    // back to the struct defaults so a partial table still loads.
    sol::optional<sol::table> pop_demand = (*econ)["population_demand"];
    if (pop_demand)
    {
        population_demand_params pd;
        sol::optional<sol::table> pd_basket = (*pop_demand)["demand_basket"];
        if (pd_basket)
            read_resource_map(*pd_basket, pd.demand_basket, "economy.population_demand.demand_basket");
        pd.demand_elasticity = pop_demand->get_or("demand_elasticity", pd.demand_elasticity);
        pd.elasticity_min    = pop_demand->get_or("elasticity_min",    pd.elasticity_min);
        pd.elasticity_max    = pop_demand->get_or("elasticity_max",    pd.elasticity_max);
        pd.demand_scale      = pop_demand->get_or("demand_scale",      pd.demand_scale);
        m_population_demand = pd;
    }

    // BL-340/BL-365 background-industrial-demand model (economy.background_demand).
    // Scalars fall back to the struct defaults so a partial table still loads.
    sol::optional<sol::table> bg_demand = (*econ)["background_demand"];
    if (bg_demand)
    {
        background_demand_params bd;
        sol::optional<sol::table> bd_basket = (*bg_demand)["demand_basket"];
        if (bd_basket)
            read_resource_map(*bd_basket, bd.demand_basket, "economy.background_demand.demand_basket");
        bd.demand_elasticity = bg_demand->get_or("demand_elasticity", bd.demand_elasticity);
        bd.elasticity_min    = bg_demand->get_or("elasticity_min",    bd.elasticity_min);
        bd.elasticity_max    = bg_demand->get_or("elasticity_max",    bd.elasticity_max);
        bd.demand_scale      = bg_demand->get_or("demand_scale",      bd.demand_scale);
        m_background_demand = bd;
    }

    // BL-442 price band (economy.price_band) — authored once here, read by BOTH
    // resolve_price (market_clearing.cpp) and wf_target_price (economy_system.cpp).
    sol::optional<sol::table> price_band = (*econ)["price_band"];
    if (price_band)
    {
        price_band_params pb;
        pb.floor_mult = price_band->get_or("floor_mult", pb.floor_mult);
        pb.ceil_mult  = price_band->get_or("ceil_mult",  pb.ceil_mult);
        m_price_band = pb;
    }

    // BL-263 spontaneous-market-emergence tunables (economy.market_emergence).
    sol::optional<sol::table> market_emergence = (*econ)["market_emergence"];
    if (market_emergence)
    {
        market_emergence_params me;
        me.price_distance_gain = market_emergence->get_or("price_distance_gain", me.price_distance_gain);
        me.pull_fraction       = market_emergence->get_or("pull_fraction",       me.pull_fraction);
        me.distance_falloff    = market_emergence->get_or("distance_falloff",    me.distance_falloff);
        m_market_emergence = me;
    }

    // BL-095 construction-gate (economy.construction).
    sol::optional<sol::table> construction = (*econ)["construction"];
    if (construction)
    {
        construction_params cp;
        cp.max_stretch = construction->get_or("max_stretch", cp.max_stretch);
        cp.max_logistics_reach = construction->get_or("max_logistics_reach", cp.max_logistics_reach);
        cp.site_time_reach_scale = construction->get_or("site_time_reach_scale", cp.site_time_reach_scale);
        cp.site_time_stack_discount = construction->get_or("site_time_stack_discount", cp.site_time_stack_discount);
        cp.site_time_stack_min = construction->get_or("site_time_stack_min", cp.site_time_stack_min);
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
            { "military_base",        building_type::military_base },        // BL-325 S1
            { "research_institute",   building_type::research_institute },   // BL-332
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
            // BL-436: richness -> rate conversion. Absent reference = 0 keeps the
            // raw pre-BL-436 behaviour, so an un-migrated economy.lua is unchanged.
            e.richness_reference   = b->get_or("richness_reference", 0.0f);
            e.richness_min         = b->get_or("richness_min", 0.25f);
            e.richness_max         = b->get_or("richness_max", 2.0f);
            // Optional resource material cost (BL-044).
            sol::optional<sol::table> rcosts = (*b)["resource_costs"];
            if (rcosts)
                read_resource_map(*rcosts, e.resource_build_cost,
                                  std::string("buildings.") + nt.key + ".resource_costs");
            e.era = read_era(*b, std::string("buildings.") + nt.key); // BL-433
            m_building_econ[static_cast<std::size_t>(nt.type)] = e;
        }
    }

    // BL-332 research accumulation rate (economy.military). BL-455 removed
    // military_points_per_base_tick; an economy.lua still carrying the key is
    // simply ignored, which is the same tolerance every other get_or here has.
    sol::optional<sol::table> military = (*econ)["military"];
    if (military)
    {
        military_capability_params mp;
        mp.science_per_research_institute_tick = military->get_or("science_per_research_institute_tick", mp.science_per_research_institute_tick);
        // BL-394: hire_unit's credit cost (floor + power-scaled component).
        mp.hire_base_cost      = military->get_or("hire_base_cost",      mp.hire_base_cost);
        mp.hire_cost_per_power = military->get_or("hire_cost_per_power", mp.hire_cost_per_power);

        // BL-454: standing-force upkeep (economy.military.unit_upkeep). Absent
        // table = every rate zero = the pre-BL-454 arithmetic, unchanged.
        sol::optional<sol::table> upk = (*military)["unit_upkeep"];
        if (upk)
        {
            unit_upkeep_params up;
            up.credits_per_head           = upk->get_or("credits_per_head",           up.credits_per_head);
            up.credits_per_head_per_power = upk->get_or("credits_per_head_per_power", up.credits_per_head_per_power);
            up.supply_decay_permille      = upk->get_or("supply_decay_permille",      up.supply_decay_permille);
            up.supply_recovery_permille   = upk->get_or("supply_recovery_permille",   up.supply_recovery_permille);
            up.out_of_supply_reach        = upk->get_or("out_of_supply_reach",        up.out_of_supply_reach);
            // The goods half of the cost VECTOR, authored as an ordinary
            // {resource_name = qty} table so naming the good (ordnance, rations)
            // is a data change and never a code change.
            sol::optional<sol::table> goods = (*upk)["goods_per_head"];
            if (goods)
                read_resource_map(*goods, up.goods_per_head, "military.unit_upkeep.goods_per_head");
            mp.upkeep = up;
        }

        // BL-470: per-class march points (economy.military.march_points_per_class).
        // Absent table = every class at 0 = no unit can march — the seam still
        // accepts march_unit and computes/stores the path, it just never
        // consumes it, mirroring how BL-454's upkeep landed inert at zero.
        sol::optional<sol::table> march = (*military)["march_points_per_class"];
        if (march)
        {
            auto& mpc = mp.march_points_per_class;
            mpc[static_cast<std::size_t>(unit_class::infantry)] =
                march->get_or("infantry", mpc[static_cast<std::size_t>(unit_class::infantry)]);
            mpc[static_cast<std::size_t>(unit_class::cavalry)] =
                march->get_or("cavalry", mpc[static_cast<std::size_t>(unit_class::cavalry)]);
            mpc[static_cast<std::size_t>(unit_class::ranged)] =
                march->get_or("ranged", mpc[static_cast<std::size_t>(unit_class::ranged)]);
            mpc[static_cast<std::size_t>(unit_class::siege)] =
                march->get_or("siege", mpc[static_cast<std::size_t>(unit_class::siege)]);
            mpc[static_cast<std::size_t>(unit_class::naval)] =
                march->get_or("naval", mpc[static_cast<std::size_t>(unit_class::naval)]);
        }

        m_military = mp;
    }

    // BL-350 procurement/contract tunables (economy.procurement).
    sol::optional<sol::table> procurement = (*econ)["procurement"];
    if (procurement)
    {
        procurement_params pp;
        pp.deposit_fraction        = procurement->get_or("deposit_fraction",        pp.deposit_fraction);
        pp.base_lead_ticks         = procurement->get_or("base_lead_ticks",         pp.base_lead_ticks);
        pp.reputation_floor        = procurement->get_or("reputation_floor",        pp.reputation_floor);
        pp.reputation_on_complete  = procurement->get_or("reputation_on_complete",  pp.reputation_on_complete);
        pp.reputation_on_cancel    = procurement->get_or("reputation_on_cancel",    pp.reputation_on_cancel);
        m_procurement = pp;
    }

    // BL-430 player-facing recipe-switch cost/cooldown (economy.recipe_switch).
    sol::optional<sol::table> recipe_switch = (*econ)["recipe_switch"];
    if (recipe_switch)
    {
        recipe_switch_params rs;
        rs.switch_cost    = recipe_switch->get_or("switch_cost",    rs.switch_cost);
        rs.cooldown_ticks = recipe_switch->get_or("cooldown_ticks", rs.cooldown_ticks);
        // cross_group_multiplier retired (BL-434 retraction, 2026-08-16): cross-group
        // switching is refused outright now, not priced — see recipe_switch_params.
        m_recipe_switch = rs;
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
