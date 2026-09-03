// recipe_margin — does every recipe pay at BASE price? (BL-744, sprint 31)
//
// Ben, 2026-09-02: "the simplest way to do this is to ensure that all recipes
// (at base price) make a greater profit than marginal costs." This harness is
// that sentence as arithmetic, over the AUTHORED tables and at AUTHORING TIME —
// no world, no clearing tick, no RNG. It reads scripts/recipes.lua,
// scripts/economy.lua and scripts/world_gen.lua through a live Lua state exactly
// as app::load_economy does (so it is in the NEEDS_LUA class — build it with
// `cmd //c tools\verify\build_lua_harness.bat recipe_margin`, or through CMake),
// then prices every processing recipe and every extraction target in BOTH era
// bands. docs/economy/PRODUCTION.md § The recipe margin anchor is the design.
//
// WHY AUTHORING TIME AND NOT THE SIM. tier_margin (BL-436) already measures what
// a building EARNS in the generated world, at live prices, and the sweeps
// (BL-723..729) measure whole fields. Both answer "what happens". Neither answers
// "is the recipe authored to be able to pay at all" — and a recipe that cannot
// clear its own marginal cost at base price is unprofitable BY AUTHORING, so no
// demand knob, band, or scorer change can rescue it. That is the anchor every
// other lever sits on, and it is cheap enough to check on every edit of the three
// tables. The 2026-09-01 field ledger found maintenance is 80% of every credit
// leaving the field against a market minting 1,485/qtr vs 2,241/qtr of drains;
// this is the per-recipe statement of that gap.
//
// TWO HALVES, because base is the MIDDLE of a band, not the price. Resolved
// prices run from floor_mult (0.25x) to ceil_mult (10x) of base
// (docs/economy/MARKETS.md § Price resolution), so a recipe positive at base can
// still lose under glut. The robust rule is therefore:
//
//   M1  MARGINAL, at base.  Per batch (per unit for extraction):
//         revenue - marginal_cost  >=  k * marginal_cost
//       where marginal_cost = inputs at base + wage per batch, and
//       k = economy.recipe_margin_anchor.profit_over_marginal (authored DATA;
//       k = 1.0 is Ben's sentence verbatim — profit at least equal to marginal
//       cost, i.e. revenue at least twice it). Wage per batch is
//       base_wage / base_rate: both wages and output scale linearly with the
//       assigned workforce (economy_system.cpp's processing pass and
//       budget_system.cpp's compute_building_opex), so the per-batch figure is
//       staffing-independent at habitability 1 and contention 1.
//
//   M2  FIXED COST AT THE FLOOR — BL-740's form (Ben, 2026-09-01: "the WORST
//       legal market still pays the lights"). Per tick at the typical staffing
//       W = economy.recipe_margin_anchor.typical_workforce (0.5, the value
//       corporation_generation.cpp seeds every generated building with):
//         (revenue - inputs) * floor_mult * batches_per_tick
//           - wages_per_tick - maintenance - goods_upkeep_at_base  >=  0
//       Inputs and outputs BOTH at the floor: a glut market has everything cheap,
//       and pricing inputs at base against a floored output would be a scenario
//       the band cannot produce.
//       maintenance is the authored constant at workforce_target 100 (material
//       floor + labour share = 1.0 x maintenance, compute_building_opex);
//       goods_upkeep is the band's per-type basket (economy.building_upkeep.goods)
//       valued at base — power's 0.25/0.40 per tick is the first non-zero entry.
//
// THE ANCHOR ROUTE AND THE ALTERNATES (NR-780). A good with several in-band
// routes is priced off its CHEAPEST route — lowest marginal cost per unit of
// primary output — and that route must clear k. Every other route must clear
// k_alt = economy.recipe_margin_anchor.alternate_profit_over_marginal (0.0:
// profitable at base, no more) and M2 regardless. Demanding k of every route at
// one price would force every route to the same input cost, which erases the
// point of alternate methods. The anchor route is marked `*` in the table.
//
// ONE PRICE TABLE FOR BOTH BANDS (Ben, 2026-09-02, overturning NR-778). A
// per-band override was tried when the ancient bloom chain priced its own steel
// at 113; the ruling was to shorten that chain to depth one instead (the
// Bloomery, recipes.lua) so one table clears the anchor in both bands. A good's
// price must satisfy the LARGER of the two bands' anchor-route needs; both
// bands are checked below against the same table.
//
// WHAT IS EXEMPT, AND SAYS SO. A recipe whose every output carries base_price 0
// (propellant: "consumed by the Launchpad, never sold", RESOURCES.md) has no
// market margin to anchor and is listed as UNPRICED rather than failed. A recipe
// with an UNPRICED INPUT is a defect, not an exemption — the input cannot be
// bought at any price — and R5 fails on it.
//
// ROWS.
//   R0  Non-vacuous: both bands loaded recipes, the anchor table was read, the
//       priced-resource count is printed (the world_gen fallback prices 10 of 47;
//       the authored table 42 — a run that saw 10 measured the wrong world).
//   R1  M1 holds for every priced processing recipe, per band.
//   R2  M2 holds for every priced processing recipe, per band.
//   R3  M1 holds for every priced extraction target, per band.
//   R4  M2 holds for every priced extraction target, per band.
//   R5  Every input of an in-band recipe carries a base price.
//   R6  Differential red-proof: the evaluator flips on a zero-revenue row, on a
//       fixed cost no floor can cover, and passes a plainly profitable one — so
//       a green R1-R4 is a statement about the tables, not about the checker.
//
// R1-R4 FAILED the day this was written - 41 of 44 priced recipes - and that
// table is the finding sprint 31 opened on. The retune that turned them green
// (BL-744 stage 2, BL-740) re-priced the roster per band; the harness is
// registered with ctest like every other (script-rooted, since it loads the
// scripts by relative path) and any table edit that breaks the anchor is red
// at the gate.
//
// Run: .\build_gen\verify\recipe_margin.exe   (from the repo root)

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/placement_rules.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/world_gen_config.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

