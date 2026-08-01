// Headless harness for the PROSPECTIVE building-profit estimate (BL-162; no SDL /
// Lua / ImGui). `estimate_prospective_profit` (src/world/building_profit.cpp) is the
// figure the tile construction ledger ranks the player's build options by, so it has
// to agree with what economy_system + budget_system would actually pay out once the
// building exists. This harness asserts every term of the model against those systems'
// OWN arithmetic rather than against a restatement of the formula:
//
//   * opex against compute_building_opex on the same synthetic component;
//   * extraction revenue against run_economy_step's realised output_quantity, priced
//     at the same market, for a building actually constructed on that tile;
//   * processing revenue / input cost the same way;
//   * the depletion taper (the reserve is per-TILE, so a spent tile must estimate low);
//   * infrastructure (port / hub) as revenue 0 and a net that is exactly its upkeep;
//   * has_data == false on a missing tile and on a tile whose body has no market.
//
// Kept outside src/ so the CMake glob does not pull it into the real build; the CTest
// block in CMakeLists.txt picks it up automatically and links the world superset.
//
// Build (from repo root, after sourcing vcvars64):
//   cl /nologo /std:c++20 /EHsc /I src tools\verify\prospective_profit_harness.cpp ^
//      src\world\world.cpp src\world\building_profit.cpp src\world\budget_system.cpp ^
//      src\world\economy_system.cpp src\world\market_clearing.cpp ^
//      src\world\construction.cpp src\world\placement_rules.cpp ^
//      /Fo:build_gen\verify\prospective_profit_harness\ ^
//      /Fe:build_gen\verify\prospective_profit_harness.exe
// Run: .\build_gen\verify\prospective_profit_harness.exe

#include "world/budget_system.hpp"
#include "world/building_profit.hpp"
#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what, float got = 0.0f, float want = 0.0f)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else { std::printf("  FAIL  %s   (got %.4f, want %.4f)\n", what, got, want); ++g_failures; }
}

static bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }
static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

