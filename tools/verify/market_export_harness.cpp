// Headless market-export harness (BL-1071, shelf stock moves; no SDL / Lua / ImGui).
//
// SUPPLY.md § Dispatch trigger, "A market exports its own shelf": stock on a
// market's shelf (market_component::inventory) belongs to no corporation, so
// each market runs the seller's net-price rule on it, after every corporation's
// own dispatch:
//
//     surplus  = shelf - last clear's local demand - this tick's shelf deliveries
//     net(d)   = price_d - haul_per_unit(src centre -> d centre)      (same body)
//     send to argmax_d net(d)  if  net(d) - price_src > margin x price_src
//     qty      = min(surplus, absorbable(d) - pending(d) - d's own shelf)
//
// The convoy's owner is null_entity; it lands on the destination SHELF; no
// balance moves (the market holds no treasury — FINANCE.md; the haul is the
// margin the export gives up).
//
// Fixture: trade_reaches_for_price's — ONE plains body 64x4, three markets on
// row 0: A (source) at column 0, N at column 6, F at column 30; iron base 10.
//
//   (a) A FAR, BETTER-PRICED MARKET TAKES THE SHELF: A's surplus shelf iron goes
//       to F (30) over N (15), sized to F's room, owner null, from A's centre.
//   (b) THE LOCAL NEED STAYS: a shelf at or under last clear's local demand
//       ships nothing; above it, only the excess.
//   (c) UNDER THE MARGIN NOTHING MOVES; a smaller margin moves it.
//   (d) GOODS ARE CONSERVED: shelf debit = cargo = the destination shelf's credit
//       on arrival; shelf + cargo is constant at every step; no balance moves.
//   (e) NO CHURN: a second pass with the cargo in flight sends nothing; once it
//       has landed on F's shelf, F's shelf counts against F's room, so nothing
//       more goes; this tick's delivery does not move again even when F -> N
//       jumps; over many full ticks every export beats its source by the margin.
//   (f) CORPORATIONS FIRST: a corporation's surplus in A claims F's room before
//       A's shelf does; the shelf gets only what is left.
//   (g) THE PASSIVE-LP CAP binds a market's export as a corporation's: no LP,
//       no export, nothing mutated.
//   (h) SAVE ROUND-TRIP: an in-flight market export survives a snapshot and, on
//       the loaded world, lands on the destination shelf.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>

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
constexpr float       k_base = 10.0f;

struct scenario
{
    world     w;
    entity_id body     = null_entity;
    entity_id corp     = null_entity;
    entity_id market_a = null_entity;
    entity_id market_n = null_entity;
    entity_id market_f = null_entity;
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
    m.base_price[r_iron] = k_base;
    m.price[r_iron]      = iron_price;
    s.w.markets[id] = m;
    return id;
}

void set_book(scenario& s, entity_id m, float supply, float demand)
{
    s.w.markets.at(m).supply[r_iron] = supply;
    s.w.markets.at(m).demand[r_iron] = demand;
}
float consistent_demand(float supply, float price) { return supply * (price / k_base) * (price / k_base); }

float& shelf(scenario& s, entity_id m) { return s.w.markets.at(m).inventory[r_iron]; }

/// One plains body, a player-flagged corp (keeps the strategic scorer out) with
/// its building at A's centre, a population centre off the haul's row as the
/// supply anchor the passive-LP gate needs.
scenario make_scenario(float price_n, float price_f)
{
    scenario s;
    s.body = s.w.create_entity();
    body_component bc{};
    bc.name              = "Anvil";
    bc.type              = body_type::planet;
    bc.orbital_radius_au = 1.0f;
    bc.grid_width        = 64;
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
    mp.active_lp_per_anchor_tick = 1.0e6f; // the LP gate is exercised only in (g)
    reg.set_military(mp);
    return reg;
}

