// Headless trade-reaches-for-price harness (BL-995; no SDL / Lua / ImGui).
//
// SUPPLY.md § Dispatch trigger: auto-dispatch is the SELLER chasing a NET
// PRICE. For every (corp, market) pool holding a good above its processor
// reservation, at last tick's resolved prices,
//
//     net(d) = price_d - haul_per_unit(src -> d)
//     send to argmax_d net(d)  if  net(d) - price_src > margin x price_src
//
// and the quantity is min(surplus, q) less what the corp already has in
// transit to d, with q = supply_d x ((price_d / landed)^2 - 1) — or d's unmet
// demand when d has no supply. The buyer-side shortfall scan it replaced
// never read a price.
//
// Fixture: ONE body, a 64x4 plains grid (columns wrap, so 64 keeps F at
// column 30 genuinely 30 tiles from A), three markets on row 0 —
//     A (source) at column 0, N (near) at column 6, F (far) at column 30.
// The corp's only goods sit in (corp, A). Iron is priced 10 at A.
//
//   (a) FARTHER AND DEARER BEATS NEARER: N prices 15, F prices 30 — the good
//       goes to F.
//   (b) THE MARGIN: a best net gain under margin x home price moves nothing;
//       the same world with a smaller margin does move it (so the margin, not
//       something else, is what held it).
//   (c) THE QUANTITY: never more than q for a destination with supply; exactly
//       the unmet demand for one with none; in-transit cargo to d is subtracted.
//   (d) NO CHURN: several full ticks (dispatch, advance, credit, clear) — every
//       convoy's destination net price beats its source's price at dispatch.
//   (e) A DELIVERY SELLS AT ITS DESTINATION: an exchange row at F, seller = the
//       corp, for the cargo delivered.
//   (f) THE RESERVATION: goods the corp's own processor in A needs are not
//       shipped; only the surplus above it is.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }
bool near_rel(float a, float b, float rel = 1e-4f)
{
    return std::fabs(a - b) <= rel * std::max(1.0f, std::fabs(b));
}

constexpr std::size_t r_iron = static_cast<std::size_t>(resource_type::iron_ore);

float pool_q(const world& w, entity_id corp, entity_id key, std::size_t r)
{
    const stockpile_component* p = w.find_pool(corp, key);
    return p ? p->quantities[r] : 0.0f;
}

float price_of(const market_component& mc, std::size_t r)
{
    return (mc.price[r] > 0.0f) ? mc.price[r] : mc.base_price[r];
}

struct scenario
{
    world     w;
    entity_id body     = null_entity;
    entity_id corp     = null_entity;
    entity_id market_a = null_entity; ///< source, column 0
    entity_id market_n = null_entity; ///< near, column 6
    entity_id market_f = null_entity; ///< far, column 30
};

entity_id tile_at(world& w, entity_id body, int c, int r)
{
    const int gw = w.bodies.at(body).grid_width;
    return body_tile_grid(w, body)[static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                                   + static_cast<std::size_t>(c)];
}

entity_id add_market(scenario& s, int col, float iron_price)
{
    const entity_id id = s.w.create_entity();
    market_component m{};
    m.body        = s.body;
    m.centre_tile = tile_at(s.w, s.body, col, 0);
    m.base_price[r_iron] = iron_price;
    m.price = m.base_price;
    s.w.markets[id] = m;
    return id;
}

// pools_per_market.cpp's scaffolding: one plains body, a player-flagged corp
// (keeps the strategic scorer out — dispatch is one rule for every corp, so
// the flag does not change what dispatch does), its lowest-id building at A's
// centre, and a supply anchor off the haul's row for the passive-LP gate.
scenario make_scenario(float price_n, float price_f)
{
    scenario s;
    s.body = s.w.create_entity();
    body_component bc{};
    bc.name              = "Anvil";
    bc.type              = body_type::planet;
    bc.orbital_radius_au = 1.0f;
    bc.grid_width        = 64; // wide enough that the column wrap never shortens A->F
    bc.grid_height       = 4;
    s.w.bodies[s.body] = bc;
    s.w.home_body = s.body;
    for (int r = 0; r < bc.grid_height; ++r)
        for (int c = 0; c < bc.grid_width; ++c)
        {
            const entity_id t = s.w.create_entity();
            tile_component tc{};
            tc.body          = s.body;
            tc.grid_x        = c;
            tc.grid_y        = r;
            tc.substrate     = terrain_substrate::sedimentary;
            tc.cover         = terrain_cover::grass;
            tc.cover_density = 150;
            tc.landform      = terrain_landform::plains;
            s.w.tiles[t] = tc;
        }

    s.corp = s.w.create_entity();
    corporation_component cc;
    cc.balance   = 100000.0f;
    cc.is_player = true;
    const entity_id anchor = s.w.create_entity();
    building_component b{};
    b.tile = tile_at(s.w, s.body, 0, 0);
    b.type = building_type::extraction_site;
    s.w.buildings[anchor] = b;
    cc.assets.push_back(anchor);
    cc.hq_building = anchor;
    s.w.corporations[s.corp] = cc;
    s.w.player_entity = s.corp;

    s.w.population_centre_tile[s.w.create_entity()] = tile_at(s.w, s.body, 0, 3);

    s.market_a = add_market(s, 0, 10.0f);
    s.market_n = add_market(s, 6, price_n);
    s.market_f = add_market(s, 30, price_f);
    return s;
}

