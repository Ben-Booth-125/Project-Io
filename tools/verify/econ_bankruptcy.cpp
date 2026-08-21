// Headless economy bankruptcy calibration harness (no SDL / Lua / ImGui).
// Builds a representative world (extraction corp, processing corp, shared market),
// runs the full economy loop (run_economy_step -> clear_markets -> apply_budget)
// up to a 500-year ceiling, and reports when (if ever) the player corp goes
// bankrupt (balance <= -5 * starting_capital).  Also accumulates per-building
// cash burn and per-resource net flow for calibration inspection.
//
// "Bankruptcy" here is a threshold trigger for calibration only — the prototype
// carries no insolvency mechanic (budget_system.hpp, balance may go negative).
//
// Build (from repo root, after sourcing vcvars64): see tools/verify/README.md.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

// ---------------------------------------------------------------------------
// Calendar constants (mirrors sim_loop.hpp)
// ---------------------------------------------------------------------------
static constexpr int k_ticks_per_year = 4;   // quarterly economy ticks
static constexpr int k_years_ceiling  = 500;
static constexpr int k_tick_ceiling   = k_years_ceiling * k_ticks_per_year; // 2000

static constexpr std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// ---------------------------------------------------------------------------
// Accumulator helpers
// ---------------------------------------------------------------------------
struct building_burn
{
    float total_maintenance = 0.0f;
    float total_wages       = 0.0f;
    int   tick_count        = 0;
};

static const char* building_type_name(building_type bt)
{
    switch (bt)
    {
    case building_type::extraction_site:     return "extraction_site";
    case building_type::processing_facility: return "processing_facility";
    case building_type::port:                return "port";
    case building_type::launchpad:           return "launchpad";
    default:                                 return "none";
    }
}

static const char* resource_name(std::size_t idx)
{
    static const char* names[] = {
        "iron_ore", "coal", "petroleum", "silica", "copper_ore",
        "rare_earth_ore", "agricultural_produce", "water",
        "iron_nickel_ore", "platinum_group_metals", "regolith",
        "stone", "timber", "sand", "clay", "peat",
        "steel", "refined_fuel", "food_rations"
    };
    // The guard must be the ARRAY's own extent, not resource_count. This table
    // is a convenience for report lines and has never covered the whole enum
    // (19 names against 23 resources even before BL-286), so testing against
    // resource_count read past the end for every resource it lacked. That was
    // invisible while the overrun was small enough to land on adjacent static
    // data; BL-286 widened the enum to 31 and the read ran far enough off the
    // end to fault outright.
    if (idx < sizeof(names) / sizeof(names[0]))
        return names[idx];
    return "unknown";
}