convoy_dispatch_tick dispatch(scenario& s, const recipe_registry& reg)
{
    return dispatch_convoys(s.w, reg, reg.logistics_cost(convoy_mode::land),
                            reg.logistics_cost(convoy_mode::space));
}

/// Per-unit haul from market `src`'s centre to market `dest`, by the market's own
/// leg pricer. Cross-checked in (a).5 against a corporation holding nothing in
/// `src`'s catchment, which hauls from the same centre by price_convoy_leg.
float centre_haul(scenario& s, const recipe_registry& reg, entity_id src, entity_id dest)
{
    const logistics_nodes nodes = collect_logistics_nodes(s.w);
    const convoy_leg leg = price_market_export_leg(s.w, reg, nodes, src, dest, 1.0f);
    return leg.viable ? leg.cost : -1.0f;
}

float ghost_corp_haul(scenario& s, const recipe_registry& reg, entity_id src, entity_id dest)
{
    const entity_id ghost = s.w.create_entity();
    s.w.corporations[ghost] = corporation_component{};
    const logistics_nodes nodes = collect_logistics_nodes(s.w);
    const convoy_leg leg = price_convoy_leg(s.w, reg, nodes, ghost, src, dest, r_iron, 1.0f,
                                            reg.logistics_cost(convoy_mode::space));
    s.w.corporations.erase(ghost);
    return leg.viable ? leg.cost : -1.0f;
}

int market_exports(const world& w)
{
    int n = 0;
    for (const convoy_component& c : w.convoys)
        if (c.corp == null_entity)
            ++n;
    return n;
}

float cargo_in_flight(const world& w)
{
    float q = 0.0f;
    for (const convoy_component& c : w.convoys)
        if (c.cargo_resource == resource_type::iron_ore)
            q += c.cargo_qty;
    return q;
}

float shelves(scenario& s)
{
    return shelf(s, s.market_a) + shelf(s, s.market_n) + shelf(s, s.market_f);
}

void land_all(scenario& s, int tick)
{
    for (int i = 0; i < 400 && !s.w.convoys.empty(); ++i)
    {
        advance_convoys(s.w);
        if (std::any_of(s.w.convoys.begin(), s.w.convoys.end(),
                        [](const convoy_component& c) { return c.arrived; }))
            break;
    }
    credit_arrived_convoys(s.w, tick);
}

} // namespace

