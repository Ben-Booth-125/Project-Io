// Headless harness for BL-146 (road-network generation). No SDL / Lua / ImGui.
// Builds the hard-coded world and asserts:
//   R1 presence   — road generation stamps road_level > 0 on some Kepler land
//                   tiles (the lattice exists), and never on ocean tiles.
//   R2 three-tier — the tier ladder obeys BL-618/BL-621 (percentile gates): the
//                   no_prehistory world's nations all tie at the qualification
//                   floor, so each grades 0.5 and the PRIMARY world carries Roads
//                   and no Highway; the full ladder — Road, and Highway across a
//                   seed sweep — is asserted on a QUALIFIED regeneration (a
//                   three-band qualification spread, the top third the most urban
//                   nations; road_level reset, generate_roads re-run). No tile
//                   ever carries a tier beyond Highway (ceiling). The Highway on
//                   that regen is a PRINTED CONTROL since BL-1159: a no_prehistory
//                   world runs no industrial urbanisation, so no nation holds two
//                   City+ centres and no link can qualify.
//   R2s shipped  — BL-1159: the Highway row asked of the SHIPPED worlds (the
//                   app's world build with the works table, per curated seed;
//                   needs a live Lua state and the repo root as cwd). The
//                   national lattice is re-laid with its trace, and every link
//                   the rule qualifies (two City+, percentile >= 0.80, computed
//                   here) must be Highway on the shipped field; the shipped
//                   worlds must carry one. `--seeds a,b,c` narrows the sweep.
//   R6 markets    — BL-1138 (roads pull toward markets), on the same shipped
//                   worlds: every market centre on its nation's backbone (asked
//                   independently), each trunk neighbour pair laid at Road+ or
//                   refused by the detour test, the neighbour set recomputed, and
//                   the before/after readings (road tiles by tier, trunk links,
//                   off-backbone markets, median market-to-market cost) off the
//                   pass's trace, which restores the field it found.
//   R7 bridges    — the bridge cap (Ben, 2026-10-03: "bridges can cross a further
//                   distance than I expected"; he ruled two). On the same shipped
//                   worlds, every route each writer of road_level LAID — the
//                   national tree/loop/spur/border (re-laid trace), the ancient
//                   corridors (stamp_history_roads' trace) and the market joins,
//                   pulls and trunk (lay_market_roads' trace) — has no contiguous
//                   water run longer than kMaxCrossingTiles, each offender named by
//                   its writer. Printed beside it: the raster's four-cardinal water
//                   runs bounded by roaded land at both ends (a control), the
//                   refusals the cap made, and the nations whose town network splits.
//   Q  differential — the same world regenerated at floor vs high qualification
//                   produces measurably different lattices (BL-618's contract):
//                   promoted tiers appear only on the qualified run, and the
//                   qualified lattice is at least as large (rationed loops).
//   R5 tree + floor — BL-1119 (roads tree and detour; LOGISTICS.md § 4), read off
//                   the pass's own write-only stats on a fresh world regenerated
//                   at spur floor 0 and at the shipped floor: every village is
//                   accounted for (below the floor, spurred, or failed); floor 0
//                   is the unfloored pass; the shipped floor leaves some villages
//                   on their street alone; the detour test refuses candidates and
//                   the ration never keeps more than it admitted; along the lay
//                   order, a nation whose predecessors laid the same routes at both
//                   floors has the same backbone (R5e - the rest are compared and
//                   reported, not asserted); every border endpoint and
//                   every on-network village reaches a same-nation town over laid
//                   road, rebuilt from the routes alone (R5f); and the stamp
//                   refuses no backbone link, sea routes being no candidates (R5g).
//   R3 connectivity — EVERY population centre sits on, or orthogonally adjacent
//                   to, a roaded tile — anchor foundings included (BL-623,
//                   provinces before roads: anchors exist when the lattice is
//                   laid, so the off-lattice class is gone and the old anchor
//                   exemption and its R3b companion are RETIRED). Street-only
//                   centres (own tile roaded, no roaded neighbour — the
//                   spur-cap / open-sea-refusal survivors) are counted and
//                   reported, not exempted: the own street satisfies the row.
//                   Uses the same 4-cardinal + column-wrap topology as gen.
//   R4 determinism — a second generation yields an identical road_level field.
// Exit non-zero on any failure.

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistics.hpp" // invalidate_logistics_caches
#include "world/market_clearing.hpp" // market_for_tile: a town's market (R6f)
#include "world/road_generation.hpp"
#include "harness_params.hpp"
#include "world/world.hpp"

#include "scripting/lua_state.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

// THE BRIDGE CAP as RULED (Ben, 2026-10-03: two water tiles), held here rather
// than read off the generator's kMaxCrossingTiles, so R6's walker and R7 ask the ruling of the
// laid field and not the code of itself.
constexpr int kBridgeCap = 2;
static int g_fail = 0;
static void check(bool ok, const char* what)
{
    std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) ++g_fail;
}

/// Re-run road generation on @p w's body with every nation's qualification forced to
/// @p qual: reset the body's road_level field, drop the A*/reach caches (they embed the
/// road discount), and call generate_roads again. Under BL-621's era-relative gates a
/// UNIFORM value grades every nation at percentile 0.5 whatever the value is — the
/// antiquity shape. The differential instrument is the SPREAD variant below. Formerly —
/// all else fixed, only the qualification input moves.
static void regen_roads_at_qualification(world& w, entity_id body, float qual)
{
    for (auto& [nid, nc] : w.nations)
        nc.qualification = qual;
    for (auto& [tid, tc] : w.tiles)
        if (tc.body == body)
            tc.road_level = 0;
    // EVERY traversal cache, through its one owner (BL-1119 round 2). This used to
    // clear only the pair cache and the reach field, so the regen read the built
    // world's road-weighted FLOOD FIELDS and laid its "road-free" network on roaded
    // costs — the instrument measured a different pass from the one generation runs.
    invalidate_logistics_caches(w);
    generate_roads(w, body);
}

/// The BL-621 differential instrument: a three-band qualification SPREAD assigned by
/// ascending nation id — bottom third 0.05 (percentile ~0.17, below the Road gate),
/// middle 0.30 (~0.5, Roads), top 0.60 (~0.83, Highways where two majors meet). All
/// else fixed; deterministic by the id sort.
/// How the spread orders nations into its three bands.
enum class spread_order
{
    by_id,    ///< ascending nation id — the Q rows' differential instrument
    by_urban, ///< most City+ centres first, then most towns, then id — R2's Highway reading
};

