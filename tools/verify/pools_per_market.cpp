// Headless pools-per-market harness (BL-1003; no SDL / Lua / ImGui).
//
// PRODUCTION.md § Stockpile and output flow: the goods pool is keyed
// (corporation, market), not (corporation, body). A building's output enters
// the pool of the market whose catchment holds its tile; its inputs draw that
// pool and that market's inventory; its want and fill book in that market. A
// body with no market keeps ONE body-level pool, which the first market to
// spawn there absorbs whole.
//
// The defect this item fixes, measured on the shipped home body (255 markets):
// a build drew from its tile's market while its want, fills, pools and
// auto-surplus booked at the corp's representative market (lowest-id
// building's tile), and a same-body convoy landed back in the seller's own
// pool. Every row below uses ONE body with TWO markets and a corp whose
// lowest-id building sits in market A's catchment, so a regression to the
// representative-market routing books at A where the row expects B.
//
//   (a) OUTPUT: a processor in B's catchment credits (corp, B), not (corp, A);
//       its fill books under (corp, B).
//   (b) WANT: a construction site in B's catchment registers its want under
//       (corp, B), and clearing puts that demand on market B, none on A.
//   (b') DRAW: the site draws the (corp, B) pool first, then B's shelf, and
//       bids and pays only for the shelf's part; stock in A is not reachable.
//   (c) CONVOY: a haul from (corp, A) to market B debits A's pool, credits
//       B's pool on arrival, and the cargo sells AT B, at B's price.
//   (d) SPAWN: a body with no market keeps one body-level pool; the market
//       that spawns there absorbs it whole.
//   (e) SAVE: a flat-binary round trip preserves every (corp, market) and
//       body-level pool key and quantity, and the state hash.
//   (f) REHOME: a stray body-level pool on a body that HAS markets moves to
//       the corp's home (HQ tile) market pool — world build's seeding order.
//   (g) SELL ORDER: a standing sell order on a body lists from each of the
//       corp's market pools there, ascending market id, each into its own
//       market, capped at the order's quantity.
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
#include "world/world_save.hpp"

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

bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) < eps; }
std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

constexpr std::size_t r_iron  = static_cast<std::size_t>(resource_type::iron_ore);
constexpr std::size_t r_steel = static_cast<std::size_t>(resource_type::steel);

float pool_q(const world& w, entity_id corp, entity_id key, std::size_t r)
{
    const stockpile_component* p = w.find_pool(corp, key);
    return p ? p->quantities[r] : 0.0f;
}

// ---------------------------------------------------------------------------
// Fixture: one body, a 32x4 plains grid (convoy_command.cpp's idiom), market A
// centred at (0,0) and market B centred at (0,3). The corp's FIRST building —
// its lowest id, the retired representative-market anchor — sits at (0,0), in
// A's catchment. Everything a row adds in B's catchment goes at (0,3).
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

    // A supply anchor off the haul's column, so a convoy clears the passive-LP
    // gate without earning a node discount (convoy_command.cpp's reasoning).
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

} // namespace

