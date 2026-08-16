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
#include "world/placement_rules.hpp"
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

/// Replicates app::load_economy's default-recipe pass, which is a REAL STAGE of
/// startup and not a harness convenience: generation cannot author recipe ids
/// (they are registry indices that do not exist yet — corporation_generation.cpp
/// says so at the assignment site), so every generated processor sits at
/// no_recipe until load_economy assigns one. Crucially this runs BEFORE
/// generate_background_firms, which authors its own recipes, so the ordering
/// below is the app's ordering.
///
/// Uses default_recipe_id() rather than naming "steel", because BL-429 made the
/// default era-aware; hard-naming it would measure an industrial default in an
/// ancient world.
void seed_default_recipes(world& w, const recipe_registry& reg)
{
    const uint16_t def = reg.default_recipe_id();
    for (auto& [id, b] : w.buildings)
        if (b.type == building_type::processing_facility && b.recipe == no_recipe)
            b.recipe = def;
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
    // Third arg: 0 to SKIP load_economy's default-recipe pass. Default 1 (run
    // it), because skipping it does not measure "the shipped economy" — it
    // measures a startup sequence the game never performs, and reports every
    // generated processor as recipe-less. Kept as an opt-out only because it
    // isolates how many processors the default pass is carrying.
    const bool seed_recipes = (argc > 3) ? (std::atoi(argv[3]) != 0) : true;
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

    // --- R3: WHY a processor produced nothing (BL-436, second pass) ----------
    //
    // The first pass measured that processors earn on only 22% of their
    // building-ticks. That number alone cannot tell a rate problem from an input
    // problem, and the two want opposite fixes — so this classifies every
    // non-producing processing tick by the reason the economy itself recorded,
    // rather than by inference. building_report already carries all of it.
    long proc_ticks = 0, proc_active = 0;
    long why_unstaffed = 0, why_no_recipe = 0, why_starved = 0, why_other = 0;
    long active_but_zero = 0;
    std::vector<long> limiting_by_resource(resource_count, 0);

    // R4 accumulators: is each recipe input actually supplied?
    std::vector<long>   tiles_with(resource_count, 0);      // tiles carrying a deposit
    std::vector<long>   sites_targeting(resource_count, 0); // extraction sites mining it
    std::vector<double> supply_units(resource_count, 0.0);  // units extracted, all seeds/ticks
    std::vector<long>   input_demand(resource_count, 0);    // recipes naming it as an input
    std::vector<long>   richest_on(resource_count, 0);      // tiles where it IS the richest (R5)
    for (int i = 0; i < reg.recipe_count(building_type::processing_facility); ++i)
    {
        const recipe& rc = reg.recipe_at(building_type::processing_facility, i);
        for (std::size_t r = 0; r < resource_count; ++r)
            if (rc.inputs[r] > 0.0f)
                ++input_demand[r];
    }

    for (int s = 0; s < n_seeds; ++s)
    {
        world_params p = no_prehistory();
        p.seed = static_cast<uint32_t>(s);
        world w = make_hard_coded_world(p);
        // app ordering, and it is load-bearing: load_economy's default pass runs
        // BEFORE generate_background_firms. Reversing them, or skipping the pass,
        // reports a quarter of all processors as recipe-less — an artefact of the
        // harness rather than a fact about the game. (Measured both ways while
        // writing this: 26.5% no-recipe without the pass, 11.3% with it.)
        if (seed_recipes)
            seed_default_recipes(w, reg);
        generate_background_firms(w, reg, static_cast<uint32_t>(s) ^ 0x8A21F00Du);

        // Static structure, counted once per seed: what the world CONTAINS and
        // what is pointed at it, independent of any tick.
        for (const auto& [tid, tc] : w.tiles)
        {
            for (std::size_t r = 0; r < resource_count; ++r)
                if (tc.resource_deposit[r] > 0.0f)
                    ++tiles_with[r];
            // R5: what richest_extractable WOULD return here. Uses the real
            // function, since its behaviour is the subject of the question.
            bool any = false;
            const resource_type best = placement_rules::richest_extractable(tc, any);
            if (any)
                ++richest_on[static_cast<std::size_t>(best)];
        }
        for (const auto& [bid, b] : w.buildings)
            if (b.type == building_type::extraction_site)
                ++sites_targeting[static_cast<std::size_t>(b.target_resource)];

        // Units extracted are measured as RESERVE DEPLETION, not from the report.
        // building_report carries one target_resource and a single total, so once
        // BL-437 made a site work a basket, attributing its whole output to the
        // primary reported coal as unmined while coal was being pulled out of the
        // ground. Reserves cannot lie: nothing but extraction moves them.
        std::vector<double> reserve_before(resource_count, 0.0);
        for (const auto& [tid, tc] : w.tiles)
            for (std::size_t r = 0; r < resource_count; ++r)
                reserve_before[r] += tc.resource_remaining[r];

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

            // R3's classification, read off the report rows the same tick
            // produced them.
            for (const building_report& br : report.buildings)
            {
                if (br.type != building_type::processing_facility)
                    continue;
                ++proc_ticks;
                if (br.active && br.output_quantity > 0.0f)
                {
                    ++proc_active;
                    continue;
                }
                if (br.active)          // reported active but credited nothing
                {
                    ++active_but_zero;
                    continue;
                }
                // Order matters: a building can satisfy more than one of these,
                // and the FIRST is the one that would have to be fixed first.
                if (br.recipe == no_recipe)
                    ++why_no_recipe;
                else if (br.effective_workforce <= 0.0f)
                    ++why_unstaffed;
                else if (br.has_limiting)
                {
                    ++why_starved;
                    limiting_by_resource[static_cast<std::size_t>(br.limiting_input)]++;
                }
                else
                    ++why_other;
            }
        }

        // Per-seed reserve delta: everything extracted this seed, by resource.
        for (const auto& [tid, tc] : w.tiles)
            for (std::size_t r = 0; r < resource_count; ++r)
                reserve_before[r] -= tc.resource_remaining[r];
        for (std::size_t r = 0; r < resource_count; ++r)
            supply_units[r] += reserve_before[r];
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

    // --- R3: why processors do not produce -----------------------------------
    //
    // Resource ids, not names: the only lookup table lives in the UI layer and
    // BL-414 owns consolidating it. A fourth hand-rolled copy to prettify a
    // harness would make that item worse. Same call chain_depth's rows made.
    std::printf("\nR3 — of %ld processing building-ticks, why did they not produce?\n", proc_ticks);
    auto pct = [&](long n) {
        return proc_ticks ? 100.0 * static_cast<double>(n) / static_cast<double>(proc_ticks) : 0.0;
    };
    std::printf("  produced output          %8ld  %5.1f%%\n", proc_active,     pct(proc_active));
    std::printf("  STARVED (input missing)  %8ld  %5.1f%%\n", why_starved,     pct(why_starved));
    std::printf("  unstaffed                %8ld  %5.1f%%\n", why_unstaffed,   pct(why_unstaffed));
    std::printf("  no recipe set            %8ld  %5.1f%%\n", why_no_recipe,   pct(why_no_recipe));
    std::printf("  active but credited 0    %8ld  %5.1f%%\n", active_but_zero, pct(active_but_zero));
    std::printf("  idle, no reason recorded %8ld  %5.1f%%\n", why_other,       pct(why_other));

    if (why_starved > 0)
    {
        std::printf("\n  binding input when starved (resource id -> ticks):\n");
        for (std::size_t r = 0; r < resource_count; ++r)
            if (limiting_by_resource[r] > 0)
                std::printf("    id %2zu  %8ld  %5.1f%% of starved ticks\n", r,
                            limiting_by_resource[r],
                            100.0 * static_cast<double>(limiting_by_resource[r])
                                  / static_cast<double>(why_starved));
    }

    // --- R4: is every recipe input actually SUPPLIED? ------------------------
    //
    // R3 found starvation dominated by one resource, which raises a question the
    // profit columns cannot answer: is that input scarce because the market fails
    // to route it, or because nothing mines it in the first place? Those want
    // completely different fixes, and the difference is visible here.
    //
    // The suspicion this exists to test: placement_rules::richest_extractable
    // gives a site the SINGLE richest deposit on its tile, so a resource that is
    // present but never the richest on any tile is never mined at all — no matter
    // how many tiles carry it, and no matter how much a recipe needs it.
    std::printf("\nR4 — is every recipe input actually produced?\n");
    std::printf("  %-4s %10s %10s %12s %14s\n",
                "id", "tiles", "sites", "units/tick", "needed-by");
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (input_demand[r] == 0)
            continue; // nothing consumes it; not this row's business
        std::printf("  %-4zu %10ld %10ld %12.1f %14ld%s\n", r,
                    tiles_with[r], sites_targeting[r],
                    supply_units[r] / static_cast<double>(n_seeds * n_ticks),
                    input_demand[r],
                    (tiles_with[r] > 0 && supply_units[r] <= 0.0) ? "   <-- PRODUCED BY NOTHING" : "");
    }

    // --- R5: never the richest ANYWHERE, or never where corps build? ---------
    //
    // BL-437 records three candidate fixes and they are not interchangeable, so
    // this is the measurement that tells them apart — run before any of them is
    // costed, per BL-436's own lesson.
    //
    //   * If a resource is the richest on ZERO tiles, then richest_extractable
    //     can never return it and no amount of better SITING would help. The
    //     rule itself has to change (option A or C).
    //   * If it IS the richest somewhere but has no sites, the deposits exist in
    //     reachable form and the problem is WHERE corps build (option B, or
    //     siting), which is a much cheaper fix.
    //
    // "Richest" is computed with the real richest_extractable, not a
    // reimplementation of it — a hand-rolled copy could disagree with the
    // function whose behaviour is the entire subject of the question.
    std::printf("\nR5 — for each unmined input, is it ever the RICHEST on any tile?\n");
    std::printf("  %-4s %12s %14s %8s %8s\n",
                "id", "tiles-with", "tiles-RICHEST", "win%", "sites");
    for (std::size_t r = 0; r < resource_count; ++r)
    {
        if (input_demand[r] == 0 || tiles_with[r] == 0)
            continue;
        const double win = 100.0 * static_cast<double>(richest_on[r])
                                 / static_cast<double>(tiles_with[r]);
        // The WIN RATE is the number that matters, not the raw count. "Richest
        // somewhere" is technically true of coal at 19 tiles out of 1671 and of
        // regolith at 15,644 out of 16,361, and those are completely different
        // situations wanting completely different fixes. A binary verdict here
        // would have been the wrong answer stated confidently.
        const char* verdict = "";
        if (sites_targeting[r] == 0)
        {
            if (richest_on[r] == 0)          verdict = "  never richest anywhere";
            else if (win >= 50.0)            verdict = "  usually richest, still unmined -> SITING/REACH";
            else                             verdict = "  rarely richest -> out-ranked in practice";
        }
        std::printf("  %-4zu %12ld %14ld %7.1f%% %8ld%s\n",
                    r, tiles_with[r], richest_on[r], win, sites_targeting[r], verdict);
    }
    std::printf("  (win%% = share of the tiles carrying it on which it is the richest —\n"
                "   i.e. how often richest_extractable would actually return it)\n");

    // The invariant that R4's table exists to state. A resource a recipe needs,
    // which is PRESENT on thousands of tiles and which nothing mines, is not a
    // balance problem — it is a chain that can never run at all.
    // Keyed on PRODUCTION, not on how many sites name it as their primary.
    // BL-437 made a site work its whole tile, so `sites_targeting` counts only
    // what a site is NAMED for — it reported coal as unmined on the very run
    // where coal was coming out of the ground. The reserve-depletion figure is
    // the one that answers the question.
    long unproduced_but_needed = 0;
    for (std::size_t r = 0; r < resource_count; ++r)
        if (input_demand[r] > 0 && tiles_with[r] > 0 && supply_units[r] <= 0.0)
            ++unproduced_but_needed;
    check(unproduced_but_needed == 0,
          "every recipe input that has deposits is actually produced ("
              + std::to_string(unproduced_but_needed) + " are not)");

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
