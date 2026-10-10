// Headless trade-reaches-for-price harness (BL-995 -> BL-1266; no SDL / Lua / ImGui).
//
// BL-1266 (TRADE.md § Auto and reserved trade): the dispatcher retired, and the
// rule it carried — the seller chasing a NET PRICE — is AUTO TRADE's. A
// corporation's unreserved trade points go, best margin first, to the routes
// among the markets its trade buildings reach, each route sized to what the
// destination can absorb above the landed cost and what the source SHELF
// holds. At last tick's resolved prices:
//
//     margin(d) = price_d - price_src - haul_per_unit(src -> d)
//     ship to the best margin  if  margin(d) > dispatch_margin x price_src
//
// The quantity is what d can absorb before its UNSMOOTHED target price
// (price_target: base x sqrt(D / S), band-clamped) falls to the landed cost
// plus the margin (BL-1203: aim = landed x (1 + margin)) —
// S* = D x (base / aim)^2, less last supply S — minus everything already
// pending into d (every corp's convoys bound there, every landing on d this
// tick); a zero-supply d is sized by the same S* (`trade_room`).
//
// Fixture: ONE body, a 64x4 plains grid (columns wrap, so 64 keeps F at
// column 30 genuinely 30 tiles from A), three markets on row 0 —
//     A (source) at column 0, N (near) at column 6, F (far) at column 30.
// Every market's iron base price is 10; A resolves at 10. The trader holds one
// staffed Planetary Marketplace (one trade point; iron's capacity per point
// is far above any room here, so points never bind).
//
//   (a) FARTHER AND DEARER BEATS NEARER: N resolves at 15, F at 30 — F.
//   (b) THE MARGIN: a best margin under dispatch_margin x home price moves
//       nothing; a smaller margin does move it (so the margin is what held it).
//   (c) THE QUANTITY: sized from the TARGET price, not the eased market price
//       (the eased price lags and over-asks every tick); exactly the unmet
//       demand for a zero-supply destination; cargo in transit — ANY corp's —
//       and landings already on d are subtracted, so two traders in one pass do
//       not each fill the same gap; a filled best destination yields to the
//       next-best that still beats home.
//   (d) NO CHURN: several full ticks (advance, credit, trade, clear) — every
//       convoy's destination net price beats its source price when it ships.
//   (e) A DELIVERY SELLS AT ITS DESTINATION: it lands at F and sells at F's
//       clear — an exchange row at F, seller = the corp.
//   (g) NO RE-EXPORT BEFORE THE CLEAR: a cargo delivered this tick has LANDED,
//       not reached F's shelf, so it cannot move again before F clears, even
//       when prices elsewhere jump — the same stock DOES move once the clear
//       has shelved it.
//
// RETIRED (BL-1265/1266): (f) THE RESERVATION (a corp's own processor's needs
// held back from shipping) went with the pool and the processor reservation —
// every good is on a shared shelf. (d)'s "every corporation convoy leaves A"
// went with the pool too: a delivery that sold onto F's shelf is ordinary
// shelf stock, and moving it on at a margin is trade working (BL-1071 ruled
// shelf stock moves); (d).1's net-price rule still binds every shipment.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/trade.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
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
constexpr float       k_base = 10.0f;

/// Iron on market @p m's SHELF (BL-1265: the shelf replaced the pool).
float shelf_q(const world& w, entity_id m)
{
    return w.markets.at(m).inventory[r_iron];
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
    m.base_price[r_iron] = k_base;
    m.price[r_iron]      = iron_price;
    s.w.markets[id] = m;
    return id;
}

/// Last clear's book for iron at `m`, stated so the market's resolved price and
/// its target agree (D = S x (price / base)^2) unless a row says otherwise.
void set_book(scenario& s, entity_id m, float supply, float demand)
{
    s.w.markets.at(m).supply[r_iron] = supply;
    s.w.markets.at(m).demand[r_iron] = demand;
}
float consistent_demand(float supply, float price) { return supply * (price / k_base) * (price / k_base); }