const char* band_name(era_band b)
{
    switch (b)
    {
    case era_band::ancient:    return "ancient";
    case era_band::industrial: return "industrial";
    default:                   return "any";
    }
}

/// The anchor's authored knobs, read from economy.recipe_margin_anchor.
struct anchor_params
{
    double profit_over_marginal           = 0.0; ///< k in M1, the anchor route.
    double alternate_profit_over_marginal = 0.0; ///< k_alt in M1, every other route.
    double typical_workforce              = 0.0; ///< W in M2.
    bool   ok                   = false;
};

/// One evaluated row — a recipe in a band, or an extraction target in a band.
/// Everything the two halves need, kept as plain numbers so the red-proof (R6)
/// can drive the evaluator with values the tables do not contain.
struct row_eval
{
    double revenue   = 0.0; ///< per batch / per unit, at base
    double inputs    = 0.0; ///< per batch, at base
    double wage_pb   = 0.0; ///< wage per batch / per unit
    double mc        = 0.0; ///< marginal cost = inputs + wage_pb
    double margin    = 0.0; ///< revenue - mc
    double ratio     = 0.0; ///< margin / mc (revenue/mc - 1); +inf when mc == 0
    bool   m1        = false;
    double fixed     = 0.0; ///< maintenance + goods upkeep at base, per tick
    double wages_pt  = 0.0; ///< wages per tick at W
    double base_net  = 0.0; ///< per tick at base (information)
    double floor_net = 0.0; ///< per tick at the floor (M2's quantity)
    bool   m2        = false;
};

row_eval evaluate(double revenue, double inputs, double wage_pb,
                  double units_per_tick, double wages_pt, double fixed,
                  double floor_mult, double k)
{
    row_eval e;
    e.revenue  = revenue;
    e.inputs   = inputs;
    e.wage_pb  = wage_pb;
    e.mc       = inputs + wage_pb;
    e.margin   = revenue - e.mc;
    e.ratio    = (e.mc > 0.0) ? e.margin / e.mc : (revenue > 0.0 ? INFINITY : 0.0);
    e.m1       = (e.mc > 0.0) ? (e.margin >= k * e.mc) : (revenue > 0.0);
    e.fixed    = fixed;
    e.wages_pt = wages_pt;
    e.base_net  = (revenue - inputs) * units_per_tick - wages_pt - fixed;
    e.floor_net = (revenue - inputs) * floor_mult * units_per_tick - wages_pt - fixed;
    e.m2        = e.floor_net >= 0.0;
    return e;
}

