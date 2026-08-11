// Headless harness for BL-368's habitability tranche: the three new resource_type
// values (clean_water, consumer_goods, medical_supplies) are each produced,
// priced, and cleared at least once over a multi-tick rollout, no priced good
// sits pegged at the [0.25x, 4x] band ceiling for the whole run, and a
// population centre's demand for the tranche survives clear_markets. Mirrors
// resource_chain_harness (BL-340)'s shape. Kept outside src/ so the CMake glob
// does not pull it into the real build.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else      { std::printf("  FAIL  %s\n", what); ++g_failures; }
}

static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

int main()
{
    world w;
    recipe_registry reg;
    reg.set_thresholds(/*t_full=*/1.0f, /*t_idle=*/0.2f);

    building_economics ex; ex.base_rate = 40.0f; ex.maintenance = 5.0f; ex.base_wage = 8.0f;
    reg.set_economics(building_type::extraction_site, ex);
    building_economics pr; pr.base_rate = 20.0f; pr.maintenance = 10.0f; pr.base_wage = 12.0f;
    reg.set_economics(building_type::processing_facility, pr);

    // Three new recipes, mirroring scripts/recipes.lua ids 14-16.
    auto add = [&](const char* name, std::initializer_list<std::pair<resource_type, float>> in,
                   resource_type out) {
        recipe rcp; rcp.name = name;
        for (auto [r, q] : in) rcp.inputs[ri(r)] = q;
        rcp.outputs[ri(out)] = 1.0f;
        return reg.add_recipe(rcp);
    };
    const uint16_t id_clean_water = add("clean_water", {{resource_type::water, 2.0f}}, resource_type::clean_water);
    const uint16_t id_consumer    = add("consumer_goods", {{resource_type::food_rations, 1.0f}, {resource_type::steel, 1.0f}}, resource_type::consumer_goods);
    const uint16_t id_medical     = add("medical_supplies", {{resource_type::water, 1.0f}, {resource_type::agricultural_produce, 1.0f}}, resource_type::medical_supplies);

    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};

    const entity_id market = w.create_entity();
    market_component mc; mc.body = body;
    mc.base_price[ri(resource_type::water)]                 = 1.5f;
    mc.base_price[ri(resource_type::food_rations)]           = 6.0f;
    mc.base_price[ri(resource_type::steel)]                  = 8.0f;
    mc.base_price[ri(resource_type::agricultural_produce)]   = 3.0f;
    mc.base_price[ri(resource_type::clean_water)]             = 3.0f;
    mc.base_price[ri(resource_type::consumer_goods)]          = 12.0f;
    mc.base_price[ri(resource_type::medical_supplies)]        = 14.0f;
    mc.price = mc.base_price;
    w.markets[market] = mc;

    // One corp owning every processing stage, seeded input pools directly
    // (steel/food_rations/water/agricultural_produce chains are pre-existing,
    // tested elsewhere — this harness scopes to the NEW tranche).
    const entity_id tile_p = w.create_entity();
    { tile_component tc{}; tc.body = body; tc.composition = terrain_composition::grassland; w.tiles[tile_p] = tc; }
    const entity_id corp = w.create_entity();
    { corporation_component cc; cc.name = "Welfare Co"; cc.starting_capital = cc.balance = 1.0e6f;
      cc.is_player = true; w.corporations[corp] = cc; }
    stockpile_component& pool = w.pool_for(corp, body);
    pool.quantities[ri(resource_type::water)]                = 1.0e5f;
    pool.quantities[ri(resource_type::food_rations)]          = 1.0e5f;
    pool.quantities[ri(resource_type::steel)]                 = 1.0e5f;
    pool.quantities[ri(resource_type::agricultural_produce)]  = 1.0e5f;

    auto make_processor = [&](uint16_t recipe_id) {
        const entity_id bld = w.create_entity();
        building_component b{}; b.tile = tile_p; b.type = building_type::processing_facility;
        b.workforce_assigned = 1.0f; b.recipe = recipe_id;
        w.buildings[bld] = b;
        w.corporations[corp].assets.push_back(bld);
    };
    make_processor(id_clean_water);
    make_processor(id_consumer);
    make_processor(id_medical);

    // A population centre on the same tile, so its BL-368 demand basket reads
    // this market and R4 can assert the tranche's demand survives clearing.
    {
        const entity_id pop = w.create_entity();
        population_centre_component pcc{};
        pcc.scale = 5;
        w.population_centres[pop]     = pcc;
        w.population_centre_tile[pop] = tile_p;
    }
    {
        population_demand_params pd;
        pd.demand_basket[ri(resource_type::clean_water)]      = 1.0f;
        pd.demand_basket[ri(resource_type::consumer_goods)]   = 1.0f;
        pd.demand_basket[ri(resource_type::medical_supplies)] = 1.0f;
        pd.demand_elasticity = 0.0f; // deterministic flat behaviour for this assertion
        reg.set_population_demand(pd);
    }

    const resource_type new_goods[] = {
        resource_type::clean_water, resource_type::consumer_goods, resource_type::medical_supplies,
    };
    const uint16_t recipe_for_good[] = { id_clean_water, id_consumer, id_medical };
    bool produced[3] = {};
    bool off_ceiling[3] = {};

    constexpr int k_ticks = 80;
    for (int t = 0; t < k_ticks; ++t)
    {
        economy_report rep = run_economy_step(w, reg);
        auto flows = clear_markets(w, reg, rep);
        apply_budget(w, reg, flows, rep.workforce_contention);

        for (const auto& br : rep.buildings)
            for (std::size_t g = 0; g < 3; ++g)
                if (br.recipe == recipe_for_good[g] && br.output_quantity > 0.0f)
                    produced[g] = true;

        for (std::size_t g = 0; g < 3; ++g)
        {
            const market_component& m = w.markets[market];
            if (m.price[ri(new_goods[g])] < 3.99f * m.base_price[ri(new_goods[g])]) off_ceiling[g] = true;
        }
    }

    std::printf("Habitability tranche harness (BL-368), %d ticks\n", k_ticks);
    const char* names[3] = {"clean_water", "consumer_goods", "medical_supplies"};
    for (std::size_t g = 0; g < 3; ++g)
    {
        char buf[128];
        std::snprintf(buf, sizeof buf, "R1 %s produced (a building's output_quantity > 0 at least once)", names[g]);
        check(produced[g], buf);
        std::snprintf(buf, sizeof buf, "R2 %s not pegged at the 4x band ceiling for the whole run", names[g]);
        check(off_ceiling[g], buf);
    }

    // R3: the population centre's demand for the tranche reached the market
    // (survived clear_markets' reset, same ordering contract as R2/R3 in
    // population_demand_harness). Scale 5, basket weight 1.0 each, elasticity
    // 0 -> flat demand == 5.0 per resource, deterministic and price-independent.
    const market_component& final_mc = w.markets[market];
    for (std::size_t g = 0; g < 3; ++g)
    {
        char buf[128];
        std::snprintf(buf, sizeof buf, "R3 population demand for %s reached the market (== 5.0)", names[g]);
        check(final_mc.demand[ri(new_goods[g])] > 0.0f - 1e-4f &&
              final_mc.demand[ri(new_goods[g])] > 4.999f && final_mc.demand[ri(new_goods[g])] < 5.001f,
              buf);
    }

    if (g_failures == 0) std::printf("\nALL PASS  (0 failures)\n");
    else                 std::printf("\nFAILURES (%d failures)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
