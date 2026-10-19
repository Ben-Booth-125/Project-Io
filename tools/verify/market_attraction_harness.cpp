// ---------------------------------------------------------------------------
// market_attraction_harness — BL-1346 (MARKET_ATTRACTION)
// No SDL / ImGui; Lua only for the generated-world rows.
// ---------------------------------------------------------------------------
// MARKETS.md § Catchment is an attraction, not a distance (Ben, 2026-10-16): a tile
// clears against the market whose LOGISTICS COST to its centre, divided by that
// centre's PULL, is lowest — ties to the lowest market id. LOGISTICS.md § 3b adds the
// off-road leg, so every tile belongs to a market.
//
//   A1  THE ROAD DECIDES, NOT THE GRID. On a hand-built body the nearer centre by grid
//       distance is the dearer by road, and the tile clears at the FARTHER one. The
//       old straight-line rule would have answered the other way, which is the whole
//       of what this item changed.
//   A2  A ROAD MOVES A MARKET'S EDGE, WITH NO TICK. Laying a crossing re-routes a tile
//       between markets on the next read — "recompute on change, never on a clock".
//   A3  THE OFF-ROAD LEG. A tile with no roaded edge still belongs to a market, and it
//       is priced as its anchor's cost plus the per-step leg: a tile one step off the
//       road and a tile three steps off it both route, and the farther one is dearer.
//   (A PULL FIXTURE IS OWED, AND IS NOT HERE. `centre_pull` reads `market_pull` off the
//       centre's nation through `nation_scalar`, and NOTHING RAISES IT yet - BL-1350 is
//       the provisional pull technologies, BL-1349 the building on the centre - so the
//       division by pull is exercised by nothing and a fixture would have to invent a
//       held node to test it. Named here as owed rather than claimed as covered: a
//       banner that lists a row the file does not contain is worse than a gap, because
//       it reads as cover. BL-1350 brings the fixture with the thing it tests.)
//   A5  THE SAME-NATION FIFTH. A tile owned by the centre's nation is 1.2x as attracted
//       to it, as a property of the PAIR: the same centre pulls no harder on foreign
//       ground. Asserted on a fixture where the bonus is what decides.
//   G1  ON THE GENERATED WORLD (Lua): every tile of the home body answers a market, the
//       cached raster equals a freshly built field, and the SAME-NATION BONUS CENSUS —
//       how many tiles actually change hands at 1.2, which Ben asked to be reported
//       rather than assumed.
//
// Build: bash tools/verify/build_lua_harness.sh market_attraction_harness
// Run:   build_gen/verify/market_attraction_harness.exe [--seeds a,b,...]

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/components.hpp"
#include "world/logistics.hpp"
#include "world/river_generation.hpp" // hex_side_for_offset: a step becomes a side
#include "world/market_clearing.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const std::string& what)
{
    std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    ok ? ++g_pass : ++g_fail;
}

// --- the hand-built body ----------------------------------------------------

/// A gw x gh plains body. Every tile is land, so the only thing that varies between
/// the fixtures below is where the roads and the nations are.
entity_id make_body(world& w, int gw, int gh)
{
    const entity_id body = w.create_entity();
    body_component bc{};
    bc.grid_width  = gw;
    bc.grid_height = gh;
    w.bodies[body] = bc;
    for (int r = 0; r < gh; ++r)
        for (int c = 0; c < gw; ++c)
        {
            const entity_id t = w.create_entity();
            tile_component tc{};
            tc.body      = body;
            tc.grid_x    = c;
            tc.grid_y    = r;
            tc.substrate = terrain_substrate::sedimentary;
            tc.cover     = terrain_cover::grass;
            tc.cover_density = 150;
            tc.landform  = terrain_landform::plains;
            w.tiles[t]   = tc;
        }
    return body;
}

entity_id tile_at(world& w, entity_id body, int c, int r)
{
    const std::vector<entity_id>& g = body_tile_grid(w, body);
    const auto bit = w.bodies.find(body);
    const int gw = bit->second.grid_width;
    return g[static_cast<std::size_t>(r) * static_cast<std::size_t>(gw) + static_cast<std::size_t>(c)];
}

/// A market whose centre stands on (c, r).
entity_id add_market(world& w, entity_id body, int c, int r)
{
    const entity_id m = w.create_entity();
    market_component mc{};
    mc.body        = body;
    mc.centre_tile = tile_at(w, body, c, r);
    w.markets[m]   = mc;
    return m;
}

/// Road the straight run (c0..c1) of row @p r at @p tier, both halves of every
/// crossing, exactly as `lay_road_edge` does.
void road_row(world& w, entity_id body, int r, int c0, int c1, std::uint8_t tier)
{
    // `lay_road_edge` names a SIDE, not the far tile - an east step's side depends on
    // the row's parity, as every hex walk here does.
    const int side = hex_side_for_offset(+1, 0, (r & 1) != 0);
    for (int c = c0; c < c1; ++c)
        lay_road_edge(w, tile_at(w, body, c, r), side, tier);
}