/// City+ (scale >= 3) and Town+ (scale >= 2) centre counts per nation on @p body.
static void urban_weight(const world& w, entity_id body, std::map<entity_id, std::pair<int, int>>& out)
{
    for (const auto& [cid, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        const auto pit = w.population_centres.find(cid);
        const auto nit = w.tile_to_nation.find(tile);
        if (tit == w.tiles.end() || tit->second.body != body || pit == w.population_centres.end()
            || nit == w.tile_to_nation.end())
            continue;
        if (pit->second.scale >= 3) ++out[nit->second].first;
        if (pit->second.scale >= 2) ++out[nit->second].second;
    }
}

/// The nations in @p order's band order (bottom band first).
static std::vector<entity_id> spread_ranking(const world& w, entity_id body, spread_order order)
{
    std::vector<entity_id> nids;
    for (const auto& [nid, nc] : w.nations)
        nids.push_back(nid);
    std::sort(nids.begin(), nids.end());
    if (order == spread_order::by_urban)
    {
        std::map<entity_id, std::pair<int, int>> wt;
        urban_weight(w, body, wt);
        // Ascending urban weight, so the most urban third lands in the TOP band; ties by
        // id (stable over the id-sorted list), deterministic.
        std::stable_sort(nids.begin(), nids.end(), [&](entity_id a, entity_id b) {
            const auto wa = wt.count(a) ? wt[a] : std::pair<int, int>{ 0, 0 };
            const auto wb = wt.count(b) ? wt[b] : std::pair<int, int>{ 0, 0 };
            return wa < wb;
        });
    }
    return nids;
}

static road_generation_stats regen_roads_with_spread(world& w, entity_id body,
                                                     spread_order order = spread_order::by_id)
{
    const std::vector<entity_id> nids = spread_ranking(w, body, order);
    const int n = static_cast<int>(nids.size());
    for (int i = 0; i < n; ++i)
        w.nations[nids[static_cast<std::size_t>(i)]].qualification =
            (i < n / 3) ? 0.05f : (i < 2 * n / 3) ? 0.30f : 0.60f;
    for (auto& [tid, tc] : w.tiles)
        if (tc.body == body)
            tc.road_level = 0;
    invalidate_logistics_caches(w); // every cache, as above (BL-1119 round 2)
    road_generation_stats st{};
    generate_roads(w, body, nullptr, kVillageSpurFloorHeads, &st);
    return st;
}

/// BL-1119: re-run road generation on @p w's body at spur floor @p floor_heads from a
/// road-free body and cold caches, returning the pass's own stats.
static road_generation_stats regen_roads_at_floor(world& w, entity_id body, long long floor_heads,
                                                  road_generation_trace* trace = nullptr)
{
    for (auto& [tid, tc] : w.tiles)
        if (tc.body == body)
            tc.road_level = 0;
    invalidate_logistics_caches(w);
    road_generation_stats st{};
    generate_roads(w, body, nullptr, floor_heads, &st, trace);
    return st;
}

/// BL-1119 round 3 — JOINED TO A TOWN, rebuilt from the trace alone, never from the
/// pass's own bookkeeping (gen_step_costs carries the same reading). Connectivity is
/// the laid routes' own paths: consecutive tiles of every tree, loop and spur route are
/// joined (water tiles included, so a strait crossing bridges, as the rule says it
/// does). Border routes are left out, so "joined" means inside the nation's own
/// network, and a bare street is never a conductor: street tiles join nothing unless
/// a laid route runs through them. A centre is joined when its tile's component holds
/// a town (scale >= 2) of the same nation — a town is joined by being one.
struct joined_reading
{
    int villages_claimed = 0, villages_unjoined = 0;
    int border_endpoints = 0, border_unjoined = 0;
};

static joined_reading read_joined(const world& w, entity_id body, const road_generation_trace& tr)
{
    std::map<entity_id, entity_id> parent;
    auto root = [&](entity_id x) -> entity_id {
        if (parent.find(x) == parent.end()) { parent.emplace(x, x); return x; }
        entity_id r = x;
        while (parent[r] != r) r = parent[r];
        while (parent[x] != r) { const entity_id nx = parent[x]; parent[x] = r; x = nx; }
        return r;
    };
    auto join = [&](entity_id a, entity_id b) {
        const entity_id ra = root(a), rb = root(b);
        if (ra != rb) parent[std::max(ra, rb)] = std::min(ra, rb);
    };
    for (const auto& r : tr.routes)
    {
        if (r.k == road_generation_trace::kind::border) continue;
        for (std::size_t i = 1; i < r.path.size(); ++i) join(r.path[i - 1], r.path[i]);
        if (!r.path.empty()) { join(r.from, r.path.front()); join(r.to, r.path.front()); }
    }
    auto nation_at = [&](entity_id t) {
        const auto it = w.tile_to_nation.find(t);
        return it != w.tile_to_nation.end() ? it->second : null_entity;
    };
    std::map<entity_id, int> scale_at;
    std::set<std::pair<entity_id, entity_id>> town_roots; // (component root, nation)
    for (const auto& [cid, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        const auto pit = w.population_centres.find(cid);
        if (tit == w.tiles.end() || tit->second.body != body || pit == w.population_centres.end())
            continue;
        scale_at[tile] = pit->second.scale;
        if (pit->second.scale >= 2) town_roots.insert({ root(tile), nation_at(tile) });
    }
    auto joined = [&](entity_id tile) {
        const auto s = scale_at.find(tile);
        if (s != scale_at.end() && s->second >= 2) return true;
        return town_roots.count({ root(tile), nation_at(tile) }) != 0;
    };
    joined_reading jr;
    for (const entity_id c : tr.on_network)
    {
        const auto ct = w.population_centre_tile.find(c);
        if (ct == w.population_centre_tile.end()) continue;
        const auto s = scale_at.find(ct->second);
        if (s == scale_at.end() || s->second >= 2) continue;
        ++jr.villages_claimed;
        if (!joined(ct->second)) ++jr.villages_unjoined;
    }
    for (const auto& r : tr.routes)
    {
        if (r.k != road_generation_trace::kind::border) continue;
        for (const entity_id e : { r.from, r.to })
        {
            ++jr.border_endpoints;
            if (!joined(e)) ++jr.border_unjoined;
        }
    }
    return jr;
}

/// Every route a nation laid inside its own network (tree, loop, spur), as a set of
/// (kind, lo, hi) — what a later nation's path costs can depend on.
static std::map<entity_id, std::set<std::tuple<int, entity_id, entity_id>>>
own_routes(const road_generation_trace& tr)
{
    std::map<entity_id, std::set<std::tuple<int, entity_id, entity_id>>> out;
    for (const auto& r : tr.routes)
        if (r.k != road_generation_trace::kind::border)
            out[r.nation].insert({ static_cast<int>(r.k), std::min(r.from, r.to),
                                   std::max(r.from, r.to) });
    return out;
}

/// Each nation's BACKBONE as the trace records it: (nation) -> sorted (lo, hi, is_tree).
static std::map<entity_id, std::set<std::tuple<entity_id, entity_id, bool>>>
backbone_links(const road_generation_trace& tr)
{
    std::map<entity_id, std::set<std::tuple<entity_id, entity_id, bool>>> out;
    for (const auto& r : tr.routes)
    {
        if (r.k != road_generation_trace::kind::tree && r.k != road_generation_trace::kind::loop)
            continue;
        out[r.nation].insert({ std::min(r.from, r.to), std::max(r.from, r.to),
                               r.k == road_generation_trace::kind::tree });
    }
    return out;
}

/// Why a world does or does not carry a Highway (BL-1119 round 2, the R2 finding): per
/// nation, its City+ centres (scale >= 3) and whether the spread put it in the top band.
struct highway_reading
{
    int nations = 0;
    int nations_two_major = 0;          ///< nations holding >= 2 City+ centres
    int two_major_band[3] = { 0, 0, 0 }; ///< of them, by band (bottom, middle, top)
    int majors = 0, max_majors_one_nation = 0;
};

static highway_reading read_highway_inputs(const world& w, entity_id body, spread_order order)
{
    highway_reading r;
    const std::vector<entity_id> nids = spread_ranking(w, body, order);
    const int n = static_cast<int>(nids.size());
    std::map<entity_id, std::pair<int, int>> wt;
    urban_weight(w, body, wt);
    r.nations = n;
    for (int i = 0; i < n; ++i)
    {
        const int band = (i < n / 3) ? 0 : (i < 2 * n / 3) ? 1 : 2; // regen_roads_with_spread's
        const auto it  = wt.find(nids[static_cast<std::size_t>(i)]);
        const int  m   = (it != wt.end()) ? it->second.first : 0;
        r.majors += m;
        r.max_majors_one_nation = std::max(r.max_majors_one_nation, m);
        if (m >= 2)
        {
            ++r.nations_two_major;
            ++r.two_major_band[band];
        }
    }
    return r;
}

/// Tier census of @p body's road_level field: counts[1..3], plus total in counts[0].
static void tier_census(const world& w, entity_id body, int counts[4])
{
    counts[0] = counts[1] = counts[2] = counts[3] = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body || tc.road_level == 0 || tc.road_level > 3) continue;
        ++counts[0];
        ++counts[tc.road_level];
    }
}

// ---------------------------------------------------------------------------
// R2s — THE HIGHWAY ROW ON THE SHIPPED WORLDS (BL-1159)
// ---------------------------------------------------------------------------
// The world the app builds: harness_params' build_app_base_world (the parsed
// world_gen config and the works table handed to make_hard_coded_world, then
// setup_world's writes and the recipe pass — centre_census's world). Only the
// shipped arc runs industrial urbanisation, so only it grows a nation two City+
// centres, which is what a Highway needs beyond the percentile gate.
//
// THE READING, per curated seed, on the home body:
//   * the SHIPPED road field as generation left it — national lattice plus the
//     ancient corridors stamped over it (LOGISTICS.md § 4a, max per tile);
//   * City+ centres, nations holding >= 2, and how many of those sit at the
//     Highway percentile (>= 0.80 of the world's nations, mid-rank on ties —
//     computed HERE from w.nations, not read off the pass);
//   * the national lattice re-laid on the same world with the pass's own stats
//     and trace (road_level reset, caches dropped): City-City links laid, at the
//     Highway tier, and its Highway tiles. The re-lay reads the pass's own inputs
//     (the tiles, nations and centres generation left), so every tile it stamps
//     must sit at or below the shipped field — R2s0, the consistency check.
//
// THE RULE, ASKED INDEPENDENTLY (LOGISTICS.md § 4): a backbone link (tree or kept
// loop) between two City+ centres in a nation at percentile >= 0.80 is a Highway.
// From the trace alone: every such QUALIFYING link's land tiles must carry
// road_level 3 on the shipped field (R2s-a — a SELF-CONSISTENCY check: the trace is
// the pass's own, so it confirms the field matches the pass's tier choice, not the
// rule from outside), and some shipped seed must lay a qualifying link at Highway
// (R2s-b — ancient-corridor Highway tiles never count). If no shipped world
// qualifies a link, R2s-b FAILS and the per-seed lines say why — never weakened.
// ---------------------------------------------------------------------------
// R6 — ROADS PULL TOWARD MARKETS (BL-1138; LOGISTICS.md § 4), on the shipped worlds
// ---------------------------------------------------------------------------
// Read on the same shipped world R2s builds, before its re-lay. The pass's trace
// records every raise it made (tile, level before), so the field at any moment of
// the pass — before it, at each link's test, after it — is rebuilt here exactly.
// Every row is asked with THIS FILE'S OWN WALKER (h_walk below), never the pass's:
// the cost contract is LOGISTICS.md § 1's one weight function, tile_traversal_cost,
// in integer millionths per cell, an edge the sum of its two cells, straits of at
// most three shore-water cells, open ocean never entered.
//   R6a every market centre is on its nation's own backbone (its nation's roads and
//       the straits between them reach one of its towns), asked per market. Exempt,
//       each listed: a market whose nation holds no town (no backbone to join), and
//       one with no land or strait route to its nation's towns at all.
//   R6a' the pass's own off-backbone count equals this walker's.
//   R6b each trunk pair, at the field its test read: a pair the detour test refused
//       has a network route within twice its direct route; a pair laid has none, a
//       non-empty route holding land, and every land tile of it at or above its tier
//       on the shipped field; an unpriced pair has no land end or no route.
//   R6c the trunk's pairs are each centre's K nearest centres by direct route,
//       unioned, on the field as the pass found it.
//   R6d some shipped world lays a trunk link that is actually stamped.

namespace {

/// This harness's own raster, rebuilt from the world's tiles.
struct h_grid
{
    int gw = 0, gh = 0;
    std::vector<entity_id>    tile;
    std::vector<std::int64_t> wgt;   // -1: no cell
    std::vector<int>          kind;  // 0 land, 1 shore water / lake, 2 open ocean
    std::vector<char>         road;
    std::vector<entity_id>    nation;
};

h_grid h_build(world& w, entity_id body)
{
    h_grid g;
    const auto bit = w.bodies.find(body);
    g.gw = bit->second.grid_width;
    g.gh = bit->second.grid_height;
    const std::size_t n = static_cast<std::size_t>(g.gw) * g.gh;
    g.tile.assign(n, null_entity);
    g.wgt.assign(n, -1);
    g.kind.assign(n, 0);
    g.road.assign(n, 0);
    g.nation.assign(n, null_entity);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body || tc.grid_x < 0 || tc.grid_x >= g.gw || tc.grid_y < 0 || tc.grid_y >= g.gh)
            continue;
        const std::size_t i = static_cast<std::size_t>(tc.grid_y) * g.gw + tc.grid_x;
        g.tile[i] = tid;
        g.wgt[i]  = static_cast<std::int64_t>(std::llround(static_cast<double>(tile_traversal_cost(tc)) * 1.0e6));
        g.kind[i] = !is_water(tc.substrate) ? 0 : (is_open_ocean(tc.substrate) ? 2 : 1);
        g.road[i] = (!is_water(tc.substrate) && tc.road_level > 0) ? 1 : 0;
        const auto nit = w.tile_to_nation.find(tid);
        g.nation[i] = nit != w.tile_to_nation.end() ? nit->second : null_entity;
    }
    return g;
}

