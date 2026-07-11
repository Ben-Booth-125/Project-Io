// Headless harness for the planetary-logistics economic core (BL-077).
// Verifies the terrain-weighted intra-body A* pathfinder (world/logistics.*):
// terrain weighting, road discount, cylinder wrap, ocean-crossing mode selection,
// and route-cache idempotence — over hand-built tile grids, no SDL / Lua / ImGui.
//
// Registered automatically as the `logistics_harness` CTest test (BL-104 glob).
// Run: ctest --test-dir build -R logistics_harness   (or the built exe directly)

#include "world/components.hpp"
#include "world/construction.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/supply_system.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else { std::printf("  FAIL  %s\n", what); ++g_failures; }
}

static bool approx(float a, float b) { return std::fabs(a - b) < 1e-4f; }

// Build a body with a gw x gh grid of uniform tiles; return the body id.
static entity_id make_grid(world& w, int gw, int gh,
                           terrain_landform lf = terrain_landform::plains,
                           terrain_composition comp = terrain_composition::grassland,
                           std::uint8_t road = 0)
{
    const entity_id body = w.create_entity();
    body_component bc{};
    bc.name = "Test"; bc.type = body_type::planet;
    bc.grid_width = gw; bc.grid_height = gh;
    w.bodies[body] = bc;
    for (int r = 0; r < gh; ++r)
        for (int c = 0; c < gw; ++c)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body = body; tc.grid_x = c; tc.grid_y = r;
            tc.composition = comp; tc.landform = lf; tc.road_level = road;
            w.tiles[t] = tc;
        }
    return body;
}

static entity_id tile_at(world& w, entity_id body, int c, int r)
{
    const auto bit = w.bodies.find(body);
    const int gw = bit->second.grid_width;
    return body_tile_grid(w, body)[static_cast<std::size_t>(r) * static_cast<std::size_t>(gw)
                                   + static_cast<std::size_t>(c)];
}

