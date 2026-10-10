// Headless pools-per-market harness (BL-1003 -> BL-1265; no SDL / Lua / ImGui).
//
// BL-1265 (MARKETS.md § The shelf economy, Ben 2026-10-10): corporations hold
// no pools — the market owns its shelf. What BL-1003 established and still
// stands is the ROUTING: a building's output LANDS on the shelf of the market
// whose catchment holds its tile; its inputs and a construction site's
// materials are bought off that same shelf; its want and fill book in that
// market. A same-body trade lands and sells at its destination. Every row uses
// ONE body with TWO markets and a corp whose lowest-id building (and HQ) sits
// in market A's catchment, so a regression to a representative-market routing
// books at A where the row expects B.
//
//   (a) OUTPUT: a processor in B's catchment lands its output on B, not A; its
//       fill books under (corp, B).
//   (b) WANT: a construction site in B's catchment registers its want under
//       (corp, B), and clearing puts that demand on market B, none on A.
//   (b') DRAW: the site draws B's SHELF; want and fill are the whole need, at
//       B; stock on A's shelf is not reachable.
//   (c) TRADE: a shipment A -> B debits A's shelf, lands at B on arrival, and
//       the cargo sells AT B, at B's price.
//   (f) OPENING STOCK: `place_opening_stock` puts a corporation's seeded stock
//       on the shelves of the markets it sits in, split by how many of its
//       buildings each catchment holds, and is idempotent.
//   (h) PROCUREMENT: a completing contract's supplier buys off its HOME
//       market's shelf on the body (and builds the rest to order); the buyer's
//       delivery LANDS on its home market; the supplier's stock elsewhere on the
//       body is not touched (goods do not teleport within a body).
//
// RETIRED with pools (BL-1265): (d) a spawned market absorbs a body-level pool;
// (e) the save round trip of pool keys; the old (f) rehoming of opening pools;
// (g) a standing sell order listing from each market pool (the order book
// retired too). The save round trip of shelves and trades is save_roundtrip's.
//
// The process exits non-zero if any assertion FAILs.

#include "world/components.hpp"
#include "world/corp_command.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <array>
#include <cmath>
#include <cstdio>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }

constexpr std::size_t r_iron  = static_cast<std::size_t>(resource_type::iron_ore);
constexpr std::size_t r_steel = static_cast<std::size_t>(resource_type::steel);

// ---------------------------------------------------------------------------
// Fixture: one body, a 32x4 plains grid (convoy_command.cpp's idiom), market A
// centred at (0,0) and market B centred at (0,3). The corp's FIRST building —
// its lowest id and HQ — sits at (0,0), in A's catchment. Everything a row adds
// in B's catchment goes at (0,3).
// ---------------------------------------------------------------------------
struct scenario
{
    world     w;
    entity_id body     = null_entity;
    entity_id corp     = null_entity;
    entity_id anchor   = null_entity; ///< The corp's lowest-id building (in A).
    entity_id market_a = null_entity;
    entity_id market_b = null_entity;
    entity_id tile_a   = null_entity; ///< (0,0)
    entity_id tile_b   = null_entity; ///< (0,3)
};

entity_id tile_at(world& w, entity_id body, int c, int r)
{
    const int gw = w.bodies.at(body).grid_width;
    return body_tile_grid(w, body)[static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                                   + static_cast<std::size_t>(c)];
}

