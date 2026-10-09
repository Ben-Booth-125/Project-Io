// Headless harness: BL-1227 (idle mines) review round 4 — every UNPOSTED BID
// site actually WRITES the record the scorer's dead-market veto reads
// (market_component::unposted_bid, AI_OPPONENT.md § 2B, Ben 2026-10-07/08), and
// the record is HELD for the scorer's cadence. No SDL / Lua / ImGui.
//
// Fixture: one body, one market M whose catchment holds tile T; a nation whose
// capital is T. M has cleared (it bids for water) but nobody posts a bid for,
// or lists, any good under test — so without the record each is a DEAD market
// and a plant making it is vetoed. Each row drives ONE write site:
//   U1 the space programme, with NO supply anywhere (its lump is wanted, never
//      filled): the want lands at the capital market and a propellant /
//      spacecraft_components plant is no longer vetoed.
//   U2 network upkeep, nothing held anywhere: its stone/timber bill is wanted.
//   U3 procurement fulfilment (run_economy_step): the contract's quantity.
//   U4 space-lane launch fuel taken from a corporation's pool (commit_convoy).
//   U5 building upkeep met from a corporation's OWN pool (run_building_upkeep).
//   U6 the cadence hold: a record 3 ticks old still counts (cadence_k 4); one
//      older than the cadence does not.
// Exits non-zero on any FAIL.

#include "world/components.hpp"
#include "world/corp_ai.hpp"
#include "world/corporation_generation.hpp" // seat_release_dial_idled (U8)
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/nation_budget.hpp"
#include "world/nation_step.hpp"
#include "world/network_upkeep.hpp"
#include "world/recipe_registry.hpp"
#include "world/space_programme.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <map>
#include <vector>

namespace {

int g_fail = 0;
void check(bool ok, const char* what)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

constexpr std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

struct fixture
{
    world     w;
    entity_id body = null_entity, tile = null_entity, market = null_entity, nation = null_entity;
    entity_id corp = null_entity;     ///< a background firm holding ground at T
    entity_id building = null_entity; ///< its processing facility at T
};

fixture make_fixture()
{
    fixture f;
    world& w = f.w;
    f.body = w.create_entity();
    body_component bc{};
    bc.name = "Anvil"; bc.type = body_type::planet; bc.grid_width = 2; bc.grid_height = 1;
    bc.survey.phase = survey_phase::surveyed;
    w.bodies[f.body] = bc;
    w.home_body = f.body;
    for (int c = 0; c < 2; ++c)
    {
        const entity_id t = w.create_entity();
        tile_component tc{};
        tc.body = f.body; tc.grid_x = c; tc.grid_y = 0;
        tc.substrate = terrain_substrate::rocky; tc.cover = terrain_cover::grass;
        w.tiles[t] = tc;
        if (c == 0) f.tile = t;
    }
    f.market = w.create_entity();
    market_component mc{};
    mc.body = f.body; mc.centre_tile = f.tile;
    for (std::size_t r = 0; r < resource_count; ++r) mc.base_price[r] = 2.0f;
    mc.price = mc.base_price;
    mc.demand[ri(resource_type::water)] = 10.0f; // the market has cleared
    w.markets[f.market] = mc;

    f.nation = w.create_entity();
    nation_component nc{};
    nc.name = "Testland"; nc.treasury = 100000.0f; nc.capital_tile = f.tile;
    w.nations[f.nation] = nc;

    f.corp = w.create_entity();
    corporation_component cc;
    cc.name = "Firm"; cc.balance = 1.0e6f; cc.starting_capital = 1.0e6f;
    f.building = w.create_entity();
    building_component b{};
    b.tile = f.tile; b.type = building_type::processing_facility; b.workforce_assigned = 0.5f;
    b.workforce_auto = false;
    w.buildings[f.building] = b;
    cc.assets.push_back(f.building);
    w.corporations[f.corp] = cc;
    w.current_econ_tick = 5;
    return f;
}

bool vetoed(const fixture& f, resource_type good)
{
    return forecast_glut_multiplier(f.w, f.tile, good, 5.0f, 3) <= 0.0f;
}

nation_budget budget_for(budget_priority line)
{
    nation_budget nb{};
    nb.reserve_fraction = 0.0f;
    nb.weights[static_cast<std::size_t>(line)] = 1.0f;
    return nb;
}

} // namespace