/// A corporation holding one staffed Planetary Marketplace on the body (one
/// trade point a tick) — on row 2, off every haul's row.
entity_id add_trader(scenario& s, int col)
{
    const entity_id c = s.w.create_entity();
    corporation_component cc;
    cc.balance   = 100000.0f;
    cc.is_player = true; // keeps the strategic scorer out; auto trade is one rule for every corp
    const entity_id mp = s.w.create_entity();
    building_component b{};
    b.tile               = tile_at(s.w, s.body, col, 2);
    b.type               = building_type::planetary_marketplace;
    b.workforce_assigned = 1.0f;
    s.w.buildings[mp] = b;
    cc.assets.push_back(mp);
    s.w.corporations[c] = cc;
    return c;
}

// One plains body, a trader, and a supply anchor off the haul's row for the
// passive-LP gate.
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

    s.corp = add_trader(s, 2);
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
    trade_params tp;
    tp.capacity[r_iron]   = 1.0e6f; // trade points never bind: the room does
    tp.marketplace_points = 1.0f;
    reg.set_trade(tp);
    return reg;
}

/// One trade pass (its purchases bill at a clear the caller may run).
void trade(scenario& s, const recipe_registry& reg)
{
    economy_report rep;
    run_trades(s.w, reg, rep);
}

/// Per-unit haul from `src` to `dest`, priced by the SAME shared function the
/// trade pass uses (cost is linear in quantity).
float haul_per_unit(scenario& s, const recipe_registry& reg, entity_id src, entity_id dest)
{
    const logistics_nodes nodes = collect_logistics_nodes(s.w);
    const convoy_leg leg = price_trade_leg(s.w, reg, nodes, s.corp, src, dest, r_iron, 1.0f);
    return leg.viable ? leg.cost : -1.0f;
}

float cargo_to(const world& w, entity_id dest)
{
    float q = 0.0f;
    for (const convoy_component& c : w.convoys)
        if (c.dest_market == dest && c.cargo_resource == resource_type::iron_ore)
            q += c.cargo_qty;
    return q;
}

/// (base / aim)^2 for a route landing at `landed` — the factor S* = D x it.
/// BL-1203 cold review, fix 1 (supply_system.cpp `dispatch_absorbable`): the
/// send is sized to leave the destination's target at the landed cost PLUS the
/// margin the route was chosen on, aim = landed x (1 + dispatch_margin) — not
/// at the bare landed cost (that aimed every destination at zero margin). The
/// zero-supply case sizes by the same S*. This harness's sizing rows predated
/// the fix and asserted the bare-landed S* and "exactly the unmet demand";
/// they were already stale before BL-1266 and are restated here.
float aim_factor(const recipe_registry& reg, float landed)
{
    const float aim = landed * (1.0f + reg.dispatch_margin());
    return (k_base / aim) * (k_base / aim);
}

int convoys_from(const world& w, entity_id src)
{
    int n = 0;
    for (const convoy_component& c : w.convoys)
        if (c.source_market == src)
            ++n;
    return n;
}

} // namespace