scenario make_scenario()
{
    scenario s;
    s.body = s.w.create_entity();
    body_component bc{};
    bc.name              = "Anvil";
    bc.type              = body_type::planet;
    bc.orbital_radius_au = 1.0f;
    bc.grid_width        = 32;
    bc.grid_height       = 4;
    s.w.bodies[s.body] = bc;
    s.w.home_body = s.body;
    for (int r = 0; r < bc.grid_height; ++r)
        for (int c = 0; c < bc.grid_width; ++c)
        {
            const entity_id t = s.w.create_entity();
            tile_component tc{};
            tc.body = s.body;
            tc.grid_x = c;
            tc.grid_y = r;
            tc.substrate = terrain_substrate::sedimentary;
            tc.cover = terrain_cover::grass;
            tc.cover_density = 150;
            tc.landform = terrain_landform::plains;
            s.w.tiles[t] = tc;
        }
    s.tile_a = tile_at(s.w, s.body, 0, 0);
    s.tile_b = tile_at(s.w, s.body, 0, 3);

    s.corp = s.w.create_entity();
    corporation_component cc;
    cc.balance   = 10000.0f;
    cc.is_player = true; // keep the strategic scorer out of this harness
    s.anchor = s.w.create_entity();
    building_component b{};
    b.tile = s.tile_a;
    b.type = building_type::extraction_site;
    s.w.buildings[s.anchor] = b;
    cc.assets.push_back(s.anchor);
    cc.hq_building = s.anchor;
    s.w.corporations[s.corp] = cc;
    s.w.player_entity = s.corp;

    // A supply anchor off the haul's column, so a shipment clears the
    // passive-LP gate without earning a node discount (convoy_command.cpp's
    // reasoning).
    s.w.population_centre_tile[s.w.create_entity()] = tile_at(s.w, s.body, 1, 0);

    s.market_a = s.w.create_entity();
    market_component ma{};
    ma.body = s.body;
    ma.centre_tile = s.tile_a;
    ma.base_price[r_iron]  = 5.0f;
    ma.base_price[r_steel] = 8.0f;
    ma.price = ma.base_price;
    s.w.markets[s.market_a] = ma;

    s.market_b = s.w.create_entity();
    market_component mb{};
    mb.body = s.body;
    mb.centre_tile = s.tile_b;
    mb.base_price[r_iron]  = 9.0f; // dearer than A, so "sold at B's price" is visible
    mb.base_price[r_steel] = 8.0f;
    mb.price = mb.base_price;
    s.w.markets[s.market_b] = mb;
    return s;
}

recipe_registry processing_registry(uint16_t& steel_id)
{
    recipe_registry reg;
    reg.set_thresholds(1.0f, 0.2f);
    building_economics pr;
    pr.base_rate = 20.0f;
    reg.set_economics(building_type::processing_facility, pr);
    recipe steel;
    steel.name = "steel";
    steel.inputs[r_iron]   = 2.0f;
    steel.outputs[r_steel] = 1.0f;
    steel_id = reg.add_recipe(steel);
    return reg;
}

entity_id add_site(scenario& s, entity_id tile)
{
    const entity_id site = s.w.create_entity();
    building_component b{};
    b.tile = tile;
    b.type = building_type::processing_facility;
    b.ticks_remaining = 1;
    s.w.buildings[site] = b;
    s.w.corporations.at(s.corp).assets.push_back(site);
    return site;
}

recipe_registry site_registry()
{
    recipe_registry reg;
    building_economics pr;
    pr.build_cost = 0.0f;
    pr.build_duration_ticks = 1.0f;
    pr.resource_build_cost[r_steel] = 10.0f;
    reg.set_economics(building_type::processing_facility, pr);
    return reg;
}

} // namespace