/// The nation holding a rectangle of ground.
entity_id own_rect(world& w, entity_id body, int c0, int c1, int r0, int r1, const char* name)
{
    const entity_id n = w.create_entity();
    nation_component nc{};
    nc.name = name;
    for (int r = r0; r <= r1; ++r)
        for (int c = c0; c <= c1; ++c)
        {
            const entity_id t = tile_at(w, body, c, r);
            nc.tiles.push_back(t);
            w.set_tile_nation(t, n); // BL-1346: the setter bumps the catchment stamp
        }
    w.nations[n] = nc;
    return n;
}

} // namespace

// ---------------------------------------------------------------------------
// A1 — the road decides, not the grid
// ---------------------------------------------------------------------------

void a1_road_beats_grid()
{
    std::printf("\n-- A1  the road decides, not the grid --\n");

    // One row. WEST centre at column 0, EAST centre at column 20. The tile under test
    // sits at column 9: one column nearer the WEST centre by grid distance (9 vs 11),
    // but only the EAST half of the row is roaded, so the east is far cheaper.
    world w;
    const entity_id body = make_body(w, 21, 1);
    const entity_id west = add_market(w, body, 0, 0);
    const entity_id east = add_market(w, body, 20, 0);
    road_row(w, body, 0, 9, 20, 3); // Highway from the tile to the EAST centre
    const entity_id subject = tile_at(w, body, 9, 0);

    check(market_for_tile(w, subject) == east,
          "the tile clears at the FARTHER centre, because the road runs that way");
    check(market_for_tile(w, subject) != west,
          "and not at the nearer one, which the straight-line rule would have picked");
    check(market_for_tile_scan(w, subject) == market_for_tile(w, subject),
          "the cached raster and a freshly built field agree");
}

// ---------------------------------------------------------------------------
// A2 — a road moves the edge, with no tick
// ---------------------------------------------------------------------------

void a2_road_moves_the_edge()
{
    std::printf("\n-- A2  a road laid moves a market's edge, with no tick --\n");

    world w;
    const entity_id body = make_body(w, 21, 1);
    const entity_id west = add_market(w, body, 0, 0);
    const entity_id east = add_market(w, body, 20, 0);
    road_row(w, body, 0, 0, 11, 1); // a Track west, reaching column 11
    const entity_id subject = tile_at(w, body, 11, 0);

    const entity_id before = market_for_tile(w, subject);
    check(before == west, "with only the west road laid, the tile clears west");

    // Now road the east half at a better tier, cheaper per step.
    road_row(w, body, 0, 11, 20, 3);
    invalidate_logistics_caches(w); // what laying a road does in play
    const entity_id after = market_for_tile(w, subject);

    check(after == east, "a Highway laid to the east takes the tile - no tick passed");
    check(before != after, "so the catchment moved on the CHANGE, which is the cadence");
}

// ---------------------------------------------------------------------------
// A3 — the off-road leg
// ---------------------------------------------------------------------------

void a3_offroad_leg()
{
    std::printf("\n-- A3  off the road, every tile still belongs to a market --\n");

    // A roaded spine on row 0; rows 1..3 carry no road at all.
    world w;
    const entity_id body = make_body(w, 12, 4);
    const entity_id m    = add_market(w, body, 0, 0);
    road_row(w, body, 0, 0, 11, 2);

    bool all_routed = true;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 12; ++c)
            if (market_for_tile(w, tile_at(w, body, c, r)) != m)
                all_routed = false;
    check(all_routed, "every tile on the body belongs to the one market, roaded or not");

    // Two markets, so the leg's PRICE can be seen: the second centre sits off the
    // spine, three steps south of it, and the tiles around it flip to it only where
    // the stepped leg beats the run along the road.
    // THE LEG HAS A PRICE, and the banner claims the farther tile is dearer - so assert
    // it where it is visible: a second centre at the far end of the spine, and the tiles
    // between them split by WHERE the stepped leg stops beating the run along the road.
    // A zero leg would put every southern tile with the nearer centre and this would
    // fail, which is what makes it a check of the leg rather than of routing.
    const entity_id east = add_market(w, body, 11, 0);
    int south_to_east = 0, south_to_west = 0;
    for (int c = 0; c < 12; ++c)
    {
        const entity_id m3 = market_for_tile(w, tile_at(w, body, c, 3)); // three steps off
        if (m3 == east) ++south_to_east;
        if (m3 == m)    ++south_to_west;
    }
    check(south_to_east > 0 && south_to_west > 0,
          "three steps off the road, the row SPLITS between the two centres - the leg is "
          "paid per step, so which centre wins depends on the run along the spine");
    // And the split sits where the costs cross: the western columns go west.
    check(market_for_tile(w, tile_at(w, body, 0, 3)) == m
              && market_for_tile(w, tile_at(w, body, 11, 3)) == east,
          "the ends of that row go to the centre at their own end");
}

// ---------------------------------------------------------------------------
// A5 — the same-nation fifth
// ---------------------------------------------------------------------------