int main()
{
    // --- registry (hand-built; mirrors scripts/economy.lua + recipes.lua) ---
    recipe_registry reg;
    reg.set_thresholds(/*t_full=*/1.0f, /*t_idle=*/0.2f);
    {
        building_economics ex;
        ex.base_rate    = 20.0f;
        ex.maintenance  = 5.0f;
        ex.base_wage    = 8.0f;
        reg.set_economics(building_type::extraction_site, ex);

        building_economics pr;
        pr.base_rate    = 8.0f;
        pr.maintenance  = 10.0f;
        pr.base_wage    = 12.0f;
        reg.set_economics(building_type::processing_facility, pr);
    }

    recipe steel;
    steel.name = "steel";
    steel.inputs[ri(resource_type::iron_ore)] = 2.0f;
    steel.outputs[ri(resource_type::steel)]   = 1.0f;
    const uint16_t steel_id = reg.add_recipe(steel);

    // --- world setup ---
    world w;

    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};
    w.bodies[body].name = "BankruptcyTestWorld";

    const entity_id market = w.create_entity();
    {
        market_component mc;
        mc.body = body;
        mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
        mc.base_price[ri(resource_type::steel)]    = 8.0f;
        mc.price = mc.base_price;
        w.markets[market] = mc;
    }

    // Player corp: one extraction site (iron ore), effectively infinite reserve
    const entity_id tile_player = w.create_entity();
    {
        tile_component tc{};
        tc.body = body;
        tc.substrate = terrain_substrate::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)]   = 2.0f;
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e8f;
        tc.hazard_level = 0.0f;
        w.tiles[tile_player] = tc;
    }
    const entity_id bld_player = w.create_entity();
    {
        building_component b{};
        b.tile               = tile_player;
        b.type               = building_type::extraction_site;
        b.workforce_assigned = 1.0f;
        b.target_resource    = resource_type::iron_ore;
        w.buildings[bld_player] = b;
    }
    const entity_id player_corp = w.create_entity();
    const float start_money = 1000.0f;
    {
        corporation_component cc;
        cc.name             = "PlayerCorp";
        cc.starting_capital = start_money;
        cc.balance          = start_money;
        cc.assets.push_back(bld_player);
        w.corporations[player_corp] = cc;
    }
    w.player_entity = player_corp;

    // Rival corp: one processing facility (steel), drives iron demand
    const entity_id tile_rival = w.create_entity();
    {
        tile_component tc{};
        tc.body = body;
        tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
        w.tiles[tile_rival] = tc;
    }
    const entity_id bld_rival = w.create_entity();
    {
        building_component b{};
        b.tile               = tile_rival;
        b.type               = building_type::processing_facility;
        b.workforce_assigned = 0.5f;
        b.recipe             = steel_id;
        w.buildings[bld_rival] = b;
    }
    {
        corporation_component cc;
        cc.name             = "RivalCorp";
        cc.starting_capital = 1000.0f;
        cc.balance          = 1000.0f;
        cc.assets.push_back(bld_rival);
        const entity_id rival_corp = w.create_entity();
        w.corporations[rival_corp] = cc;
        // Seed rival's pool so the processor isn't immediately idle
        w.pool_for(rival_corp, body).quantities[ri(resource_type::iron_ore)] = 4.0f;
    }

    // --- per-building burn accumulators (indexed by building_type enum value) ---
    static constexpr std::size_t k_bt_count = 5;
    building_burn burn_by_type[k_bt_count] = {};

    // --- per-resource net flow accumulator (production minus consumption) ---
    std::array<float, resource_count> net_flow_total = {};
    net_flow_total.fill(0.0f);

    // --- run loop ---
    const float bankruptcy_threshold = -5.0f * start_money;
    bool went_bankrupt  = false;
    int  bankruptcy_tick = -1;

    for (int tick = 0; tick < k_tick_ceiling; ++tick)
    {
        economy_report rep   = run_economy_step(w, reg);
        auto           flows = clear_markets(w, reg, rep);
        apply_budget(w, reg, flows, rep.workforce_contention);

        // Accumulate per-building cash burn (maintenance + wages on effective workforce)
        for (const auto& br : rep.buildings)
        {
            const std::size_t bt_idx = static_cast<std::size_t>(br.type);
            if (bt_idx >= k_bt_count)
                continue;
            const building_economics& eco = reg.economics(br.type);
            burn_by_type[bt_idx].total_maintenance += eco.maintenance;
            burn_by_type[bt_idx].total_wages       += br.effective_workforce * eco.base_wage;
            burn_by_type[bt_idx].tick_count        += 1;

            // Per-resource net flow from extraction output
            if (br.type == building_type::extraction_site)
            {
                net_flow_total[ri(br.target_resource)] += br.output_quantity;
            }
            // Per-resource net flow from processing: outputs credited, inputs debited.
            // output_quantity for a processing_facility = batches produced this tick.
            else if (br.type == building_type::processing_facility && br.output_quantity > 0.0f)
            {
                const auto bld_it = w.buildings.find(br.building);
                if (bld_it != w.buildings.end())
                {
                    const recipe* rec = reg.get_recipe(bld_it->second.recipe);
                    if (rec)
                    {
                        const float batches = br.output_quantity;
                        for (std::size_t i = 0; i < resource_count; ++i)
                        {
                            net_flow_total[i] += rec->outputs[i] * batches;
                            net_flow_total[i] -= rec->inputs[i]  * batches;
                        }
                    }
                }
            }
        }

        // Bankruptcy check
        const float balance = w.corporations.at(player_corp).balance;
        if (balance <= bankruptcy_threshold)
        {
            went_bankrupt   = true;
            bankruptcy_tick = tick + 1; // 1-based
            break;
        }
    }

    // --- report ---
    const float years_run = went_bankrupt
        ? static_cast<float>(bankruptcy_tick) / static_cast<float>(k_ticks_per_year)
        : static_cast<float>(k_years_ceiling);

    std::printf("ECON_BANKRUPTCY RESULT\n");
    if (went_bankrupt)
        std::printf("Years to bankruptcy: %.2f  (tick %d of %d)\n",
                    years_run, bankruptcy_tick, k_tick_ceiling);
    else
        std::printf("Solvent at 500-year ceiling  (balance: %.2f)\n",
                    w.corporations.at(player_corp).balance);

    std::printf("\nPer-building cash burn (per year):\n");
    for (std::size_t i = 1; i < k_bt_count; ++i)
    {
        const building_burn& bb = burn_by_type[i];
        if (bb.tick_count == 0)
            continue;
        const float per_year       = (bb.total_maintenance + bb.total_wages) / years_run;
        const float maint_per_year = bb.total_maintenance / years_run;
        const float wages_per_year = bb.total_wages       / years_run;
        std::printf("  %s: %.2f  (maint/yr %.2f + wages/yr %.2f)\n",
                    building_type_name(static_cast<building_type>(i)),
                    per_year, maint_per_year, wages_per_year);
    }

    std::printf("\nPer-resource net flow (per year):\n");
    bool any_flow = false;
    for (std::size_t i = 0; i < resource_count; ++i)
    {
        if (net_flow_total[i] == 0.0f)
            continue;
        std::printf("  %s: %+.2f\n", resource_name(i), net_flow_total[i] / years_run);
        any_flow = true;
    }
    if (!any_flow)
        std::printf("  (none)\n");

    std::printf("\nPASS\n");
    return 0;
}