int main()
{
    std::printf("=== pools_per_market (BL-1003: goods pools key (corp, market)) ===\n");

    // -----------------------------------------------------------------------
    // Key resolution — the fixture is what every row leans on.
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario();
        check(pool_key_for_tile(s.w, s.tile_a) == s.market_a &&
              pool_key_for_tile(s.w, s.tile_b) == s.market_b,
              "K.1 fixture: (0,0) resolves to market A and (0,3) to market B");
        check(pool_key_body(s.w, s.market_b) == s.body && pool_key_body(s.w, s.body) == s.body,
              "K.2 a market key and a body key both resolve to their body");
    }

    // -----------------------------------------------------------------------
    // (a) a building's output lands in its TILE market's pool
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
        check(near(pool_q(s.w, s.corp, s.market_b, r_steel), out),
              "(a).2 its whole output entered the (corp, market B) pool");
        check(pool_q(s.w, s.corp, s.market_a, r_steel) == 0.0f,
              "(a).3 nothing entered (corp, market A) — the representative market");
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
        recipe_registry reg;
        building_economics pr;
        pr.build_cost = 0.0f;
        pr.build_duration_ticks = 1.0f;
        pr.resource_build_cost[r_steel] = 10.0f;
        reg.set_economics(building_type::processing_facility, pr);

        const entity_id site = s.w.create_entity();
        building_component b{};
        b.tile = s.tile_b;
        b.type = building_type::processing_facility;
        b.ticks_remaining = 1;
        s.w.buildings[site] = b;
        s.w.corporations.at(s.corp).assets.push_back(site);

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

    // (b') the site draws its owner's pool AT ITS MARKET first, then the shelf,
    // and bids and pays only for what comes off the shelf.
    {
        scenario s = make_scenario();
        recipe_registry reg;
        building_economics pr;
        pr.build_cost = 0.0f;
        pr.build_duration_ticks = 1.0f;
        pr.resource_build_cost[r_steel] = 10.0f;
        reg.set_economics(building_type::processing_facility, pr);

        s.w.pool_at(s.corp, s.market_b).quantities[r_steel] = 6.0f;  // own stock at B
        s.w.pool_at(s.corp, s.market_a).quantities[r_steel] = 50.0f; // elsewhere: not reachable
        s.w.markets.at(s.market_b).inventory[r_steel] = 10.0f;

        const entity_id site = s.w.create_entity();
        building_component b{};
        b.tile = s.tile_b;
        b.type = building_type::processing_facility;
        b.ticks_remaining = 1;
        s.w.buildings[site] = b;
        s.w.corporations.at(s.corp).assets.push_back(site);

        const economy_report rep = run_economy_step(s.w, reg);
        const auto wit = rep.wants.find({s.corp, s.market_b});
        const auto pit = rep.purchases.find({s.corp, s.market_b});
        check(s.w.buildings.at(site).ticks_remaining == 0,
              "(b').1 pool 6 + shelf 10 cover the need of 10: the build completes at full rate");
        check(near(pool_q(s.w, s.corp, s.market_b, r_steel), 0.0f) &&
              near(s.w.markets.at(s.market_b).inventory[r_steel], 6.0f),
              "(b').2 it drew the (corp, B) pool first (6), then 4 off B's shelf");
        check(wit != rep.wants.end() && near(wit->second[r_steel], 4.0f) &&
              pit != rep.purchases.end() && near(pit->second[r_steel], 4.0f),
              "(b').3 want and fill are the shelf's 4 only — own stock is not bid for or billed");
        check(near(pool_q(s.w, s.corp, s.market_a, r_steel), 50.0f),
              "(b').4 the corp's stock in market A is untouched: goods do not teleport within a body");
    }

    // -----------------------------------------------------------------------
    // (c) a convoy A -> B credits B's pool, and the goods sell at B's price
    // -----------------------------------------------------------------------
    world after_convoy; // kept for (e)
    {
        scenario s = make_scenario();
        recipe_registry reg;
        military_capability_params mp = reg.military();
        mp.active_lp_per_anchor_tick = 1.0e6f;
        reg.set_military(mp);

        s.w.pool_at(s.corp, s.market_a).quantities[r_iron] = 100.0f;

        corp_command cmd;
        cmd.corp         = s.corp;
        cmd.verb         = corp_verb::dispatch_convoy;
        cmd.subject      = s.market_a;
        cmd.counterparty = s.market_b;
        cmd.target       = resource_type::iron_ore;
        cmd.quantity     = 30.0f;
        check(apply_corp_command(s.w, reg, cmd) == corp_command_result::applied,
              "(c).1 fixture: the corp dispatches 30 iron from market A to market B");
        check(near(pool_q(s.w, s.corp, s.market_a, r_iron), 70.0f),
              "(c).2 dispatch debits the SOURCE market's pool (100 -> 70)");

        for (int i = 0; i < 50 && !s.w.convoys.empty(); ++i)
        {
            advance_convoys(s.w);
            credit_arrived_convoys(s.w, i);
        }
        check(s.w.convoys.empty(), "(c).3 the convoy arrives and is retired");
        check(near(pool_q(s.w, s.corp, s.market_b, r_iron), 30.0f) &&
              near(pool_q(s.w, s.corp, s.market_a, r_iron), 70.0f),
              "(c).4 arrival credits the DESTINATION market's pool (A 70, B 30) — a same-body "
              "haul no longer lands back in the pool it left");

        after_convoy = s.w; // (e) snapshots the two-pool state before anything sells

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
              pool_q(s.w, s.corp, s.market_b, r_iron) == 0.0f,
              "(c).7 B's inventory gains exactly the cargo and B's pool empties");
    }

    // -----------------------------------------------------------------------
    // (d) a body with no market keeps one pool; a spawned market absorbs it
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario();
        recipe_registry reg;

        const entity_id outpost = s.w.create_entity();
        body_component ob{};
        ob.name = "Cinder";
        ob.type = body_type::planet;
        ob.orbital_radius_au = 2.0f;
        ob.grid_width = 4;
        ob.grid_height = 1;
        s.w.bodies[outpost] = ob;
        entity_id first_tile = null_entity;
        for (int c = 0; c < 4; ++c)
        {
            const entity_id t = s.w.create_entity();
            tile_component tc{};
            tc.body = outpost;
            tc.grid_x = c;
            tc.grid_y = 0;
            s.w.tiles[t] = tc;
            if (c == 0)
                first_tile = t;
        }
        const entity_id rival = s.w.create_entity();
        s.w.corporations[rival] = corporation_component{};

        const entity_id key = pool_key_for_tile(s.w, first_tile);
        check(key == outpost, "(d).1 on a market-less body the pool key is the BODY");
        s.w.pool_at(s.corp, key).quantities[r_iron] = 40.0f;
        s.w.pool_at(rival, key).quantities[r_steel] = 7.0f;

        const entity_id mid = maybe_spawn_market(s.w, reg, outpost, first_tile);
        check(mid != null_entity, "(d).2 fixture: a market spawns on the outpost");
        check(s.w.find_pool(s.corp, outpost) == nullptr && s.w.find_pool(rival, outpost) == nullptr,
              "(d).3 no body-level pool survives the spawn");
        check(near(pool_q(s.w, s.corp, mid, r_iron), 40.0f) &&
              near(pool_q(s.w, rival, mid, r_steel), 7.0f),
              "(d).4 the new market's pool absorbed every corp's body-level pool whole");
        check(pool_key_for_tile(s.w, first_tile) == mid,
              "(d).5 the tile now resolves to the new market");
    }

    // -----------------------------------------------------------------------
    // (e) save round-trip preserves the pools
    // -----------------------------------------------------------------------
    {
        world& w = after_convoy;
        // Add a body-level pool on a market-less body so both key kinds travel.
        const entity_id bare = w.create_entity();
        body_component bb{};
        bb.name = "Bare";
        bb.type = body_type::planet;
        w.bodies[bare] = bb;
        w.pool_at(w.player_entity, bare).quantities[r_steel] = 3.5f;

        std::stringstream buf(std::ios::in | std::ios::out | std::ios::binary);
        write_world_snapshot(w, buf);
        world loaded;
        const bool ok = read_world_snapshot(loaded, buf);
        check(ok, "(e).1 the snapshot reads back whole");

        bool same = ok && loaded.corp_market_pools.size() == w.corp_market_pools.size();
        if (same)
            for (const auto& [k, p] : w.corp_market_pools)
            {
                const auto it = loaded.corp_market_pools.find(k);
                if (it == loaded.corp_market_pools.end() || it->second.quantities != p.quantities)
                {
                    same = false;
                    break;
                }
            }
        check(same, "(e).2 every (corp, key) pool and every quantity survives the round trip");
        check(ok && loaded.find_pool(loaded.player_entity, bare) != nullptr &&
              w.markets.find(bare) == w.markets.end(),
              "(e).3 the body-level key survives as a body-level key");
        check(ok && loaded.state_hash(0) == w.state_hash(0),
              "(e).4 the state hash (which folds the pools) is identical after load");
    }

    // -----------------------------------------------------------------------
    // (f) rehome: a stray body-level pool on a body with markets moves home
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario();
        // Move the corp's HQ into B's catchment; its lowest-id building stays in A.
        const entity_id hq = s.w.create_entity();
        building_component b{};
        b.tile = s.tile_b;
        b.type = building_type::extraction_site;
        s.w.buildings[hq] = b;
        s.w.corporations.at(s.corp).assets.push_back(hq);
        s.w.corporations.at(s.corp).hq_building = hq;

        s.w.pool_at(s.corp, s.body).quantities[r_iron] = 25.0f; // world build's seeding order
        rehome_body_pools(s.w);
        check(s.w.find_pool(s.corp, s.body) == nullptr,
              "(f).1 no body-level pool is left on a body that has markets");
        check(near(pool_q(s.w, s.corp, s.market_b, r_iron), 25.0f) &&
              pool_q(s.w, s.corp, s.market_a, r_iron) == 0.0f,
              "(f).2 it moved to the HQ tile's market (B), not the lowest-id building's (A)");
    }

    // -----------------------------------------------------------------------
    // (g) a standing sell order on a body lists from each market pool there
    // -----------------------------------------------------------------------
    {
        scenario s = make_scenario();
        recipe_registry reg;
        s.w.pool_at(s.corp, s.market_a).quantities[r_steel] = 10.0f;
        s.w.pool_at(s.corp, s.market_b).quantities[r_steel] = 20.0f;
        sell_order o;
        o.id          = s.w.allocate_order_id();
        o.corp        = s.corp;
        o.body        = s.body;
        o.resource    = resource_type::steel;
        o.quantity    = 25.0f;
        o.floor_price = 0.0f;
        s.w.sell_orders.push_back(o);

        clear_markets(s.w, reg, economy_report{});
        float sold_a = 0.0f, sold_b = 0.0f;
        for (const exchange_record& e : s.w.exchanges.entries)
            if (e.seller == s.corp && e.resource == resource_type::steel)
            {
                if (e.market == s.market_a) sold_a += e.quantity;
                if (e.market == s.market_b) sold_b += e.quantity;
            }
        check(near(sold_a, 10.0f) && near(sold_b, 15.0f),
              "(g).1 the 25-unit order sold A's 10 at A and 15 of B's 20 at B (ascending market id)");
        check(near(pool_q(s.w, s.corp, s.market_a, r_steel), 0.0f) &&
              near(pool_q(s.w, s.corp, s.market_b, r_steel), 5.0f),
              "(g).2 each pool was debited only what sold from it (A 0, B 5)");
    }

    std::printf("\n%s  (%d passed, %d failed)\n", g_fail == 0 ? "ALL PASS" : "FAILURES",
                g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
