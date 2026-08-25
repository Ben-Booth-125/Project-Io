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
    // BL-130: real inventory now gates what a processor can draw beyond its own
    // pool. Ample stock here restores this fixture's original intent — a market
    // backs P's shortfall so it runs a full batch, same as the pre-BL-130
    // unconditional auto-buy did when a market existed at all.
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

    // --- processing corp P: steel recipe, workforce 0.5, seeded pool of iron ---
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
    // Seed P's pool with 4 iron ore -> coverage 4/8 = 0.5 (between t_idle and t_full).
    w.pool_for(corp_p, body).quantities[ri(resource_type::iron_ore)] = 4.0f;

    // --- run one tick ---
    economy_report rep = run_economy_step(w, reg);
    auto flows = clear_markets(w, reg, rep);
    apply_budget(w, reg, flows, rep.workforce_contention);

    std::printf("Layer 3 economy harness\n");

    // Extraction: output = 20 * 2.0 * 0.5 * 1.0 = 20 (then all sold -> pool 0).
    // Find E's report.
    float e_out = 0.0f, p_out = 0.0f; bool p_idle = true; bool p_has_lim = false;
    for (const auto& br : rep.buildings)
    {
        if (br.building == bld_e) e_out = br.output_quantity;
        if (br.building == bld_p) { p_out = br.output_quantity; p_idle = br.idle; p_has_lim = br.has_limiting; }
    }
    check(near(e_out, 20.0f), "R3.1 extraction output = base*richness*workforce*(1-hazard)", e_out, 20.0f);

    // Processing: batches_full = 8*0.5 = 4 -> steel produced = 4; iron bought = 8-4 = 4.
    check(near(p_out, 4.0f), "R3.2 processing produces full batch outputs", p_out, 4.0f);
    check(!p_idle && p_has_lim, "R3.2 processor active with a limiting input", p_idle ? 1.0f : 0.0f, 0.0f);
    check(near(rep.purchases[{corp_p, body}][ri(resource_type::iron_ore)], 4.0f),
          "R4.2 auto-bought shortfall = need - pool = 4", rep.purchases[{corp_p, body}][ri(resource_type::iron_ore)], 4.0f);

    // Deposit depletion (Brief B, R2): the reserve is drawn down by the output.
    check(near(w.tiles[tile_e].resource_remaining[ri(resource_type::iron_ore)], 1.0e6f - 20.0f),
          "B.R2 extraction draws the reserve down by its output",
          w.tiles[tile_e].resource_remaining[ri(resource_type::iron_ore)], 1.0e6f - 20.0f);

    // Market supply/demand (R4.1/R4.2): steel supply 4, iron demand 4.
    const market_component& m = w.markets[market];
    check(near(m.supply[ri(resource_type::steel)], 4.0f), "R4.1 market supply = listed surplus (steel 4)", m.supply[ri(resource_type::steel)], 4.0f);
    check(near(m.demand[ri(resource_type::iron_ore)], 4.0f), "R4.2 market demand = auto-bought (iron 4)", m.demand[ri(resource_type::iron_ore)], 4.0f);

    // Price resolution (Brief A, R1/R2): target = base*sqrt(D/S), clamped, EMA from base.
    //   iron: S=20 D=4  -> base2.5 * sqrt(0.2)=1.118; EMA 2.5 + 0.5*(1.118-2.5) = 1.809
    //   steel: S=4 D=0  -> target 0 -> floor 0.25*8=2.0; EMA 8 + 0.5*(2-8) = 5.0
    check(near(m.price[ri(resource_type::iron_ore)], 1.809017f),
          "A.R1/R2 iron price eased toward base*sqrt(D/S)", m.price[ri(resource_type::iron_ore)], 1.809017f);
    check(near(m.price[ri(resource_type::steel)], 5.0f),
          "A.R2 steel price floored (no demand) and eased from base", m.price[ri(resource_type::steel)], 5.0f);

    // Budget (Brief A, R3 + L3 R5): cash flows valued at the resolved price.
    //   E: income 20*1.809017=36.180, maint 5, wage 0.5*8=4 -> +27.180 -> 1027.180
    //   P: income 4*5=20, expend 4*1.809017=7.236, maint 10, wage 0.5*12=6 -> -3.236 -> 996.764
    check(near(w.corporations[corp_e].balance, 1027.180f), "A.R3 extraction corp balance at resolved price", w.corporations[corp_e].balance, 1027.180f);
    check(near(w.corporations[corp_p].balance, 996.764f),  "A.R3 processing corp balance at resolved price", w.corporations[corp_p].balance, 996.764f);

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
        // empty pool -> coverage 0 < t_idle -> idle
        economy_report r = run_economy_step(w2, r2);
        bool idle = false;
        for (const auto& br : r.buildings) if (br.building == pb) idle = br.idle;
        check(idle, "R3.3 processor idles below t_idle (empty pool)");
    }

    // --- Brief B: deposit depletion taper + exhaustion (R3, R4) ---
    // nominal = base_rate 20 * richness 1 * workforce 1 * (1-hazard) = 20.
    // taper_band = deposit_taper_ticks(8) * nominal = 160; exhausted below 5% -> remaining < 8.
    {
        auto run_once = [&](float remaining, float& out, bool& exhausted)
        {
            world wd;
            const entity_id bd = wd.create_entity(); wd.bodies[bd] = body_component{};
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

    // --- Player sell orders: the floor is a reservation price (BL-386) ---
    // A corp pool holds 10 steel on a body whose market trades steel at base 8 with
    // no demand. Resolved price floors: target base*sqrt(0/10)=0 -> 0.25*8=2, EMA
    // from prior 8 -> 5.0. An order at floor 4 (below the market) clears all 10 at
    // the RESOLVED price 5 — never at the floor, never above the market. An order
    // whose floor exceeds the resolved price holds instead (order_book_harness R6
    // covers the hold side in depth).
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
        ws.pool_for(corp, b).quantities[ri(resource_type::steel)] = 10.0f;

        // The order goes into the WORLD's book (BL-293), not into a vector handed
        // to clear_markets — clearing reads the book itself now.
        sell_order o; o.id = ws.allocate_order_id();
        o.corp = corp; o.body = b; o.resource = resource_type::steel;
        o.quantity = 10.0f; o.floor_price = 4.0f;
        ws.sell_orders.push_back(o);

        economy_report empty; // no production this scenario
        auto f = clear_markets(ws, reg, empty);
        check(near(ws.markets[m].price[ri(resource_type::steel)], 5.0f),
              "SO.1 steel price floored+eased to 5.0", ws.markets[m].price[ri(resource_type::steel)], 5.0f);
        check(near(f[corp].income, 50.0f),
              "SO.2 standing order sells 10 at the resolved price 5, not the floor (income 50)", f[corp].income, 50.0f);
        check(near(ws.pool_for(corp, b).quantities[ri(resource_type::steel)], 0.0f),
              "SO.3 pool debited by the order", ws.pool_for(corp, b).quantities[ri(resource_type::steel)], 0.0f);
    }

    // --- BL-351: duplicate sell orders cannot over-commit the pool ---
    // Two identical orders (qty 10, floor 4) against a pool of 10 steel: the
    // second lists only the running remainder (0), so at most the pool clears,
    // the pool floors at 0, and income prices only the cleared quantity.
    // Price as SO.1: no demand -> ref 5.0; floor 4 permits, clearing pays 5.
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
        ws.pool_for(corp, b).quantities[ri(resource_type::steel)] = 10.0f;

        // BL-293: the book is world state, so the duplicate orders are placed on
        // the world rather than handed to clear_markets.
        sell_order o; o.corp = corp; o.body = b; o.resource = resource_type::steel;
        o.quantity = 10.0f; o.floor_price = 4.0f;
        o.id = ws.allocate_order_id(); ws.sell_orders.push_back(o);
        o.id = ws.allocate_order_id(); ws.sell_orders.push_back(o);

        economy_report empty;
        auto f = clear_markets(ws, reg, empty);
        const float pool_after = ws.pool_for(corp, b).quantities[ri(resource_type::steel)];
        const float cleared    = 10.0f - pool_after;
        check(cleared <= 10.0f + 1e-3f && near(cleared, 10.0f),
              "BL351.1 duplicate orders clear at most the pool (10 total)", cleared, 10.0f);
        check(pool_after >= 0.0f && near(pool_after, 0.0f),
              "BL351.2 pool floors at zero (never negative)", pool_after, 0.0f);
        check(near(f[corp].income, cleared * 5.0f),
              "BL351.3 income == cleared quantity x resolved price (10*5)", f[corp].income, cleared * 5.0f);
    }

    // --- BL-351: a multi-order seller's unmatched remainder clears per order ---
    // One seller lists two orders of 5 (floor 2); one buyer takes 5. The matched 5
    // drains one order; the OTHER order's own remainder (5) auto-clears at the
    // resolved price (floor 2 permits) — the old per-seller aggregate zeroed both
    // orders' remainder.
    // Ref price: S=10 D=5 base 8 -> target 8*sqrt(0.5)=5.657, EMA -> 6.828.
    // Income = 5*2 (matched at ask) + 5*6.828 (auto-clear) = 44.142.
    {
        world ws;
        const entity_id b = ws.create_entity(); ws.bodies[b] = body_component{};
        const entity_id m = ws.create_entity();
        market_component mc{}; mc.body = b;
        mc.base_price[ri(resource_type::steel)] = 8.0f;
        mc.price = mc.base_price;
        ws.markets[m] = mc;
        const entity_id seller = ws.create_entity();
        { corporation_component cc; cc.balance = 0.0f; ws.corporations[seller] = cc; }
        const entity_id buyer = ws.create_entity();
        { corporation_component cc; cc.balance = 0.0f; ws.corporations[buyer] = cc; }
        ws.pool_for(seller, b).quantities[ri(resource_type::steel)] = 10.0f;

        // BL-293: both sides of the book are world state now.
        sell_order so; so.corp = seller; so.body = b; so.resource = resource_type::steel;
        so.quantity = 5.0f; so.floor_price = 2.0f;
        so.id = ws.allocate_order_id(); ws.sell_orders.push_back(so);
        so.id = ws.allocate_order_id(); ws.sell_orders.push_back(so);
        buy_order bo; bo.corp = buyer; bo.body = b; bo.resource = resource_type::steel;
        bo.quantity = 5.0f; bo.max_price = 10.0f;
        bo.id = ws.allocate_order_id(); ws.buy_orders.push_back(bo);

        economy_report empty;
        auto f = clear_markets(ws, reg, empty);
        check(near(f[seller].income, 5.0f * 2.0f + 5.0f * 6.828427f),
              "BL351.4 multi-order seller: matched order + other order's auto-clear",
              f[seller].income, 44.142f);
        check(near(ws.pool_for(seller, b).quantities[ri(resource_type::steel)], 0.0f),
              "BL351.5 pool debited by both orders' full listed quantity",
              ws.pool_for(seller, b).quantities[ri(resource_type::steel)], 0.0f);
        check(near(f[buyer].expenditure, 5.0f * 2.0f),
              "BL351.6 buyer pays the matched quantity at the ask", f[buyer].expenditure, 10.0f);
    }

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
        wm.pool_for(corp_a, b).quantities[ri(resource_type::steel)] = 10.0f;
        wm.pool_for(corp_b, b).quantities[ri(resource_type::steel)] = 10.0f;

        economy_report empty;
        clear_markets(wm, reg, empty);
        check(near(wm.markets[mkt_a].supply[ri(resource_type::steel)], 10.0f),
              "MM.3 corp A surplus lists in catchment market A",
              wm.markets[mkt_a].supply[ri(resource_type::steel)], 10.0f);
        check(near(wm.markets[mkt_b].supply[ri(resource_type::steel)], 10.0f),
              "MM.4 corp B surplus lists in catchment market B",
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
            qw.pool_for(ec, qb).quantities[ri(resource_type::iron_ore)] = 100.0f;
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