int main()
{
    std::printf("=== trade_reaches_for_price (BL-995 -> BL-1266: auto trade chases the margin) ===\n");

    // Fixture sanity: the three markets own their columns, and the margin the
    // harness registry carries is the nonzero default.
    {
        scenario s = make_scenario(15.0f, 30.0f);
        check(market_for_tile(s.w, tile_at(s.w, s.body, 0, 0)) == s.market_a &&
              market_for_tile(s.w, tile_at(s.w, s.body, 6, 0)) == s.market_n &&
              market_for_tile(s.w, tile_at(s.w, s.body, 30, 0)) == s.market_f,
              "K.1 fixture: columns 0 / 6 / 30 resolve to markets A / N / F");
        const recipe_registry reg = base_registry();
        check(reg.dispatch_margin() > 0.0f,
              "K.2 a registry that never authors the margin carries a nonzero default");
        const float hn = haul_per_unit(s, reg, s.market_a, s.market_n);
        const float hf = haul_per_unit(s, reg, s.market_a, s.market_f);
        std::printf("      haul/unit A->N %.4f, A->F %.4f\n", hn, hf);
        check(hn > 0.0f && hf > hn, "K.3 both legs are viable and F is the costlier haul");
        check(price_target(k_base, 100.0f, 900.0f, reg.price_band().floor_mult,
                           reg.price_band().ceil_mult) == 30.0f,
              "K.4 price_target is the unsmoothed law: base 10, D/S = 9 -> 30");
        check(corp_trade_markets(s.w, s.corp).size() == 3,
              "K.5 the trader's Marketplace reaches all three markets on the body");
    }

    // -----------------------------------------------------------------------
    // (a) farther and dearer beats nearer and cheaper
    // -----------------------------------------------------------------------
    world after_a; // (e) delivers this one
    recipe_registry reg_a = base_registry();
    entity_id a_corp = null_entity, a_f = null_entity;
    {
        scenario s = make_scenario(15.0f, 30.0f);
        set_book(s, s.market_n, 100.0f, consistent_demand(100.0f, 15.0f));
        set_book(s, s.market_f, 100.0f, consistent_demand(100.0f, 30.0f));
        s.w.markets.at(s.market_a).inventory[r_iron] = 1000.0f;

        trade(s, reg_a);
        check(!s.w.convoys.empty() && s.w.convoys[0].dest_market == s.market_f &&
                  s.w.convoys[0].source_market == s.market_a,
              "(a).1 the first (best-margin) shipment leaves A for the FARTHER market F (30), "
              "over the nearer N (15)");
        after_a = s.w;
        a_corp  = s.corp;
        a_f     = s.market_f;

        // (c) the quantity is the room below the target price: S* - S, S*
        // aimed at the landed cost plus the margin (aim_factor).
        const float haul   = haul_per_unit(s, reg_a, s.market_a, s.market_f);
        const float landed = 10.0f + haul;
        const float s_star = 900.0f * aim_factor(reg_a, landed);
        const float room   = s_star - 100.0f;
        const float sent   = cargo_to(s.w, s.market_f);
        std::printf("      S* - S = %.3f, sent to F = %.3f, shelf left %.3f\n", room, sent,
                    shelf_q(s.w, s.market_a));
        check(sent <= room + 1e-2f && near_rel(sent, room),
              "(c).1 the send is S* - S with S* = D x (base / (landed x (1 + margin)))^2, never more");
        check(near(shelf_q(s.w, s.market_a), 1000.0f - sent - cargo_to(s.w, s.market_n), 1e-2f),
              "(c).2 the source shelf is debited exactly the cargo shipped");
    }

    // (c) OVERSEND: F's eased price still reads 30, but its book (D 400, S 100)
    // targets 20. The send is sized from the target, not from the lagging 30.
    {
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(10.0f, 30.0f);
        set_book(s, s.market_f, 100.0f, 400.0f);
        s.w.markets.at(s.market_a).inventory[r_iron] = 5000.0f;
        trade(s, reg);
        const float landed      = 10.0f + haul_per_unit(s, reg, s.market_a, s.market_f);
        const float by_target   = 400.0f * aim_factor(reg, landed) - 100.0f;
        const float by_eased    = 100.0f * ((30.0f / landed) * (30.0f / landed) - 1.0f);
        const float sent        = cargo_to(s.w, s.market_f);
        std::printf("      sized by target %.3f, by eased price %.3f, sent %.3f\n", by_target,
                    by_eased, sent);
        check(near_rel(sent, by_target) && sent < by_eased,
              "(c).1b the send is sized from the UNSMOOTHED target (20), not the eased price (30)");
    }

    // -----------------------------------------------------------------------
    // (b) nothing moves when the best margin is under dispatch_margin
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry(); // margin 0.05: gate at 10.5 net
        scenario s = make_scenario(10.3f, 10.9f);
        set_book(s, s.market_n, 100.0f, consistent_demand(100.0f, 10.3f));
        set_book(s, s.market_f, 100.0f, consistent_demand(100.0f, 10.9f));
        s.w.markets.at(s.market_a).inventory[r_iron] = 1000.0f;
        const float hf = haul_per_unit(s, reg, s.market_a, s.market_f);
        std::printf("      net F = %.4f vs home 10 (margin gate %.4f)\n", 10.9f - hf,
                    10.0f * (1.0f + reg.dispatch_margin()));
        check(10.9f - hf > 10.0f && 10.9f - hf < 10.0f * (1.0f + reg.dispatch_margin()),
              "(b).0 fixture: F's net beats home, by less than the margin");
        trade(s, reg);
        check(s.w.convoys.empty() && near(shelf_q(s.w, s.market_a), 1000.0f),
              "(b).1 a net gain under margin x home price moves nothing");

        recipe_registry loose = base_registry();
        loose.set_dispatch_margin(0.001f);
        trade(s, loose);
        check(!s.w.convoys.empty() && s.w.convoys[0].dest_market == s.market_f,
              "(b).2 the same world with a 0.1% margin does ship to F — the margin held it");
    }

    // -----------------------------------------------------------------------
    // (c) a zero-supply destination: S* = D x factor with no supply to
    // subtract (BL-1203: the same S*, not the whole unmet demand); pending
    // subtracts. With F's book 0 / 50, k = aim_factor: the first send is 50k.
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(10.0f, 30.0f); // N at home's price: never a target
        set_book(s, s.market_f, 0.0f, 50.0f);
        s.w.markets.at(s.market_a).inventory[r_iron] = 1000.0f;
        const float k = aim_factor(reg, 10.0f + haul_per_unit(s, reg, s.market_a, s.market_f));

        trade(s, reg);
        std::printf("      zero-supply F: k = %.4f, sent %.3f (50k = %.3f)\n", k,
                    cargo_to(s.w, s.market_f), 50.0f * k);
        check(near(cargo_to(s.w, s.market_f), 50.0f * k, 1e-2f) && 50.0f * k < 50.0f,
              "(c).3 a destination with ZERO supply receives S* = D x (base / aim)^2 (50k), "
              "under its unmet demand");

        // The convoy is still on the lane (nothing advanced it). Trade again:
        // the gap is already filled by cargo in transit, so nothing more goes.
        const std::size_t before = s.w.convoys.size();
        trade(s, reg);
        check(s.w.convoys.size() == before && near(cargo_to(s.w, s.market_f), 50.0f * k, 1e-2f),
              "(c).4 with 50k already in transit to F, a second pass sends F nothing more");

        // Widen the gap to 80: the next pass sends only the 30k not yet in transit.
        s.w.markets.at(s.market_f).demand[r_iron] = 80.0f;
        trade(s, reg);
        check(s.w.convoys.size() == before + 1 && near(s.w.convoys.back().cargo_qty, 30.0f * k, 1e-2f),
              "(c).5 demand 80 with 50k in transit: the pass sends the missing 30k");

        // A HELD convoy's cargo is still committed: it keeps counting.
        s.w.convoys.front().held = true;
        trade(s, reg);
        check(s.w.convoys.size() == before + 1 && near(cargo_to(s.w, s.market_f), 80.0f * k, 1e-2f),
              "(c).5b a held convoy still counts as pending — nothing more goes to F");
    }
    {
        // FLOOD: two traders on A's one shelf, one pass, one gap of 50 at F.
        // Every corp's cargo bound for F counts, including what the first
        // trader committed earlier in the same pass — so the second sends nothing.
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(10.0f, 30.0f);
        set_book(s, s.market_f, 0.0f, 50.0f);
        s.w.markets.at(s.market_a).inventory[r_iron] = 2000.0f;
        add_trader(s, 4);
        const float k = aim_factor(reg, 10.0f + haul_per_unit(s, reg, s.market_a, s.market_f));
        trade(s, reg);
        check(s.w.convoys.size() == 1 && near(cargo_to(s.w, s.market_f), 50.0f * k, 1e-2f),
              "(c).6 two traders, one pass, one gap: 50k go in total, not 50k each");
    }
    {
        // Another corp's goods already LANDED on F this tick list at F's next
        // clear: they are pending too.
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(10.0f, 30.0f);
        set_book(s, s.market_f, 0.0f, 50.0f);
        s.w.markets.at(s.market_a).inventory[r_iron] = 1000.0f;
        const entity_id other = s.w.create_entity();
        s.w.corporations[other] = corporation_component{};
        s.w.land_goods(other, s.market_f, r_iron, 20.0f);
        const float k = aim_factor(reg, 10.0f + haul_per_unit(s, reg, s.market_a, s.market_f));
        trade(s, reg);
        check(near(cargo_to(s.w, s.market_f), 50.0f * k - 20.0f, 1e-2f),
              "(c).6b 20 of another corp's goods already landed on F: the send is 50k less the 20");
    }
    {
        // A filled best destination yields to the next-best that beats home.
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(15.0f, 30.0f);
        set_book(s, s.market_n, 100.0f, consistent_demand(100.0f, 15.0f));
        set_book(s, s.market_f, 0.0f, 50.0f);
        s.w.markets.at(s.market_a).inventory[r_iron] = 1000.0f;
        const entity_id other = s.w.create_entity();
        s.w.corporations[other] = corporation_component{};
        s.w.land_goods(other, s.market_f, r_iron, 50.0f); // F is already full
        trade(s, reg);
        check(s.w.convoys.size() == 1 && s.w.convoys[0].dest_market == s.market_n,
              "(c).7 F (best margin) has no room left, so the cargo goes to N (next best, beats home)");
    }

    // -----------------------------------------------------------------------
    // (d) no churn: a good never moves to a lower net price, over full ticks
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(15.0f, 30.0f);
        set_book(s, s.market_n, 100.0f, consistent_demand(100.0f, 15.0f));
        set_book(s, s.market_f, 100.0f, consistent_demand(100.0f, 30.0f));

        std::set<std::uint32_t> seen;
        int shipped = 0, bad = 0, from_non_a = 0;
        for (int t = 0; t < 40; ++t)
        {
            s.w.current_econ_tick = t;
            s.w.current_day_tick  = t;
            // A stand-in for production: A's shelf is topped up every tick.
            s.w.markets.at(s.market_a).inventory[r_iron] += 200.0f;

            // The app's order: advance -> arrivals -> (economy) -> trade -> clear.
            advance_convoys(s.w);
            credit_arrived_convoys(s.w, t);
            // Prices as they stand when the trade pass reads them (last clear's).
            std::map<entity_id, float> px;
            for (const entity_id m : { s.market_a, s.market_n, s.market_f })
                px[m] = price_of(s.w.markets.at(m), r_iron);
            economy_report rep;
            run_trades(s.w, reg, rep);
            for (const convoy_component& c : s.w.convoys)
            {
                if (!seen.insert(c.id).second)
                    continue;
                ++shipped;
                if (c.source_market != s.market_a)
                    ++from_non_a;
                const float p_src  = px[c.source_market];
                const float p_dest = px[c.dest_market];
                const float net    = p_dest - c.cost_paid / c.cargo_qty;
                if (!(net > p_src))
                {
                    ++bad;
                    std::printf("      tick %d convoy %u: dest net %.4f <= source %.4f\n", t, c.id,
                                net, p_src);
                }
            }
            clear_markets(s.w, reg, rep);
            // No population lives in this fixture, so the clear sees no demand
            // and prices fall. The demand a real market would carry is restated
            // on the book the trade pass reads (last clear's), so the loop keeps
            // trading.
            s.w.markets.at(s.market_n).demand[r_iron] = 225.0f;
            s.w.markets.at(s.market_f).demand[r_iron] = 900.0f;
        }
        std::printf("      %d shipments over 40 ticks (%d from a shelf other than A); "
                    "prices A %.3f N %.3f F %.3f\n",
                    shipped, from_non_a, price_of(s.w.markets.at(s.market_a), r_iron),
                    price_of(s.w.markets.at(s.market_n), r_iron),
                    price_of(s.w.markets.at(s.market_f), r_iron));
        check(shipped > 0, "(d).0 fixture: the loop ships at all");
        check(bad == 0,
              "(d).1 every shipment's destination net price beats its source price when it ships");
    }

    // -----------------------------------------------------------------------
    // (e) the (a) cargo, delivered, lands at F and sells at F's clear
    // -----------------------------------------------------------------------
    {
        world& w = after_a;
        float cargo = 0.0f;
        for (const convoy_component& c : w.convoys)
            if (c.dest_market == a_f) cargo += c.cargo_qty;
        for (int i = 0; i < 200 && !w.convoys.empty(); ++i)
        {
            advance_convoys(w);
            credit_arrived_convoys(w, i);
            if (w.landed(a_corp, a_f, r_iron) >= cargo - 1e-2f)
                break;
        }
        check(near(w.landed(a_corp, a_f, r_iron), cargo, 1e-2f),
              "(e).1 the convoy arrives and LANDS for the corp at F");
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
    // (g) a delivery meets its destination's clear before it can move again
    // -----------------------------------------------------------------------
    {
        const recipe_registry reg = base_registry();
        scenario s = make_scenario(15.0f, 30.0f);
        set_book(s, s.market_n, 0.0f, 0.0f); // N not a target yet: the cargo goes to F alone
        set_book(s, s.market_f, 100.0f, consistent_demand(100.0f, 30.0f));
        s.w.markets.at(s.market_a).inventory[r_iron] = 1000.0f;
        s.w.markets.at(s.market_n).price[r_iron] = 10.0f;
        trade(s, reg);
        check(s.w.convoys.size() == 1 && s.w.convoys[0].dest_market == s.market_f,
              "(g).0 fixture: a cargo leaves A for F");
        const float cargo = s.w.convoys.empty() ? 0.0f : s.w.convoys[0].cargo_qty;
        for (int i = 0; i < 200 && !s.w.convoys.empty() && !s.w.convoys[0].arrived; ++i)
            advance_convoys(s.w);
        credit_arrived_convoys(s.w, 1); // it LANDS at F this tick

        // Perturb: N jumps to 35 with deep demand, so F -> N now clears the
        // margin by a wide gap (F's home price is 30).
        s.w.markets.at(s.market_n).price[r_iron] = 35.0f;
        set_book(s, s.market_n, 100.0f, 100000.0f);
        const float f_to_n = 35.0f - haul_per_unit(s, reg, s.market_f, s.market_n);
        check(near(s.w.landed(s.corp, s.market_f, r_iron), cargo, 1e-2f) &&
                  f_to_n - 30.0f > reg.dispatch_margin() * 30.0f,
              "(g).1 fixture: the delivery has landed at F and F -> N now beats F's home price");
        trade(s, reg);
        check(convoys_from(s.w, s.market_f) == 0 &&
                  near(s.w.landed(s.corp, s.market_f, r_iron), cargo, 1e-2f),
              "(g).2 this tick's delivery does NOT move again before F clears (it is not on the shelf)");

        // Control: F's clear sells the landing onto F's shelf; the next pass,
        // at the same prices, the same gap moves it.
        clear_markets(s.w, reg, economy_report{});
        check(s.w.landed_this_tick.empty() && shelf_q(s.w, s.market_f) > 0.0f,
              "(g).3a fixture: the clear shelved the landing at F");
        s.w.markets.at(s.market_f).price[r_iron] = 30.0f;
        s.w.markets.at(s.market_n).price[r_iron] = 35.0f;
        set_book(s, s.market_n, 100.0f, 100000.0f);
        trade(s, reg);
        check(convoys_from(s.w, s.market_f) == 1,
              "(g).3 control: once the clear has shelved it, the same gap moves it");
    }

    std::printf("\n%s  (%d passed, %d failed)\n", g_fail == 0 ? "ALL PASS" : "FAILURES", g_pass,
                g_fail);
    return g_fail == 0 ? 0 : 1;
}
