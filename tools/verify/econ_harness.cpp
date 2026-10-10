// Throwaway headless harness for the Layer 3 economy logic (no SDL / Lua / ImGui).
// Builds a tiny world + a hand-built recipe registry, runs one economy tick
// (production -> market clearing -> budget), and asserts the documented outcomes.
// Kept outside src/ so the CMake glob does not pull it into the real build.
//
// Build (from repo root, after sourcing vcvars64):
//   cl /nologo /std:c++20 /EHsc /I src econ_harness.cpp ^
//      src\world\world.cpp src\world\economy_system.cpp ^
//      src\world\market_clearing.cpp src\world\budget_system.cpp ^
//      /Fo:build_gen\verify\econ_harness\ /Fe:build_gen\verify\econ_harness.exe
// Run: .\build_gen\verify\econ_harness.exe

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

static int g_failures = 0;

static void check(bool cond, const char* what, float got = 0.0f, float want = 0.0f)
{
    if (cond)
    {
        std::printf("  PASS  %s\n", what);
    }
    else
    {
        std::printf("  FAIL  %s   (got %.3f, want %.3f)\n", what, got, want);
        ++g_failures;
    }
}

static bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

static std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

int main()
{
    world w;

    // --- registry (hand-built; mirrors scripts/economy.lua + recipes.lua) ---
    recipe_registry reg;
    reg.set_thresholds(/*t_full=*/1.0f, /*t_idle=*/0.2f);
    {
        // BL-1172/BL-1209: the shelf's share of supply at the SHIPPED k = 1 (Ben,
        // 2026-10-07, under the k x (demand + silenced want) law; restated —
        // this harness reads no Lua). Band multipliers keep their defaults;
        // reservation_mult stays 0, the ceiling OFF, so processors buy as ever
        // and nothing is silenced (the register is 0).
        price_band_params pb;
        pb.shelf_supply_ticks = 1.0f;
        reg.set_price_band(pb);
    }
    {
        building_economics ex; ex.base_rate = 20.0f; ex.maintenance = 5.0f;  ex.base_wage = 8.0f;
        reg.set_economics(building_type::extraction_site, ex);
        building_economics pr; pr.base_rate = 8.0f;  pr.maintenance = 10.0f; pr.base_wage = 12.0f;
        reg.set_economics(building_type::processing_facility, pr);
    }
    recipe steel;
    steel.name = "steel";
    steel.inputs[ri(resource_type::iron_ore)] = 2.0f;
    steel.outputs[ri(resource_type::steel)]   = 1.0f;
    const uint16_t steel_id = reg.add_recipe(steel);

    // --- a body + market ---
    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};
    w.bodies[body].name = "TestWorld";

    const entity_id market = w.create_entity();
    market_component mc;
    mc.body = body;
    mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
    mc.base_price[ri(resource_type::steel)]    = 8.0f;
    mc.price = mc.base_price;
    // BL-130: real inventory gates what a processor can draw. Ample stock here
    // restores this fixture's original intent — a market backs P's need so it
    // runs a full batch. BL-1265: with no pools, the shelf is P's WHOLE source.
    mc.inventory[ri(resource_type::iron_ore)] = 1000.0f;
    w.markets[market] = mc;

    // --- extraction corp E: iron_ore richness 2.0, workforce 0.5, no hazard ---
    const entity_id tile_e = w.create_entity();
    {
        tile_component tc{};
        tc.body = body;
        tc.substrate = terrain_substrate::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)] = 2.0f;
        // Ample reserve so this tick runs at full rate (depletion taper untouched).
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f;
        tc.hazard_level = 0.0f;
        w.tiles[tile_e] = tc;
    }
    const entity_id bld_e = w.create_entity();
    {
        building_component b{};
        b.tile = tile_e;
        b.type = building_type::extraction_site;
        b.workforce_assigned = 0.5f;
        b.target_resource = resource_type::iron_ore;
        w.buildings[bld_e] = b;
    }
    const entity_id corp_e = w.create_entity();
    {
        corporation_component cc;
        cc.name = "Extractor Co";
        cc.starting_capital = 1000.0f;
        cc.balance = 1000.0f;
        cc.assets.push_back(bld_e);
        // NOT AI-DRIVEN (added 2026-07-31). run_economy_step stopped being pure
        // economy arithmetic when BL-202's strategic tier landed: it commands every
        // non-player corp at the end of the tick, and a workforce command rewrites
        // workforce_target, which scales BOTH maintenance and wages
        // (compute_building_opex). This fixture exists to pin the wage/maintenance
        // formula, so the AI's decisions have to be out of the picture or the
        // assertions below measure the wrong thing. `is_player` is the supported
        // "this corp is not AI-driven" exclusion (corp_ai.cpp: is_player || ==
        // player_entity). Every asserted value is UNCHANGED by this - it restores
        // the isolation the assertions always assumed rather than relaxing them.
        cc.is_player = true;
        w.corporations[corp_e] = cc;
    }

    // --- processing corp P: steel recipe, workforce 0.5, fed off the shelf ---
    const entity_id tile_p = w.create_entity();
    {
        tile_component tc{};
        tc.body = body;
        tc.substrate = terrain_substrate::sedimentary; tc.cover = terrain_cover::grass; tc.cover_density = 150;
        w.tiles[tile_p] = tc;
    }
    const entity_id bld_p = w.create_entity();
    {
        building_component b{};
        b.tile = tile_p;
        b.type = building_type::processing_facility;
        b.workforce_assigned = 0.5f;
        b.recipe = steel_id;
        w.buildings[bld_p] = b;
    }
    const entity_id corp_p = w.create_entity();
    {
        corporation_component cc;
        cc.name = "Smelter Co";
        cc.starting_capital = 1000.0f;
        cc.balance = 1000.0f;
        cc.assets.push_back(bld_p);
        cc.is_player = true; // not AI-driven — see corp_e above
        w.corporations[corp_p] = cc;
    }
    // BL-1265: no pool to seed — P buys its whole need (8 iron) off the shelf.

    // --- run one tick ---
    economy_report rep = run_economy_step(w, reg);
    auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention);

    std::printf("Layer 3 economy harness\n");

    // Extraction: output = 20 * 2.0 * 0.5 * 1.0 = 20 (landed, and sold at the clear).
    // Find E's report.
    float e_out = 0.0f, p_out = 0.0f; bool p_idle = true; bool p_has_lim = false;
    for (const auto& br : rep.buildings)
    {
        if (br.building == bld_e) e_out = br.output_quantity;
        if (br.building == bld_p) { p_out = br.output_quantity; p_idle = br.idle; p_has_lim = br.has_limiting; }
    }
    check(near(e_out, 20.0f), "R3.1 extraction output = base*richness*workforce*(1-hazard)", e_out, 20.0f);

    // Processing: batches_full = 8*0.5 = 4 -> steel produced = 4; iron bought = 8 (no pool).
    check(near(p_out, 4.0f), "R3.2 processing produces full batch outputs", p_out, 4.0f);
    check(!p_idle && p_has_lim, "R3.2 processor active with a limiting input", p_idle ? 1.0f : 0.0f, 0.0f);
    check(near(rep.purchases[{corp_p, market}][ri(resource_type::iron_ore)], 8.0f),
          "R4.2 bought off the shelf = the whole need = 8 (BL-1265: no pool covers part of it)",
          rep.purchases[{corp_p, market}][ri(resource_type::iron_ore)], 8.0f);

    // Deposit depletion (Brief B, R2): the reserve is drawn down by the output.
    check(near(w.tiles[tile_e].resource_remaining[ri(resource_type::iron_ore)], 1.0e6f - 20.0f),
          "B.R2 extraction draws the reserve down by its output",
          w.tiles[tile_e].resource_remaining[ri(resource_type::iron_ore)], 1.0e6f - 20.0f);

    // Market supply/demand (R4.1/R4.2): steel supply 4 (P's landing), iron demand 8.
    const market_component& m = w.markets[market];
    check(near(m.supply[ri(resource_type::steel)], 4.0f), "R4.1 market supply = the landed output (steel 4)", m.supply[ri(resource_type::steel)], 4.0f);
    check(near(m.demand[ri(resource_type::iron_ore)], 8.0f), "R4.2 market demand = the posted want (iron 8)", m.demand[ri(resource_type::iron_ore)], 8.0f);

    // Price resolution (Brief A, R1/R2): target = base*sqrt(D/S), clamped, EMA from base.
    // BL-1172/BL-1209 (MARKETS.md § Price resolution): S is the listings plus
    // the shelf's share min(inventory, k x (demand + silenced want)); shipped
    // k = 1 (Ben, 2026-10-07), silenced want 0 here (the ceiling is OFF).
    // BL-1265 moved this row: P buys all 8 off the shelf (it drew 4 from its
    // pool before), so D = 8 and the shelf's share is 8:
    //   iron: S = 20 (E's landing) + min(992, 1 x 8) = 28, D = 8
    //         -> base 2.5 * sqrt(8/28) = 1.336306; EMA 2.5 + 0.5*(1.336306-2.5) = 1.918153
    //         (was 1.760310 with P's pool covering 4)
    //   steel: S=4 D=0  -> target 0 -> floor 0.25*8=2.0; EMA 8 + 0.5*(2-8) = 5.0
    check(near(m.price[ri(resource_type::iron_ore)], 1.918153f),
          "A.R1/R2 iron price eased toward base*sqrt(D/S), S = landings + the shelf's share (shipped k = 1)", m.price[ri(resource_type::iron_ore)], 1.918153f);
    check(near(m.price[ri(resource_type::steel)], 5.0f),
          "A.R2 steel price floored (no demand) and eased from base", m.price[ri(resource_type::steel)], 5.0f);

    // Budget (Brief A, R3 + L3 R5): sales valued at the resolved price; a shelf
    // draw billed at the POSTED price it was decided against (BL-1172, FINANCE.md
    // § Standing-force upkeep, Ben 2026-10-03: "A draw pays the posted price").
    //   E: income 20*1.918153=38.363 (its LANDING, paid at the clearing price),
    //      maint 5, wage 0.5*8=4 -> +29.363 -> 1029.363   (was 1026.206)
    //   P: income 4*5=20, expend 8*2.5 (iron posted at base) = 20, maint 10,
    //      wage 0.5*12=6 -> -16 -> 984.000   (was 994.000: its pool covered 4)
    check(near(w.corporations[corp_e].balance, 1029.363f), "A.R3 extraction corp balance: its landing paid at the resolved price (shipped k = 1)", w.corporations[corp_e].balance, 1029.363f);
    check(near(w.corporations[corp_p].balance, 984.0f),    "A.R3 processing corp balance: sales at resolved, the shelf draw at posted", w.corporations[corp_p].balance, 984.0f);

    // R3.3 idle below t_idle: zero P's workforce-pool scenario -> empty pool, run again.
    {
        world w2;
        recipe_registry r2 = reg;
        const entity_id b2 = w2.create_entity(); w2.bodies[b2] = body_component{};
        const entity_id t2 = w2.create_entity();
        tile_component tc{}; tc.body = b2; w2.tiles[t2] = tc;
        const entity_id pb = w2.create_entity();
        building_component bc{}; bc.tile = t2; bc.type = building_type::processing_facility;
        bc.workforce_assigned = 0.5f; bc.recipe = steel_id; w2.buildings[pb] = bc;
        const entity_id pc = w2.create_entity();
        corporation_component cc; cc.balance = 0.0f; cc.assets.push_back(pb); w2.corporations[pc] = cc;
        // no pool, no market -> coverage 0 < t_idle -> idle
        economy_report r = run_economy_step(w2, r2);
        bool idle = false;
        for (const auto& br : r.buildings) if (br.building == pb) idle = br.idle;
        check(idle, "R3.3 processor idles below t_idle (nothing to buy)");
    }

    // --- Brief B: deposit depletion taper + exhaustion (R3, R4) ---
    // nominal = base_rate 20 * richness 1 * workforce 1 * (1-hazard) = 20.
    // taper_band = deposit_taper_ticks(8) * nominal = 160; exhausted below 5% -> remaining < 8.
    {
        auto run_once = [&](float remaining, float& out, bool& exhausted)
        {
            world wd;
            const entity_id bd = wd.create_entity(); wd.bodies[bd] = body_component{};
            { market_component mc{}; mc.body = bd; wd.markets[wd.create_entity()] = mc; } // BL-1265: output lands on a market
            const entity_id td = wd.create_entity();
            tile_component tc{}; tc.body = bd;
            tc.resource_deposit[ri(resource_type::iron_ore)]   = 1.0f;
            tc.resource_remaining[ri(resource_type::iron_ore)] = remaining;
            wd.tiles[td] = tc;
            const entity_id eb = wd.create_entity();
            building_component b{}; b.tile = td; b.type = building_type::extraction_site;
            b.workforce_assigned = 1.0f; b.target_resource = resource_type::iron_ore;
            wd.buildings[eb] = b;
            const entity_id ec = wd.create_entity();
            corporation_component cc; cc.assets.push_back(eb); wd.corporations[ec] = cc;
            economy_report r = run_economy_step(wd, reg);
            out = 0.0f; exhausted = false;
            for (const auto& br : r.buildings) if (br.building == eb) { out = br.output_quantity; exhausted = br.exhausted; }
        };

        float out = 0.0f; bool ex = false;
        run_once(400.0f, out, ex); // 400/160 clamps to 1 -> full rate
        check(near(out, 20.0f) && !ex, "B.R2 full-rate draw at ample reserve (out=20)", out, 20.0f);
        run_once(80.0f, out, ex);  // 80/160 = 0.5 -> half rate
        check(near(out, 10.0f) && !ex, "B.R3 output tapers as the reserve nears empty (out=10)", out, 10.0f);
        run_once(5.0f, out, ex);   // 5/160 = 0.031 < 0.05 -> exhausted
        check(near(out, 0.0f) && ex, "B.R4 reports exhausted (out of resources) below the floor", out, 0.0f);
    }

    // R5 (uncontended): the main scenario's single-building corps demand 0.5 each,
    // well under default supply 3.0 — contention scalar is 1.0, so every assertion
    // above (which assumed no throttling) still holds.
    check(near(rep.workforce_contention[{corp_e, body}], 1.0f),
          "WF.R5 single-building corp is uncontended (scalar 1.0)",
          rep.workforce_contention[{corp_e, body}], 1.0f);

    // --- Workforce pool, step 1: a contended (corp, body) clears by WAGE ---
    // BL-614 (wage competition, POPULATION.md § Contention): four extraction
    // sites (workforce 1.0 each) -> demand 4.0 > default supply 3.0. The pool
    // AGGREGATE min(1, supply/demand) = 0.75 survives as the report figure, but
    // allocation is per building: with no wage_bid set every offered wage ties,
    // the id-ascending order decides, and the pool staffs the first three sites
    // IN FULL and the fourth NOT AT ALL — priced scarcity, not silent averaging.
    // Total effective labour (3.0) and total output (60) match the superseded
    // proportional model exactly; where the labour LANDS is what changed.
    {
        world ww;
        const entity_id wb = ww.create_entity(); ww.bodies[wb] = body_component{};
        { market_component mc{}; mc.body = wb; ww.markets[ww.create_entity()] = mc; } // BL-1265: output lands on a market
        corporation_component cc; cc.balance = 1000.0f;
        cc.is_player = true; // not AI-driven — see corp_e above
        std::vector<entity_id> site_ids;
        for (int i = 0; i < 4; ++i)
        {
            const entity_id t = ww.create_entity();
            tile_component tc{}; tc.body = wb;
            tc.resource_deposit[ri(resource_type::iron_ore)]   = 1.0f;
            tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f;
            ww.tiles[t] = tc;
            const entity_id eb = ww.create_entity();
            building_component b{}; b.tile = t; b.type = building_type::extraction_site;
            b.workforce_assigned = 1.0f; b.target_resource = resource_type::iron_ore;
            ww.buildings[eb] = b;
            cc.assets.push_back(eb);
            site_ids.push_back(eb);
        }
        const entity_id wc = ww.create_entity();
        ww.corporations[wc] = cc;

        economy_report wr = run_economy_step(ww, reg);
        check(near(wr.workforce_contention[{wc, wb}], 0.75f),
              "WF.R2 pool aggregate = min(1, supply/demand) = 3/4",
              wr.workforce_contention[{wc, wb}], 0.75f);

        check(near(wr.building_labour[site_ids[0]], 1.0f)
                  && near(wr.building_labour[site_ids[1]], 1.0f)
                  && near(wr.building_labour[site_ids[2]], 1.0f),
              "WF.R3 (BL-614) the first three sites by id are staffed in full",
              wr.building_labour[site_ids[0]], 1.0f);
        check(near(wr.building_labour[site_ids[3]], 0.0f),
              "WF.R3 (BL-614) the marginal site gets nothing (pool spent)",
              wr.building_labour[site_ids[3]], 0.0f);

        float total_out = 0.0f;
        for (const auto& br : wr.buildings)
            if (br.corp == wc) total_out += br.output_quantity;
        check(near(total_out, 60.0f),
              "WF.R3 (BL-614) total output = supply's worth (3 * 20), conserved vs the proportional model",
              total_out, 60.0f);

        // Wages on ALLOCATED workforce: 3 staffed * (1.0 * base_wage 8) = 24 —
        // the same bill the proportional 4 * 0.75 * 8 charged, landing on the
        // staffed buildings only. Passing building_labour exercises the BL-614
        // per-building wage path in apply_budget.
        auto wf = clear_markets(ww, reg, wr);
        const float before = ww.corporations[wc].balance;
        apply_budget(ww, reg, wf, wr.workforce_contention, nullptr, nullptr,
                     &wr.building_labour);
        check(near(before - ww.corporations[wc].balance, 20.0f + 24.0f),
              "WF.R4 wages paid on allocated workforce (maint 20 + wages 24)",
              before - ww.corporations[wc].balance, 44.0f);
    }

    // --- BL-614: a wage bid MOVES the allocation, and is paid as offered ---
    // Same fixture, but the LAST-placed site bids +50 % (offered 12 vs 8). It
    // now sorts first and staffs in full; the tie among the other three breaks
    // by id, so the THIRD-placed site is the one squeezed out. Wages: the
    // bidder pays 12, the two staffed non-bidders 8 each -> 28 total, against
    // 24 had nobody bid — outbidding your own buildings costs real money.
    {
        world ww;
        const entity_id wb = ww.create_entity(); ww.bodies[wb] = body_component{};
        { market_component mc{}; mc.body = wb; ww.markets[ww.create_entity()] = mc; } // BL-1265: output lands on a market
        corporation_component cc; cc.balance = 1000.0f;
        cc.is_player = true;
        std::vector<entity_id> site_ids;
        for (int i = 0; i < 4; ++i)
        {
            const entity_id t = ww.create_entity();
            tile_component tc{}; tc.body = wb;
            tc.resource_deposit[ri(resource_type::iron_ore)]   = 1.0f;
            tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f;
            ww.tiles[t] = tc;
            const entity_id eb = ww.create_entity();
            building_component b{}; b.tile = t; b.type = building_type::extraction_site;
            b.workforce_assigned = 1.0f; b.target_resource = resource_type::iron_ore;
            if (i == 3)
                b.wage_bid = 0.5f; // offered wage 8 * 1.5 = 12
            ww.buildings[eb] = b;
            cc.assets.push_back(eb);
            site_ids.push_back(eb);
        }
        const entity_id wc = ww.create_entity();
        ww.corporations[wc] = cc;

        economy_report wr = run_economy_step(ww, reg);
        check(near(wr.building_labour[site_ids[3]], 1.0f),
              "WF.R6 (BL-614) the bidding site wins full staffing over lower ids",
              wr.building_labour[site_ids[3]], 1.0f);
        check(near(wr.building_labour[site_ids[0]], 1.0f)
                  && near(wr.building_labour[site_ids[1]], 1.0f),
              "WF.R6 (BL-614) the first two non-bidders keep their staffing",
              wr.building_labour[site_ids[0]], 1.0f);
        check(near(wr.building_labour[site_ids[2]], 0.0f),
              "WF.R6 (BL-614) the last non-bidder by id is the one squeezed out",
              wr.building_labour[site_ids[2]], 0.0f);

        auto wf = clear_markets(ww, reg, wr);
        const float before = ww.corporations[wc].balance;
        apply_budget(ww, reg, wf, wr.workforce_contention, nullptr, nullptr,
                     &wr.building_labour);
        check(near(before - ww.corporations[wc].balance, 20.0f + 28.0f),
              "WF.R6 (BL-614) wages paid at the OFFERED rate (maint 20 + wages 8+8+12)",
              before - ww.corporations[wc].balance, 48.0f);
    }

    // --- A landing sells at the RESOLVED price (BL-1265; was BL-386's sell order) ---
    // A corp lands 10 steel on a body whose market trades steel at base 8 with
    // no demand. Resolved price floors: target base*sqrt(0/10)=0 -> 0.25*8=2, EMA
    // from prior 8 -> 5.0. The landing sells all 10 at the RESOLVED price 5 — the
    // market is the counterparty, bid or no bid (MARKETS.md § The shelf economy).
    // The sell order, its floor, and the BL-351 over-commit rows retired with
    // the order book.
    {
        world ws;
        const entity_id b = ws.create_entity(); ws.bodies[b] = body_component{};
        const entity_id m = ws.create_entity();
        market_component mc{}; mc.body = b;
        mc.base_price[ri(resource_type::steel)] = 8.0f;
        mc.price = mc.base_price;
        ws.markets[m] = mc;
        const entity_id corp = ws.create_entity();
        { corporation_component cc; cc.balance = 0.0f; ws.corporations[corp] = cc; }
        ws.land_goods(corp, m, ri(resource_type::steel), 10.0f);

        economy_report empty; // no production this scenario
        auto f = clear_markets(ws, reg, empty);
        check(near(ws.markets[m].price[ri(resource_type::steel)], 5.0f),
              "SO.1 steel price floored+eased to 5.0", ws.markets[m].price[ri(resource_type::steel)], 5.0f);
        check(near(f[corp].income, 50.0f),
              "SO.2 the landing sells all 10 at the resolved price 5 (income 50)", f[corp].income, 50.0f);
        check(near(ws.markets[m].inventory[ri(resource_type::steel)], 10.0f) && ws.landed_this_tick.empty(),
              "SO.3 the landing is spent onto the shelf (inventory 10)",
              ws.markets[m].inventory[ri(resource_type::steel)], 10.0f);
    }

    // --- BL-351 rows (duplicate sell orders cannot over-commit the pool; a
    // multi-order seller's remainder clears per order) RETIRED with the order
    // book and corporation pools (BL-1265, MARKETS.md § The shelf economy):
    // there are no orders to duplicate and no pool to over-commit. ---
    // --- Multiple markets per body: nearest-centre catchment routing ---
    // A body carries two markets centred on tiles 100 columns apart. A tile near
    // each centre resolves (market_for_tile) to that centre's market, and a corp
    // whose building sits in one catchment lists its surplus in that market only.
    {
        world wm;
        const entity_id b = wm.create_entity(); wm.bodies[b] = body_component{};

        const entity_id centre_a = wm.create_entity();
        { tile_component tc{}; tc.body = b; tc.grid_x = 0;   tc.grid_y = 0; wm.tiles[centre_a] = tc; }
        const entity_id centre_b = wm.create_entity();
        { tile_component tc{}; tc.body = b; tc.grid_x = 100; tc.grid_y = 0; wm.tiles[centre_b] = tc; }

        const entity_id mkt_a = wm.create_entity();
        { market_component mc{}; mc.body = b; mc.centre_tile = centre_a;
          mc.base_price[ri(resource_type::steel)] = 8.0f; mc.price = mc.base_price; wm.markets[mkt_a] = mc; }
        const entity_id mkt_b = wm.create_entity();
        { market_component mc{}; mc.body = b; mc.centre_tile = centre_b;
          mc.base_price[ri(resource_type::steel)] = 8.0f; mc.price = mc.base_price; wm.markets[mkt_b] = mc; }

        const entity_id tile_a = wm.create_entity();
        { tile_component tc{}; tc.body = b; tc.grid_x = 10; tc.grid_y = 0; wm.tiles[tile_a] = tc; }
        const entity_id tile_b = wm.create_entity();
        { tile_component tc{}; tc.body = b; tc.grid_x = 90; tc.grid_y = 0; wm.tiles[tile_b] = tc; }
        check(market_for_tile(wm, tile_a) == mkt_a, "MM.1 tile near centre A routes to market A");
        check(market_for_tile(wm, tile_b) == mkt_b, "MM.2 tile near centre B routes to market B");

        const entity_id corp_a = wm.create_entity();
        { corporation_component cc; const entity_id bid = wm.create_entity();
          building_component bld{}; bld.tile = tile_a; wm.buildings[bid] = bld;
          cc.assets.push_back(bid); wm.corporations[corp_a] = cc; }
        const entity_id corp_b = wm.create_entity();
        { corporation_component cc; const entity_id bid = wm.create_entity();
          building_component bld{}; bld.tile = tile_b; wm.buildings[bid] = bld;
          cc.assets.push_back(bid); wm.corporations[corp_b] = cc; }
        // BL-1265: each corp's goods LAND on ITS building's catchment market.
        wm.land_goods(corp_a, market_for_tile(wm, tile_a), ri(resource_type::steel), 10.0f);
        wm.land_goods(corp_b, market_for_tile(wm, tile_b), ri(resource_type::steel), 10.0f);

        economy_report empty;
        clear_markets(wm, reg, empty);
        check(near(wm.markets[mkt_a].supply[ri(resource_type::steel)], 10.0f),
              "MM.3 corp A's landing lists in catchment market A",
              wm.markets[mkt_a].supply[ri(resource_type::steel)], 10.0f);
        check(near(wm.markets[mkt_b].supply[ri(resource_type::steel)], 10.0f),
              "MM.4 corp B's landing lists in catchment market B",
              wm.markets[mkt_b].supply[ri(resource_type::steel)], 10.0f);
    }

    // --- Qualified pool (BL-613): a deep method throttles against the host
    // nation's qualification, and unthrottles when the nation has the heads.
    // One Town (scale 3 -> 10 labour units) carries the body's whole pool; the
    // recipe wants half its labour qualified. qualification 0 -> the qualified
    // pool is empty and the method IDLES; qualification 1 -> ample, full run.
    // (POPULATION.md § Qualification; the R2 row of the BL-613 group.)
    {
        recipe deep;
        deep.name = "deep_steel";
        deep.inputs[ri(resource_type::iron_ore)] = 2.0f;
        deep.outputs[ri(resource_type::steel)]   = 1.0f;
        deep.qualified_workforce = 0.5f;
        const uint16_t deep_id = reg.add_recipe(deep);

        auto run_with_qualification = [&](float qual) -> std::pair<float, float> {
            world qw;
            const entity_id qb = qw.create_entity();
            qw.bodies[qb] = body_component{};
            const entity_id nid = qw.create_entity();
            nation_component nc; nc.name = "Qualland"; nc.qualification = qual;
            qw.nations[nid] = nc;
            const entity_id t_centre = qw.create_entity();
            { tile_component tc{}; tc.body = qb; qw.tiles[t_centre] = tc; }
            const entity_id centre = qw.create_entity();
            population_centre_component pcc; pcc.scale = 3; pcc.population = 200;
            qw.population_centres[centre] = pcc;
            qw.population_centre_tile[centre] = t_centre;
            const entity_id t_bld = qw.create_entity();
            { tile_component tc{}; tc.body = qb; qw.tiles[t_bld] = tc; }
            qw.tile_to_nation[t_centre] = nid;
            qw.tile_to_nation[t_bld]    = nid;
            const entity_id eb = qw.create_entity();
            building_component b{}; b.tile = t_bld;
            b.type = building_type::processing_facility;
            b.workforce_assigned = 0.5f; b.recipe = deep_id;
            qw.buildings[eb] = b;
            const entity_id ec = qw.create_entity();
            corporation_component cc; cc.balance = 1000.0f; cc.is_player = true;
            cc.assets.push_back(eb);
            qw.corporations[ec] = cc;
            // BL-1265: the iron is on a market's shelf on the body (no pools).
            const entity_id qm = qw.create_entity();
            { market_component qmc{}; qmc.body = qb;
              qmc.base_price[ri(resource_type::iron_ore)] = 2.5f; qmc.price = qmc.base_price;
              qmc.inventory[ri(resource_type::iron_ore)]  = 100.0f; qw.markets[qm] = qmc; }
            economy_report r = run_economy_step(qw, reg);
            float out = -1.0f;
            for (const auto& br : r.buildings)
                if (br.building == eb) out = br.output_quantity;
            const auto qc = r.qualified_contention.find({nid, qb});
            const float scalar = (qc != r.qualified_contention.end()) ? qc->second : 1.0f;
            return { out, scalar };
        };

        const auto [out0, sc0] = run_with_qualification(0.0f);
        check(near(out0, 0.0f), "QF.R1 (BL-613) a deep method IDLES in a zero-qualification nation", out0, 0.0f);
        check(near(sc0, 0.0f),  "QF.R2 (BL-613) the qualified pool reports zero grant (contention 0)", sc0, 0.0f);
        const auto [out1, sc1] = run_with_qualification(1.0f);
        check(out1 > 0.0f, "QF.R3 (BL-613) the same method RUNS when the nation is qualified", out1, 0.0f);
        check(near(sc1, 1.0f), "QF.R4 (BL-613) ample qualified pool grants in full (contention 1)", sc1, 1.0f);
    }

    std::printf("\n%s  (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES", g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
