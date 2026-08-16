// tier_margin — does refining actually pay? (BL-436)
//
// The economy's whole tier model says margin WIDENS as you go up it: RESOURCES.md
// promises Tier 3 the "widest price divergence", BL-340 priced the chain that way
// on purpose (spacecraft_components at 56x iron ore), and PRODUCTION.md's
// processing chain is the value-add step the Trade pillar rests on. Chain depth
// (BL-428) then asks the player to CLIMB that chain as their growth track.
//
// BL-435 measured the opposite by accident. Swapping one extraction site for one
// processing facility on the same corp, same seed, halved its balance, and the
// same swap across the generated economy moved ai_skill_harness's five seeds by
// -9% to -71% on final net worth. This harness turns that accident into a
// standing check, and answers the question BL-436 could not: WHY.
//
// WHAT IT MEASURES, and why this shape. It runs the REAL generated world with the
// REAL Lua economy and reads `estimate_building_profit` — the same per-building
// breakdown the Profitability page shows a player — so what it reports is what
// the game itself believes, not a parallel model that could be wrong in its own
// interesting way. Per building type it reports mean per-tick revenue, input
// cost, maintenance, wages and net, plus capex payback.
//
// The four candidate causes BL-436 records are each separately visible in that
// breakdown, which is the point of not collapsing it to one number:
//
//   1. input cost vs output price   -> the revenue/input_cost columns
//   2. wages and maintenance        -> those two columns, against revenue
//   3. market depth for the inputs  -> the starved/idle count (a processor that
//                                      cannot buy inputs earns nothing while
//                                      still paying wages and maintenance)
//   4. throughput                   -> revenue per building against base_rate
//
// R1 is deliberately a REPORT, not a threshold: this harness exists to find the
// cause, and a pass/fail bar picked before the cause is known would be a guess
// wearing a test's clothes. R2 is the real assertion and it is the one BL-436
// closes on.
//
// Run: .\build\tier_margin.exe [seeds] [ticks]

#include "scripting/lua_state.hpp"
#include "world/budget_system.hpp"
#include "world/building_profit.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

int g_failures = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what.c_str());
    if (!ok)
        ++g_failures;
}

/// Running totals for one building_type across every sampled building-tick.
struct tier_acc
{
    double revenue = 0.0, input_cost = 0.0, maintenance = 0.0, wages = 0.0;
    long   samples = 0;   ///< building-ticks with a profit row.
    long   earning = 0;   ///< building-ticks with revenue > 0.
    double capex   = 0.0; ///< build_cost, for payback.

    double net()      const { return revenue - input_cost - maintenance - wages; }
    double per_tick() const { return samples ? net() / static_cast<double>(samples) : 0.0; }
};

void seed_default_recipes(world& w, const recipe_registry& reg)
{
    const entity_id steel = reg.recipe_id("steel");
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = static_cast<uint16_t>(steel);
}

const char* type_name(building_type t)
{
    switch (t)
    {
        case building_type::extraction_site:     return "extraction_site";
        case building_type::processing_facility: return "processing_facility";
        default:                                 return nullptr; // not a producer; skipped
    }
}

} // namespace