int main()
{
    std::printf("=== pools_per_market (BL-1003 -> BL-1265: goods route by market, on shelves) ===\n");

    // -----------------------------------------------------------------------
    // Key resolution — the fixture is what every row leans on.
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario();
        check(market_for_tile(s.w, s.tile_a) == s.market_a &&
              market_for_tile(s.w, s.tile_b) == s.market_b,
              "K.1 fixture: (0,0) resolves to market A and (0,3) to market B");
        check(market_body(s.w, s.market_b) == s.body && corp_hq_market(s.w, s.corp) == s.market_a,
              "K.2 a market resolves to its body; the corp's HQ market is A");
    }

    // -----------------------------------------------------------------------
    // (a) a building's output lands on its TILE market
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario();
        uint16_t steel_id = 0;
        const recipe_registry reg = processing_registry(steel_id);
        s.w.markets.at(s.market_b).inventory[r_iron] = 1000.0f;

        const entity_id proc = s.w.create_entity();
        building_component b{};
        b.tile = s.tile_b;
        b.type = building_type::processing_facility;
        b.workforce_assigned = 0.5f;
        b.recipe = steel_id;
        // Pinned: the player's auto-solver would idle a steelworks buying iron
        // at B's deliberately dear price, and this row is about WHERE output
        // lands, not whether it pays.
        b.workforce_auto = false;
        s.w.buildings[proc] = b;
        s.w.corporations.at(s.corp).assets.push_back(proc);

        const economy_report rep = run_economy_step(s.w, reg);
        float out = 0.0f;
        for (const building_report& br : rep.buildings)
            if (br.building == proc)
                out = br.output_quantity;

        check(out > 0.0f, "(a).1 fixture: the processor in B's catchment ran");
        check(near(s.w.landed(s.corp, s.market_b, r_steel), out),
              "(a).2 its whole output LANDED on market B");
        check(s.w.landed(s.corp, s.market_a, r_steel) == 0.0f,
              "(a).3 nothing landed on market A — the corp's HQ (representative) market");
        const auto pit = rep.purchases.find({s.corp, s.market_b});
        check(pit != rep.purchases.end() && pit->second[r_iron] > 0.0f &&
              rep.purchases.find({s.corp, s.market_a}) == rep.purchases.end() &&
              rep.purchases.find({s.corp, s.body}) == rep.purchases.end(),
              "(a).4 its fill books under (corp, market B) only");
    }

    // -----------------------------------------------------------------------
    // (b) a construction site's want lands on its OWN tile market's demand
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario();
        const recipe_registry reg = site_registry();
        add_site(s, s.tile_b);

        const economy_report rep = run_economy_step(s.w, reg);
        const auto wit = rep.wants.find({s.corp, s.market_b});
        check(wit != rep.wants.end() && near(wit->second[r_steel], 10.0f),
              "(b).1 the site's steel want is keyed (corp, market B)");
        check(rep.wants.find({s.corp, s.market_a}) == rep.wants.end() &&
              rep.wants.find({s.corp, s.body}) == rep.wants.end(),
              "(b).2 no want is keyed to market A or to the body");

        clear_markets(s.w, reg, rep);
        check(s.w.markets.at(s.market_b).demand[r_steel] >= 10.0f - 1e-3f,
              "(b).3 clearing puts the want on market B's demand");
        check(s.w.markets.at(s.market_a).demand[r_steel] == 0.0f,
              "(b).4 market A — the corp's representative market — sees none of it");
    }

    // (b') the site draws its market's SHELF; stock on another market's shelf
    // is not reachable, and the whole need is bid for and billed at B.
    {
        scenario s = make_scenario();
        const recipe_registry reg = site_registry();
        s.w.markets.at(s.market_a).inventory[r_steel] = 50.0f; // elsewhere: not reachable
        s.w.markets.at(s.market_b).inventory[r_steel] = 10.0f;
        const entity_id site = add_site(s, s.tile_b);

        const economy_report rep = run_economy_step(s.w, reg);
        const auto wit = rep.wants.find({s.corp, s.market_b});
        const auto pit = rep.purchases.find({s.corp, s.market_b});
        check(s.w.buildings.at(site).ticks_remaining == 0,
              "(b').1 the shelf's 10 covers the need of 10: the build completes at full rate");
        check(near(s.w.markets.at(s.market_b).inventory[r_steel], 0.0f),
              "(b').2 it drew all 10 off B's shelf");
        check(wit != rep.wants.end() && near(wit->second[r_steel], 10.0f) &&
              pit != rep.purchases.end() && near(pit->second[r_steel], 10.0f),
              "(b').3 want and fill are the whole need of 10, booked at market B");
        check(near(s.w.markets.at(s.market_a).inventory[r_steel], 50.0f),
              "(b').4 market A's shelf is untouched: goods do not teleport within a body");
    }

    // -----------------------------------------------------------------------
    // (c) a trade A -> B lands at B, and the goods sell at B's price
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario();
        recipe_registry reg;
        military_capability_params mp = reg.military();
        mp.active_lp_per_anchor_tick = 1.0e6f;
        reg.set_military(mp);

        s.w.markets.at(s.market_a).inventory[r_iron] = 100.0f;

        economy_report ship_rep;
        const logistics_nodes nodes = collect_logistics_nodes(s.w);
        const convoy_leg leg = price_trade_leg(s.w, reg, nodes, s.corp, s.market_a, s.market_b,
                                               r_iron, 30.0f);
        check(leg.viable && commit_trade_shipment(s.w, reg, ship_rep, s.corp, s.market_a,
                                                  s.market_b, r_iron, 30.0f, leg),
              "(c).1 fixture: the corp ships 30 iron from market A to market B");
        check(near(s.w.markets.at(s.market_a).inventory[r_iron], 70.0f),
              "(c).2 the shipment buys off the SOURCE market's shelf (100 -> 70)");

        for (int i = 0; i < 50 && !s.w.convoys.empty(); ++i)
        {
            advance_convoys(s.w);
            credit_arrived_convoys(s.w, i);
            if (s.w.convoys.empty())
                break;
        }
        check(s.w.convoys.empty(), "(c).3 the convoy arrives and is retired");
        check(near(s.w.landed(s.corp, s.market_b, r_iron), 30.0f) &&
              s.w.landed(s.corp, s.market_a, r_iron) == 0.0f,
              "(c).4 arrival LANDS the cargo on the DESTINATION market — a same-body trade "
              "never lands back where it left");

        const float inv_b_before = s.w.markets.at(s.market_b).inventory[r_iron];
        clear_markets(s.w, reg, economy_report{});
        bool sold_at_b = false, price_is_b = false;
        for (const exchange_record& e : s.w.exchanges.entries)
            if (e.market == s.market_b && e.seller == s.corp &&
                e.resource == resource_type::iron_ore && near(e.quantity, 30.0f))
            {
                sold_at_b  = true;
                price_is_b = near(e.unit_price, s.w.markets.at(s.market_b).price[r_iron]);
            }
        check(sold_at_b, "(c).5 the delivered 30 sell at market B (an exchange row at B, seller = corp)");
        check(price_is_b && s.w.markets.at(s.market_b).price[r_iron] >
                                s.w.markets.at(s.market_a).price[r_iron],
              "(c).6 ...at B's resolved price, which is above A's");
        check(near(s.w.markets.at(s.market_b).inventory[r_iron] - inv_b_before, 30.0f) &&
              s.w.landed_this_tick.empty(),
              "(c).7 B's shelf gains exactly the cargo and the landing is spent");
    }

    // -----------------------------------------------------------------------
    // (f) the opening stock goes on the shelves the corporation sits in
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario();
        // Two more works in B's catchment: weights A 1, B 2.
        for (int k = 0; k < 2; ++k)
        {
            const entity_id b2 = s.w.create_entity();
            building_component b{};
            b.tile = tile_at(s.w, s.body, 1 + k, 3);
            b.type = building_type::extraction_site;
            s.w.buildings[b2] = b;
            s.w.corporations.at(s.corp).assets.push_back(b2);
        }
        std::array<float, resource_count> stock{};
        stock[r_iron] = 30.0f;
        seed_opening_stock(s.w, s.corp, stock);
        check(s.w.markets.at(s.market_a).inventory[r_iron] == 0.0f,
              "(f).0 seeding holds the stock off the shelves until it is placed");
        place_opening_stock(s.w);
        check(near(s.w.markets.at(s.market_a).inventory[r_iron], 10.0f) &&
              near(s.w.markets.at(s.market_b).inventory[r_iron], 20.0f),
              "(f).1 placed by building count: A (1 work) 10, B (2 works) 20 — the whole 30");
        check(s.w.gen_opening_stock.empty(), "(f).2 the held stock is emptied once placed");
        place_opening_stock(s.w);
        check(near(s.w.markets.at(s.market_a).inventory[r_iron], 10.0f) &&
              near(s.w.markets.at(s.market_b).inventory[r_iron], 20.0f),
              "(f).3 placing again is a no-op (idempotent)");
    }

    // -----------------------------------------------------------------------
    // (h) a completing contract: the supplier buys off its HOME shelf on the
    //     body and builds the rest to order; the buyer's delivery LANDS
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario();
        recipe_registry reg;
        const entity_id buyer = s.w.create_entity();
        s.w.corporations[buyer] = corporation_component{};
        s.w.corporations.at(buyer).balance = 1.0e6f;
        s.w.markets.at(s.market_a).inventory[r_iron] = 10.0f; // the supplier's home shelf (HQ in A)
        s.w.markets.at(s.market_b).inventory[r_iron] = 50.0f; // another shelf on the body

        procurement_contract c;
        c.id              = 1;
        c.buyer           = buyer;
        c.supplier        = s.corp;
        c.body            = s.body;
        c.resource        = resource_type::iron_ore;
        c.quantity        = 40.0f;
        c.lead_time_ticks = 1;
        s.w.procurement_contracts.push_back(c);

        run_economy_step(s.w, reg);
        check(s.w.procurement_contracts.empty(), "(h).1 the contract completed");
        check(near(s.w.markets.at(s.market_a).inventory[r_iron], 0.0f) &&
              near(s.w.markets.at(s.market_b).inventory[r_iron], 50.0f),
              "(h).2 the supplier bought its HOME shelf's 10 (A 0) and touched no other shelf (B 50)");
        check(near(s.w.landed(buyer, corp_home_market(s.w, buyer, s.body), r_iron), 40.0f),
              "(h).3 the buyer's whole 40 LANDS on its home market (the rest built to order)");
    }

    std::printf("\n%s  (%d passed, %d failed)\n", g_fail == 0 ? "ALL PASS" : "FAILURES",
                g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