int main()
{
    std::printf("BL-162 prospective building-profit harness\n");

    // ---------------------------------------------------------------------
    // Fixture: one body, one market, a rich iron tile and a bare land tile.
    // ---------------------------------------------------------------------
    recipe_registry reg;
    reg.set_thresholds(/*t_full=*/1.0f, /*t_idle=*/0.2f);
    {
        building_economics ex;
        ex.base_rate = 20.0f; ex.maintenance = 5.0f; ex.base_wage = 8.0f; ex.build_cost = 100.0f;
        reg.set_economics(building_type::extraction_site, ex);

        building_economics pr;
        pr.base_rate = 8.0f; pr.maintenance = 10.0f; pr.base_wage = 12.0f; pr.build_cost = 150.0f;
        reg.set_economics(building_type::processing_facility, pr);

        building_economics po;
        po.base_rate = 0.0f; po.maintenance = 7.0f; po.base_wage = 9.0f; po.build_cost = 80.0f;
        reg.set_economics(building_type::port, po);
        reg.set_economics(building_type::inland_logistics_hub, po);
    }
    recipe steel;
    steel.name = "steel";
    steel.inputs[ri(resource_type::iron_ore)] = 2.0f;
    steel.outputs[ri(resource_type::steel)]   = 1.0f;
    const uint16_t steel_id = reg.add_recipe(steel);

    world w;
    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};
    w.bodies[body].name = "TestWorld";

    const entity_id market = w.create_entity();
    {
        market_component mc;
        mc.body = body;
        mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
        mc.base_price[ri(resource_type::steel)]    = 8.0f;
        mc.price = mc.base_price;
        w.markets[market] = mc;
    }
    const float p_iron  = 2.5f;
    const float p_steel = 8.0f;

    // A rich iron tile with an ample reserve (taper == 1), and a bare land tile.
    const entity_id tile_iron = w.create_entity();
    {
        tile_component tc{};
        tc.body = body;
        tc.composition = terrain_composition::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)]   = 2.0f;
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f;
        tc.hazard_level = 0.0f;
        w.tiles[tile_iron] = tc;
    }
    const entity_id tile_bare = w.create_entity();
    {
        tile_component tc{};
        tc.body = body;
        tc.composition = terrain_composition::grassland;
        w.tiles[tile_bare] = tc;
    }

    const entity_id corp = w.create_entity();
    {
        corporation_component cc;
        cc.name = "Estimator Co";
        cc.starting_capital = 100000.0f;
        cc.balance          = 100000.0f;
        // Not AI-driven: BL-202's strategic tier rewrites workforce_target at the end
        // of run_economy_step, which would move the very opex terms being asserted.
        cc.is_player = true;
        w.corporations[corp] = cc;
    }
    w.player_entity = corp;

    // ---------------------------------------------------------------------
    // P.R1 — has_data is false where there is nothing honest to report.
    // ---------------------------------------------------------------------
    {
        const building_profit missing = estimate_prospective_profit(
            w, reg, /*tile_id=*/999999, building_type::extraction_site, resource_type::iron_ore);
        check(!missing.has_data, "P.R1 missing tile -> has_data == false");

        // A second body with tiles but no market: market_for_tile returns null_entity.
        world w2;
        const entity_id b2 = w2.create_entity();
        w2.bodies[b2] = body_component{};
        const entity_id t2 = w2.create_entity();
        { tile_component tc{}; tc.body = b2;
          tc.resource_deposit[ri(resource_type::iron_ore)]   = 2.0f;
          tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f;
          w2.tiles[t2] = tc; }
        const building_profit unpriced = estimate_prospective_profit(
            w2, reg, t2, building_type::extraction_site, resource_type::iron_ore);
        check(!unpriced.has_data && near(unpriced.revenue, 0.0f),
              "P.R1 unresolved market -> has_data == false, no zero-chart");
    }

    // ---------------------------------------------------------------------
    // P.R2 — opex parity with compute_building_opex on the same component.
    // The estimate must be the SAME quantity apply_budget will charge, or the
    // pre-build bar and the post-build Net disagree.
    // ---------------------------------------------------------------------
    const building_profit est_ext = estimate_prospective_profit(
        w, reg, tile_iron, building_type::extraction_site, resource_type::iron_ore);
    check(est_ext.has_data, "P.R2 extraction candidate has data");
    {
        building_component synthetic{};
        synthetic.tile               = tile_iron;
        synthetic.type               = building_type::extraction_site;
        synthetic.workforce_assigned = 0.5f;   // what construct_building authors
        synthetic.target_resource    = resource_type::iron_ore;
        const building_opex opex = compute_building_opex(
            synthetic, reg.economics(building_type::extraction_site),
            /*contention=*/1.0f, body_mean_habitability(w, body));
        check(near(est_ext.maintenance, opex.maintenance),
              "P.R2 maintenance == compute_building_opex", est_ext.maintenance, opex.maintenance);
        check(near(est_ext.wages, opex.wages),
              "P.R2 wages == compute_building_opex", est_ext.wages, opex.wages);
    }

    // ---------------------------------------------------------------------
    // P.R3 — extraction revenue == run_economy_step's realised output, priced at
    // the same market. Build the candidate for real, run one tick, compare.
    // ---------------------------------------------------------------------
    {
        world wx = w; // copy the fixture so later checks see an unbuilt tile
        entity_id built = null_entity;
        const construction_result cr = construct_building(
            wx, reg, corp, tile_iron, building_type::extraction_site,
            resource_type::iron_ore, built);
        check(cr == construction_result::placed && built != null_entity,
              "P.R3 candidate constructs for the cross-check");

        // Pin the BL-181 workforce dial. The estimate models workforce_target 100 —
        // building_component's own default, and what construct_building authors — but
        // the very first economy tick auto-solves that dial for the player's corp and
        // moves it (here to 200, doubling realised output). That is real, sanctioned
        // behaviour, not a defect in the estimator; it is simply a LATER decision than
        // the moment the ledger prices. Opt out of it so this check isolates the
        // extraction formula rather than measuring the solver.
        wx.buildings[built].workforce_auto   = false;
        wx.buildings[built].workforce_target = 100;

        const economy_report rep = run_economy_step(wx, reg);
        float realised = 0.0f;
        for (const building_report& br : rep.buildings)
            if (br.building == built) realised = br.output_quantity;

        check(realised > 0.0f, "P.R3 the built site actually produced", realised, 1.0f);
        check(near(est_ext.revenue, realised * p_iron, 1e-2f),
              "P.R3 estimated revenue == realised output x market price",
              est_ext.revenue, realised * p_iron);
        check(near(est_ext.input_cost, 0.0f),
              "P.R3 extraction has no input cost", est_ext.input_cost, 0.0f);
        check(est_ext.net() > 0.0f, "P.R3 a rich unworked deposit nets positive", est_ext.net(), 1.0f);
    }

    // ---------------------------------------------------------------------
    // P.R4 — the depletion taper. resource_remaining is the TILE's reserve, drawn
    // down by every site that ever worked it, so a spent tile must estimate low —
    // and below run_extraction's floor it must estimate zero revenue (a negative
    // net), not the tallest bar in the ledger.
    // ---------------------------------------------------------------------
    {
        // nominal = base_rate(20) * richness(2) * wf(0.5) * (1 - hazard 0) = 20/tick.
        // taper_band = deposit_taper_ticks(8) * nominal = 160.
        const float nominal = 20.0f;

        world wt = w;
        wt.tiles[tile_iron].resource_remaining[ri(resource_type::iron_ore)] = 80.0f; // taper 0.5
        const building_profit half = estimate_prospective_profit(
            wt, reg, tile_iron, building_type::extraction_site, resource_type::iron_ore);
        check(near(half.revenue, nominal * 0.5f * p_iron, 1e-2f),
              "P.R4 half-spent reserve halves the estimated revenue",
              half.revenue, nominal * 0.5f * p_iron);
        check(half.revenue < est_ext.revenue,
              "P.R4 a worked deposit estimates below an untouched one");

        wt.tiles[tile_iron].resource_remaining[ri(resource_type::iron_ore)] = 1.0f; // taper < 0.05
        const building_profit spent = estimate_prospective_profit(
            wt, reg, tile_iron, building_type::extraction_site, resource_type::iron_ore);
        check(near(spent.revenue, 0.0f),
              "P.R4 below run_extraction's floor -> zero revenue", spent.revenue, 0.0f);
        check(spent.net() < 0.0f,
              "P.R4 a worked-out deposit nets NEGATIVE (sorts last, red bar)", spent.net(), -1.0f);
    }

    // ---------------------------------------------------------------------
    // P.R5 — processing: batches = base_rate * wf; revenue and input cost are the
    // recipe's outputs / inputs at that batch count, priced locally. Cross-checked
    // against run_processing via a real build with an ample input pool.
    // ---------------------------------------------------------------------
    const building_profit est_proc = estimate_prospective_profit(
        w, reg, tile_bare, building_type::processing_facility, resource_type::iron_ore, steel_id);
    {
        const float batches = 8.0f * 0.5f; // base_rate * workforce_assigned
        check(near(est_proc.revenue, 1.0f * batches * p_steel, 1e-2f),
              "P.R5 processing revenue == outputs x batches x price",
              est_proc.revenue, 1.0f * batches * p_steel);
        check(near(est_proc.input_cost, 2.0f * batches * p_iron, 1e-2f),
              "P.R5 processing input cost == inputs x batches x price",
              est_proc.input_cost, 2.0f * batches * p_iron);

        world wp = w;
        entity_id built = null_entity;
        const construction_result cr = construct_building(
            wp, reg, corp, tile_bare, building_type::processing_facility,
            resource_type::iron_ore, built, steel_id);
        check(cr == construction_result::placed && wp.buildings[built].recipe == steel_id,
              "P.R5 construct_building seeds the CHOSEN recipe (BL-162 seam)");
        // Give the corp the inputs the recipe needs so the run is not input-starved,
        // and pin the BL-181 dial for the same reason as P.R3 above.
        wp.pool_for(corp, body).quantities[ri(resource_type::iron_ore)] = 1000.0f;
        wp.buildings[built].workforce_auto   = false;
        wp.buildings[built].workforce_target = 100;
        const economy_report rep = run_economy_step(wp, reg);
        float realised = 0.0f;
        for (const building_report& br : rep.buildings)
            if (br.building == built) realised = br.output_quantity;
        check(near(est_proc.revenue, realised * p_steel, 1e-2f),
              "P.R5 estimated revenue == run_processing's realised output x price",
              est_proc.revenue, realised * p_steel);
    }

    // ---------------------------------------------------------------------
    // P.R6 — the recipe genuinely changes the answer. This is the whole reason the
    // ledger lists one row per recipe: a single steel-priced row would misreport.
    // ---------------------------------------------------------------------
    {
        recipe rations;
        rations.name = "food_rations";
        rations.inputs[ri(resource_type::iron_ore)] = 1.0f;
        rations.outputs[ri(resource_type::steel)]   = 3.0f; // stand-in high-margin output
        const uint16_t rations_id = reg.add_recipe(rations);

        const building_profit alt = estimate_prospective_profit(
            w, reg, tile_bare, building_type::processing_facility,
            resource_type::iron_ore, rations_id);
        check(!near(alt.net(), est_proc.net()),
              "P.R6 a different recipe gives a different net (per-recipe rows are load-bearing)",
              alt.net(), est_proc.net());

        const building_profit none = estimate_prospective_profit(
            w, reg, tile_bare, building_type::processing_facility, resource_type::iron_ore);
        check(near(none.revenue, 0.0f) && near(none.input_cost, 0.0f) && none.has_data,
              "P.R6 no_recipe prices nothing but still reports opex");
    }

    // ---------------------------------------------------------------------
    // P.R7 — infrastructure: no revenue, no inputs, staffed at zero, so net() is
    // exactly minus its upkeep (which is why the ledger draws it no bar).
    // ---------------------------------------------------------------------
    {
        const building_profit port = estimate_prospective_profit(
            w, reg, tile_bare, building_type::port, resource_type::iron_ore);
        check(port.has_data && near(port.revenue, 0.0f) && near(port.input_cost, 0.0f),
              "P.R7 port produces nothing directly");
        check(near(port.wages, 0.0f),
              "P.R7 port staffs at 0.0 (construct_building parity) -> no wages", port.wages, 0.0f);
        check(near(port.net(), -(port.maintenance + port.wages)),
              "P.R7 port net() is exactly its upkeep", port.net(), -(port.maintenance + port.wages));

        const building_profit hub = estimate_prospective_profit(
            w, reg, tile_bare, building_type::inland_logistics_hub, resource_type::iron_ore);
        check(near(hub.wages, 0.0f), "P.R7 inland logistics hub also staffs at 0.0", hub.wages, 0.0f);
    }

    // ---------------------------------------------------------------------
    // P.R8 — purity: the estimate mutates nothing. It is called every frame the
    // ledger is open, against a const world.
    // ---------------------------------------------------------------------
    {
        const std::size_t buildings_before = w.buildings.size();
        const float       reserve_before   =
            w.tiles[tile_iron].resource_remaining[ri(resource_type::iron_ore)];
        const float       balance_before   = w.corporations[corp].balance;

        for (int i = 0; i < 5; ++i)
            (void)estimate_prospective_profit(w, reg, tile_iron, building_type::extraction_site,
                                              resource_type::iron_ore);
        const building_profit again = estimate_prospective_profit(
            w, reg, tile_iron, building_type::extraction_site, resource_type::iron_ore);

        check(w.buildings.size() == buildings_before &&
              near(w.tiles[tile_iron].resource_remaining[ri(resource_type::iron_ore)], reserve_before) &&
              near(w.corporations[corp].balance, balance_before),
              "P.R8 repeated calls mutate no world state");
        check(near(again.net(), est_ext.net()),
              "P.R8 repeated calls give the identical answer (no caches, no RNG)",
              again.net(), est_ext.net());
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