int main()
{
    std::printf("=== market_export_harness (BL-1071: a market exports its own shelf) ===\n");

    // -----------------------------------------------------------------------
    // (a) a far, better-priced market takes the shelf
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(15.0f, 30.0f);
        set_book(s, s.market_a, 100.0f, 100.0f);          // A: local demand 100
        set_book(s, s.market_n, 100.0f, consistent_demand(100.0f, 15.0f));
        set_book(s, s.market_f, 100.0f, consistent_demand(100.0f, 30.0f));
        shelf(s, s.market_a) = 1000.0f;

        const float haul_f = centre_haul(s, reg, s.market_a, s.market_f);
        const float landed = 10.0f + haul_f;
        const float room   = 900.0f * (k_base / landed) * (k_base / landed) - 100.0f;
        const float bal0   = s.w.corporations.at(s.corp).balance;
        const float before = shelves(s);

        const convoy_dispatch_tick dt = dispatch(s, reg);
        std::printf("      haul/unit A->F %.4f; F's room %.3f; surplus %.1f\n", haul_f, room, 900.0f);
        check(dt.market_exports == 1 && dt.dispatched == 0 && s.w.convoys.size() == 1,
              "(a).1 one convoy leaves, counted as a MARKET export, not a corporation's dispatch");
        const convoy_component c = s.w.convoys.empty() ? convoy_component{} : s.w.convoys[0];
        check(c.corp == null_entity && c.source_market == s.market_a && c.dest_market == s.market_f,
              "(a).2 owner null, from A to the FARTHER, dearer F (30) over N (15)");
        check(near_rel(c.cargo_qty, room) && c.cargo_qty < 900.0f,
              "(a).3 the cargo is F's room (S* - S at the landed cost), less than the 900 surplus");
        check(near_rel(c.cost_paid, haul_f * c.cargo_qty),
              "(a).4 the haul is priced from A's centre (cost_paid = haul/unit x cargo)");
        check(haul_f > 0.0f && haul_f == ghost_corp_haul(s, reg, s.market_a, s.market_f) &&
                  price_market_export_leg(s.w, reg, collect_logistics_nodes(s.w), s.market_a,
                                          s.market_a, 1.0f).viable == false,
              "(a).5 the market's leg is the corporation leg from the same centre, to the unit; "
              "a leg to itself is not a haul");

        // (d) conservation on this same export.
        check(near(shelf(s, s.market_a), 1000.0f - c.cargo_qty, 1e-2f) &&
                  near(shelves(s) + cargo_in_flight(s.w), before, 1e-2f),
              "(d).1 A's shelf is debited exactly the cargo; shelf + cargo is unchanged");
        check(s.w.corporations.at(s.corp).balance == bal0,
              "(d).2 no balance moves — the market holds no treasury, the haul is the margin given up");
        const float f0 = shelf(s, s.market_f);
        land_all(s, 1);
        check(s.w.convoys.empty() && near(shelf(s, s.market_f), f0 + c.cargo_qty, 1e-2f) &&
                  s.w.find_pool(null_entity, s.market_f) == nullptr,
              "(d).3 on arrival the cargo lands on F's SHELF, whole, and in no pool");
        check(near(shelves(s), before, 1e-2f), "(d).4 after arrival every unit is on a shelf again");

        // (e) this tick's delivery does not move again; F's shelf is F's pending.
        s.w.markets.at(s.market_n).price[r_iron] = 45.0f;
        set_book(s, s.market_n, 100.0f, 100000.0f);
        set_book(s, s.market_f, 100.0f, 0.0f); // F wants none of it any more
        const float f_to_n = 45.0f - centre_haul(s, reg, s.market_f, s.market_n);
        check(f_to_n - 30.0f > reg.dispatch_margin() * 30.0f,
              "(e).0 fixture: F -> N now beats F's home price by far more than the margin");
        const std::size_t conv_before = s.w.convoys.size();
        dispatch(s, reg);
        bool f_moved = false;
        for (const convoy_component& cv : s.w.convoys)
            if (cv.source_market == s.market_f)
                f_moved = true;
        check(!f_moved, "(e).1 this tick's shelf delivery at F does NOT move again before F's economy step");
        credit_arrived_convoys(s.w, 2); // next tick: nothing arrives, the record resets
        dispatch(s, reg);
        f_moved = false;
        for (const convoy_component& cv : s.w.convoys)
            if (cv.source_market == s.market_f && cv.corp == null_entity)
                f_moved = true;
        check(f_moved && s.w.convoys.size() > conv_before,
              "(e).2 control: a tick later the same shelf stock DOES move to N (it is shelf stock now)");
    }

    // -----------------------------------------------------------------------
    // (b) the local need stays
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(10.0f, 30.0f);
        set_book(s, s.market_a, 100.0f, 100.0f);
        set_book(s, s.market_f, 0.0f, 1000.0f); // a deep gap at F
        shelf(s, s.market_a) = 100.0f;
        check(market_shelf_surplus(s.w, s.market_a, r_iron) == 0.0f,
              "(b).0 a shelf of 100 against local demand 100 has no surplus");
        dispatch(s, reg);
        check(s.w.convoys.empty() && shelf(s, s.market_a) == 100.0f,
              "(b).1 a shelf at its local need ships nothing, however deep F's gap");
        shelf(s, s.market_a) = 160.0f;
        dispatch(s, reg);
        check(s.w.convoys.size() == 1 && near(s.w.convoys[0].cargo_qty, 60.0f) &&
                  near(shelf(s, s.market_a), 100.0f),
              "(b).2 a shelf of 160 ships only the 60 above the local need; 100 stays home");
    }

    // -----------------------------------------------------------------------
    // (c) under the margin nothing moves
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry(); // margin 0.05: gate at 10.5 net
        scenario s = make_scenario(10.3f, 10.9f);
        set_book(s, s.market_n, 100.0f, consistent_demand(100.0f, 10.3f));
        set_book(s, s.market_f, 100.0f, consistent_demand(100.0f, 10.9f));
        shelf(s, s.market_a) = 1000.0f;
        const float hf = centre_haul(s, reg, s.market_a, s.market_f);
        check(10.9f - hf > 10.0f && 10.9f - hf < 10.0f * (1.0f + reg.dispatch_margin()),
              "(c).0 fixture: F's net beats home, by less than the margin");
        dispatch(s, reg);
        check(s.w.convoys.empty() && shelf(s, s.market_a) == 1000.0f,
              "(c).1 a net gain under margin x home price moves nothing off the shelf");
        recipe_registry loose = base_registry();
        loose.set_dispatch_margin(0.001f);
        dispatch(s, loose);
        check(market_exports(s.w) == 1 && s.w.convoys[0].dest_market == s.market_f,
              "(c).2 the same shelf with a 0.1% margin does go to F — the margin held it");
    }

    // -----------------------------------------------------------------------
    // (e) no churn: in flight, landed, and over full ticks
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(10.0f, 30.0f);
        set_book(s, s.market_f, 0.0f, 50.0f); // F absorbs exactly its unmet 50
        shelf(s, s.market_a) = 1000.0f;
        dispatch(s, reg);
        check(market_exports(s.w) == 1 && near(cargo_in_flight(s.w), 50.0f),
              "(e).3 a zero-supply F takes exactly its unmet demand (50) off A's shelf");
        dispatch(s, reg);
        check(s.w.convoys.size() == 1 && near(cargo_in_flight(s.w), 50.0f),
              "(e).4 with 50 in flight to F, a second pass sends nothing more");
        land_all(s, 1);
        credit_arrived_convoys(s.w, 2);
        dispatch(s, reg);
        check(s.w.convoys.empty() && near(shelf(s, s.market_f), 50.0f),
              "(e).5 once the 50 sit on F's shelf they count against F's room: nothing more goes");
    }
    {
        // Full ticks with a clear, the harness's own (d) loop shape: a corp
        // tops A up every tick; auto-surplus puts what it does not haul on A's
        // shelf; the shelf then moves by the market's rule.
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(15.0f, 30.0f);
        set_book(s, s.market_n, 100.0f, consistent_demand(100.0f, 15.0f));
        set_book(s, s.market_f, 100.0f, consistent_demand(100.0f, 30.0f));
        int exports = 0, bad = 0, back_and_forth = 0;
        std::vector<std::pair<entity_id, entity_id>> lanes;
        for (int t = 0; t < 40; ++t)
        {
            s.w.current_econ_tick = t;
            s.w.current_day_tick  = t;
            s.w.pool_at(s.corp, s.market_a).quantities[r_iron] += 200.0f;
            advance_convoys(s.w);
            credit_arrived_convoys(s.w, t);
            const std::size_t n0 = s.w.convoys.size();
            dispatch(s, reg);
            for (std::size_t i = n0; i < s.w.convoys.size(); ++i)
            {
                const convoy_component& c = s.w.convoys[i];
                if (c.corp != null_entity)
                    continue;
                ++exports;
                const float p_src  = s.w.markets.at(c.source_market).price[r_iron];
                const float p_dest = s.w.markets.at(c.dest_market).price[r_iron];
                if (!(p_dest - c.cost_paid / c.cargo_qty - p_src > reg.dispatch_margin() * p_src))
                    ++bad;
                for (const auto& l : lanes)
                    if (l.first == c.dest_market && l.second == c.source_market)
                        ++back_and_forth;
                lanes.push_back({c.source_market, c.dest_market});
            }
            clear_markets(s.w, reg, economy_report{});
            s.w.markets.at(s.market_n).demand[r_iron] = 225.0f;
            s.w.markets.at(s.market_f).demand[r_iron] = 900.0f;
        }
        std::printf("      %d market exports over 40 ticks; shelves A %.1f N %.1f F %.1f\n", exports,
                    shelf(s, s.market_a), shelf(s, s.market_n), shelf(s, s.market_f));
        check(exports > 0, "(e).6 fixture: the full-tick loop exports shelf stock at all");
        check(bad == 0, "(e).7 every market export beat its source price by the margin at dispatch");
        check(back_and_forth == 0, "(e).8 no good ever ran back down a lane it came up (no churn)");
    }

    // -----------------------------------------------------------------------
    // (f) corporations first
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(10.0f, 30.0f);
        set_book(s, s.market_f, 0.0f, 50.0f);
        s.w.pool_at(s.corp, s.market_a).quantities[r_iron] = 30.0f;
        shelf(s, s.market_a) = 1000.0f;
        const convoy_dispatch_tick dt = dispatch(s, reg);
        float corp_q = 0.0f, market_q = 0.0f;
        for (const convoy_component& c : s.w.convoys)
            (c.corp == null_entity ? market_q : corp_q) += c.cargo_qty;
        check(dt.dispatched == 1 && dt.market_exports == 1 && near(corp_q, 30.0f) &&
                  near(market_q, 20.0f),
              "(f).1 F's room of 50: the corporation's 30 go first, A's shelf sends the other 20");
    }

    // -----------------------------------------------------------------------
    // (g) the passive-LP cap binds
    // -----------------------------------------------------------------------
    {
        recipe_registry reg = base_registry();
        military_capability_params mp = reg.military();
        mp.active_lp_per_anchor_tick = 0.0f;
        reg.set_military(mp);
        scenario s = make_scenario(10.0f, 30.0f);
        set_book(s, s.market_f, 0.0f, 50.0f);
        shelf(s, s.market_a) = 1000.0f;
        const convoy_dispatch_tick dt = dispatch(s, reg);
        check(s.w.convoys.empty() && dt.refused_no_lp == 1 && shelf(s, s.market_a) == 1000.0f,
              "(g).1 with no passive LP the export is refused and the shelf is untouched");
    }

    // -----------------------------------------------------------------------
    // (h) save round-trip of an in-flight market export
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(10.0f, 30.0f);
        set_book(s, s.market_f, 0.0f, 50.0f);
        shelf(s, s.market_a) = 1000.0f;
        dispatch(s, reg);
        std::stringstream buf(std::ios::in | std::ios::out | std::ios::binary);
        write_world_snapshot(s.w, buf);
        world loaded;
        const bool ok = read_world_snapshot(loaded, buf);
        const bool same = ok && loaded.convoys.size() == 1 && loaded.convoys[0].corp == null_entity &&
                          loaded.convoys[0].source_market == s.market_a &&
                          loaded.convoys[0].cargo_qty == s.w.convoys[0].cargo_qty &&
                          loaded.markets.at(s.market_a).inventory[r_iron] == shelf(s, s.market_a);
        check(same, "(h).1 the in-flight export and A's debited shelf survive a snapshot round trip");
        for (int i = 0; i < 400 && ok && !loaded.convoys.empty() && !loaded.convoys[0].arrived; ++i)
            advance_convoys(loaded);
        if (ok)
            credit_arrived_convoys(loaded, 1);
        check(ok && loaded.convoys.empty() && near(loaded.markets.at(s.market_f).inventory[r_iron], 50.0f),
              "(h).2 on the loaded world it lands on F's shelf");
    }

    std::printf("\n%s  (%d passed, %d failed)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_pass,
                g_fail);
    return g_fail == 0 ? 0 : 1;
}