/// Cheapest cost from @p src to every cell (min over the strait-run states), -1 where
/// unreached. @p roads_only: land must be roaded; @p held_by: land must be that
/// nation's. @p target >= 0 stops once it is settled; @p bound >= 0 stops past it.
std::vector<std::int64_t> h_walk(const h_grid& g, int src, bool roads_only, entity_id held_by,
                                 int target = -1, std::int64_t bound = -1)
{
    constexpr int R = kBridgeCap + 1; // runs 0..kBridgeCap (the bridge cap)
    const std::size_t n = g.wgt.size();
    std::vector<std::int64_t> best(n * R, -1);
    std::vector<std::int64_t> out(n, -1);
    std::set<std::tuple<std::int64_t, int, int>> open; // (cost, cell, run)
    if (src < 0 || g.wgt[static_cast<std::size_t>(src)] < 0) return out;
    const int r0 = g.kind[static_cast<std::size_t>(src)] == 0 ? 0 : 1;
    best[static_cast<std::size_t>(src) * R + r0] = 0;
    open.insert({ 0, src, r0 });
    while (!open.empty())
    {
        const auto [d, c, r] = *open.begin();
        open.erase(open.begin());
        if (bound >= 0 && d > bound) break;
        if (out[static_cast<std::size_t>(c)] < 0) out[static_cast<std::size_t>(c)] = d;
        if (c == target) break;
        const int x = c % g.gw, y = c / g.gw;
        for (int k = 0; k < 4; ++k)
        {
            const int nx0 = x + (k == 0 ? -1 : k == 1 ? 1 : 0);
            const int ny  = y + (k == 2 ? -1 : k == 3 ? 1 : 0);
            if (ny < 0 || ny >= g.gh) continue;
            const int v = ny * g.gw + ((nx0 + g.gw) % g.gw);
            const std::size_t vi = static_cast<std::size_t>(v);
            if (g.wgt[vi] < 0 || g.kind[vi] == 2) continue;
            int nr = 0;
            if (g.kind[vi] == 1) { nr = r + 1; if (nr > kBridgeCap) continue; }
            else if ((roads_only && !g.road[vi]) || (held_by != null_entity && g.nation[vi] != held_by)) continue;
            const std::int64_t nd = d + g.wgt[static_cast<std::size_t>(c)] + g.wgt[vi];
            std::int64_t& b = best[vi * R + nr];
            if (b >= 0 && b <= nd) continue;
            if (b >= 0) open.erase({ b, v, nr });
            b = nd;
            open.insert({ nd, v, nr });
        }
    }
    return out;
}

} // namespace

struct market_road_row
{
    market_road_stats st{};
    int tiers_before[4] = { 0, 0, 0, 0 };
    int tiers_after[4]  = { 0, 0, 0, 0 };
    int off_here = 0;               // R6a: off the backbone, asked here
    int exempt_no_backbone = 0;     // R6a: nation holds no town
    int exempt_no_route = 0;        // R6a: no land or strait route to its nation's towns
    int off_unexcused = 0;          // R6a: off with neither excuse (a FAIL)
    int trunk_bad = 0;              // R6b: pairs whose test or stamp disagrees
    int trunk_stamped = 0;          // R6d: laid trunk links actually stamped
    int neighbour_mismatch = 0;     // R6c
    int tier_bad = 0;               // R6e: laid trunks whose tier is not the rule's, or not stamped at it
    int cliff_tiles = 0;            // R6g: tiles the pass raised on sub-0.40 or unowned land
    int cliff_bad = 0;              // R6g: of them, left above Track by the pass
    int pull_tests = 0;             // R6f: pull links and pull-walk ends checked
    int pull_order_bad = 0;         // R6f: a pull laid while a heavier town of its market failed
    int pull_unserved = 0;          // R6f: towns still failing the test when their market's walk ended
    double median_cost_before = 0.0, median_cost_after = 0.0; // trunk pairs, intra_body_path
    std::vector<std::string> notes; // one line per exemption or failure
};

static double median_of(std::vector<double> v)
{
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

static void read_market_rows(world& w, const generation_report& rep, market_road_row& r)
{
    const entity_id body = w.home_body;
    r.st = rep.market_roads;
    const market_road_trace& tr = rep.market_road_links;
    tier_census(w, body, r.tiers_after);

    std::vector<std::pair<entity_id, entity_id>> mk; // (market, centre tile), ascending id
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body && w.tiles.find(mc.centre_tile) != w.tiles.end())
            mk.push_back({ mid, mc.centre_tile });
    std::sort(mk.begin(), mk.end());
    const int gw = w.bodies.at(body).grid_width;
    auto cell = [&](entity_id t) {
        const tile_component& tc = w.tiles.at(t);
        return tc.grid_y * gw + tc.grid_x;
    };
    auto nation_at = [&](entity_id t) {
        const auto it = w.tile_to_nation.find(t);
        return it != w.tile_to_nation.end() ? it->second : null_entity;
    };
    std::map<entity_id, std::vector<int>> town_cells; // nation -> its towns' cells
    std::map<entity_id, int> scale_at;                // the highest centre scale per tile
    std::vector<std::pair<entity_id, entity_id>> town_list; // (tile, nation), ascending tile
    {
        for (const auto& [cid, tile] : w.population_centre_tile)
        {
            const auto tit = w.tiles.find(tile);
            const auto pit = w.population_centres.find(cid);
            if (tit == w.tiles.end() || tit->second.body != body || pit == w.population_centres.end())
                continue;
            int& sc = scale_at[tile];
            sc = std::max(sc, static_cast<int>(pit->second.scale));
        }
        for (const auto& [t, sc] : scale_at)
            if (sc >= 2 && nation_at(t) != null_entity)
            {
                town_cells[nation_at(t)].push_back(cell(t));
                town_list.push_back({ t, nation_at(t) });
            }
    }

    // --- The shipped field: R6a, and the laid trunks' tiers. -------------------------
    h_grid g = h_build(w, body);
    for (const auto& [mid, ct] : mk)
    {
        const entity_id n = nation_at(ct);
        const int c = cell(ct);
        char buf[160];
        const auto tc = town_cells.find(n);
        if (n == null_entity || tc == town_cells.end())
        {
            ++r.off_here; ++r.exempt_no_backbone;
            std::snprintf(buf, sizeof buf, "market %u exempt: its nation holds no town (no backbone)",
                          static_cast<unsigned>(mid));
            r.notes.push_back(buf);
            continue;
        }
        bool on = false;
        if (g.kind[static_cast<std::size_t>(c)] != 0 || g.road[static_cast<std::size_t>(c)])
        {
            const std::vector<std::int64_t> f = h_walk(g, c, /*roads_only=*/true, n);
            for (const int t : tc->second) if (f[static_cast<std::size_t>(t)] >= 0) { on = true; break; }
        }
        if (on) continue;
        ++r.off_here;
        // A route the nation could lay: over its own land and the straits between.
        const std::vector<std::int64_t> f = h_walk(g, c, /*roads_only=*/false, n);
        bool route = false;
        for (const int t : tc->second) if (f[static_cast<std::size_t>(t)] >= 0) { route = true; break; }
        if (!route)
        {
            ++r.exempt_no_route;
            std::snprintf(buf, sizeof buf, "market %u exempt: no route over its nation's own land and straits to its towns",
                          static_cast<unsigned>(mid));
        }
        else
        {
            ++r.off_unexcused;
            std::snprintf(buf, sizeof buf, "market %u OFF its nation's backbone, with a route to it (FAIL)",
                          static_cast<unsigned>(mid));
        }
        r.notes.push_back(buf);
    }
    std::vector<char> laid_ok(tr.links.size(), 1);
    for (std::size_t i = 0; i < tr.links.size(); ++i)
    {
        const auto& l = tr.links[i];
        if (l.k != market_road_trace::kind::trunk || !l.laid) continue;
        int land = 0;
        bool ok = !l.path.empty() && l.tier >= 1;
        for (const entity_id t : l.path)
        {
            const tile_component& tc = w.tiles.at(t);
            if (is_water(tc.substrate)) continue;
            ++land;
            if (tc.road_level < 1) ok = false; // stamped; the tier per tile is R6e's
        }
        laid_ok[i] = (ok && land > 0) ? 1 : 0;
        if (laid_ok[i]) ++r.trunk_stamped;
    }
    // R6e — THE TIER, FROM OUTSIDE THE PASS (LOGISTICS.md § 4's gates): a market end
    // reads the highest centre scale on its tile, at least a Town; the percentile is the
    // lower of the two ends' nations' (road_qualification_percentiles; unowned reads 0).
    // Highway: both ends City+ and pct >= 0.80; Road: a Town+ end and pct >= 0.40; else
    // Track. Every land tile of the laid route carries at least that tier.
    {
        const std::map<entity_id, float> pct = road_qualification_percentiles(w);
        auto pct_of = [&](entity_id n) {
            const auto it = pct.find(n);
            return it != pct.end() ? it->second : 0.0f;
        };
        auto end_scale = [&](entity_id t) {
            const auto it = scale_at.find(t);
            return std::max(2, it != scale_at.end() ? it->second : 0);
        };
        for (const auto& l : tr.links)
        {
            if (l.k != market_road_trace::kind::trunk || !l.laid) continue;
            const int sa = end_scale(l.from), sb = end_scale(l.to);
            const float q = std::min(pct_of(nation_at(l.from)), pct_of(nation_at(l.to)));
            const std::uint8_t want = (sa >= 3 && sb >= 3 && q >= 0.80f) ? 3
                                    : ((sa >= 2 || sb >= 2) && q >= 0.40f) ? 2 : 1;
            bool ok = l.tier == want;
            for (const entity_id t : l.path)
            {
                const tile_component& tc = w.tiles.at(t);
                if (is_water(tc.substrate)) continue;
                // Each tile at the gate of the nation whose land it crosses (Ben, 2026-10-03).
                const float qt = pct_of(nation_at(t));
                const std::uint8_t cap = (sa >= 3 && sb >= 3 && qt >= 0.80f) ? 3
                                       : ((sa >= 2 || sb >= 2) && qt >= 0.40f) ? 2 : 1;
                if (tc.road_level < std::min(want, cap)) ok = false;
            }
            if (!ok)
            {
                ++r.tier_bad;
                char buf[160];
                std::snprintf(buf, sizeof buf, "trunk %u-%u tier %d, the rule's %d (pct %.3f)",
                              static_cast<unsigned>(l.market_a), static_cast<unsigned>(l.market_b),
                              static_cast<int>(l.tier), static_cast<int>(want), static_cast<double>(q));
                r.notes.push_back(buf);
            }
        }
    }

    // R6g — THE ROAD CLIFF, PER TILE (Ben, 2026-10-03): every raise the pass made on the
    // land of a nation under the Road gate (0.40), or on unowned land, leaves it at Track.
    {
        const std::map<entity_id, float> pct = road_qualification_percentiles(w);
        std::map<entity_id, std::uint8_t> level_after; // the last raise per tile wins
        for (const auto& [t, before] : tr.raises) level_after[t] = 0;
        for (const auto& [t, lvl] : level_after) level_after[t] = w.tiles.at(t).road_level;
        std::map<entity_id, std::uint8_t> prior_level;
        for (const auto& [t, before] : tr.raises)
            if (prior_level.count(t) == 0) prior_level[t] = before;
        for (const auto& [t, after] : level_after)
        {
            const entity_id n = nation_at(t);
            const auto it = pct.find(n);
            const float q = (n != null_entity && it != pct.end()) ? it->second : 0.0f;
            if (q >= 0.40f) continue;
            // The pass raised this tile; above Track is only legitimate when it was already
            // above Track before the pass (a national or ancient road it found there).
            if (after > 1 && after > prior_level[t]) ++r.cliff_bad;
            ++r.cliff_tiles;
        }
    }

    // The trunk pairs, and the median traversal cost over them (intra_body_path).
    std::map<entity_id, entity_id> centre_of(mk.begin(), mk.end());
    std::set<std::pair<entity_id, entity_id>> theirs;
    for (const auto& l : tr.links)
        if (l.k == market_road_trace::kind::trunk) theirs.insert({ l.market_a, l.market_b });
    auto pair_costs = [&]() {
        invalidate_logistics_caches(w);
        std::vector<double> c;
        for (const auto& p : theirs)
        {
            const logistics_path& lp = intra_body_path(w, body, centre_of[p.first], centre_of[p.second]);
            if (lp.reachable) c.push_back(static_cast<double>(lp.cost));
        }
        return c;
    };
    r.median_cost_after = median_of(pair_costs());

    // --- The field as the pass found it: "before", and R6c. -------------------------
    std::map<entity_id, std::uint8_t> final_level;
    for (const auto& [t, lvl] : tr.raises) final_level[t] = w.tiles.at(t).road_level;
    // The level each raise left: the next raise's "before" on that tile, else the final.
    std::vector<std::uint8_t> after_level(tr.raises.size(), 0);
    {
        std::map<entity_id, std::uint8_t> next = final_level;
        for (std::size_t i = tr.raises.size(); i-- > 0;)
        {
            after_level[i] = next[tr.raises[i].first];
            next[tr.raises[i].first] = tr.raises[i].second;
        }
    }
    for (std::size_t i = tr.raises.size(); i-- > 0;)
        w.tiles.at(tr.raises[i].first).road_level = tr.raises[i].second;
    tier_census(w, body, r.tiers_before);
    r.median_cost_before = median_of(pair_costs());
    g = h_build(w, body);
    {
        std::vector<std::vector<std::int64_t>> f;
        for (const auto& [mid, ct] : mk) f.push_back(h_walk(g, cell(ct), /*roads_only=*/false, null_entity));
        std::set<std::pair<entity_id, entity_id>> mine;
        for (std::size_t i = 0; i < mk.size(); ++i)
        {
            std::vector<std::pair<std::int64_t, std::size_t>> near;
            for (std::size_t j = 0; j < mk.size(); ++j)
            {
                if (j == i || mk[j].second == mk[i].second) continue;
                const std::int64_t d = f[i][static_cast<std::size_t>(cell(mk[j].second))];
                if (d >= 0) near.push_back({ d, j });
            }
            std::sort(near.begin(), near.end());
            for (int k = 0; k < kMarketTrunkNeighbours && k < static_cast<int>(near.size()); ++k)
            {
                const std::size_t j = near[static_cast<std::size_t>(k)].second;
                mine.insert({ std::min(mk[i].first, mk[j].first), std::max(mk[i].first, mk[j].first) });
            }
        }
        for (const auto& p : mine)   if (theirs.count(p) == 0) ++r.neighbour_mismatch;
        for (const auto& p : theirs) if (mine.count(p) == 0)   ++r.neighbour_mismatch;
    }

    // --- R6b / R6f: replay the raises forward, testing each step at its own field. ---
    std::map<entity_id, std::set<entity_id>> pulled; // market -> towns already pulled to it
    std::size_t applied = 0;
    bool dirty = false;
    for (std::size_t i = 0; i < tr.links.size(); ++i)
    {
        const auto& l = tr.links[i];
        while (applied < l.raises_before && applied < tr.raises.size())
        {
            w.tiles.at(tr.raises[applied].first).road_level = after_level[applied];
            ++applied;
            dirty = true;
        }
        if (l.k == market_road_trace::kind::pull || l.k == market_road_trace::kind::pull_settled)
        {
            // R6f — THE PULL, at the field this step read. The market's towns: in its
            // catchment, in its centre's nation, not on its tile, not yet pulled.
            if (dirty) { g = h_build(w, body); dirty = false; }
            const entity_id mid = l.market_a;
            const entity_id mt  = centre_of[mid];
            const entity_id mn  = nation_at(mt);
            const int mc = cell(mt);
            const std::vector<std::int64_t> dfield = h_walk(g, mc, false, null_entity);
            const std::vector<std::int64_t> nfield = h_walk(g, mc, true, null_entity);
            std::int64_t max_gain = -1;
            int failing = 0;
            std::int64_t laid_gain = -1;
            for (const auto& [tt, tn] : town_list)
            {
                if (tn != mn || cell(tt) == mc || market_for_tile(w, tt) != mid) continue;
                if (pulled[mid].count(tt) != 0 && tt != l.from) continue;
                const std::int64_t d = dfield[static_cast<std::size_t>(cell(tt))];
                if (d < 0) continue;
                const std::int64_t n = nfield[static_cast<std::size_t>(cell(tt))];
                if (n >= 0 && n <= 2 * d) continue;
                ++failing;
                const std::int64_t gain = n < 0 ? std::numeric_limits<std::int64_t>::max() : n - d;
                if (tt == l.from) laid_gain = gain;
                max_gain = std::max(max_gain, gain);
            }
            ++r.pull_tests;
            char buf[160];
            if (l.k == market_road_trace::kind::pull)
            {
                pulled[mid].insert(l.from);
                if (laid_gain < 0 || laid_gain != max_gain)
                {
                    ++r.pull_order_bad;
                    std::snprintf(buf, sizeof buf, "pull to market %u: laid gain %lld, heaviest %lld",
                                  static_cast<unsigned>(mid), static_cast<long long>(laid_gain),
                                  static_cast<long long>(max_gain));
                    r.notes.push_back(buf);
                }
            }
            else if (failing > 0)
            {
                r.pull_unserved += failing;
                std::snprintf(buf, sizeof buf, "market %u: %d town(s) still fail the detour test",
                              static_cast<unsigned>(mid), failing);
                r.notes.push_back(buf);
            }
            continue;
        }
        if (l.k != market_road_trace::kind::trunk) continue;
        if (dirty) { g = h_build(w, body); dirty = false; }
        const int a = cell(l.from), b = cell(l.to);
        bool ok = true;
        const bool land_end = g.kind[static_cast<std::size_t>(a)] == 0;
        const std::int64_t d = land_end ? h_walk(g, a, false, null_entity, b)[static_cast<std::size_t>(b)] : -1;
        if (l.pair_q < 0 || l.direct_q < 0)
            ok = !l.laid && d < 0;                     // unpriced: truly no price from this end
        else
        {
            const std::int64_t net = h_walk(g, a, true, null_entity, b, 2 * d)[static_cast<std::size_t>(b)];
            const bool served = net >= 0 && net <= 2 * d;
            ok = d == l.direct_q && (l.laid ? (!served && laid_ok[i] != 0) : served);
        }
        if (!ok)
        {
            ++r.trunk_bad;
            char buf[200];
            std::snprintf(buf, sizeof buf, "trunk %u-%u disagrees: laid %d, pass direct %lld, here %lld",
                          static_cast<unsigned>(l.market_a), static_cast<unsigned>(l.market_b),
                          l.laid ? 1 : 0, static_cast<long long>(l.direct_q), static_cast<long long>(d));
            r.notes.push_back(buf);
        }
    }
    for (const auto& [t, lvl] : final_level) w.tiles.at(t).road_level = lvl; // the shipped field again
    invalidate_logistics_caches(w);
}