recipe_registry base_registry()
{
    recipe_registry reg;
    military_capability_params mp = reg.military();
    mp.active_lp_per_anchor_tick = 1.0e6f; // the LP gate is not under test here
    reg.set_military(mp);
    return reg;
}

void dispatch(scenario& s, const recipe_registry& reg)
{
    dispatch_convoys(s.w, reg, reg.logistics_cost(convoy_mode::land),
                     reg.logistics_cost(convoy_mode::space));
}

/// Per-unit haul from A to `dest`, priced by the SAME shared function the
/// dispatcher uses (cost is linear in quantity).
float haul_per_unit(scenario& s, const recipe_registry& reg, entity_id src, entity_id dest)
{
    const logistics_nodes nodes = collect_logistics_nodes(s.w);
    const convoy_leg leg = price_convoy_leg(s.w, reg, nodes, s.corp, src, dest, r_iron, 1.0f,
                                            reg.logistics_cost(convoy_mode::space));
    return leg.viable ? leg.cost : -1.0f;
}

float cargo_to(const world& w, entity_id corp, entity_id dest)
{
    float q = 0.0f;
    for (const convoy_component& c : w.convoys)
        if (c.corp == corp && c.dest_market == dest && c.cargo_resource == resource_type::iron_ore)
            q += c.cargo_qty;
    return q;
}

} // namespace