int main(int argc, char** argv)
{
    const int n_seeds = (argc > 1) ? std::atoi(argv[1]) : 3;
    const int n_ticks = (argc > 2) ? std::atoi(argv[2]) : 20;
    if (n_seeds <= 0 || n_ticks <= 0)
    {
        std::printf("usage: %s [seeds] [ticks]  (both positive)\n", argv[0]);
        return 2;
    }

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    recipe_registry reg;
    reg.load_from_lua(lua);
    // The standing vacuity guard (interbody_pull_harness's lesson): a registry
    // that loaded nothing would report every tier as equally worthless, which is
    // a clean-looking answer about an economy that was not there.
    if (reg.recipe_count(building_type::processing_facility) == 0)
    {
        std::printf("FATAL: no recipes loaded — run from the repo root.\n");
        return 2;
    }

    std::printf("=== tier_margin (BL-436) — does refining pay? ===\n");
    std::printf("%d seeds x %d ticks, real generated world, real Lua economy\n\n",
                n_seeds, n_ticks);

    // The authored inputs, printed up front: half of BL-436's candidate causes
    // are visible here before a single tick runs.
    const building_economics& ex = reg.economics(building_type::extraction_site);
    const building_economics& pr = reg.economics(building_type::processing_facility);
    std::printf("authored economics       base_rate  maint   wage  build_cost\n");
    std::printf("  extraction_site        %8.1f  %5.1f  %5.1f  %10.1f\n",
                static_cast<double>(ex.base_rate), static_cast<double>(ex.maintenance),
                static_cast<double>(ex.base_wage), static_cast<double>(ex.build_cost));
    std::printf("  processing_facility    %8.1f  %5.1f  %5.1f  %10.1f\n\n",
                static_cast<double>(pr.base_rate), static_cast<double>(pr.maintenance),
                static_cast<double>(pr.base_wage), static_cast<double>(pr.build_cost));

    tier_acc extraction, processing;
    extraction.capex = ex.build_cost;
    processing.capex = pr.build_cost;

    for (int s = 0; s < n_seeds; ++s)
    {
        world_params p = no_prehistory();
        p.seed = static_cast<uint32_t>(s);
        world w = make_hard_coded_world(p);
        seed_default_recipes(w, reg);
        generate_background_firms(w, reg, static_cast<uint32_t>(s) ^ 0x8A21F00Du);

        for (int t = 1; t <= n_ticks; ++t)
        {
            dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                             reg.logistics_cost(convoy_mode::space));
            advance_convoys(w);
            const economy_report report = run_economy_step(w, reg);
            const auto flows = clear_markets(w, reg, report);
            apply_budget(w, reg, flows, report.workforce_contention, nullptr);
            credit_arrived_convoys(w, t);

            for (const auto& [bid, b] : w.buildings)
            {
                const char* tn = type_name(b.type);
                if (tn == nullptr)
                    continue;
                const building_profit bp = estimate_building_profit(w, reg, report, bid);
                if (!bp.has_data)
                    continue;
                tier_acc& acc = (b.type == building_type::extraction_site) ? extraction
                                                                          : processing;
                acc.revenue     += bp.revenue;
                acc.input_cost  += bp.input_cost;
                acc.maintenance += bp.maintenance;
                acc.wages       += bp.wages;
                ++acc.samples;
                if (bp.revenue > 0.0f)
                    ++acc.earning;
            }
        }
    }

    auto row = [](const char* label, const tier_acc& a) {
        const double n = a.samples ? static_cast<double>(a.samples) : 1.0;
        std::printf("  %-22s %8.2f %9.2f %7.2f %6.2f %9.2f  %6.0f%%  %8ld\n",
                    label,
                    a.revenue / n, a.input_cost / n, a.maintenance / n,
                    a.wages / n, a.per_tick(),
                    a.samples ? 100.0 * static_cast<double>(a.earning) / n : 0.0,
                    a.samples);
    };

    std::printf("MEASURED, per building-tick (mean over every sampled building)\n");
    std::printf("  %-22s %8s %9s %7s %6s %9s %7s %9s\n",
                "type", "revenue", "input$", "maint", "wage", "NET", "earning", "samples");
    row("extraction_site", extraction);
    row("processing_facility", processing);

    std::printf("\npayback on capex at the measured net rate:\n");
    auto payback = [](const tier_acc& a) {
        return (a.per_tick() > 0.0) ? a.capex / a.per_tick() : -1.0;
    };
    const double pe = payback(extraction), pp = payback(processing);
    std::printf("  extraction_site       %10.1f cr capex  ->  %s\n",
                extraction.capex,
                pe > 0.0 ? (std::to_string(static_cast<int>(pe)) + " ticks").c_str()
                         : "NEVER (net <= 0)");
    std::printf("  processing_facility   %10.1f cr capex  ->  %s\n",
                processing.capex,
                pp > 0.0 ? (std::to_string(static_cast<int>(pp)) + " ticks").c_str()
                         : "NEVER (net <= 0)");

    // --- R1: the report is non-vacuous ---------------------------------------
    std::printf("\nR1 — the measurement actually happened\n");
    check(extraction.samples > 0, "extraction sites were sampled");
    check(processing.samples > 0, "processing facilities were sampled");

    // --- R2: the assertion BL-436 closes on ----------------------------------
    //
    // Stated as the DESIGN's own claim rather than as a tuning target: refining
    // is the value-add step, so a processing facility should out-earn the
    // extraction site whose output it consumes. This row is expected to FAIL on
    // the day it is written — that failure is the item — and to pass when BL-436
    // lands. It is deliberately a comparison between the two tiers and not a
    // magic number, so it stays meaningful as prices are retuned.
    std::printf("\nR2 — refining pays better than mining (the design's claim)\n");
    std::printf("      extraction net/tick  %8.2f\n", extraction.per_tick());
    std::printf("      processing net/tick  %8.2f\n", processing.per_tick());
    check(processing.per_tick() > extraction.per_tick(),
          "a processing facility out-earns an extraction site per tick");

    std::printf("\n=== %s (%d failure%s) ===\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