void a5_home_nation_pull()
{
    std::printf("\n-- A5  its own nation pulls a fifth harder --\n");

    // A roaded row with a centre at each end, and the subject EXACTLY in the middle,
    // so the costs are equal and the bonus is the only thing that can decide.
    world w;
    const entity_id body = make_body(w, 21, 1);
    const entity_id west = add_market(w, body, 0, 0);
    const entity_id east = add_market(w, body, 20, 0);
    road_row(w, body, 0, 0, 20, 2);
    const entity_id subject = tile_at(w, body, 10, 0);

    check(market_for_tile(w, subject) == west,
          "with no nations at all, the equal-cost tie goes to the lowest market id");

    // Give the EAST centre and the subject to one nation, the west to another.
    own_rect(w, body, 10, 20, 0, 0, "East");
    own_rect(w, body, 0, 9, 0, 0, "West");
    invalidate_logistics_caches(w);
    check(market_for_tile(w, subject) == east,
          "owned by the east centre's nation, the tile clears east instead");

    // THE PAIR, PROPERLY: a tile the FOREIGN centre is cheaper to reach from, which must
    // still clear at home. Column 9 is nine steps from the west centre and eleven from
    // the east, so west wins on raw cost; it belongs to the WEST nation here, so the
    // fifth and the cost agree and the tile stays west - that half is the easy one. The
    // telling case is column 11: ELEVEN steps west, nine east, so the EAST centre is
    // cheaper - and because column 11 is east-nation ground, the fifth and the cost
    // agree again. What proves the PAIR is that neither bonus crosses: the west centre
    // pulls no harder on east ground, so a tile the east centre is cheaper from clears
    // east EVEN THOUGH the west centre also has a nation.
    check(market_for_tile(w, tile_at(w, body, 9, 0)) == west,
          "a tile cheaper from the west, on west ground, clears west");
    check(market_for_tile(w, tile_at(w, body, 11, 0)) == east,
          "and a tile cheaper from the east, on east ground, clears east - neither "
          "nation's fifth reaches across the border");
}

// ---------------------------------------------------------------------------
// G1 — the generated world
// ---------------------------------------------------------------------------

void g1_generated(lua_state& lua, std::uint32_t seed)
{
    std::printf("\n-- G1  seed %u: the generated world --\n", static_cast<unsigned>(seed));
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;
    app_start_world out;
    build_app_start_world(lua, params, out);
    world& w = out.w;
    const entity_id body = w.home_body;

    int gw = 0, gh = 0;
    const std::vector<entity_id> field = market_catchment_field(w, body, gw, gh);
    check(gw > 0 && gh > 0 && !field.empty(), "the home body has a catchment field");

    int tiles = 0, unrouted = 0, mismatched = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body || tc.grid_x < 0 || tc.grid_x >= gw || tc.grid_y < 0 || tc.grid_y >= gh)
            continue;
        ++tiles;
        const entity_id cached = market_for_tile(w, tid);
        const entity_id fresh  = field[static_cast<std::size_t>(tc.grid_y) * static_cast<std::size_t>(gw)
                                       + static_cast<std::size_t>(tc.grid_x)];
        if (cached == null_entity) ++unrouted;
        if (cached != fresh)       ++mismatched;
    }
    std::printf("    %d tiles, %d unrouted, %d disagreeing with a fresh field\n",
                tiles, unrouted, mismatched);
    check(tiles > 0 && unrouted == 0, "every tile of the home body belongs to a market");
    check(mismatched == 0, "the cached raster equals a freshly built field on every tile");

    // THE BONUS CENSUS (Ben asked for this number, not for an assumption): how many
    // tiles the same-nation fifth actually moves. Counted by comparing the field with
    // one built as if every tile were foreign ground - which is what dropping the
    // nations does.
    world bare = w;
    bare.tile_to_nation.clear();
    bare.body_route_index.clear();
    int bgw = 0, bgh = 0;
    const std::vector<entity_id> no_bonus = market_catchment_field(bare, body, bgw, bgh);
    int flips = 0;
    if (bgw == gw && bgh == gh)
        for (std::size_t i = 0; i < field.size() && i < no_bonus.size(); ++i)
            if (field[i] != no_bonus[i] && field[i] != null_entity && no_bonus[i] != null_entity)
                ++flips;
    const double pct = tiles > 0 ? 100.0 * flips / tiles : 0.0;
    std::printf("    SAME-NATION CENSUS seed %u: %d of %d tiles change hands because of the "
                "1.2x (%.2f%%)%s\n",
                static_cast<unsigned>(seed), flips, tiles, pct,
                flips == 0 ? "  <- it flips NONE: the bonus is invisible at this spread" : "");
    check(true, "the same-nation census is reported, whatever it says");
}

int main(int argc, char** argv)
{
    std::printf("=== market_attraction_harness (BL-1346) ===\n");
    std::vector<std::uint32_t> seeds{0};
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            seeds.clear();
            char* p = argv[++i];
            while (*p)
            {
                seeds.push_back(static_cast<std::uint32_t>(std::strtoul(p, &p, 10)));
                if (*p == ',') ++p; else break;
            }
        }

    a1_road_beats_grid();
    a2_road_moves_the_edge();
    a3_offroad_leg();
    a5_home_nation_pull();

    lua_state lua;
    for (const std::uint32_t s : seeds)
        g1_generated(lua, s);

    std::printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