struct shipped_highway_row
{
    market_road_row market;
    uint32_t seed = 0;
    int    majors = 0, nations = 0, nations_two_major = 0, two_major_at_gate = 0;
    int    links_two_major = 0, links_highway = 0;    // the pass's own stats (re-laid)
    int    qualifying = 0, qualifying_at_highway = 0; // from the trace, the rule asked here
    int    shipped_tiers[4]  = { 0, 0, 0, 0 };
    int    national_tiers[4] = { 0, 0, 0, 0 };
    int    above_shipped = 0;                         // re-laid tiles above the shipped field
    // The why: the qualification spread and where the multi-City nations stand in it.
    int    nations_at_gate = 0, majors_at_gate = 0, distinct_qual = 0;
    float  max_pct = 0.0f, max_qual = 0.0f, max_pct_two_major = 0.0f, max_qual_two_major = 0.0f;
    int    pct_band[3] = { 0, 0, 0 };   // nations at pct < 0.40 / < 0.80 / >= 0.80
    std::string qual_hist;               // "value x count" per distinct qualification
    int    corp_focus[3] = { 0, 0, 0 }; // extraction / processing / trade
    int    highway_ancient_only = 0;     // shipped Highway tiles the national lattice does not lay
    double build_s = 0.0;
    // R7 — the bridge cap.
    std::map<int, int> run_hist;         // longest water run on a laid route -> routes
    std::map<int, int> raster_hist;      // four-cardinal water runs between roaded land -> runs
    int    longest_route_run = 0, longest_raster_run = 0;
    int    routes = 0, over_cap = 0;
    std::vector<std::string> offenders;  // writer, ends, run
    int    refused_candidates = 0, refused_spurs = 0, refused_border = 0, refused_ancient = 0;
    int    ancient_laid = 0, ancient_corridors = 0;
    int    nations_with_towns = 0, nations_split = 0;
};

static bool g_read_corps = false; // --corps: count the searched roster's focus too


