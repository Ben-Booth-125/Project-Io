#include "recipe_registry.hpp"

#include "resource_names.hpp"
#include "scripting/lua_state.hpp"

#include <sol/sol.hpp>

#include <cmath>     // std::isfinite -- the nation_ai loader's own rejection (Sprint N3 T1)
#include <limits>
#include <stdexcept>
#include <string>

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

/// BL-640: read an optional `baskets = { { era = "...", demand_basket = {...} }, ... }`
/// list of era-banded demand tranches onto @p dst, in authored order.
///
/// THE SAME MECHANISM RECIPES USE, not a parallel one: the band comes from
/// `read_era` above, so an unknown string ("industial") throws at load exactly as
/// it does on a recipe, and the mask applied later is `era_permits`.
///
/// STRICTER THAN A RECIPE IN ONE RESPECT, deliberately: `era` is REQUIRED on a
/// row here, where it is optional on a recipe. A recipe with no band is the
/// common case (most are shared); a row in `baskets` exists ONLY to carry a band,
/// so an absent one is a typo rather than an intent — the shared tranche is the
/// sibling `demand_basket` key, which is where a band-independent weight belongs.
/// A row with no `demand_basket` is rejected for the same reason: it asserts a
/// band and consumes nothing, which is never what was meant.
void read_era_baskets(const sol::table& tbl, std::vector<era_basket>& dst,
                      const std::string& context)
{
    sol::optional<sol::table> rows = tbl["baskets"];
    if (!rows)
        return;
    for (std::size_t i = 1; i <= rows->size(); ++i)
    {
        const std::string rc = context + ".baskets[" + std::to_string(i) + "]";
        sol::optional<sol::table> row = (*rows)[i];
        if (!row)
            throw std::runtime_error("recipe_registry: " + rc + " is not a table");
        if (!(*row)["era"].valid())
            throw std::runtime_error("recipe_registry: " + rc + " has no era "
                                     "(a banded tranche must name its band; put a "
                                     "band-independent weight in demand_basket instead)");
        era_basket b;
        b.era = read_era(*row, rc);
        sol::optional<sol::table> basket = (*row)["demand_basket"];
        if (!basket)
            throw std::runtime_error("recipe_registry: " + rc + " has no demand_basket");
        read_resource_map(*basket, b.demand_basket, rc + ".demand_basket");
        dst.push_back(std::move(b));
    }
}