int main()
{
    std::printf("unposted_bid_harness — BL-1227 review round 4\n");

    std::printf("U1 the space programme's want, filled or not\n");
    {
        fixture f = make_fixture();
        check(vetoed(f, resource_type::propellant) && vetoed(f, resource_type::spacecraft_components),
              "U1 not vacuous: before the want, both space goods are a dead market at M");
        std::map<entity_id, nation_budget> budgets{{f.nation, budget_for(budget_priority::space_programme)}};
        space_programme_params sp;
        sp.components_lump = 4.0f;
        sp.propellant_lump = 8.0f;
        std::vector<budget_claim> claims;
        std::vector<market_want>  wants;
        const auto purchases = derive_space_programme_claims(f.w, budgets, sp, 0.0f, claims, &wants);
        record_market_wants(f.w, wants);
        check(purchases.empty() && claims.empty(),
              "U1 no pool and no shelf holds a lump anywhere: nothing is bought");
        const market_component& m = f.w.markets.at(f.market);
        check(m.unposted_bid[ri(resource_type::propellant)] == 8.0f
                  && m.unposted_bid[ri(resource_type::spacecraft_components)] == 4.0f,
              "U1 ... yet both lumps are recorded as wanted at the capital's market");
        check(!vetoed(f, resource_type::propellant),
              "U1 a cleared market whose only buyer is the space programme (no supply anywhere) does not veto a propellant plant");
        check(!vetoed(f, resource_type::spacecraft_components),
              "U1 ... nor a spacecraft_components plant");
    }

    std::printf("U2 network upkeep's want\n");
    {
        fixture f = make_fixture();
        f.w.tile_to_nation[f.tile] = f.nation;
        f.w.tiles.at(f.tile).road_level = 1;
        check(vetoed(f, resource_type::stone), "U2 not vacuous: before the want, stone is a dead market at M");
        std::map<entity_id, nation_budget> budgets{{f.nation, budget_for(budget_priority::logistics_maintenance)}};
        network_upkeep_params np;
        np.stone_per_level = {3.0f, 3.0f, 3.0f};
        std::vector<budget_claim> claims;
        std::vector<market_want>  wants;
        derive_network_upkeep_claims(f.w, budgets, np, 0.0f, claims, &wants);
        record_market_wants(f.w, wants);
        check(f.w.markets.at(f.market).unposted_bid[ri(resource_type::stone)] == 3.0f,
              "U2 the road's stone bill, held nowhere, is recorded as wanted at the capital's market");
        check(!vetoed(f, resource_type::stone), "U2 ... and a stone site there is not vetoed");
    }

    std::printf("U3 procurement fulfilment\n");
    {
        fixture f = make_fixture();
        const recipe_registry reg;
        const entity_id buyer = f.w.create_entity();
        corporation_component bc;
        bc.name = "Buyer"; bc.balance = 1.0e6f;
        f.w.corporations[buyer] = bc;
        procurement_contract c{};
        c.id = 1; c.buyer = buyer; c.supplier = f.corp; c.body = f.body;
        c.resource = resource_type::machinery; c.delivery_body = f.body;
        c.quantity = 6.0f; c.unit_price = 1.0f; c.lead_time_ticks = 2;
        f.w.procurement_contracts.push_back(c);
        check(vetoed(f, resource_type::machinery), "U3 not vacuous: before the contract runs, machinery is a dead market");
        (void)run_economy_step(f.w, reg);
        check(!f.w.procurement_contracts.empty()
                  && f.w.markets.at(f.market).unposted_bid[ri(resource_type::machinery)] == 6.0f
                  && !vetoed(f, resource_type::machinery),
              "U3 on a lead-time tick, before it fulfils, the contract's want is already recorded (wanted, filled or not)");
        f.w.current_econ_tick = 6;
        (void)run_economy_step(f.w, reg);
        check(f.w.procurement_contracts.empty(), "U3 the contract fulfilled on its second step");
        check(f.w.markets.at(f.market).unposted_bid[ri(resource_type::machinery)] == 6.0f,
              "U3 its quantity is recorded as wanted at the supplier's home market");
        check(!vetoed(f, resource_type::machinery), "U3 ... and a machinery plant there is not vetoed");
    }

    std::printf("U4 space-lane launch fuel from a corporation's pool\n");
    {
        fixture f = make_fixture();
        const recipe_registry reg;
        // A second body with its own market: the lane's destination.
        const entity_id b2 = f.w.create_entity();
        body_component b2c{};
        b2c.name = "Far"; b2c.type = body_type::planet; b2c.grid_width = 1; b2c.grid_height = 1;
        f.w.bodies[b2] = b2c;
        const entity_id t2 = f.w.create_entity();
        tile_component t2c{};
        t2c.body = b2;
        f.w.tiles[t2] = t2c;
        const entity_id m2 = f.w.create_entity();
        market_component m2c{};
        m2c.body = b2; m2c.centre_tile = t2;
        f.w.markets[m2] = m2c;

        const auto& fuel = launch_draw_per_convoy();
        stockpile_component& pool = f.w.pool_at(f.corp, f.market);
        pool.quantities[ri(resource_type::iron_ore)] = 100.0f;
        std::size_t fuel_good = resource_count;
        for (std::size_t r = 0; r < resource_count; ++r)
            if (fuel[r] > 0.0f) { pool.quantities[r] += 10.0f * fuel[r]; if (fuel_good == resource_count) fuel_good = r; }
        check(fuel_good < resource_count, "U4 not vacuous: a launch draws a fuel good");
        const resource_type fg = static_cast<resource_type>(fuel_good < resource_count ? fuel_good : 0);
        f.w.markets.at(f.market).demand[ri(resource_type::iron_ore)] = 1.0f;
        check(vetoed(f, fg), "U4 not vacuous: before the launch, the fuel good is a dead market at M");
        convoy_leg leg;
        leg.viable = true; leg.mode = convoy_mode::space; leg.cost = 0.0f; leg.travel_ticks = 2;
        const bool sent = commit_convoy(f.w, reg, f.corp, f.body, f.market, m2,
                                        ri(resource_type::iron_ore), 10.0f, leg, nullptr, nullptr, false, nullptr);
        check(sent, "U4 the space convoy launched");
        check(fuel_good < resource_count && f.w.markets.at(f.market).unposted_bid[fuel_good] == fuel[fuel_good],
              "U4 the launch fuel taken from the corporation's pool is recorded at the source market");
        check(!vetoed(f, fg), "U4 ... and a fuel plant there is not vetoed");
    }

    std::printf("U5 building upkeep met from the corporation's own pool\n");
    {
        fixture f = make_fixture();
        recipe_registry reg;
        building_upkeep_params up;
        up.goods[static_cast<std::size_t>(building_type::processing_facility)]
                [static_cast<std::size_t>(era_band::any)][ri(resource_type::tools)] = 1.5f;
        reg.set_building_upkeep(up);
        f.w.pool_at(f.corp, f.market).quantities[ri(resource_type::tools)] = 10.0f;
        check(vetoed(f, resource_type::tools), "U5 not vacuous: before the upkeep, tools are a dead market at M");
        economy_report rep;
        run_building_upkeep(f.w, reg, rep);
        check(f.w.pool_at(f.corp, f.market).quantities[ri(resource_type::tools)] == 8.5f,
              "U5 the upkeep was met from the corporation's own pool");
        check(f.w.markets.at(f.market).unposted_bid[ri(resource_type::tools)] == 1.5f,
              "U5 ... and that own-pool take is recorded as an unposted bid on its market");
        check(!vetoed(f, resource_type::tools), "U5 ... and a tools plant there is not vetoed");
    }

    std::printf("U6 the record is held for the scorer's cadence\n");
    {
        fixture f = make_fixture();
        note_unposted_bid(f.w.markets.at(f.market), ri(resource_type::coal), 2.0f, 10);
        f.w.current_econ_tick = 13; // a corp evaluating 3 ticks after the want
        check(!vetoed(f, resource_type::coal), "U6 a corporation evaluating 3 ticks after the want still sees it (cadence 4)");
        f.w.current_econ_tick = 14; // age == cadence_k: the boundary, still held
        check(!vetoed(f, resource_type::coal), "U6 boundary: a record exactly cadence_k (4) ticks old is still held");
        f.w.current_econ_tick = 15; // age == cadence_k + 1
        check(vetoed(f, resource_type::coal), "U6 boundary: one tick past the cadence it no longer counts: dead again");
        f.w.current_econ_tick = 8;  // the record is dated AHEAD of the current tick
        check(vetoed(f, resource_type::coal), "U6 a record dated ahead of the current tick (a settle replayed from tick 1) is never read");
        note_unposted_bid(f.w.markets.at(f.market), ri(resource_type::coal), 1.0f, 15);
        note_unposted_bid(f.w.markets.at(f.market), ri(resource_type::coal), 0.5f, 15);
        check(f.w.markets.at(f.market).unposted_bid[ri(resource_type::coal)] == 1.5f
                  && f.w.markets.at(f.market).unposted_bid_tick[ri(resource_type::coal)] == 15,
              "U6 a later tick's first record overwrites; same-tick records add");
    }

    // U7 — A PAD'S POOL KEEPS ITS PROPELLANT (MARKETS.md step 4, Ben 2026-10-09).
    // Propellant is priced at M (the fixture prices every good). A corp holding a
    // Launchpad on the body makes propellant into its pool; a clear runs; the
    // launch still dispatches. Contrary cases: without a pad the clear's
    // auto-surplus sells it; with a pad a standing sell order still can.
    std::printf("U7 a pad's pool keeps its propellant through a clear\n");
    {
        const std::size_t prop = ri(resource_type::propellant);
        const auto& fuel = launch_draw_per_convoy();
        check(fuel[prop] > 0.0f, "U7 not vacuous: the launch draw burns propellant");

        // One run: optional pad, optional standing order; returns the fixture
        // after one clear, and the far market id through `far`.
        auto run = [&](bool pad, bool order, entity_id& far) {
            fixture f = make_fixture();
            const entity_id b2 = f.w.create_entity();
            body_component b2c{};
            b2c.name = "Far"; b2c.type = body_type::planet; b2c.grid_width = 1; b2c.grid_height = 1;
            b2c.orbital_radius_au = 1.0f;
            f.w.bodies[b2] = b2c;
            const entity_id t2 = f.w.create_entity();
            tile_component t2c{};
            t2c.body = b2;
            f.w.tiles[t2] = t2c;
            far = f.w.create_entity();
            market_component m2c{};
            m2c.body = b2; m2c.centre_tile = t2;
            for (std::size_t r = 0; r < resource_count; ++r) m2c.base_price[r] = 2.0f;
            m2c.price = m2c.base_price;
            f.w.markets[far] = m2c;
            if (pad)
            {
                const entity_id lp = f.w.create_entity();
                building_component lb{};
                lb.tile = f.tile; lb.type = building_type::launchpad;
                f.w.buildings[lp] = lb;
                f.w.corporations.at(f.corp).assets.push_back(lp);
            }
            if (order)
            {
                sell_order so;
                so.id = f.w.allocate_order_id();
                so.corp = f.corp; so.body = f.body; so.resource = resource_type::propellant;
                f.w.sell_orders.push_back(so);
            }
            stockpile_component& pool = f.w.pool_at(f.corp, f.market);
            pool.quantities[prop]                         = 5.0f; // this tick's make
            pool.quantities[ri(resource_type::iron_ore)] = 100.0f;
            f.w.markets.at(f.market).demand[prop] = 50.0f; // a buyer is there
            const recipe_registry reg;
            economy_report rep{};
            clear_markets(f.w, reg, rep);
            return f;
        };

        {
            entity_id far = null_entity;
            fixture f = run(/*pad=*/true, /*order=*/false, far);
            const float left = f.w.pool_at(f.corp, f.market).quantities[prop];
            check(left == 5.0f, "U7a with a pad on the body, the clear's auto-surplus lists none of the pool's propellant");
            const recipe_registry reg;
            const logistics_nodes nodes = collect_logistics_nodes(f.w);
            const convoy_leg leg = price_convoy_leg(f.w, reg, nodes, f.corp, f.market, far,
                                                    ri(resource_type::iron_ore), 10.0f, 1.0f);
            check(leg.viable && leg.mode == convoy_mode::space,
                  "U7a ... so the space-lane gate still finds the pad fuelled after the clear");
            const bool sent = leg.viable
                && commit_convoy(f.w, reg, f.corp, f.body, f.market, far, ri(resource_type::iron_ore),
                                 10.0f, leg, nullptr, nullptr, false, nullptr);
            check(sent, "U7a ... and the launch dispatches");
            check(f.w.pool_at(f.corp, f.market).quantities[prop] == 5.0f - fuel[prop],
                  "U7a ... burning exactly the launch draw from the pad's pool");
        }
        {
            entity_id far = null_entity;
            fixture f = run(/*pad=*/false, /*order=*/false, far);
            check(f.w.pool_at(f.corp, f.market).quantities[prop] == 0.0f,
                  "U7b with no pad, the clear's auto-surplus sells the pool's propellant like any surplus");
        }
        {
            entity_id far = null_entity;
            fixture f = run(/*pad=*/true, /*order=*/true, far);
            check(f.w.pool_at(f.corp, f.market).quantities[prop] < 5.0f,
                  "U7c with a pad, a standing sell order still sells the pool's propellant");
        }
        {
            fixture f = make_fixture();
            const recipe_registry reg;
            f.w.pool_at(f.corp, f.market).quantities[prop] = 5.0f;
            check(auto_surplus_reservation(f.w, reg, f.corp, f.market)[prop] == 0.0f
                      && !launch_burns_from_pool(f.w, f.corp, f.market),
                  "U7d no pad: the auto-surplus reservation holds no propellant, and no launch burns from the pool");
        }
    }

    // U8 — NR-986 (Ben, 2026-10-09): at the handoff the seat's dial-idled plants
    // return to auto. Sibling of U-rows only by fixture; the subject is the
    // dial's hold, which the unposted bid's veto also reads.
    std::printf("U8 the seat's dial-idled plants return to auto at the handoff\n");
    {
        fixture f = make_fixture();
        building_component& idled = f.w.buildings.at(f.building);
        idled.workforce_auto = false; idled.workforce_target = 0; idled.ticks_remaining = 0;
        const entity_id run_id = f.w.create_entity();
        building_component running{};
        running.tile = f.tile; running.type = building_type::processing_facility;
        running.workforce_auto = false; running.workforce_target = 40;
        f.w.buildings[run_id] = running;
        f.w.corporations.at(f.corp).assets.push_back(run_id);
        check(dial_idled(f.w.buildings.at(f.building)) && !dial_idled(f.w.buildings.at(run_id)),
              "U8 not vacuous: one plant is dial-idled, one is dialled above zero");
        const int n = seat_release_dial_idled(f.w, f.corp);
        check(n == 1 && f.w.buildings.at(f.building).workforce_auto,
              "U8 the dial-idled plant is handed over on auto");
        check(!f.w.buildings.at(run_id).workforce_auto && f.w.buildings.at(run_id).workforce_target == 40,
              "U8 ... and a plant the dial left running keeps its dial");
    }

    std::printf("\n%s (%d failure%s)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_fail,
                g_fail == 1 ? "" : "s");
    return g_fail == 0 ? 0 : 1;
}