int main()
{
    std::printf("=== trade_reaches_for_price (BL-995: the seller hauls before it sells) ===\n");

    // Fixture sanity: the three markets own their columns, and the margin the
    // harness registry carries is the nonzero default.
    {
        scenario s = make_scenario(15.0f, 30.0f);
        check(pool_key_for_tile(s.w, tile_at(s.w, s.body, 0, 0)) == s.market_a &&
              pool_key_for_tile(s.w, tile_at(s.w, s.body, 6, 0)) == s.market_n &&
              pool_key_for_tile(s.w, tile_at(s.w, s.body, 30, 0)) == s.market_f,
              "K.1 fixture: columns 0 / 6 / 30 resolve to markets A / N / F");
        const recipe_registry reg = base_registry();
        check(reg.dispatch_margin() > 0.0f,
              "K.2 a registry that never authors the margin carries a nonzero default");
        const float hn = haul_per_unit(s, reg, s.market_a, s.market_n);
        const float hf = haul_per_unit(s, reg, s.market_a, s.market_f);
        std::printf("      haul/unit A->N %.4f, A->F %.4f\n", hn, hf);
        check(hn > 0.0f && hf > hn, "K.3 both legs are viable and F is the costlier haul");
    }

    // -----------------------------------------------------------------------
    // (a) farther and dearer beats nearer and cheaper
    // -----------------------------------------------------------------------
    world after_a; // (e) delivers this one
    recipe_registry reg_a = base_registry();
    entity_id a_corp = null_entity, a_f = null_entity;
    {
        scenario s = make_scenario(15.0f, 30.0f);
        s.w.markets.at(s.market_n).supply[r_iron] = 100.0f;
        s.w.markets.at(s.market_f).supply[r_iron] = 100.0f;
        s.w.pool_at(s.corp, s.market_a).quantities[r_iron] = 1000.0f;

        dispatch(s, reg_a);
        check(s.w.convoys.size() == 1, "(a).1 exactly one convoy leaves A");
        check(!s.w.convoys.empty() && s.w.convoys[0].dest_market == s.market_f &&
                  s.w.convoys[0].source_market == s.market_a,
              "(a).2 it goes to the FARTHER market F (30) over the nearer N (15)");
        after_a = s.w;
        a_corp  = s.corp;
        a_f     = s.market_f;

        // (c) the quantity against a destination with supply is q, exactly.
        const float haul   = haul_per_unit(s, reg_a, s.market_a, s.market_f);
        const float landed = 10.0f + haul;
        const float q      = 100.0f * ((30.0f / landed) * (30.0f / landed) - 1.0f);
        const float sent   = s.w.convoys.empty() ? 0.0f : s.w.convoys[0].cargo_qty;
        std::printf("      q = %.3f, sent = %.3f, pool left %.3f\n", q, sent,
                    pool_q(s.w, s.corp, s.market_a, r_iron));
        check(sent <= q + 1e-2f && near_rel(sent, q),
              "(c).1 the send to a destination with supply is q = supply x ((p_d/landed)^2 - 1), "
              "never more");
        check(near(pool_q(s.w, s.corp, s.market_a, r_iron), 1000.0f - sent, 1e-2f),
              "(c).2 the source pool is debited exactly the cargo (the rest sells at home)");
    }

    // -----------------------------------------------------------------------
    // (b) nothing moves when the best net gain is under the margin
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry(); // margin 0.05: gate at 10.5 net
        scenario s = make_scenario(10.3f, 10.9f);
        s.w.markets.at(s.market_n).supply[r_iron] = 100.0f;
        s.w.markets.at(s.market_f).supply[r_iron] = 100.0f;
        s.w.pool_at(s.corp, s.market_a).quantities[r_iron] = 1000.0f;
        const float hf = haul_per_unit(s, reg, s.market_a, s.market_f);
        std::printf("      net F = %.4f vs home 10 (margin gate %.4f)\n", 10.9f - hf,
                    10.0f * (1.0f + reg.dispatch_margin()));
        check(10.9f - hf > 10.0f && 10.9f - hf < 10.0f * (1.0f + reg.dispatch_margin()),
              "(b).0 fixture: F's net beats home, by less than the margin");
        dispatch(s, reg);
        check(s.w.convoys.empty() &&
                  near(pool_q(s.w, s.corp, s.market_a, r_iron), 1000.0f),
              "(b).1 a net gain under margin x home price moves nothing");

        recipe_registry loose = base_registry();
        loose.set_dispatch_margin(0.001f);
        dispatch(s, loose);
        check(s.w.convoys.size() == 1 && s.w.convoys[0].dest_market == s.market_f,
              "(b).2 the same world with a 0.1% margin does haul to F — the margin held it");
    }

    // -----------------------------------------------------------------------
    // (c) zero-supply destination absorbs its unmet demand; in-transit subtracts
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(12.0f, 30.0f);
        s.w.markets.at(s.market_n).supply[r_iron] = 100.0f;
        s.w.markets.at(s.market_f).supply[r_iron] = 0.0f;
        s.w.markets.at(s.market_f).demand[r_iron] = 50.0f;
        s.w.pool_at(s.corp, s.market_a).quantities[r_iron] = 1000.0f;

        dispatch(s, reg);
        check(near(cargo_to(s.w, s.corp, s.market_f), 50.0f),
              "(c).3 a destination with ZERO supply receives exactly its unmet demand (50)");

        // The convoy is still on the lane (nothing advanced it). Dispatch again:
        // the gap is already filled by cargo in transit, so nothing more goes.
        const std::size_t before = s.w.convoys.size();
        dispatch(s, reg);
        check(s.w.convoys.size() == before && near(cargo_to(s.w, s.corp, s.market_f), 50.0f),
              "(c).4 with 50 already in transit to F, a second pass sends F nothing more");

        // Widen the gap to 80: the next pass sends only the 30 not yet in transit.
        s.w.markets.at(s.market_f).demand[r_iron] = 80.0f;
        dispatch(s, reg);
        check(s.w.convoys.size() == before + 1 && near(s.w.convoys.back().cargo_qty, 30.0f),
              "(c).5 demand 80 with 50 in transit: the pass sends the missing 30");
    }

    // -----------------------------------------------------------------------
    // (d) no churn: a good never moves to a lower net price, over full ticks
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(15.0f, 30.0f);
        s.w.markets.at(s.market_n).supply[r_iron] = 100.0f;
        s.w.markets.at(s.market_f).supply[r_iron] = 100.0f;

        std::set<std::uint32_t> seen;
        int  dispatched = 0, bad = 0, from_non_a = 0;
        for (int t = 0; t < 40; ++t)
        {
            s.w.current_econ_tick = t;
            s.w.current_day_tick  = t;
            // A stand-in for production: A's pool is topped up every tick.
            s.w.pool_at(s.corp, s.market_a).quantities[r_iron] += 200.0f;

            // The app's order: advance -> arrivals -> (economy) -> dispatch -> clear.
            advance_convoys(s.w);
            credit_arrived_convoys(s.w, t);
            dispatch(s, reg);
            for (const convoy_component& c : s.w.convoys)
            {
                if (!seen.insert(c.id).second)
                    continue;
                ++dispatched;
                if (c.source_market != s.market_a)
                    ++from_non_a;
                const float p_src  = price_of(s.w.markets.at(c.source_market), r_iron);
                const float p_dest = price_of(s.w.markets.at(c.dest_market), r_iron);
                const float net    = p_dest - c.cost_paid / c.cargo_qty;
                if (!(net > p_src))
                {
                    ++bad;
                    std::printf("      tick %d convoy %u: dest net %.4f <= source %.4f\n", t, c.id,
                                net, p_src);
                }
            }
            clear_markets(s.w, reg, economy_report{});
        }
        std::printf("      %d convoys over 40 ticks (%d from a pool other than A); "
                    "prices A %.3f N %.3f F %.3f\n",
                    dispatched, from_non_a, price_of(s.w.markets.at(s.market_a), r_iron),
                    price_of(s.w.markets.at(s.market_n), r_iron),
                    price_of(s.w.markets.at(s.market_f), r_iron));
        check(dispatched > 0, "(d).0 fixture: the loop dispatches at all");
        check(bad == 0,
              "(d).1 every convoy's destination net price beats its source price at dispatch");
    }

    // -----------------------------------------------------------------------
    // (e) the (a) cargo, delivered, sells at F's clear
    // -----------------------------------------------------------------------
    {
        world& w = after_a;
        const float cargo = w.convoys.empty() ? 0.0f : w.convoys[0].cargo_qty;
        for (int i = 0; i < 200 && !w.convoys.empty(); ++i)
        {
            advance_convoys(w);
            credit_arrived_convoys(w, i);
        }
        check(w.convoys.empty() && near(pool_q(w, a_corp, a_f, r_iron), cargo, 1e-2f),
              "(e).1 the convoy arrives and credits the (corp, F) pool");
        clear_markets(w, reg_a, economy_report{});
        bool sold_at_f = false;
        for (const exchange_record& e : w.exchanges.entries)
            if (e.market == a_f && e.seller == a_corp && e.resource == resource_type::iron_ore &&
                near(e.quantity, cargo, 1e-2f))
                sold_at_f = true;
        check(cargo > 0.0f && sold_at_f,
              "(e).2 the delivered cargo sells at F's clear (exchange row at F, seller = corp)");
    }

    // -----------------------------------------------------------------------
    // (f) the corp's own processor reservation in A is not shipped
    // -----------------------------------------------------------------------
    {
        recipe_registry reg = base_registry();
        building_economics pr;
        pr.base_rate = 20.0f;
        reg.set_economics(building_type::processing_facility, pr);
        recipe steel;
        steel.name = "steel";
        steel.inputs[r_iron] = 2.0f;
        steel.outputs[static_cast<std::size_t>(resource_type::steel)] = 1.0f;
        const std::uint16_t steel_id = reg.add_recipe(steel);

        scenario s = make_scenario(12.0f, 30.0f);
        const entity_id proc = s.w.create_entity();
        building_component pb{};
        pb.tile               = tile_at(s.w, s.body, 1, 0); // A's catchment
        pb.type               = building_type::processing_facility;
        pb.recipe             = steel_id;
        pb.workforce_assigned = 1.0f;
        s.w.buildings[proc] = pb;
        s.w.corporations.at(s.corp).assets.push_back(proc);
        s.w.markets.at(s.market_f).supply[r_iron] = 0.0f;
        s.w.markets.at(s.market_f).demand[r_iron] = 1000.0f;

        const float reserve = processor_reservation(s.w, reg, s.corp, s.market_a)[r_iron];
        check(near(reserve, 40.0f), "(f).0 fixture: the processor reserves 40 iron in A");

        s.w.pool_at(s.corp, s.market_a).quantities[r_iron] = 40.0f;
        dispatch(s, reg);
        check(s.w.convoys.empty(), "(f).1 a pool holding only the reservation ships nothing");

        s.w.pool_at(s.corp, s.market_a).quantities[r_iron] = 100.0f;
        dispatch(s, reg);
        check(s.w.convoys.size() == 1 && near(s.w.convoys[0].cargo_qty, 60.0f) &&
                  near(pool_q(s.w, s.corp, s.market_a, r_iron), 40.0f),
              "(f).2 a pool of 100 ships only the 60 above the reservation; 40 stays for the processor");
    }

    std::printf("\n%s  (%d passed, %d failed)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_pass,
                g_fail);
    return g_fail == 0 ? 0 : 1;
}