double basket_value(const std::array<float, resource_count>& basket,
                    const std::array<float, resource_count>& price)
{
    double v = 0.0;
    for (std::size_t r = 0; r < resource_count; ++r)
        v += static_cast<double>(basket[r]) * static_cast<double>(price[r]);
    return v;
}

void print_header()
{
    std::printf("  %-26s %-16s %8s %8s %7s %8s %7s %3s | %8s %9s %9s %3s\n",
                "recipe", "group", "rev", "inputs", "wage/b", "margin", "ratio", "M1",
                "fixed/t", "base/t", "floor/t", "M2");
}

void print_row(const std::string& name, const std::string& group, const row_eval& e)
{
    char ratio[16];
    if (std::isinf(e.ratio))
        std::snprintf(ratio, sizeof ratio, "%7s", "inf");
    else
        std::snprintf(ratio, sizeof ratio, "%7.2f", e.ratio);
    std::printf("  %-26s %-16s %8.2f %8.2f %7.2f %8.2f %s %3s | %8.2f %9.2f %9.2f %3s\n",
                name.c_str(), group.c_str(), e.revenue, e.inputs, e.wage_pb, e.margin,
                ratio, e.m1 ? "ok" : "RED", e.fixed, e.base_net, e.floor_net,
                e.m2 ? "ok" : "RED");
}

} // namespace

