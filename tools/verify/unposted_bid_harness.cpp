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
//   U4 space-lane launch fuel, bought off the source shelf (commit_trade_shipment;
//      BL-1265: no pools — posted as a want, demand after the clear).
//   U5 building upkeep, bought off its market's shelf (run_building_upkeep; posted).
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

    // BL-1265 (MARKETS.md § The shelf economy): corporations hold no pools, so a
    // launch's fuel and a building's upkeep are no longer OWN-POOL takes that
    // post nothing — they are BOUGHT off the market's shelf and posted as wants,
    // which the clear turns into demand. U4/U5 now assert that: the purchase
    // lands in the tick's want register at the market, and after the clear the
    // good carries demand there and its plant is not vetoed.
    std::printf("U4 space-lane launch fuel is bought off the source shelf\n");
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
        market_component& shelf = f.w.markets.at(f.market);
        shelf.inventory[ri(resource_type::iron_ore)] = 100.0f;
        std::size_t fuel_good = resource_count;
        for (std::size_t r = 0; r < resource_count; ++r)
            // Exactly ONE launch's fuel: the launch empties the shelf, so the
            // clear sees the want with nothing standing to glut it.
            if (fuel[r] > 0.0f) { shelf.inventory[r] += fuel[r]; if (fuel_good == resource_count) fuel_good = r; }
        check(fuel_good < resource_count, "U4 not vacuous: a launch draws a fuel good");
        const resource_type fg = static_cast<resource_type>(fuel_good < resource_count ? fuel_good : 0);
        check(vetoed(f, fg), "U4 not vacuous: before the launch, the fuel good is a dead market at M");
        convoy_leg leg;
        leg.viable = true; leg.mode = convoy_mode::space; leg.cost = 0.0f; leg.travel_ticks = 2;
        economy_report rep;
        const bool sent = commit_trade_shipment(f.w, reg, rep, f.corp, f.market, m2,
                                                ri(resource_type::iron_ore), 10.0f, leg);
        check(sent, "U4 the space shipment launched");
        const auto wit = rep.wants.find(std::make_pair(f.corp, f.market));
        check(fuel_good < resource_count && wit != rep.wants.end() && wit->second[fuel_good] == fuel[fuel_good],
              "U4 the launch fuel is bought off the source shelf and posted as a want there");
        (void)clear_markets(f.w, reg, rep);
        check(fuel_good < resource_count && f.w.markets.at(f.market).demand[fuel_good] > 0.0f,
              "U4 ... the clear turns it into demand at the source market");
        // The want is POSTED now, so the market is no longer DEAD for the fuel
        // good: it carries a bid (the dead-market veto's test). Whether a
        // 5-a-tick plant against a one-launch want is then a GLUT is the glut
        // forecast's separate question, and it rightly says yes here.
        check(fuel_good < resource_count
                  && composite_bid(f.w.markets.at(f.market), fuel_good, f.w.current_econ_tick, 4) > 0.0f,
              "U4 ... and the fuel good is no longer a dead market at M (it carries a bid)");
    }

    std::printf("U5 building upkeep is bought off the building's market shelf\n");
    {
        fixture f = make_fixture();
        recipe_registry reg;
        building_upkeep_params up;
        up.goods[static_cast<std::size_t>(building_type::processing_facility)]
                [static_cast<std::size_t>(era_band::any)][ri(resource_type::tools)] = 1.5f;
        reg.set_building_upkeep(up);
        // The upkeep draw buys only under the fair-price ceiling, and with the
        // ceiling OFF (the hand-built default, reservation_mult 0) it buys
        // nothing: the shelf is the only source now, so the ceiling is set.
        price_band_params pb = reg.price_band();
        pb.reservation_mult = 2.0f;
        reg.set_price_band(pb);
        // Exactly the draw: the upkeep empties the shelf, so the clear sees the
        // want with nothing standing to glut it.
        f.w.markets.at(f.market).inventory[ri(resource_type::tools)] = 1.5f;
        check(vetoed(f, resource_type::tools), "U5 not vacuous: before the upkeep, tools are a dead market at M");
        economy_report rep;
        run_building_upkeep(f.w, reg, rep);
        check(f.w.markets.at(f.market).inventory[ri(resource_type::tools)] == 0.0f,
              "U5 the upkeep was met off the market's shelf (1.5 of 1.5)");
        const auto wit = rep.wants.find(std::make_pair(f.corp, f.market));
        check(wit != rep.wants.end() && wit->second[ri(resource_type::tools)] == 1.5f,
              "U5 ... and posted as the corporation's want at its market");
        (void)clear_markets(f.w, reg, rep);
        check(f.w.markets.at(f.market).demand[ri(resource_type::tools)] > 0.0f,
              "U5 ... which the clear turns into demand there");
        check(composite_bid(f.w.markets.at(f.market), ri(resource_type::tools), f.w.current_econ_tick, 4) > 0.0f,
              "U5 ... and tools are no longer a dead market at M (they carry a bid)");
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

    // U7 — THE SPACE LANE NEEDS A PAD AND ITS PROPELLANT ON THE SHELF (TRADE.md
    // § A trade, "Between bodies"; BL-1266). The pool-keeps-its-propellant rows
    // (MARKETS.md step 4: a pad's pool, auto-surplus, a standing sell order)
    // retired with pools (BL-1265): the trader now BUYS the launch's propellant
    // off the source shelf. What stands is the gate: `price_trade_leg` is viable
    // on a space lane only with a Launchpad on the source body AND the launch's
    // propellant on that shelf.
    std::printf("U7 the space lane needs a pad and the launch's propellant on the shelf\n");
    {
        const std::size_t prop = ri(resource_type::propellant);
        const auto& fuel = launch_draw_per_convoy();
        check(fuel[prop] > 0.0f, "U7 not vacuous: the launch draw burns propellant");

        auto leg_for = [&](bool pad, float propellant) {
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
            const entity_id far = f.w.create_entity();
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
            f.w.markets.at(f.market).inventory[prop]                         = propellant;
            f.w.markets.at(f.market).inventory[ri(resource_type::iron_ore)] = 100.0f;
            const recipe_registry reg;
            const logistics_nodes nodes = collect_logistics_nodes(f.w);
            return price_trade_leg(f.w, reg, nodes, f.corp, f.market, far,
                                   ri(resource_type::iron_ore), 10.0f);
        };
        const convoy_leg ok = leg_for(/*pad=*/true, 5.0f * fuel[prop]);
        check(ok.viable && ok.mode == convoy_mode::space,
              "U7a a pad on the body and the launch's propellant on the shelf: the space lane is viable");
        check(!leg_for(/*pad=*/false, 5.0f * fuel[prop]).viable,
              "U7b no pad on the source body: no space lane");
        check(!leg_for(/*pad=*/true, 0.0f).viable,
              "U7c a pad but no propellant on the shelf: no space lane");
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

    // U9 — propellant routes follow the body's air (Ben, 2026-10-09): the one
    // predicate's table, over all four atmosphere classes, and the tile form's
    // read of the tile's body.
    std::printf("U9 the propellant routes follow the body's air\n");
    {
        recipe airless_r, atmos_r, any_r;
        airless_r.air = recipe_air::airless;
        atmos_r.air   = recipe_air::atmosphere;
        const atmosphere_class classes[4] = {atmosphere_class::none, atmosphere_class::thin,
                                             atmosphere_class::moderate, atmosphere_class::thick};
        bool table_ok = true;
        for (int k = 0; k < 4; ++k)
        {
            body_component bc{};
            bc.atmosphere = classes[k];
            const bool airless = (k <= 1); // none, thin: planetology's own `airless`
            table_ok = table_ok && atmosphere_is_airless(bc.atmosphere) == airless
                && recipe_runs_on_body(airless_r, bc) == airless
                && recipe_runs_on_body(atmos_r, bc) == !airless
                && recipe_runs_on_body(any_r, bc);
        }
        check(table_ok, "U9 none/thin run only the airless route; moderate/thick only the atmosphere route; any runs everywhere");
        fixture f = make_fixture();
        check(f.w.bodies.at(f.body).atmosphere == atmosphere_class::moderate
                  && recipe_runs_at_tile(f.w, atmos_r, f.tile) && !recipe_runs_at_tile(f.w, airless_r, f.tile),
              "U9 a body built with no generated profile reads moderate: the atmosphere route, never the airless one");
        f.w.bodies.at(f.body).atmosphere = atmosphere_class::none;
        check(!recipe_runs_at_tile(f.w, atmos_r, f.tile) && recipe_runs_at_tile(f.w, airless_r, f.tile),
              "U9 the tile form reads the tile's body: an airless body flips both");
    }

    std::printf("\n%s (%d failure%s)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_fail,
                g_fail == 1 ? "" : "s");
    return g_fail == 0 ? 0 : 1;
}
