// Headless measurement harness (BL-443): decompose a benchmark corp's final
// balance into OPERATING flows versus ACCRUED INTEREST.
//
// This is an INSTRUMENT, not a check. It asserts nothing about the economy and
// changes nothing about it: it runs the same rollout ai_skill_harness runs (same
// registry, same seeds, same 300 ticks, same run_tick ordering) and passes the
// optional `breakdown` sink apply_budget already exposes, accumulating the five
// flows per corp over the whole run. It exists to answer one question with an
// observation rather than with arithmetic inference:
//
//   is a rival's -2M to -3.4M final net worth mostly OPERATING loss, or mostly
//   compounding on k_debt_interest_per_quarter?
//
// It also records the tick each corp FIRST crossed zero, because that decides
// whether the spiral is the disease or the symptom.
//
// Note the residual line. Build/hire spending mutates the balance through
// apply_corp_command, NOT through apply_budget, so the five flows do not sum to
// the balance delta. The gap is capital spend, and it is reported rather than
// hidden — an unexplained residual would make the decomposition unreadable.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/hard_coded_world.hpp"
#include "harness_params.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/tech_gate.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <vector>

namespace {

// Mirrors ai_skill_harness::make_registry exactly (scripts/economy.lua +
// scripts/recipes.lua). Any drift here makes the two harnesses measure
// different economies, which is the failure mode BL-436 already hit once.
recipe_registry make_registry()
{
    recipe_registry reg;
    const auto steel_i = static_cast<std::size_t>(resource_type::steel);
    const auto fuel_i  = static_cast<std::size_t>(resource_type::refined_fuel);

    building_economics extraction;
    extraction.base_rate            = 20.0f;
    extraction.maintenance          = 5.0f;
    extraction.base_wage            = 8.0f;
    extraction.build_cost           = 100.0f;
    extraction.build_duration_ticks = 2.0f;
    extraction.resource_build_cost[steel_i] = 20.0f;
    extraction.richness_reference = 24.9f;
    extraction.richness_min       = 0.25f;
    extraction.richness_max       = 2.0f;
    reg.set_economics(building_type::extraction_site, extraction);

    building_economics processing;
    processing.base_rate            = 8.0f;
    processing.maintenance          = 10.0f;
    processing.base_wage            = 12.0f;
    processing.build_cost           = 200.0f;
    processing.build_duration_ticks = 3.0f;
    processing.resource_build_cost[steel_i] = 25.0f;
    reg.set_economics(building_type::processing_facility, processing);

    building_economics port;
    port.maintenance          = 8.0f;
    port.base_wage            = 6.0f;
    port.build_cost           = 150.0f;
    port.build_duration_ticks = 2.0f;
    port.resource_build_cost[steel_i] = 20.0f;
    reg.set_economics(building_type::port, port);

    building_economics launchpad;
    launchpad.maintenance          = 20.0f;
    launchpad.base_wage            = 15.0f;
    launchpad.build_cost           = 500.0f;
    launchpad.build_duration_ticks = 6.0f;
    launchpad.resource_build_cost[steel_i] = 50.0f;
    launchpad.resource_build_cost[fuel_i]  = 20.0f;
    reg.set_economics(building_type::launchpad, launchpad);

    building_economics hub;
    hub.maintenance          = 12.0f;
    hub.base_wage            = 8.0f;
    hub.build_cost           = 250.0f;
    hub.build_duration_ticks = 3.0f;
    hub.resource_build_cost[steel_i] = 30.0f;
    reg.set_economics(building_type::inland_logistics_hub, hub);

    building_economics base;
    base.maintenance          = 15.0f;
    base.base_wage            = 10.0f;
    base.build_cost           = 300.0f;
    base.build_duration_ticks = 4.0f;
    base.resource_build_cost[steel_i] = 35.0f;
    reg.set_economics(building_type::military_base, base);

    recipe steel;
    steel.name = "steel";
    steel.inputs[static_cast<std::size_t>(resource_type::iron_ore)] = 2.0f;
    steel.outputs[steel_i] = 1.0f;
    reg.add_recipe(steel);

    recipe fuel;
    fuel.name = "refined_fuel";
    fuel.inputs[static_cast<std::size_t>(resource_type::petroleum)] = 2.0f;
    fuel.outputs[fuel_i] = 1.0f;
    reg.add_recipe(fuel);

    recipe food;
    food.name = "food_rations";
    food.inputs[static_cast<std::size_t>(resource_type::agricultural_produce)] = 2.0f;
    food.outputs[static_cast<std::size_t>(resource_type::food_rations)] = 1.0f;
    reg.add_recipe(food);

    return reg;
}

// Per-corp accumulation over the whole rollout. Doubles, because 300 ticks of
// float addition on figures spanning six orders of magnitude is exactly where a
// summary loses its low digits.
struct corp_ledger
{
    double income = 0.0, expenditure = 0.0, maintenance = 0.0, wages = 0.0;
    double interest = 0.0, levies = 0.0;
    double budget_net = 0.0;   ///< sum of corp_budget::net() — the operating delta
    double start_balance = 0.0;
    double final_balance = 0.0;
    int    first_negative_tick = -1;
    double balance_at_first_negative = 0.0;
    bool   is_player = false;
};

constexpr int k_ticks = 300;

void run_seed(uint32_t seed)
{
    world_params params;
    params.seed = seed;
    world w = make_hard_coded_world(no_prehistory(params));
    const recipe_registry reg = make_registry();
    assign_default_recipes(w, reg);

    std::map<entity_id, corp_ledger> led;
    for (const auto& [id, cc] : w.corporations)
    {
        led[id].start_balance = cc.balance;
        led[id].is_player     = cc.is_player;
    }

    for (int t = 1; t <= k_ticks; ++t)
    {
        w.current_day_tick  = t;
        w.current_econ_tick = t;
        const economy_report rep = run_economy_step(w, reg);
        const auto flows = clear_markets(w, reg, rep);

        std::map<entity_id, corp_budget> breakdown;
        apply_budget(w, reg, flows, rep.workforce_contention, &breakdown, &rep.buildings);
        advance_tech_gates(w);

        for (const auto& [id, b] : breakdown)
        {
            corp_ledger& L = led[id]; // corps generated after t=0 start at zero
            L.income      += b.income;
            L.expenditure += b.expenditure;
            L.maintenance += b.maintenance;
            L.wages       += b.wages;
            L.interest    += b.interest;
            L.levies      += b.levies;
            L.budget_net  += b.net();
        }

        for (const auto& [id, cc] : w.corporations)
        {
            corp_ledger& L = led[id];
            if (L.first_negative_tick < 0 && cc.balance < 0.0f)
            {
                L.first_negative_tick       = t;
                L.balance_at_first_negative = cc.balance;
            }
        }
    }

    for (auto& [id, cc] : w.corporations)
        led[id].final_balance = cc.balance;

    // --- report ---
    std::printf("\n=== SEED %u (%d ticks) ===\n", seed, k_ticks);
    std::printf("%-6s %-4s %10s %12s %12s %12s %12s %12s %12s %8s\n",
                "corp", "ply", "start", "final", "op_net", "interest",
                "maint+wage", "income", "capital", "neg@");
    double t_op = 0.0, t_int = 0.0, t_final = 0.0;
    int n_neg = 0, n_ai = 0;
    for (const auto& [id, L] : led)
    {
        // Operating net EXCLUDING interest — what the corp's trading and
        // running of buildings actually did.
        const double op_net = L.budget_net + L.interest;
        // Whatever moved the balance that apply_budget did not: build/hire spend.
        const double capital = (L.final_balance - L.start_balance) - L.budget_net;
        std::printf("%-6llu %-4s %10.0f %12.0f %12.0f %12.0f %12.0f %12.0f %12.0f %8d\n",
                    static_cast<unsigned long long>(id), L.is_player ? "yes" : "no",
                    L.start_balance, L.final_balance, op_net, -L.interest,
                    -(L.maintenance + L.wages), L.income, capital,
                    L.first_negative_tick);
        if (!L.is_player)
        {
            ++n_ai;
            t_op += op_net; t_int += L.interest; t_final += L.final_balance;
            if (L.first_negative_tick >= 0) ++n_neg;
        }
    }
    std::printf("--- AI totals (n=%d): final=%.0f  operating=%+.0f  interest=%.0f"
                "  ever-negative=%d\n", n_ai, t_final, t_op, -t_int, n_neg);
    if (t_final < 0.0)
    {
        const double share = (t_final != 0.0) ? (t_int / -t_final) * 100.0 : 0.0;
        std::printf("--- interest accounts for %.1f%% of the aggregate deficit\n", share);
    }

    // The corps that actually went underwater, ordered by when.
    std::vector<std::pair<int, entity_id>> dips;
    for (const auto& [id, L] : led)
        if (!L.is_player && L.first_negative_tick >= 0)
            dips.emplace_back(L.first_negative_tick, id);
    std::sort(dips.begin(), dips.end());
    if (!dips.empty())
    {
        std::printf("--- first-crossing detail (tick, balance at crossing, final, interest):\n");
        for (const auto& [t, id] : dips)
        {
            const corp_ledger& L = led.at(id);
            std::printf("      t=%-4d corp %-5llu  dip=%10.1f  final=%12.0f  interest=%12.0f\n",
                        t, static_cast<unsigned long long>(id),
                        L.balance_at_first_negative, L.final_balance, -L.interest);
        }
    }
}

} // namespace

int main()
{
    std::printf("DEBT_DECOMPOSITION (BL-443 measurement; asserts nothing)\n");
    std::printf("k_debt_interest_per_quarter = %.4f\n", k_debt_interest_per_quarter);
    for (uint32_t seed = 0; seed < 5; ++seed)
        run_seed(seed);
    std::printf("\nDONE (measurement only — no PASS/FAIL)\n");
    return 0;
}