int main()
{
    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    const world_gen_config gen_cfg = parsed_gen_config(lua);
    const std::array<float, resource_count>& price_shared = gen_cfg.kepler_base_price;

    // The standing vacuity guard: a registry that loaded nothing would report
    // every recipe as costless and pass.
    if (reg.recipe_count() == 0)
    {
        std::printf("FATAL: no recipes loaded — run from the repo root.\n");
        return 2;
    }

    anchor_params anchor;
    {
        sol::state& L = lua.state();
        sol::optional<sol::table> econ = L["economy"];
        if (econ)
        {
            sol::optional<sol::table> a = (*econ)["recipe_margin_anchor"];
            if (a)
            {
                sol::optional<double> k  = (*a)["profit_over_marginal"];
                sol::optional<double> ka = (*a)["alternate_profit_over_marginal"];
                sol::optional<double> w  = (*a)["typical_workforce"];
                if (k && ka && w && std::isfinite(*k) && std::isfinite(*ka) && std::isfinite(*w)
                    && *k >= 0.0 && *ka >= 0.0 && *w > 0.0)
                {
                    anchor.profit_over_marginal           = *k;
                    anchor.alternate_profit_over_marginal = *ka;
                    anchor.typical_workforce              = *w;
                    anchor.ok                             = true;
                }
            }
        }
    }
    if (!anchor.ok)
    {
        std::printf("FATAL: economy.recipe_margin_anchor is missing or malformed "
                    "(needs finite profit_over_marginal >= 0, alternate_profit_over_marginal >= 0, typical_workforce > 0).\n");
        return 2;
    }

    const double k          = anchor.profit_over_marginal;
    const double k_alt      = anchor.alternate_profit_over_marginal;
    const double W          = anchor.typical_workforce;
    const double floor_mult = reg.price_band().floor_mult;

    int priced = 0;
    for (std::size_t r = 0; r < resource_count; ++r)
        if (price_shared[r] > 0.0f)
            ++priced;

    std::printf("=== recipe_margin (BL-744) — every recipe at BASE price ===\n");
    std::printf("priced resources: %d of %zu%s\n", priced, resource_count,
                gen_cfg.is_fallback ? "   <-- FALLBACK TABLE, not the authored one" : "");
    std::printf("anchor: k = %.2f on the anchor route (*), k_alt = %.2f on alternates "
                "(M1: margin >= k x marginal cost), typical_workforce W = %.2f, floor_mult = %.2f\n\n",
                k, k_alt, W, floor_mult);

    const era_band bands[] = { era_band::ancient, era_band::industrial };

    struct band_tally
    {
        int recipes = 0, priced_recipes = 0, unpriced_out = 0, unpriced_in = 0;
        int m1_fail = 0, m2_fail = 0;
        int ext = 0, ext_m1_fail = 0, ext_m2_fail = 0;
        std::vector<std::string> m1_red, m2_red, ext_m1_red, ext_m2_red, unpriced_inputs;
        int at_k[4] = { 0, 0, 0, 0 }; ///< recipes clearing k' in {0, 0.5, 1, 2}
    };
    const double k_ladder[4] = { 0.0, 0.5, 1.0, 2.0 };
    band_tally tally[2];

    for (int bi = 0; bi < 2; ++bi)
    {
        const era_band band = bands[bi];
        band_tally&    t    = tally[bi];
        reg.set_era(band);
        // One table for both bands (Ben, 2026-09-02, overturning NR-778).
        const std::array<float, resource_count>& price = price_shared;

        // --- processing ---------------------------------------------------
        const building_economics& pe = reg.economics(building_type::processing_facility);
        const auto pbasket = building_upkeep_goods(reg.building_upkeep(),
                                                   building_type::processing_facility, band);
        const double p_fixed = static_cast<double>(pe.maintenance) + basket_value(pbasket, price);
        const double p_wage_pb = (pe.base_rate > 0.0f)
            ? static_cast<double>(pe.base_wage) / static_cast<double>(pe.base_rate) : 0.0;

        std::printf("--- band %s — processing (base_rate %.2f batches/tick at W=1, "
                    "maintenance %.2f, wage %.2f, goods upkeep %.2f/tick) ---\n",
                    band_name(band), pe.base_rate, pe.maintenance, pe.base_wage,
                    basket_value(pbasket, price));
        print_header();

        const int n = reg.recipe_count(building_type::processing_facility);

        // The anchor route per primary output: lowest marginal cost per unit of
        // that output among the band's routes; ties go to the earlier recipe.
        std::array<int, resource_count>    anchor_of;
        std::array<double, resource_count> anchor_mc;
        anchor_of.fill(-1);
        anchor_mc.fill(0.0);
        for (int i = 0; i < n; ++i)
        {
            const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
            const resource_type po = primary_output_resource(rc);
            const std::size_t   pi = static_cast<std::size_t>(po);
            if (rc.outputs[pi] <= 0.0f || price[pi] <= 0.0f)
                continue;
            double inp = 0.0;
            for (std::size_t r = 0; r < resource_count; ++r)
                if (rc.inputs[r] > 0.0f)
                    inp += static_cast<double>(rc.inputs[r]) * price[r];
            const double mc_per_unit = (inp + p_wage_pb) / static_cast<double>(rc.outputs[pi]);
            if (anchor_of[pi] < 0 || mc_per_unit < anchor_mc[pi])
            {
                anchor_of[pi] = i;
                anchor_mc[pi] = mc_per_unit;
            }
        }

        for (int i = 0; i < n; ++i)
        {
            const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
            ++t.recipes;
            // A recipe whose primary output is an EXTRACTABLE raw (hydroponics ->
            // agricultural_produce) is never the anchor: the extraction site is
            // that good's cheapest route and prices it, so the recipe is an
            // alternate to the Farm and clears k_alt.
            bool primary_is_raw = false;
            for (const resource_type x : placement_rules::k_extractable)
                if (x == primary_output_resource(rc))
                    primary_is_raw = true;
            const bool   is_anchor = !primary_is_raw
                && anchor_of[static_cast<std::size_t>(primary_output_resource(rc))] == i;
            const double kk        = is_anchor ? k : k_alt;

            double rev = 0.0, inp = 0.0;
            bool   any_out_priced = false, any_out = false;
            for (std::size_t r = 0; r < resource_count; ++r)
            {
                if (rc.outputs[r] > 0.0f)
                {
                    any_out = true;
                    if (price[r] > 0.0f)
                    {
                        any_out_priced = true;
                        rev += static_cast<double>(rc.outputs[r]) * price[r];
                    }
                }
                if (rc.inputs[r] > 0.0f)
                {
                    if (price[r] > 0.0f)
                        inp += static_cast<double>(rc.inputs[r]) * price[r];
                    else
                    {
                        ++t.unpriced_in;
                        t.unpriced_inputs.push_back(rc.name + " <- " + resource_names::name_of(
                            static_cast<resource_type>(r)));
                    }
                }
            }
            if (!any_out || !any_out_priced)
            {
                ++t.unpriced_out;
                std::printf("  %-26s %-16s %8s  (every output unpriced — exempt)\n",
                            rc.name.c_str(), rc.group.c_str(), "-");
                continue;
            }
            ++t.priced_recipes;

            const double batches = static_cast<double>(pe.base_rate) * W;
            const double wages   = W * static_cast<double>(pe.base_wage);
            const row_eval e = evaluate(rev, inp, p_wage_pb, batches, wages, p_fixed,
                                        floor_mult, kk);
            print_row((is_anchor ? "*" : " ") + rc.name, rc.group, e);
            if (!e.m1) { ++t.m1_fail; t.m1_red.push_back(rc.name); }
            if (!e.m2) { ++t.m2_fail; t.m2_red.push_back(rc.name); }
            if (is_anchor)
                for (int j = 0; j < 4; ++j)
                    if (e.mc > 0.0 ? e.margin >= k_ladder[j] * e.mc : e.revenue > 0.0)
                        ++t.at_k[j];
        }

        // --- extraction ---------------------------------------------------
        if (reg.building_available(building_type::extraction_site))
        {
            const building_economics& xe = reg.economics(building_type::extraction_site);
            const auto xbasket = building_upkeep_goods(reg.building_upkeep(),
                                                       building_type::extraction_site, band);
            const double x_fixed = static_cast<double>(xe.maintenance) + basket_value(xbasket, price);
            const double x_wage_pu = (xe.base_rate > 0.0f)
                ? static_cast<double>(xe.base_wage) / static_cast<double>(xe.base_rate) : 0.0;
            std::printf("--- band %s — extraction at richness 1 (base_rate %.2f units/tick at W=1, "
                        "maintenance %.2f, wage %.2f, goods upkeep %.2f/tick) ---\n",
                        band_name(band), xe.base_rate, xe.maintenance, xe.base_wage,
                        basket_value(xbasket, price));
            print_header();
            for (const resource_type r : placement_rules::k_extractable)
            {
                const std::size_t ri = static_cast<std::size_t>(r);
                if (price[ri] <= 0.0f)
                    continue; // unpriced raw: nothing to anchor (reported by the census)
                ++t.ext;
                const double units = static_cast<double>(xe.base_rate) * W;
                const double wages = W * static_cast<double>(xe.base_wage);
                const row_eval e = evaluate(price[ri], 0.0, x_wage_pu, units, wages, x_fixed,
                                            floor_mult, k);
                const std::string name = resource_names::name_of(r);
                print_row(name, "extraction", e);
                if (!e.m1) { ++t.ext_m1_fail; t.ext_m1_red.push_back(name); }
                if (!e.m2) { ++t.ext_m2_fail; t.ext_m2_red.push_back(name); }
            }
        }
        std::printf("\n");
    }

    auto join = [](const std::vector<std::string>& v) {
        std::string s;
        for (std::size_t i = 0; i < v.size(); ++i)
            s += (i ? ", " : "") + v[i];
        return s.empty() ? std::string("-") : s;
    };

    // --- summary --------------------------------------------------------------
    std::printf("=== summary ===\n");
    for (int bi = 0; bi < 2; ++bi)
    {
        const band_tally& t = tally[bi];
        std::printf("band %-10s recipes %2d (priced %2d, unpriced-output %d)  M1 red %2d  M2 red %2d "
                    "| extraction targets %2d  M1 red %2d  M2 red %2d\n",
                    band_name(bands[bi]), t.recipes, t.priced_recipes, t.unpriced_out,
                    t.m1_fail, t.m2_fail, t.ext, t.ext_m1_fail, t.ext_m2_fail);
        std::printf("  anchor routes clearing margin >= k' x marginal cost:  k'=0: %d  k'=0.5: %d  "
                    "k'=1: %d  k'=2: %d  (of %d priced recipes)\n",
                    t.at_k[0], t.at_k[1], t.at_k[2], t.at_k[3], t.priced_recipes);
        std::printf("  M1 red: %s\n  M2 red: %s\n  extraction M1 red: %s\n  extraction M2 red: %s\n",
                    join(t.m1_red).c_str(), join(t.m2_red).c_str(),
                    join(t.ext_m1_red).c_str(), join(t.ext_m2_red).c_str());
        if (!t.unpriced_inputs.empty())
            std::printf("  UNPRICED INPUTS: %s\n", join(t.unpriced_inputs).c_str());
    }
    std::printf("\n");

    // --- R0 -------------------------------------------------------------------
    check(tally[0].recipes > 0 && tally[1].recipes > 0,
          "R0: both bands loaded recipes (ancient " + std::to_string(tally[0].recipes) +
          ", industrial " + std::to_string(tally[1].recipes) + ")");
    check(!gen_cfg.is_fallback && priced > 10,
          "R0: the AUTHORED base_price table was read (" + std::to_string(priced) + " priced)");
    check(anchor.ok, "R0: economy.recipe_margin_anchor read (k=" + std::to_string(k) +
          ", k_alt=" + std::to_string(k_alt) + ", W=" + std::to_string(W) + ")");

    // --- R1..R4 ---------------------------------------------------------------
    for (int bi = 0; bi < 2; ++bi)
    {
        const band_tally& t = tally[bi];
        const std::string b = band_name(bands[bi]);
        check(t.priced_recipes > 0 && t.m1_fail == 0,
              "R1 [" + b + "]: every priced processing recipe clears M1 (anchor route k=" +
              std::to_string(k) + ", alternates k=" + std::to_string(k_alt) + ") — red: " +
              std::to_string(t.m1_fail));
        check(t.priced_recipes > 0 && t.m2_fail == 0,
              "R2 [" + b + "]: every priced processing recipe covers its fixed cost at the floor "
              "(M2, W=" + std::to_string(W) + ") — red: " + std::to_string(t.m2_fail));
        check(t.ext > 0 && t.ext_m1_fail == 0,
              "R3 [" + b + "]: every priced extraction target clears M1 — red: " +
              std::to_string(t.ext_m1_fail));
        check(t.ext > 0 && t.ext_m2_fail == 0,
              "R4 [" + b + "]: every priced extraction target covers its fixed cost at the floor "
              "(M2) — red: " + std::to_string(t.ext_m2_fail));
    }

    // --- R5 -------------------------------------------------------------------
    check(tally[0].unpriced_in == 0 && tally[1].unpriced_in == 0,
          "R5: every input of an in-band recipe carries a base price (unpriced: ancient " +
          std::to_string(tally[0].unpriced_in) + ", industrial " +
          std::to_string(tally[1].unpriced_in) + ")");

    // --- R6: the evaluator is not a constant ----------------------------------
    {
        const row_eval dead = evaluate(0.0, 5.0, 1.5, 4.0, 6.0, 10.0, floor_mult, k);
        const row_eval rich = evaluate(1000.0, 5.0, 1.5, 4.0, 6.0, 10.0, floor_mult, k);
        const row_eval lights_off = evaluate(1000.0, 5.0, 1.5, 4.0, 6.0, 1.0e9, floor_mult, k);
        check(!dead.m1 && !dead.m2, "R6: a zero-revenue row fails BOTH halves");
        check(rich.m1 && rich.m2, "R6: a plainly profitable row passes BOTH halves");
        check(lights_off.m1 && !lights_off.m2,
              "R6: a fixed cost no floor can cover fails M2 alone (M1 is per-batch)");
        // The two halves disagree on the right thing: M2 gets STRICTER as the
        // floor drops, M1 does not move with it.
        const row_eval lower_floor = evaluate(rich.revenue, rich.inputs, rich.wage_pb, 4.0, 6.0,
                                              10.0, floor_mult * 0.5, k);
        check(lower_floor.floor_net < rich.floor_net && lower_floor.m1 == rich.m1,
              "R6: halving floor_mult lowers floor_net and leaves M1 untouched");
    }

    std::printf("\n=== %s (%d failure%s) ===\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