static shipped_highway_row read_shipped_highways(lua_state& lua, uint32_t seed)
{
    shipped_highway_row r;
    r.seed = seed;
    world_params params{};
    params.seed = seed;
    app_start_world out;
    const auto t0 = std::chrono::steady_clock::now(); // diagnostic wall time only
    build_app_base_world(lua, params, out);
    r.build_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    world& w = out.w;
    const entity_id body = w.home_body;

    read_market_rows(w, out.report, r.market); // R6, BL-1138: before the re-lay resets roads
    tier_census(w, body, r.shipped_tiers);
    std::map<entity_id, std::uint8_t> shipped_level;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == body) shipped_level[tid] = tc.road_level;

    // Centre scale per tile (the highest standing there) and City+ per nation.
    std::map<entity_id, int> scale_at;
    for (const auto& [cid, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        const auto pit = w.population_centres.find(cid);
        if (tit == w.tiles.end() || tit->second.body != body || pit == w.population_centres.end())
            continue;
        int& s = scale_at[tile];
        s = std::max(s, static_cast<int>(pit->second.scale));
    }
    std::map<entity_id, std::pair<int, int>> wt;
    urban_weight(w, body, wt);

    // The percentile, mid-rank on ties, over every nation (LOGISTICS.md § 4).
    std::map<entity_id, float> pct;
    {
        const int nn = static_cast<int>(w.nations.size());
        for (const auto& [nid, nc] : w.nations)
        {
            int below = 0, tied = 0;
            for (const auto& [nid2, nc2] : w.nations)
            {
                below += nc2.qualification < nc.qualification;
                tied  += nc2.qualification == nc.qualification;
            }
            pct[nid] = nn > 0 ? (static_cast<float>(below) + 0.5f * static_cast<float>(tied))
                                    / static_cast<float>(nn)
                              : 0.0f;
        }
    }
    constexpr float kGate = 0.80f; // LOGISTICS.md § 4: a Highway needs percentile >= 0.80
    r.nations = static_cast<int>(w.nations.size());
    // The why (BL-1159): the qualification spread, and where the multi-City nations
    // stand in it.
    {
        std::set<float> distinct;
        for (const auto& [nid, nc] : w.nations)
        {
            distinct.insert(nc.qualification);
            if (pct[nid] >= kGate) ++r.nations_at_gate;
            r.max_pct = std::max(r.max_pct, pct[nid]);
            r.max_qual = std::max(r.max_qual, nc.qualification);
            const auto it = wt.find(nid);
            const int m = it != wt.end() ? it->second.first : 0;
            if (pct[nid] >= kGate) r.majors_at_gate += m;
            if (m >= 2)
            {
                r.max_pct_two_major = std::max(r.max_pct_two_major, pct[nid]);
                r.max_qual_two_major = std::max(r.max_qual_two_major, nc.qualification);
            }
        }
        r.distinct_qual = static_cast<int>(distinct.size());
        // The opening qualified fraction per nation, as a value histogram (few values),
        // and the percentile bands the road gates read (< 0.40 Track, < 0.80 Road, Highway).
        std::map<float, int> hist;
        for (const auto& [nid, nc] : w.nations)
        {
            ++hist[nc.qualification];
            const float p = pct[nid];
            ++r.pct_band[p < 0.40f ? 0 : p < kGate ? 1 : 2];
        }
        char buf[48];
        for (const auto& [q, n] : hist)
        {
            std::snprintf(buf, sizeof buf, "%s%.3fx%d", r.qual_hist.empty() ? "" : " ", q, n);
            r.qual_hist += buf;
        }
    }
    // Corporation focus counts (extraction / processing / trade). The roster is laid by
    // the landscape search, which the base world has not run, so --corps builds a
    // SECOND world through the search (apply_app_start_landscape) and counts its
    // roster — a separate world, so the road reading above stays the generation's.
    if (g_read_corps)
    {
        app_start_world o2;
        build_app_base_world(lua, params, o2);
        apply_app_start_landscape(o2);
        for (const auto& [cid, cc] : o2.w.corporations)
        {
            const int f = static_cast<int>(cc.focus);
            if (f >= 0 && f < 3) ++r.corp_focus[f];
        }
    }
    for (const auto& [nid, m] : wt)
    {
        r.majors += m.first;
        if (m.first >= 2 && nid != null_entity)
        {
            ++r.nations_two_major;
            const auto pit = pct.find(nid);
            if (pit != pct.end() && pit->second >= kGate) ++r.two_major_at_gate;
        }
    }

    // Re-lay the national lattice with the pass's stats and trace. Generation lays it
    // BEFORE the sea lanes are stamped (hard_coded_world.cpp, stamp_sea_lanes follows
    // generate_roads), and a lane discounts water in tile_traversal_cost, so the
    // lanes are lifted for the re-lay — or it prices straits on a cheaper sea than the
    // pass saw and lays different routes (326 tiles over the shipped field, measured).
    std::map<entity_id, std::uint8_t> lanes;
    for (auto& [tid, tc] : w.tiles)
        if (tc.body == body && tc.lane_level != 0)
        {
            lanes[tid]    = tc.lane_level;
            tc.lane_level = 0;
        }
    road_generation_trace tr;
    const road_generation_stats st = regen_roads_at_floor(w, body, kVillageSpurFloorHeads, &tr);
    for (const auto& [tid, lvl] : lanes) w.tiles[tid].lane_level = lvl;
    r.links_two_major = st.links_two_major;
    r.links_highway   = st.links_highway;
    tier_census(w, body, r.national_tiers);
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != body) continue;
        const auto it = shipped_level.find(tid);
        if (it != shipped_level.end() && tc.road_level > it->second) ++r.above_shipped;
        if (it != shipped_level.end() && it->second == 3 && tc.road_level < 3)
            ++r.highway_ancient_only;
    }

    // The rule, from the trace: a qualifying link's land tiles are Highway on the
    // SHIPPED field.
    auto scale_of = [&](entity_id t) {
        const auto it = scale_at.find(t);
        return it != scale_at.end() ? it->second : 0;
    };
    for (const auto& rt : tr.routes)
    {
        if (rt.k != road_generation_trace::kind::tree && rt.k != road_generation_trace::kind::loop)
            continue;
        if (scale_of(rt.from) < 3 || scale_of(rt.to) < 3) continue;
        const auto pit = pct.find(rt.nation);
        if (pit == pct.end() || pit->second < kGate) continue;
        ++r.qualifying;
        bool all = true;
        int  land = 0;
        for (const entity_id t : rt.path)
        {
            const auto tit = w.tiles.find(t);
            if (tit == w.tiles.end() || is_water(tit->second.substrate)) continue;
            ++land;
            const auto sit = shipped_level.find(t);
            if (sit == shipped_level.end() || sit->second != 3) all = false;
        }
        if (all && land > 0) ++r.qualifying_at_highway;
    }

    // R7 — THE BRIDGE CAP, asked of every route every writer laid.
    auto note_route = [&](const char* writer, entity_id from, entity_id to,
                          const std::vector<entity_id>& path) {
        const int run = longest_water_run(w, path);
        ++r.routes;
        ++r.run_hist[run];
        r.longest_route_run = std::max(r.longest_route_run, run);
        if (run > kBridgeCap)
        {
            ++r.over_cap;
            if (r.offenders.size() < 12)
            {
                const auto& ta = w.tiles.at(from);
                const auto& tb = w.tiles.at(to);
                char buf[160];
                std::snprintf(buf, sizeof buf, "%s (%d,%d) -> (%d,%d): water run %d", writer,
                              ta.grid_x, ta.grid_y, tb.grid_x, tb.grid_y, run);
                r.offenders.push_back(buf);
            }
        }
    };
    for (const auto& rt : tr.routes)
    {
        const char* k = rt.k == road_generation_trace::kind::tree   ? "national tree"
                      : rt.k == road_generation_trace::kind::loop   ? "national loop"
                      : rt.k == road_generation_trace::kind::spur   ? "village spur"
                                                                    : "border link";
        note_route(k, rt.from, rt.to, rt.path);
    }
    for (const auto& rt : out.report.history_road_links.routes)
        note_route("ancient corridor (stamp_history_roads)", rt.from, rt.to, rt.path);
    for (const auto& l : out.report.market_road_links.links)
    {
        if (!l.laid || l.path.empty()) continue;
        const char* k = l.k == market_road_trace::kind::join ? "market join"
                      : l.k == market_road_trace::kind::pull ? "market pull"
                                                             : "market trunk";
        note_route(k, l.path.front(), l.path.back(), l.path);
    }
    r.refused_candidates = st.candidates_long_crossing;
    r.refused_spurs      = st.spurs_long_crossing;
    r.refused_border     = st.border_long_crossing;
    r.refused_ancient    = out.report.history_roads.refused_long_crossing;
    r.ancient_laid       = out.report.history_roads.laid;
    r.ancient_corridors  = out.report.history_roads.corridors;

    // The raster control: a maximal four-cardinal run of water tiles with SHIPPED
    // roaded land at both ends (column wrap on rows). Roads never stamp water, so
    // this is where a gap between two road ends reads as a bridge on the map.
    {
        const auto bit = w.bodies.find(body);
        const int gw = bit->second.grid_width, gh = bit->second.grid_height;
        const std::vector<entity_id>& grid = body_tile_grid(w, body);
        auto wet = [&](entity_id t) { return is_water(w.tiles.at(t).substrate); };
        auto roaded = [&](entity_id t) {
            const auto it = shipped_level.find(t);
            return !wet(t) && it != shipped_level.end() && it->second > 0;
        };
        auto scan = [&](auto at, int len, bool wraps) {
            for (int i = 0; i < len; ++i)
            {
                if (!roaded(at(i))) continue;
                int j = 1;
                while (j < len && (wraps || i + j < len) && wet(at((i + j) % len))) ++j;
                const int run = j - 1;
                if (run == 0 || (!wraps && i + j >= len)) continue;
                if (roaded(at((i + j) % len)))
                {
                    ++r.raster_hist[run];
                    r.longest_raster_run = std::max(r.longest_raster_run, run);
                }
            }
        };
        for (int y = 0; y < gh; ++y)
            scan([&](int c) { return grid[static_cast<std::size_t>(y) * gw + c]; }, gw, true);
        for (int x = 0; x < gw; ++x)
            scan([&](int y) { return grid[static_cast<std::size_t>(y) * gw + x]; }, gh, false);
    }

    // Nations whose town network SPLITS: towns (scale >= 2) joined through the laid
    // national tree and loops, union-find per nation; more than one component splits.
    {
        std::map<entity_id, entity_id> parent;
        std::function<entity_id(entity_id)> find = [&](entity_id t) {
            auto it = parent.find(t);
            if (it == parent.end()) { parent[t] = t; return t; }
            if (it->second == t) return t;
            const entity_id root = find(it->second);
            parent[t] = root;
            return root;
        };
        for (const auto& rt : tr.routes)
            if (rt.k == road_generation_trace::kind::tree || rt.k == road_generation_trace::kind::loop)
            {
                const entity_id a = find(rt.from), b = find(rt.to);
                if (a != b) parent[std::max(a, b)] = std::min(a, b);
            }
        std::map<entity_id, std::set<entity_id>> comps;
        for (const auto& [tile, sc] : scale_at)
        {
            if (sc < 2) continue;
            const auto nit = w.tile_to_nation.find(tile);
            if (nit == w.tile_to_nation.end() || nit->second == null_entity) continue;
            comps[nit->second].insert(find(tile));
        }
        r.nations_with_towns = static_cast<int>(comps.size());
        for (const auto& [n, c] : comps)
            if (c.size() > 1) ++r.nations_split;
    }
    return r;
}

static std::vector<uint32_t> library_seeds(const char* path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    std::vector<uint32_t> out;
    std::size_t pos = text.find("\"seeds\"");
    if (pos == std::string::npos) return out;
    const std::string key = "\"seed\"";
    while ((pos = text.find(key, pos)) != std::string::npos)
    {
        pos += key.size();
        std::size_t p = pos;
        while (p < text.size() && (text[p] == ' ' || text[p] == ':' || text[p] == '\t')) ++p;
        if (p < text.size() && text[p] >= '0' && text[p] <= '9')
            out.push_back(static_cast<uint32_t>(std::strtoul(text.c_str() + p, nullptr, 10)));
    }
    return out;
}

static std::vector<uint32_t> parse_seed_list(const std::string& s)
{
    std::vector<uint32_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ','))
        if (!tok.empty())
            out.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