/// Sprint N3 (NR-568): read an optional DECAY RATE, validated AS THE VALUE THAT
/// LANDS — a finite number in [0, 1] — and REJECTED at load otherwise, naming
/// the key. Never clamped: `decayed()` in sentiment.cpp does clamp a rate above
/// 1 as an in-process backstop, but an AUTHORED nonsense rate reaching that
/// clamp would be a silent reinterpretation, which is the thing the
/// untrusted-input rule (io-standing-rules § Determinism & data model) forbids.
/// Checked as a double BEFORE the narrowing cast, so a value that is finite as a
/// Lua number but would not be as a float (> FLT_MAX) is caught by the range
/// test rather than becoming +inf on the way in. A non-number under the key is
/// rejected too, not defaulted — a typo'd `"0.07"` is not an absence.
/// Absent is fine and keeps @p fallback (the inert zero today).
float read_unit_rate(const sol::table& tbl, const char* key, float fallback,
                     const std::string& context)
{
    const sol::object o = tbl[key];
    if (!o.valid() || o.get_type() == sol::type::none || o.get_type() == sol::type::lua_nil)
        return fallback;
    if (o.get_type() != sol::type::number)
        throw std::runtime_error(context + "." + key + " is not a number "
                                 "(expected a finite rate in [0, 1])");
    const double v = o.as<double>();
    if (!std::isfinite(v) || v < 0.0 || v > 1.0)
        throw std::runtime_error(context + "." + key + " = " + std::to_string(v)
                                 + " is not a finite rate in [0, 1]");
    return static_cast<float>(v);
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

/// Sprint N3 T1: a STRICT numeric read for `economy.nation_ai`. Unlike the
/// `get_or` reads elsewhere in this loader -- which fall back to the default on
/// a wrong type and accept any double at all -- this one rejects, naming the
/// key: a non-number, a NaN, an infinity, or a value outside [lo, hi] throws.
/// The scorer these tune is replayable only if its constants are; a NaN that
/// slipped through `get_or` would make every weight it touches NaN, and
/// `nation_budget` would then normalise a vector of NaNs into a spend nobody
/// could reproduce from the authored file. Absent keeps the default.
void read_checked(const sol::table& tbl, const char* key, float& dst, double lo, double hi,
                  const std::string& context)
{
    const sol::object o = tbl[key];
    if (!o.valid() || o.get_type() == sol::type::lua_nil)
        return;
    if (o.get_type() != sol::type::number)
        throw std::runtime_error(context + "." + key + " must be a number");
    const double v = o.as<double>();
    if (!std::isfinite(v) || v < lo || v > hi)
        throw std::runtime_error(context + "." + key + " = " + std::to_string(v)
                                 + " is outside [" + std::to_string(lo) + ", "
                                 + std::to_string(hi) + "] or not finite");
    dst = static_cast<float>(v);
}

/// Integer twin of the above: rejects a fractional value as well as a
/// non-number or an out-of-range one, so `cadence_k = 2.5` does not silently
/// become 2.
void read_checked(const sol::table& tbl, const char* key, int& dst, int lo, int hi,
                  const std::string& context)
{
    const sol::object o = tbl[key];
    if (!o.valid() || o.get_type() == sol::type::lua_nil)
        return;
    if (o.get_type() != sol::type::number)
        throw std::runtime_error(context + "." + key + " must be an integer");
    const double v = o.as<double>();
    if (!std::isfinite(v) || v != std::floor(v) || v < lo || v > hi)
        throw std::runtime_error(context + "." + key + " = " + std::to_string(v)
                                 + " must be an integer in [" + std::to_string(lo) + ", "
                                 + std::to_string(hi) + "]");
    dst = static_cast<int>(v);
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

        // BL-615: per-recipe stratum-gate radius (the heavy processor class).
        // Absent = 0 = ungated. A negative value is authored nonsense and is
        // rejected at load rather than clamped — the same refuse-don't-coerce
        // contract every other malformed-data path here holds.
        r.centre_proximity_radius = entry->get_or("centre_proximity_radius", 0);
        if (r.centre_proximity_radius < 0)
            throw std::runtime_error("recipe_registry: recipe '" + r.name
                                     + "' has a negative centre_proximity_radius");
        // BL-613: qualified-labour requirement — a fraction by contract, so it
        // rides the same validated [0, 1] reader the sentiment decay rates use
        // (reject, never clamp; absent means 0, ordinary labour only).
        r.qualified_workforce =
            read_unit_rate(*entry, "qualified_workforce", 0.0f, "recipe '" + r.name + "'");

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

    // BL-617 population-migration rates (economy.migration). Validated, never
    // clamped (the authored-rate rule the sentiment decay reader set): a
    // permille outside [0, 1000], a non-finite/negative wage weight, or a
    // selectivity below 1 (which would make emigration RAISE the origin
    // nation's qualification — the design inverted) is refused outright.
    sol::optional<sol::table> migr = (*econ)["migration"];
    if (migr)
    {
        migration_params mp;
        mp.rate_permille         = migr->get_or("rate_permille",         mp.rate_permille);
        mp.neutral_gate_permille = migr->get_or("neutral_gate_permille", mp.neutral_gate_permille);
        mp.wage_weight           = migr->get_or("wage_weight",           mp.wage_weight);
        mp.qualified_selectivity = migr->get_or("qualified_selectivity", mp.qualified_selectivity);
        if (mp.rate_permille < 0 || mp.rate_permille > 1000)
            throw std::runtime_error("economy.migration.rate_permille must be in [0, 1000]");
        if (mp.neutral_gate_permille < 0 || mp.neutral_gate_permille > 1000)
            throw std::runtime_error("economy.migration.neutral_gate_permille must be in [0, 1000]");
        if (!std::isfinite(mp.wage_weight) || mp.wage_weight < 0.0f)
            throw std::runtime_error("economy.migration.wage_weight must be finite and >= 0");
        if (!std::isfinite(mp.qualified_selectivity) || mp.qualified_selectivity < 1.0f)
            throw std::runtime_error("economy.migration.qualified_selectivity must be finite and >= 1");
        m_migration = mp;
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
        // BL-640: the era-banded tranches beside the shared basket above.
        read_era_baskets(*pop_demand, pd.baskets, "economy.population_demand");
        set_population_demand(pd); // folds the tranches under the current band
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
        read_era_baskets(*bg_demand, bd.baskets, "economy.background_demand"); // BL-640
        set_background_demand(bd);
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
            { "schooling",            building_type::schooling },            // BL-615
            { "university",           building_type::university },           // BL-615
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

            // BL-615 stratum placement gate (POPULATION.md § Strata gate
            // buildings). All three fields optional; the defaults gate nothing.
            // Out-of-range values are REJECTED at load, never clamped — the
            // scale ladder is 1..5, so a min_centre_scale outside [0,5] names
            // a stratum that does not exist.
            e.gate.requires_centre = b->get_or("requires_centre", false);
            e.gate.min_centre_scale = b->get_or("min_centre_scale", 0);
            if (e.gate.min_centre_scale < 0 || e.gate.min_centre_scale > 5)
                throw std::runtime_error(std::string("recipe_registry: buildings.") + nt.key
                                         + ".min_centre_scale is outside the 0-5 stratum ladder");
            e.gate.centre_proximity_radius = b->get_or("centre_proximity_radius", 0);
            if (e.gate.centre_proximity_radius < 0)
                throw std::runtime_error(std::string("recipe_registry: buildings.") + nt.key
                                         + ".centre_proximity_radius is negative");

            m_building_econ[static_cast<std::size_t>(nt.type)] = e;

            // BL-590: per-named-building material overrides, authored beside the
            // type's own resource_costs. extraction_site is keyed by target
            // resource NAME; processing_facility by recipe NAME (resolved to an
            // id here, while m_recipes is already loaded — recipes load before
            // economy constants, see this function's own top). An unrecognised
            // name throws the same as resource_costs' own read_resource_map does
            // — this seam must not silently drop a typo'd override.
            sol::optional<sol::table> overrides = (*b)["material_overrides"];
            if (overrides && nt.type == building_type::extraction_site)
            {
                for (const auto& kv : *overrides)
                {
                    const std::string key = kv.first.as<std::string>();
                    bool ok = false;
                    const resource_type target = resource_names::resource_from_name(key, ok);
                    if (!ok)
                        throw std::runtime_error("recipe_registry: buildings.extraction_site."
                                                 "material_overrides has an unknown target '"
                                                 + key + "'");
                    sol::table entry = kv.second.as<sol::table>();
                    std::array<float, resource_count> cost{};
                    read_resource_map(entry, cost,
                                      "buildings.extraction_site.material_overrides." + key);
                    m_extraction_material_overrides[target] = cost;
                }
            }
            else if (overrides && nt.type == building_type::processing_facility)
            {
                for (const auto& kv : *overrides)
                {
                    const std::string key = kv.first.as<std::string>();
                    const std::uint16_t rid = recipe_id(key);
                    if (rid == no_recipe)
                        throw std::runtime_error("recipe_registry: buildings.processing_facility."
                                                 "material_overrides names an unknown recipe '"
                                                 + key + "'");
                    sol::table entry = kv.second.as<sol::table>();
                    std::array<float, resource_count> cost{};
                    read_resource_map(entry, cost,
                                      "buildings.processing_facility.material_overrides." + key);
                    m_processing_material_overrides[rid] = cost;
                }
            }
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

        // BL-596: active Logistic Points (economy.military.active_lp_*).
        // Absent keys default to 0 = no active LP anywhere = every march
        // refused, mirroring how march_points_per_class's own absent-table
        // case leaves no unit able to march.
        mp.active_lp_per_anchor_tick =
            military->get_or("active_lp_per_anchor_tick", mp.active_lp_per_anchor_tick);
        mp.active_lp_credit_per_unit_distance =
            military->get_or("active_lp_credit_per_unit_distance", mp.active_lp_credit_per_unit_distance);

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
        // BL-392 commitment terms.
        pp.volume_discount_max            = procurement->get_or("volume_discount_max",            pp.volume_discount_max);
        pp.volume_discount_half_quantity  = procurement->get_or("volume_discount_half_quantity",  pp.volume_discount_half_quantity);
        pp.offbody_freight_fraction       = procurement->get_or("offbody_freight_fraction",       pp.offbody_freight_fraction);
        m_procurement = pp;
    }

    // BL-628 whole-firm acquisition (economy.acquisition).
    //
    // One key, read through `read_checked` and REJECTED by name on violation
    // rather than clamped — the seam's untrusted-input rule applied at the
    // authoring boundary, as economy.nation_ai already does. The domain is the
    // honest one: a multiple is a count of quarters' earnings, so it is
    // non-negative and finite; the upper bound is left open because a high
    // multiple is a tuning choice, not a malformed value. A NEGATIVE multiple
    // would invert the profit term (a profitable firm made cheap by its own
    // earnings) and is refused rather than accepted as an exotic setting.
    sol::optional<sol::table> acquisition_tbl = (*econ)["acquisition"];
    if (acquisition_tbl)
    {
        acquisition_params ap = m_acquisition;
        read_checked(*acquisition_tbl, "multiple", ap.multiple,
                     0.0, std::numeric_limits<double>::infinity(), "economy.acquisition");
        m_acquisition = ap;
    }

    // BL-545/BL-546 relational substrate (economy.sentiment).
    //
    // The two `contract_*` Trust weights are SEEDED FROM PROCUREMENT FIRST, so
    // a build that authors `procurement` and nothing else keeps exactly the
    // reputation behaviour it had before the migration. `economy.sentiment`,
    // where it exists, then overrides — a factor row it does not name keeps the
    // seeded (or zero) value, so authoring one weight cannot silently zero the
    // rest.
    //
    // The factor table is read AS A LOOP OVER THE ROSTER, never ten branches:
    // `sentiment_factor_names` is indexed by the kind, so naming a new factor
    // costs an enumerator and a string and nothing here changes.
    seed_procurement_sentiment(m_sentiment, m_procurement);
    sol::optional<sol::table> sentiment_tbl = (*econ)["sentiment"];
    if (sentiment_tbl)
    {
        sentiment_params sp = m_sentiment;
        // The two rates are validated as the value that lands (finite, in
        // [0, 1]) and REJECTED at load otherwise — see read_unit_rate. NR-568
        // authored them at 1 - 2^(-1/9) (a nine-quarter half-life).
        sp.access_decay_per_tick = read_unit_rate(*sentiment_tbl, "access_decay_per_tick",
                                                  sp.access_decay_per_tick, "economy.sentiment");
        sp.trust_decay_per_tick  = read_unit_rate(*sentiment_tbl, "trust_decay_per_tick",
                                                  sp.trust_decay_per_tick,  "economy.sentiment");
        sp.neutral_epsilon       = sentiment_tbl->get_or("neutral_epsilon",       sp.neutral_epsilon);
        sp.limit                 = sentiment_tbl->get_or("limit",                 sp.limit);
        sp.band_edge_near        = sentiment_tbl->get_or("band_edge_near",        sp.band_edge_near);
        sp.band_edge_far         = sentiment_tbl->get_or("band_edge_far",         sp.band_edge_far);

        sol::optional<sol::table> factors = (*sentiment_tbl)["factors"];
        if (factors)
        {
            for (std::size_t i = 0; i < sentiment_factor_count; ++i)
            {
                sol::optional<sol::table> row = (*factors)[sentiment_factor_names[i]];
                if (!row)
                    continue;
                sp.factors[i].access = row->get_or("access", sp.factors[i].access);
                sp.factors[i].trust  = row->get_or("trust",  sp.factors[i].trust);
            }
        }

        m_sentiment = sp;
    }

    // BL-542 nation-scorer tunables (economy.nation_ai), Sprint N3 T1.
    //
    // Field by field over the struct's own defaults, so authoring one number
    // does not zero the rest. Every key is range-checked through `read_checked`
    // and rejected by name on violation -- never clamped. The domain is the
    // honest one: `cadence_k` is a modulus (>= 1); every float is a scale,
    // weight, floor or bias the scorer only ever multiplies or divides by, and
    // none has a negative meaning (the divisors already substitute 1.0 for a
    // non-positive value in nation_ai.cpp, so the reject here is the loud
    // version of a guard the scorer carries quietly). Upper bound is left open
    // except for the four [0, 1] fractions.
    sol::optional<sol::table> nation_ai_tbl = (*econ)["nation_ai"];
    if (nation_ai_tbl)
    {
        const std::string ctx = "economy.nation_ai";
        const double      inf = std::numeric_limits<double>::infinity();
        nation_ai_params  np  = m_nation_ai;
        const sol::table& t   = *nation_ai_tbl;

        read_checked(t, "cadence_k", np.cadence_k, 1, 1048576, ctx);

        read_checked(t, "lack_scale",  np.lack_scale,  0.0, inf, ctx);
        read_checked(t, "price_floor", np.price_floor, 0.0, inf, ctx);
        read_checked(t, "niche_scale", np.niche_scale, 0.0, inf, ctx);
        read_checked(t, "gap_scale",   np.gap_scale,   0.0, inf, ctx);

        read_checked(t, "force_scale",         np.force_scale,         0.0, inf, ctx);
        read_checked(t, "hostility_amplifier", np.hostility_amplifier, 0.0, inf, ctx);
        read_checked(t, "deterrence_scale",    np.deterrence_scale,    0.0, inf, ctx);
        read_checked(t, "threat_scale",        np.threat_scale,        0.0, inf, ctx);

        read_checked(t, "niche_grudge_bias",        np.niche_grudge_bias,        0.0, 1.0, ctx);
        read_checked(t, "conflict_grudge_bias",     np.conflict_grudge_bias,     0.0, 1.0, ctx);
        read_checked(t, "grudge_border_saturation", np.grudge_border_saturation, 0.0, inf, ctx);
        read_checked(t, "grudge_border_weight",     np.grudge_border_weight,     0.0, inf, ctx);
        read_checked(t, "grudge_posture_weight",    np.grudge_posture_weight,    0.0, inf, ctx);

        read_checked(t, "base_weight",       np.base_weight,       0.0, inf, ctx);
        read_checked(t, "niche_charters",    np.niche_charters,    0.0, inf, ctx);
        read_checked(t, "niche_works",       np.niche_works,       0.0, inf, ctx);
        read_checked(t, "niche_logistics",   np.niche_logistics,   0.0, inf, ctx);
        read_checked(t, "gap_exploration",   np.gap_exploration,   0.0, inf, ctx);
        read_checked(t, "gap_academic",      np.gap_academic,      0.0, inf, ctx);
        read_checked(t, "calm_schooling",    np.calm_schooling,    0.0, inf, ctx);
        read_checked(t, "calm_academic",     np.calm_academic,     0.0, inf, ctx);
        read_checked(t, "calm_works",        np.calm_works,        0.0, inf, ctx);
        read_checked(t, "threat_contracted", np.threat_contracted, 0.0, inf, ctx);
        read_checked(t, "threat_reserve",    np.threat_reserve,    0.0, inf, ctx);
        read_checked(t, "threat_milres",     np.threat_milres,     0.0, inf, ctx);

        read_checked(t, "base_reserve",       np.base_reserve,       0.0, 1.0, ctx);
        read_checked(t, "calm_reserve_bonus", np.calm_reserve_bonus, 0.0, 1.0, ctx);

        m_nation_ai = np;
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