int main()
{
    std::printf("=== logistics_harness (BL-077 planetary logistics core) ===\n");

    // T1 — cylinder wrap: on a 6-wide single-row grid, col0->col5 is one edge the
    // short way (wrap), not five the long way; col0->col3 is three either way.
    {
        world w; const entity_id b = make_grid(w, 6, 1);
        const logistics_path p05 = intra_body_path(w, b, tile_at(w,b,0,0), tile_at(w,b,5,0));
        const logistics_path p03 = intra_body_path(w, b, tile_at(w,b,0,0), tile_at(w,b,3,0));
        check(p05.reachable && approx(p05.cost, 1.0f), "wrap: col0->col5 costs one edge (1.0), not five");
        check(approx(p03.cost, 3.0f), "col0->col3 costs three edges (3.0) either way");
        check(!p05.crosses_ocean, "all-land wrap path is land mode (no ocean)");
    }

    // T2 — terrain weighting: a straight column of mountains costs 2x the same column
    // of plains (gw=1 removes any wrap shortcut).
    {
        world wp; const entity_id bp = make_grid(wp, 1, 5, terrain_landform::plains);
        world wm; const entity_id bm = make_grid(wm, 1, 5, terrain_landform::mountain);
        const float cp = intra_body_path(wp, bp, tile_at(wp,bp,0,0), tile_at(wp,bp,0,4)).cost;
        const float cm = intra_body_path(wm, bm, tile_at(wm,bm,0,0), tile_at(wm,bm,0,4)).cost;
        check(approx(cp, 4.0f), "plains column (4 edges) costs 4.0");
        check(approx(cm, 8.0f), "mountain column (4 edges) costs 8.0");
        check(cm > cp, "mountains cost more than plains over the same path");
    }

    // T3 — road discount: road_level lowers the traversal cost of the crossed tiles.
    {
        world w0; const entity_id b0 = make_grid(w0, 1, 5, terrain_landform::plains,
                                                 terrain_composition::grassland, /*road=*/0);
        world w2; const entity_id b2 = make_grid(w2, 1, 5, terrain_landform::plains,
                                                 terrain_composition::grassland, /*road=*/2);
        const float c0 = intra_body_path(w0, b0, tile_at(w0,b0,0,0), tile_at(w0,b0,0,4)).cost;
        const float c2 = intra_body_path(w2, b2, tile_at(w2,b2,0,0), tile_at(w2,b2,0,4)).cost;
        check(c2 < c0, "a roaded corridor costs less than an unroaded one");
        check(approx(c2, 2.0f), "road_level 2 halves the plains column cost (4.0 -> 2.0)");
    }

    // T4 — route-cache idempotence: the same query returns an identical cost.
    {
        world w; const entity_id b = make_grid(w, 4, 4, terrain_landform::highland);
        const entity_id s = tile_at(w,b,0,0), d = tile_at(w,b,3,3);
        const float first  = intra_body_path(w, b, s, d).cost;
        const float second = intra_body_path(w, b, s, d).cost;
        const float rev    = intra_body_path(w, b, d, s).cost; // symmetric
        check(approx(first, second), "re-querying the same lane returns an identical cost (cache)");
        check(approx(first, rev), "the path cost is symmetric in its endpoints");
    }

    // T5 — mode selection: a path forced through ocean reports crosses_ocean (=> sea);
    // an all-land path does not (=> land).
    {
        world wl; const entity_id bl = make_grid(wl, 1, 3, terrain_landform::plains);
        const logistics_path land = intra_body_path(wl, bl, tile_at(wl,bl,0,0), tile_at(wl,bl,0,2));
        check(land.reachable && !land.crosses_ocean, "all-land column -> land mode");

        world ws; const entity_id bs = make_grid(ws, 1, 3, terrain_landform::plains);
        // Make the middle tile ocean, so the only col-0 path must cross it.
        const entity_id mid = tile_at(ws, bs, 0, 1);
        ws.tiles[mid].composition = terrain_composition::ocean;
        const logistics_path sea = intra_body_path(ws, bs, tile_at(ws,bs,0,0), tile_at(ws,bs,0,2));
        check(sea.reachable && sea.crosses_ocean, "a path forced through ocean -> sea mode");
        check(approx(sea.cost, 3.5f), "ocean sea-leg raises the crossing cost (0.5*(1+2.5)*2 = 3.5)");
    }

    // T6 — data model: road_level defaults to 0; land_use has the infrastructure value.
    {
        tile_component tc{};
        check(tc.road_level == 0, "tile_component.road_level defaults to 0");
        const land_use_component::type infra = land_use_component::type::infrastructure;
        check(infra == land_use_component::type::infrastructure, "land_use has an infrastructure value");
    }

    // T7 — intra-body dispatch: a short market is served from the corp's on-body pool via a
    // land convoy whose cost uses the A* tile distance (not the space lane).
    {
        world w;
        const entity_id corp = w.create_entity();
        corporation_component cc{};
        cc.is_player = true;
        cc.balance   = 1000.0f;

        const entity_id body = make_grid(w, 1, 3, terrain_landform::plains); // 3-tall plains column
        const entity_id anchor = tile_at(w, body, 0, 0); // corp production anchor
        const entity_id bld = w.create_entity();
        building_component bc{};
        bc.tile = anchor;
        w.buildings[bld] = bc;
        cc.assets.push_back(bld);
        w.corporations[corp] = cc;
        w.player_entity = corp;

        constexpr std::size_t ri = 0;
        w.pool_for(corp, body).quantities[ri] = 100.0f; // on-body stockpile surplus

        const entity_id short_market = w.create_entity();
        market_component mm{};
        mm.body        = body;
        mm.centre_tile = tile_at(w, body, 0, 2); // far end of the column
        mm.demand[ri]  = 10.0f;
        mm.supply[ri]  = 0.0f;
        w.markets[short_market] = mm;

        recipe_registry reg; // default per-mode logistics costs {land .02, sea .05, air .15, space 1}
        const std::size_t convoys_before = w.convoys.size();
        const float bal_before = w.corporations[corp].balance;
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space));

        check(w.convoys.size() == convoys_before + 1, "intra-body shortfall dispatches one convoy");
        if (w.convoys.size() == convoys_before + 1)
        {
            const convoy_component& cv = w.convoys.back();
            check(cv.mode == convoy_mode::land, "intra-body plains route uses land mode, not space");
            check(cv.corp == corp, "convoy attributed to the dispatching corp");
        }
        const float spent = bal_before - w.corporations[corp].balance;
        check(spent > 0.0f, "intra-body logistics cost is debited from the corp balance");
        // A* (0,0)->(0,2) = 2 plains edges = 2.0; qty = min(100,10) = 10; land unit cost 0.02.
        check(approx(spent, 0.02f * 2.0f * 10.0f), "intra-body cost = land(0.02) * A*(2.0) * qty(10) = 0.4");
    }

    // Shared T7-shape setup for the BL-148/149 node-discount tests: a 3-tall plains column,
    // a corp with an anchor at (0,0) + a pool surplus, a short market at (0,2). The path
    // (0,0)->(0,2) crosses tile (0,1). Undiscounted cost is 0.02*2.0*10 = 0.4; a logistics
    // node on the path discounts it. Returns the credits spent on the single dispatched convoy.
    auto dispatch_spend = [](world& w, entity_id& out_corp, entity_id& out_mid_tile) -> float {
        const entity_id corp = w.create_entity();
        corporation_component cc{}; cc.is_player = true; cc.balance = 1000.0f;
        const entity_id body = make_grid(w, 1, 3, terrain_landform::plains);
        const entity_id anchor = tile_at(w, body, 0, 0);
        const entity_id bld = w.create_entity();
        building_component bc{}; bc.tile = anchor;
        w.buildings[bld] = bc;
        cc.assets.push_back(bld);
        w.corporations[corp] = cc;
        w.player_entity = corp;
        w.pool_for(corp, body).quantities[0] = 100.0f;
        const entity_id short_market = w.create_entity();
        market_component mm{}; mm.body = body; mm.centre_tile = tile_at(w, body, 0, 2);
        mm.demand[0] = 10.0f; mm.supply[0] = 0.0f;
        w.markets[short_market] = mm;
        out_corp     = corp;
        out_mid_tile = tile_at(w, body, 0, 1); // the tile the path crosses
        return w.corporations[corp].balance;
    };

    // T8 — BL-148 city discount: a population centre (scale 3) on the crossed tile discounts
    // the haul by city_per_scale(0.04) * 3 = 0.12, so cost 0.4 -> 0.352.
    {
        world w; entity_id corp, mid;
        const float bal_before = dispatch_spend(w, corp, mid);
        const entity_id centre = w.create_entity();
        population_centre_component pc{}; pc.scale = 3;
        w.population_centres[centre]    = pc;
        w.population_centre_tile[centre] = mid;
        recipe_registry reg; // default node discount: city_per_scale 0.04, hub 0.12, cap 0.50
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space));
        const float spent = bal_before - w.corporations[corp].balance;
        check(spent < 0.4f, "a route through a city costs less than the undiscounted 0.4");
        check(approx(spent, 0.4f * (1.0f - 0.12f)), "city scale-3 discount: 0.4 * (1 - 0.12) = 0.352");
    }

    // T9 — BL-149 hub discount: a completed Inland Logistics Hub on the crossed tile discounts
    // the haul by the flat hub rate (0.12), reusing the same node-scan, so cost 0.4 -> 0.352.
    {
        world w; entity_id corp, mid;
        const float bal_before = dispatch_spend(w, corp, mid);
        const entity_id hub = w.create_entity();
        building_component hb{};
        hb.tile = mid; hb.type = building_type::inland_logistics_hub; hb.ticks_remaining = 0;
        w.buildings[hub] = hb;
        recipe_registry reg;
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space));
        const float spent = bal_before - w.corporations[corp].balance;
        check(spent < 0.4f, "a route through an inland logistics hub costs less than 0.4");
        check(approx(spent, 0.4f * (1.0f - 0.12f)), "hub flat discount: 0.4 * (1 - 0.12) = 0.352");
    }

    // T9b — a DECOMMISSIONED hub confers NO discount: an inert hub is treated like the
    // production loop treats it (economy_system.cpp), so the haul pays the full 0.4.
    {
        world w; entity_id corp, mid;
        const float bal_before = dispatch_spend(w, corp, mid);
        const entity_id hub = w.create_entity();
        building_component hb{};
        hb.tile = mid; hb.type = building_type::inland_logistics_hub;
        hb.ticks_remaining = 0; hb.decommissioned = true;
        w.buildings[hub] = hb;
        recipe_registry reg;
        dispatch_convoys(w, reg, reg.logistics_cost(convoy_mode::land),
                         reg.logistics_cost(convoy_mode::space));
        const float spent = bal_before - w.corporations[corp].balance;
        check(approx(spent, 0.4f), "a decommissioned hub confers no discount (full 0.4 cost)");
    }

    // T10 — BL-147/BL-172 road placement: place_road(tier) on a road-free land tile raises
    // road_level to that tier, debits the tier's flat cost (no market -> no materials), and
    // invalidates the A* cache. Upgrade-in-place: a higher tier on the same tile succeeds; the
    // same or a lower tier is rejected (already at or above).
    {
        world w;
        const entity_id corp = w.create_entity();
        corporation_component cc{}; cc.balance = 1000.0f;
        w.corporations[corp] = cc;
        const entity_id body = make_grid(w, 3, 1, terrain_landform::plains);
        const entity_id t = tile_at(w, body, 1, 0);

        recipe_registry reg; // default road_econ (no Lua): Track 25 / Road 45 / Highway 90 cr
        (void)intra_body_path(w, body, tile_at(w,body,0,0), tile_at(w,body,2,0)); // warm the cache
        const bool cache_warm = !w.astar_cost_cache.empty();
        float bal = w.corporations[corp].balance;

        const construction_result r = place_road(w, reg, corp, t, 1); // Track
        check(r == construction_result::placed, "place_road (Track) on a land tile succeeds");
        check(w.tiles[t].road_level == 1, "placed Track raises the tile road_level to 1");
        check(approx(bal - w.corporations[corp].balance, 25.0f),
              "Track debits its flat build_cost (25 cr, no market materials)");
        check(cache_warm && w.astar_cost_cache.empty(),
              "road placement invalidates the A* cost cache");

        // Same tier again -> rejected (already at or above this tier).
        const construction_result r_same = place_road(w, reg, corp, t, 1);
        check(r_same == construction_result::invalid_tile,
              "re-laying the same tier on a road tile is rejected (already at or above)");

        // Upgrade Track -> Highway succeeds, raises the tier, and debits the Highway cost.
        (void)intra_body_path(w, body, tile_at(w,body,0,0), tile_at(w,body,2,0)); // re-warm
        bal = w.corporations[corp].balance;
        const construction_result r_up = place_road(w, reg, corp, t, 3); // Highway
        check(r_up == construction_result::placed, "upgrading Track -> Highway succeeds");
        check(w.tiles[t].road_level == 3, "upgrade raises the tile road_level to 3 (Highway)");
        check(approx(bal - w.corporations[corp].balance, 90.0f),
              "Highway debits its flat build_cost (90 cr)");
        check(w.astar_cost_cache.empty(), "the upgrade invalidates the A* cost cache");

        // A lower tier on the Highway tile -> rejected.
        const construction_result r_down = place_road(w, reg, corp, t, 2); // Road < Highway
        check(r_down == construction_result::invalid_tile,
              "a lower tier on an existing higher road is rejected");
    }

    std::printf("\n%s  (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