static void run_shipped_highway_rows(const std::vector<uint32_t>& seeds)
{
    std::printf("      (R2s the shipped worlds, harness_params' build_app_base_world, %zu seed(s))\n",
                seeds.size());
    std::printf("      seed | City+ | nations, >=2 City+, of them pct>=0.80 | City-City links"
                " laid, at Highway | qualifying, at Highway shipped | Highway tiles shipped"
                " (national) | t/r/h shipped | re-laid above shipped | build s\n");
    std::fflush(stdout);
    lua_state lua; // one long-lived state, as the app keeps m_lua
    int qualifying = 0, qualifying_ok = 0, seeds_qualifying_with_highway = 0, above = 0;
    std::vector<market_road_row> mrows;
    std::vector<shipped_highway_row> hrows;
    for (const uint32_t seed : seeds)
    {
        const shipped_highway_row r = read_shipped_highways(lua, seed);
        mrows.push_back(r.market);
        hrows.push_back(r);
        std::printf("      %4u | %5d | %3d, %3d, %3d | %5d, %5d | %4d, %4d | %6d (%6d) |"
                    " %d/%d/%d | %d | %.1f\n",
                    r.seed, r.majors, r.nations, r.nations_two_major, r.two_major_at_gate,
                    r.links_two_major, r.links_highway, r.qualifying, r.qualifying_at_highway,
                    r.shipped_tiers[3], r.national_tiers[3], r.shipped_tiers[1],
                    r.shipped_tiers[2], r.shipped_tiers[3], r.above_shipped, r.build_s);
        std::printf("           why: %d distinct qualification values, max %.3f (pct %.3f); %d nations"
                    " at pct >= 0.80 holding %d City+ | the >=2-City+ nations: best qualification"
                    " %.3f, best pct %.3f\n",
                    r.distinct_qual, r.max_qual, r.max_pct, r.nations_at_gate, r.majors_at_gate,
                    r.max_qual_two_major, r.max_pct_two_major);
        std::printf("           spread: pct bands <0.40/<0.80/>=0.80 %d/%d/%d | Highway tiles national"
                    " %d, ancient-only %d | corp focus ext/proc/trade %d/%d/%d (--corps) | qualification %s\n",
                    r.pct_band[0], r.pct_band[1], r.pct_band[2], r.national_tiers[3],
                    r.highway_ancient_only, r.corp_focus[0], r.corp_focus[1], r.corp_focus[2],
                    r.qual_hist.c_str());
        std::fflush(stdout);
        qualifying    += r.qualifying;
        qualifying_ok += r.qualifying_at_highway;
        above         += r.above_shipped;
        // BL-1159 review fix: a seed counts only when a QUALIFYING link itself is
        // Highway, so ancient-corridor Highway tiles alone can never satisfy R2s-b.
        if (r.qualifying_at_highway > 0) ++seeds_qualifying_with_highway;
    }
    std::printf("      (R2s pooled: qualifying links %d, at Highway %d; seeds carrying a qualifying"
                " Highway %d of %zu; re-laid tiles above the shipped field %d)\n",
                qualifying, qualifying_ok, seeds_qualifying_with_highway, seeds.size(), above);
    check(above == 0, "R2s0 the re-laid national lattice sits within the shipped field");

    // R7 — the bridge cap (Ben, 2026-10-03), per seed then pooled.
    std::printf("      (R7 the bridge cap: ruled %d, kMaxCrossingTiles = %d)\n", kBridgeCap,
                kMaxCrossingTiles);
    std::printf("      seed | routes | longest water run on a laid route (raster control) | over cap |"
                " refused by the cap: candidates/spurs/border/ancient | ancient laid of corridors |"
                " nations split of with towns | run histogram routes [raster]\n");
    long long r7_over = 0, r7_routes = 0;
    int r7_longest = 0, r7_longest_raster = 0;
    std::map<int, long long> r7_hist, r7_raster;
    for (std::size_t i = 0; i < hrows.size(); ++i)
    {
        const shipped_highway_row& r = hrows[i];
        std::string hist, rh;
        char buf[32];
        for (const auto& [k, n] : r.run_hist)
        {
            std::snprintf(buf, sizeof buf, " %d:%d", k, n);
            hist += buf;
            r7_hist[k] += n;
        }
        for (const auto& [k, n] : r.raster_hist)
        {
            std::snprintf(buf, sizeof buf, " %d:%d", k, n);
            rh += buf;
            r7_raster[k] += n;
        }
        std::printf("      %4u | %5d | %d (%d) | %d | %d/%d/%d/%d | %d of %d | %d of %d |%s [%s ]\n",
                    r.seed, r.routes, r.longest_route_run, r.longest_raster_run, r.over_cap,
                    r.refused_candidates, r.refused_spurs, r.refused_border, r.refused_ancient,
                    r.ancient_laid, r.ancient_corridors, r.nations_split, r.nations_with_towns,
                    hist.c_str(), rh.c_str());
        for (const std::string& o : r.offenders)
            std::printf("           over the cap: %s\n", o.c_str());
        r7_over += r.over_cap;
        r7_routes += r.routes;
        r7_longest = std::max(r7_longest, r.longest_route_run);
        r7_longest_raster = std::max(r7_longest_raster, r.longest_raster_run);
    }
    {
        std::string hist, rh;
        char buf[40];
        for (const auto& [k, n] : r7_hist) { std::snprintf(buf, sizeof buf, " %d:%lld", k, n); hist += buf; }
        for (const auto& [k, n] : r7_raster) { std::snprintf(buf, sizeof buf, " %d:%lld", k, n); rh += buf; }
        std::printf("      (R7 pooled: %lld routes, %lld over the cap; longest %d (raster %d);"
                    " routes by longest run%s; raster runs%s)\n",
                    r7_routes, r7_over, r7_longest, r7_longest_raster, hist.c_str(), rh.c_str());
    }
    check(r7_routes > 0 && r7_over == 0,
          "R7 no laid road route, by any writer (national tree/loop/spur/border, ancient corridor,"
          " market join/pull/trunk), crosses a water run longer than two tiles (Ben, 2026-10-03),"
          " each offender named by its writer");

    // R6 — roads pull toward markets (BL-1138), per seed then pooled.
    std::printf("      (R6 roads pull toward markets, BL-1138; K = %d neighbours, detour ratio %.2f)\n",
                kMarketTrunkNeighbours, kDetourRatio);
    std::printf("      seed | markets | off before/after (here; no backbone, no route) | joins laid/failed |"
                " pull cand/laid | trunk pairs/unpriced/refused/laid/stamped | road tiles t/r/h before -> after |"
                " median pair cost before -> after | walks\n");
    long long off_b = 0, off_a = 0, off_here = 0, ex_nb = 0, ex_nr = 0, unexcused = 0;
    long long joins = 0, joins_failed = 0, pulls = 0, pull_c = 0;
    long long tp = 0, tun = 0, tl = 0, tref = 0, tstamped = 0, bad = 0, mism = 0;
    long long tier_bad = 0, pull_tests = 0, pull_order_bad = 0, pull_unserved = 0;
    long long cliff_tiles = 0, cliff_bad = 0;
    int seeds_count_mismatch = 0;
    long long tb[4] = { 0, 0, 0, 0 }, ta[4] = { 0, 0, 0, 0 };
    std::vector<double> med_b, med_a;
    for (std::size_t i = 0; i < mrows.size(); ++i)
    {
        const market_road_row& m = mrows[i];
        std::printf("      %4u | %3d | %2d / %2d (%2d; %d, %d) | %3d / %d | %3d / %3d |"
                    " %3d / %2d / %3d / %2d / %2d | %5d/%5d/%4d -> %5d/%5d/%4d | %.1f -> %.1f | %lld\n",
                    seeds[i], m.st.markets, m.st.off_backbone_before, m.st.off_backbone_after,
                    m.off_here, m.exempt_no_backbone, m.exempt_no_route, m.st.joins_laid,
                    m.st.joins_failed, m.st.pull_candidates, m.st.pull_laid, m.st.trunk_pairs,
                    m.st.trunk_unpriced, m.st.trunk_refused, m.st.trunk_laid, m.trunk_stamped,
                    m.tiers_before[1], m.tiers_before[2], m.tiers_before[3], m.tiers_after[1],
                    m.tiers_after[2], m.tiers_after[3], m.median_cost_before, m.median_cost_after,
                    m.st.walks);
        for (const std::string& note : m.notes)
            std::printf("           %s\n", note.c_str());
        off_b += m.st.off_backbone_before; off_a += m.st.off_backbone_after; off_here += m.off_here;
        ex_nb += m.exempt_no_backbone; ex_nr += m.exempt_no_route; unexcused += m.off_unexcused;
        joins += m.st.joins_laid; joins_failed += m.st.joins_failed;
        pulls += m.st.pull_laid; pull_c += m.st.pull_candidates;
        tp += m.st.trunk_pairs; tun += m.st.trunk_unpriced; tl += m.st.trunk_laid;
        tref += m.st.trunk_refused; tstamped += m.trunk_stamped; bad += m.trunk_bad;
        mism += m.neighbour_mismatch;
        tier_bad += m.tier_bad; pull_tests += m.pull_tests;
        cliff_tiles += m.cliff_tiles; cliff_bad += m.cliff_bad;
        pull_order_bad += m.pull_order_bad; pull_unserved += m.pull_unserved;
        if (m.off_here != m.st.off_backbone_after) ++seeds_count_mismatch;
        for (int k = 1; k < 4; ++k) { tb[k] += m.tiers_before[k]; ta[k] += m.tiers_after[k]; }
        med_b.push_back(m.median_cost_before); med_a.push_back(m.median_cost_after);
    }
    std::printf("      (R6 pooled: off backbone %lld -> %lld (here %lld: %lld no backbone, %lld no route,"
                " %lld unexcused); joins %lld laid, %lld failed; pulls %lld of %lld candidates;"
                " trunk %lld pairs, %lld unpriced, %lld refused, %lld laid, %lld stamped;"
                " road tiles t/r/h %lld/%lld/%lld -> %lld/%lld/%lld; median of per-seed median pair"
                " cost %.1f -> %.1f)\n",
                off_b, off_a, off_here, ex_nb, ex_nr, unexcused, joins, joins_failed, pulls, pull_c,
                tp, tun, tref, tl, tstamped, tb[1], tb[2], tb[3], ta[1], ta[2], ta[3],
                median_of(med_b), median_of(med_a));
    check(unexcused == 0,
          "R6a every market centre is on its nation's own backbone, per market (exempt, each listed:"
          " a nation holding no town, or no route over its own land and straits to its towns)");
    std::printf("      (R6e tier: %lld laid trunk(s) off the rule | R6f pull: %lld steps checked, %lld"
                " out of order, %lld town(s) left failing | R6a' seeds disagreeing %d)\n",
                tier_bad, pull_tests, pull_order_bad, pull_unserved, seeds_count_mismatch);
    check(seeds_count_mismatch == 0,
          "R6a' per seed, the pass's off-backbone count equals this harness's own walker's");
    check(tier_bad == 0,
          "R6e every laid trunk's tier is the gate at the lower of its two nations' percentiles"
          " (re-derived here), and its route is stamped at it");
    std::printf("      (R6g the Road cliff per tile: %lld tile(s) raised on sub-0.40 or unowned land,"
                " %lld left above Track)\n", cliff_tiles, cliff_bad);
    check(cliff_bad == 0,
          "R6g no join, pull or trunk tile the pass raised on a sub-0.40 nation's land (or unowned"
          " land) is above Track (Ben, 2026-10-03)");
    check(pull_tests > 0 && pull_order_bad == 0 && pull_unserved == 0,
          "R6f pulls were laid heaviest-first, and when a market's pull walk ended no town of it"
          " failed the detour test toward it");
    check(bad == 0,
          "R6b each trunk pair, at the field its test read: refused only when served within twice its"
          " direct route, laid only when not, with a non-empty stamped route at its tier");
    check(mism == 0, "R6c the trunk's pairs are each centre's K nearest centres by direct route, unioned"
                     " (recomputed here)");
    check(tstamped > 0 && tstamped == tl,
          "R6d some shipped world stamps a trunk link, and every link counted laid is stamped");
    // R2s-a is a SELF-CONSISTENCY check, not proof of the rule: the qualifying set is
    // read off the pass's own trace and re-derives the gate the pass applied, so it
    // shows the stamped field agrees with the pass's own tier choice on those links.
    check(qualifying_ok == qualifying,
          "R2s-a (self-consistency) every traced qualifying link (two City+, pct >= 0.80) is"
          " Highway on the shipped field");
    check(seeds_qualifying_with_highway > 0,
          "R2s-b some shipped seed lays a QUALIFYING link at Highway (ancient-corridor Highways"
          " do not count)");
}

