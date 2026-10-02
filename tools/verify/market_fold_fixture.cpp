// market_fold_fixture — BL-1125 (markets can die): a folded market's catchment
// passes to its ABSORBER, not to whichever standing market is nearest.
//
// THE ONE HONEST ROW the census cannot give. On a generated world every reading
// of "the catchment passed" goes through market_for_tile itself; here the
// answer is known by construction, on a hand-laid strip:
//
//   one row, 40 columns (the column wraps), three markets, ascending id:
//     A at col 0,  B at col 10,  C at col 15.
//   Before the fold, by nearest centre (ties -> lowest id):
//     A holds cols 28..39 and 0..5   (col 5 ties A/B -> A; A/C meet at 27.5)
//     B holds cols 6..12             (col 12: B 2, C 3)
//     C holds cols 13..27
//   B folds into A. Then B's tiles 6..12 must route to A -- col 12 included,
//   though C (3 away) is far nearer than A (12 away). A tile that went to C
//   there would be the bug this item's review round fixed: the catchment going
//   to a nearer third market instead of passing whole.
//
// Also checked: every other tile routes where it did; the catchment raster
// (market_for_tile) and the full scan (market_for_tile_scan) agree on every
// tile; B's inventory and a pool keyed by B land in A; and a world snapshot
// round trip keeps every route.
//
// Build:  node tools/verify/build_harness.js market_fold_fixture
// Run:    ./build_gen/verify/market_fold_fixture.exe

#include "world/components.hpp"
#include "world/market_clearing.hpp"
#include "world/market_fold.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <cstdio>
#include <sstream>
#include <string>
#include <vector>

namespace {

int g_fail = 0;

void check(bool ok, const char* id, const std::string& what)
{
    std::printf("%s  %s  %s\n", ok ? "PASS" : "FAIL", id, what.c_str());
    if (!ok) ++g_fail;
}

constexpr int k_gw = 40;

struct strip
{
    world     w;
    entity_id body = null_entity;
    std::vector<entity_id> tile; // by column
    entity_id a = null_entity, b = null_entity, c = null_entity;
};

entity_id add_market(strip& s, int col)
{
    const entity_id id = s.w.create_entity();
    market_component m{};
    m.body        = s.body;
    m.centre_tile = s.tile[static_cast<std::size_t>(col)];
    s.w.markets[id] = m;
    return id;
}

strip make_strip()
{
    strip s;
    s.body = s.w.create_entity();
    body_component bc{};
    bc.name        = "Strip";
    bc.type        = body_type::planet;
    bc.grid_width  = k_gw;
    bc.grid_height = 1;
    s.w.bodies[s.body] = bc;
    s.w.home_body      = s.body;
    for (int c = 0; c < k_gw; ++c)
    {
        const entity_id t = s.w.create_entity();
        tile_component tc{};
        tc.body      = s.body;
        tc.grid_x    = c;
        tc.grid_y    = 0;
        tc.substrate = terrain_substrate::sedimentary;
        tc.landform  = terrain_landform::plains;
        s.w.tiles[t] = tc;
        s.tile.push_back(t);
    }
    s.a = add_market(s, 0);
    s.b = add_market(s, 10);
    s.c = add_market(s, 15);
    return s;
}

std::vector<entity_id> routes(const world& w, const strip& s)
{
    std::vector<entity_id> r;
    for (const entity_id t : s.tile) r.push_back(market_for_tile(w, t));
    return r;
}

std::string name_of(const strip& s, entity_id m)
{
    return m == s.a ? "A" : m == s.b ? "B" : m == s.c ? "C" : "?";
}

} // namespace

int main()
{
    std::printf("market_fold_fixture — BL-1125: a folded market's catchment passes to its absorber\n");
    strip s = make_strip();

    // -- before the fold: the known partition --
    const std::vector<entity_id> before = routes(s.w, s);
    {
        bool ok = true;
        for (int c = 0; c < k_gw; ++c)
        {
            const entity_id want = (c <= 5 || c >= 28) ? s.a : (c <= 12) ? s.b : s.c;
            if (before[static_cast<std::size_t>(c)] != want) ok = false;
        }
        check(ok, "F0", "before the fold: A holds 28..39 and 0..5, B 6..12, C 13..27 (column wrapped)");
    }

    // -- stock on B, to watch it move --
    const entity_id corp = s.w.create_entity();
    s.w.markets.at(s.b).inventory[0] = 7.0f;
    s.w.pool_at(corp, s.b).quantities[0] = 3.0f;

    fold_market_into(s.w, s.b, s.a);

    const std::vector<entity_id> after = routes(s.w, s);
    {
        bool b_tiles_to_a = true;
        for (int c = 6; c <= 12; ++c)
            if (after[static_cast<std::size_t>(c)] != s.a) b_tiles_to_a = false;
        check(b_tiles_to_a, "F1", "every tile B held (cols 6..12) routes to its absorber A");
        check(after[12] == s.a,
              "F2", "col 12 routes to A (12 away), not to C (3 away): the catchment does not leak to a "
                    "nearer third market (got " + name_of(s, after[12]) + ")");
        bool rest_unchanged = true;
        for (int c = 0; c < k_gw; ++c)
            if ((c < 6 || c > 12) && after[static_cast<std::size_t>(c)] != before[static_cast<std::size_t>(c)])
                rest_unchanged = false;
        check(rest_unchanged, "F3", "every tile B did not hold routes exactly where it did before");
    }
    {
        bool same = true;
        for (const entity_id t : s.tile)
            if (market_for_tile(s.w, t) != market_for_tile_scan(s.w, t)) same = false;
        check(same, "F4", "the catchment raster and the full scan route every tile alike");
    }
    check(s.w.markets.count(s.b) == 0 && s.w.folded_markets.count(s.b) == 1
              && s.w.folded_markets.at(s.b).into == s.a,
          "F5", "B is gone from the markets and its fold record names A");
    {
        const stockpile_component* pa = s.w.find_pool(corp, s.a);
        check(s.w.markets.at(s.a).inventory[0] == 7.0f && pa != nullptr && pa->quantities[0] == 3.0f
                  && s.w.find_pool(corp, s.b) == nullptr,
              "F6", "B's inventory (7) and its pool (3) land in A; no pool is left keyed by B");
    }

    // -- a second fold re-points the first record: A into C --
    {
        strip t = make_strip();
        fold_market_into(t.w, t.b, t.a);
        fold_market_into(t.w, t.a, t.c);
        bool all_c = true;
        for (const entity_id tile : t.tile)
            if (market_for_tile(t.w, tile) != t.c) all_c = false;
        check(t.w.folded_markets.at(t.b).into == t.c && all_c,
              "F7", "folding the absorber re-points the earlier record: B's and A's tiles all route to C");
    }

    // -- the round trip keeps every route --
    {
        std::stringstream ss;
        write_world_snapshot(s.w, ss);
        world back;
        const bool ok = read_world_snapshot(back, ss);
        bool same = ok;
        for (std::size_t c = 0; same && c < s.tile.size(); ++c)
            same = market_for_tile(back, s.tile[c]) == after[c];
        check(same, "F8", "a snapshot round trip keeps the fold map and every route");
    }

    std::printf("\n%s (%d failure(s))\n", g_fail ? "FAIL" : "ALL PASS", g_fail);
    return g_fail ? 1 : 0;
}