int main(int argc, char** argv)
{
    // BL-1159: the shipped-world Highway row's seeds — the curated library by default
    // (read by relative path: run from the repo root), or --seeds a,b,c.
    std::vector<uint32_t> shipped_seeds;
    for (int a = 1; a < argc; ++a)
    {
        const std::string s = argv[a];
        if (s == "--seeds" && a + 1 < argc) shipped_seeds = parse_seed_list(argv[++a]);
        else if (s == "--corps") g_read_corps = true;
        else
        {
            std::printf("usage: road_generation_harness [--seeds a,b,c] [--corps]\n");
            return 2;
        }
    }
    if (shipped_seeds.empty())
    {
        shipped_seeds = library_seeds("docs/generation/seed_library.json");
        if (shipped_seeds.empty())
        {
            std::printf("[FAIL] docs/generation/seed_library.json not found or carries no seeds"
                        " (run from the repo root)\n");
            return 1;
        }
    }

    world w = make_hard_coded_world(no_prehistory());
    const entity_id kepler = w.home_body;
    const auto bit = w.bodies.find(kepler);
    if (bit == w.bodies.end()) { std::printf("[FAIL] no home body\n"); return 1; }
    const int gw = bit->second.grid_width;
    const int gh = bit->second.grid_height;

    // Raster index of Kepler tiles (grid_y*gw + grid_x -> tile), for adjacency.
    std::vector<entity_id> grid(static_cast<std::size_t>(gw) * gh, null_entity);
    int roaded_land = 0, roaded_ocean = 0;
    int track_tiles = 0, road_tiles = 0, highway_tiles = 0, over_highway = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        if (tc.body != kepler) continue;
        if (tc.grid_x >= 0 && tc.grid_x < gw && tc.grid_y >= 0 && tc.grid_y < gh)
            grid[static_cast<std::size_t>(tc.grid_y) * gw + tc.grid_x] = tid;
        if (tc.road_level > 0)
        {
            if (is_water(tc.substrate)) ++roaded_ocean;
            else                                              ++roaded_land;
            if      (tc.road_level == 1) ++track_tiles;
            else if (tc.road_level == 2) ++road_tiles;
            else if (tc.road_level == 3) ++highway_tiles;
            else                         ++over_highway;
        }
    }

    // R1 presence.
    check(roaded_land > 0, "R1 road lattice exists (some Kepler land tile has road_level > 0)");
    check(roaded_ocean == 0, "R1 no road_level stamped on ocean tiles");

    // R2 three-tier ladder (BL-172), gated by qualification PERCENTILE (BL-618 as
    // amended by BL-621 — Ben's 2026-08-25 ruling on NR-641). The no_prehistory
    // world's nations all tie at the seeding floor, which under era-relative gates
    // grades every one at percentile 0.5: the primary world carries a Roads
    // backbone and no Highways — the antiquity shape, and that IS the assertion.
    std::printf("      (tiers, all-tied world: track=%d road=%d highway=%d over=%d)\n",
                track_tiles, road_tiles, highway_tiles, over_highway);
    check(track_tiles > 0, "R2 track roads present (road_level 1)");
    check(road_tiles > 0 && highway_tiles == 0,
          "R2 an all-tied world keeps Roads and promotes no Highway (BL-621)");

    // The full ladder needs qualified nations. The highway tier further needs two
    // City+ centres ADJACENT in the backbone graph (both endpoints scale >= 3;
    // since BL-620 the backbone spans towns-and-up only). Centre scales are carved
    // from the Era -1 demography (BL-610) and City+ centres are rare, so whether
    // any such PAIR shares a nation is spatial luck, not a property of the
    // generator — assert the tier is REACHABLE across a seed sweep, each world
    // regenerated at high qualification. If no seed produces one, that is a real
    // regression and this still fails.
    //
    // WHICH NATIONS ARE "QUALIFIED" (BL-1119 round 2, why this row read 0 of 8). Since
    // BL-621 the gates read the PERCENTILE, so "every nation high" grades everyone 0.5
    // and promotes nothing; the regen needs a spread, and the spread decides which third
    // is top. Banded by ascending nation id, the top third was never a multi-City nation:
    // on all eight seeds every nation holding two City+ centres sat in the bottom or
    // middle band (17 nations, none top — about 0.1% by chance), because nation ids run
    // big-first. The City-City links were laid and the gate was right; the instrument
    // simply never qualified a nation that could hold a Highway. So this row bands by
    // URBAN WEIGHT (most City+, then most towns): the qualified third is the most urban,
    // which is the shape qualification has in a real world (industrialisation is urban),
    // and the assertion is unchanged — a Highway tile must actually be stamped. The
    // id-banded placement is still printed per seed so the finding stays on record.
    {
        int seeds_with_highway = 0, seeds_with_road = 0, first = -1;
        for (uint32_t s = 0; s < 8; ++s)
        {
            world_params wp;
            wp.seed = s * 0x9E3779B1u;
            world ws = make_hard_coded_world(no_prehistory(wp));
            // Where the multi-City nations fall under the ID-banded spread — the instrument
            // this row used to read, and the reason it could never pass (see above).
            const highway_reading by_id = read_highway_inputs(ws, ws.home_body, spread_order::by_id);
            const road_generation_stats hs =
                regen_roads_with_spread(ws, ws.home_body, spread_order::by_urban);
            const highway_reading hr = read_highway_inputs(ws, ws.home_body, spread_order::by_urban);
            int tc4[4];
            tier_census(ws, ws.home_body, tc4);
            // The why, per seed: a Highway needs ONE backbone link whose two ends are both
            // City+, in a nation at percentile >= 0.80 (the spread's top third).
            std::printf("      (seed %u: nations %d | City+ %d, most in one nation %d | nations with"
                        " >= 2 City+ %d: by-id bands b/m/t %d/%d/%d, by-urban bands %d/%d/%d |"
                        " City-City links laid %d, at Highway %d | highway tiles %d)\n",
                        s, hr.nations, hr.majors, hr.max_majors_one_nation, hr.nations_two_major,
                        by_id.two_major_band[0], by_id.two_major_band[1], by_id.two_major_band[2],
                        hr.two_major_band[0], hr.two_major_band[1], hr.two_major_band[2],
                        hs.links_two_major, hs.links_highway, tc4[3]);
            if (tc4[2] > 0) ++seeds_with_road;
            if (tc4[3] > 0)
            {
                ++seeds_with_highway;
                if (first < 0) first = static_cast<int>(s);
            }
        }
        std::printf("      (qualified regen: road tier on %d of 8 seeds; highway on %d of 8, first at seed index %d)\n",
                    seeds_with_road, seeds_with_highway, first);
        check(seeds_with_road == 8, "R2 road tier (road_level 2) appears on every qualified seed");
        // BL-1159: A PRINTED CONTROL, NO LONGER THE ROW. A no_prehistory world runs no
        // industrial urbanisation, so a nation holds at most one City+ and no link can be
        // City-City (0 of 8 since BL-1141). The Highway row reads the SHIPPED worlds below.
        std::printf("      (control, no-prehistory qualified regen: highway tier on %d of 8 -- "
                    "not asserted; R2s reads the shipped worlds, BL-1159)\n",
                    seeds_with_highway);
    }
    check(over_highway == 0, "R2 no tile exceeds the highway tier (road_level <= 3)");

    // R3 connectivity — EVERY centre must touch a road tile (itself or a
    // 4-cardinal neighbour, column-wrapped).
    //
    // STRENGTHENED BACK TO EVERY-CENTRE with BL-623 (provinces before roads):
    // anchor foundings now land BEFORE generate_roads, so they carry a local
    // street (pass 1b) and spur like any village, and the off-lattice centre
    // class — with its anchor exemption and the R3b companion row — is retired.
    // What legitimately remains is the SPUR-CAP / OPEN-SEA survivor: a village
    // whose spur found no reachable target inside kMaxSpurGridDist, or whose
    // every route crossed open sea — and, since BL-1119, every village below the
    // spur floor, which lays no spur by rule. Such a centre keeps only its own
    // street — which still satisfies this row — and is counted and reported below
    // (street-only: own tile roaded, no roaded 4-neighbour).
    auto road_at = [&](int r, int c) -> bool {
        if (r < 0 || r >= gh) return false;
        const entity_id t = grid[static_cast<std::size_t>(r) * gw + c];
        if (t == null_entity) return false;
        const auto it = w.tiles.find(t);
        return it != w.tiles.end() && it->second.road_level > 0;
    };
    auto is_anchor = [&](entity_id cid) -> bool {
        const auto it = w.population_centres.find(cid);
        return it != w.population_centres.end() && it->second.province_anchor;
    };
    int total = 0, connected = 0, anchors = 0, anchors_connected = 0, street_only = 0,
        off_lattice = 0;
    for (const auto& [cid, tile] : w.population_centre_tile)
    {
        const auto tit = w.tiles.find(tile);
        if (tit == w.tiles.end() || tit->second.body != kepler) continue;
        const int  r = tit->second.grid_y, c = tit->second.grid_x;
        const bool own       = road_at(r, c);
        const bool neighbour = road_at(r, (c + 1) % gw) || road_at(r, (c + gw - 1) % gw)
                               || road_at(r - 1, c) || road_at(r + 1, c);
        const bool touches = own || neighbour;
        ++total;
        if (touches) ++connected;
        else         ++off_lattice;
        if (own && !neighbour) ++street_only;
        if (is_anchor(cid))
        {
            ++anchors;
            if (touches) ++anchors_connected;
        }
    }
    std::printf("      (connectivity: %d/%d centres touch a road; %d/%d anchors;"
                " %d street-only — below the spur floor (BL-1119), or spur-cap/open-sea survivors)\n",
                connected, total, anchors_connected, anchors, street_only);
    check(total > 0 && connected == total,
          "R3 every population centre touches the road lattice (anchors included, BL-623)");

    // R4 determinism — regenerate and compare the whole road_level field by tile.
    world w2 = make_hard_coded_world(no_prehistory());
    int mismatches = 0;
    for (const auto& [tid, tc] : w.tiles)
    {
        const auto it = w2.tiles.find(tid);
        if (it == w2.tiles.end() || it->second.road_level != tc.road_level) ++mismatches;
    }
    check(mismatches == 0, "R4 road_level field identical across two generations");

    // R5 — the tree and the floor (BL-1119). A fresh world, so Q below keeps its own.
    {
        world w5 = make_hard_coded_world(no_prehistory());
        road_generation_trace tr0, trf;
        const road_generation_stats s0 = regen_roads_at_floor(w5, w5.home_body, 0, &tr0);
        int t0[4];
        tier_census(w5, w5.home_body, t0);
        const joined_reading j0 = read_joined(w5, w5.home_body, tr0);
        const road_generation_stats sf =
            regen_roads_at_floor(w5, w5.home_body, kVillageSpurFloorHeads, &trf);
        int tf[4];
        tier_census(w5, w5.home_body, tf);
        const joined_reading jf = read_joined(w5, w5.home_body, trf);
        auto line = [](const char* what, const road_generation_stats& s, const int t[4]) {
            std::printf("      (%s: road tiles %d | towns %d tree %d candidates %d admitted %d kept %d |"
                        " villages %d below %d spurs %d failed %d | border pairs %d, %d with no"
                        " network endpoint, links %d, %d on a bare street | floods %lld)\n",
                        what, t[0], s.towns, s.mst_links, s.loop_candidates, s.loops_admitted,
                        s.loops_kept, s.villages, s.villages_below_floor, s.spurs_laid,
                        s.spurs_failed, s.border_pairs, s.border_pairs_no_endpoint,
                        s.border_links, s.border_links_street_only, s.flood_fields);
        };
        line("floor 0", s0, t0);
        std::printf("      (shipped floor %lld heads, detour ratio %.2f)\n",
                    kVillageSpurFloorHeads, kDetourRatio);
        line("shipped floor", sf, tf);
        auto links_line = [](const char* what, const road_generation_stats& s, const joined_reading& j) {
            std::printf("      (%s: sea-route candidates %d, refused by the stamp tree %d loop %d |"
                        " villages on the network %d, not joined to a town %d | border endpoints %d,"
                        " not joined %d)\n",
                        what, s.candidates_unlayable, s.tree_links_refused, s.loops_refused,
                        j.villages_claimed, j.villages_unjoined, j.border_endpoints, j.border_unjoined);
        };
        links_line("floor 0", s0, j0);
        links_line("shipped floor", sf, jf);
        auto accounted = [](const road_generation_stats& s) {
            return s.villages == s.villages_below_floor + s.spurs_laid + s.spurs_failed;
        };
        check(s0.villages > 0 && accounted(s0) && accounted(sf),
              "R5a every village is below the floor, spurred, or failed (both floors)");
        check(s0.villages_below_floor == 0, "R5b floor 0 is the unfloored pass (no village below it)");
        check(kVillageSpurFloorHeads <= 0 || sf.villages_below_floor > 0,
              "R5c the shipped floor leaves some villages on their street alone");
        check(s0.loop_candidates > 0 && s0.loops_admitted < s0.loop_candidates
                  && s0.loops_kept <= s0.loops_admitted,
              "R5d the detour test refuses candidates; the ration keeps no more than it admitted");
        // R5e, RELABELLED HONESTLY (BL-1119 round 3, the cold review). "The floor never
        // touches the backbone" is NOT true by construction: nations are laid in id order,
        // so a lower nation's floor-dependent spurs lower the traversal cost of ground a
        // later nation's backbone is priced over, and that tree can differ. What IS true
        // by construction is the implication: a nation's backbone is priced over streets
        // (floor-free) plus the routes the nations laid BEFORE it, so while every earlier
        // nation laid the same routes at both floors, the next nation's backbone must be
        // identical. The row asserts that implication along the lay order until the
        // first floor-dependent difference; past it, backbones are compared and
        // reported (tree and loop links apart), not asserted.
        {
            using link_set = std::set<std::tuple<entity_id, entity_id, bool>>;
            using route_set = std::set<std::tuple<int, entity_id, entity_id>>;
            const auto b0 = backbone_links(tr0);
            const auto bf = backbone_links(trf);
            const auto o0 = own_routes(tr0);
            const auto of = own_routes(trf);
            // The lay order: every nation holding a centre on the body, ascending id —
            // the pass's own std::map order, unowned centres (null) first.
            std::set<entity_id> order;
            for (const auto& [cid, tile] : w5.population_centre_tile)
            {
                const auto tit = w5.tiles.find(tile);
                if (tit == w5.tiles.end() || tit->second.body != w5.home_body) continue;
                const auto nit = w5.tile_to_nation.find(tile);
                order.insert(nit != w5.tile_to_nation.end() ? nit->second : null_entity);
            }
            const link_set none;
            const route_set no_routes;
            bool prefix_same = true, implication_holds = true;
            int  covered = 0, covered_backbones = 0, same = 0, differ = 0, only0 = 0, onlyf = 0;
            for (const entity_id n : order)
            {
                const link_set& a = b0.count(n) ? b0.at(n) : none;
                const link_set& b = bf.count(n) ? bf.at(n) : none;
                const bool has_backbone = !a.empty() || !b.empty();
                if (prefix_same)
                {
                    ++covered;
                    if (has_backbone) ++covered_backbones;
                    if (a != b) implication_holds = false;
                }
                if (has_backbone)
                {
                    if (a == b) ++same;
                    else
                    {
                        ++differ;
                        for (const auto& x : a) if (!b.count(x)) ++only0;
                        for (const auto& x : b) if (!a.count(x)) ++onlyf;
                    }
                }
                const route_set& ra = o0.count(n) ? o0.at(n) : no_routes;
                const route_set& rb = of.count(n) ? of.at(n) : no_routes;
                if (ra != rb) prefix_same = false; // this nation's spurs differ: later costs may
            }
            std::printf("      (backbone across floors: lay-order prefix of identical routes covers %d"
                        " of %d nations, %d of them with a backbone | all %d backbones: %d identical,"
                        " %d differ - %d links only at floor 0, %d only at the shipped floor)\n",
                        covered, static_cast<int>(order.size()), covered_backbones, same + differ,
                        same, differ, only0, onlyf);
            check(implication_holds,
                  "R5e while every earlier nation laid the same routes at both floors, the next"
                  " nation's backbone is identical (later nations reported, not asserted)");
        }
        // R5f, MADE LIVE (BL-1119 round 3). It used to count border links ending off the
        // network, which cannot fail: the endpoints are DRAWN from on-network centres. Now
        // the network is rebuilt from the laid routes alone (read_joined), and every border
        // endpoint, and every village the pass counts on its network, must reach a town of
        // its own nation over laid road.
        check(sf.border_links > 0 && j0.border_unjoined == 0 && jf.border_unjoined == 0
                  && j0.villages_unjoined == 0 && jf.villages_unjoined == 0,
              "R5f every border endpoint and every on-network village reaches a town of its own"
              " nation over laid road (both floors)");
        // R5g (NR-945): only a layable link is a candidate, so the stamp refuses none.
        check(s0.tree_links_refused == 0 && s0.loops_refused == 0
                  && sf.tree_links_refused == 0 && sf.loops_refused == 0,
              "R5g the stamp refuses no tree link and no loop (sea routes never enter the tree)");
    }

    // Q — the BL-621 era-relative contract (Ben, 2026-08-25, ruling on NR-641): gates
    // read percentile standing, not absolute qualification. (Mutates w and w2; keep last.)
    {
        regen_roads_at_qualification(w, kepler, 0.05f); // uniform: everyone percentile 0.5
        int uni[4];
        tier_census(w, kepler, uni);
        std::printf("      (uniform floor: total=%d t/r/h=%d/%d/%d)\n",
                    uni[0], uni[1], uni[2], uni[3]);
        check(uni[3] == 0, "Q1a an all-tied world promotes no Highway (percentile 0.5 < 0.8)");
        check(uni[2] > 0,  "Q1b an all-tied world keeps a Roads backbone (0.5 >= 0.4) - the antiquity shape");

        regen_roads_at_qualification(w2, kepler, 0.60f); // uniform at a DIFFERENT absolute level
        int uni2[4];
        tier_census(w2, kepler, uni2);
        check(uni[0] == uni2[0] && uni[1] == uni2[1] && uni[2] == uni2[2] && uni[3] == uni2[3],
              "Q2 absolute level is irrelevant: uniform 0.05 and uniform 0.60 lattices are identical");

        regen_roads_with_spread(w2, kepler);
        int spr[4];
        tier_census(w2, kepler, spr);
        std::printf("      (spread: total=%d t/r/h=%d/%d/%d)\n",
                    spr[0], spr[1], spr[2], spr[3]);
        check(spr[1] != uni[1] || spr[2] != uni[2] || spr[3] != uni[3],
              "Q3 relative standing is what matters: a spread world's tier census differs from uniform");

        // Determinism of the instrument itself: the uniform regen equals a second uniform regen.
        regen_roads_at_qualification(w2, kepler, 0.05f);
        int uni3[4];
        tier_census(w2, kepler, uni3);
        check(uni[0] == uni3[0] && uni[1] == uni3[1] && uni[2] == uni3[2] && uni[3] == uni3[3],
              "Q4 the regeneration instrument is deterministic (two uniform regens agree)");
    }

    // R2s — the Highway row on the shipped worlds (BL-1159). Last: it is the slow part.
    run_shipped_highway_rows(shipped_seeds);

    std::printf("%s (%d failure(s))\n", g_fail ? "ROAD GEN AUDIT FAILED" : "ROAD GEN AUDIT OK", g_fail);
    return g_fail ? 1 : 0;
}
