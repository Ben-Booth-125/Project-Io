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
//                   ever carries a tier beyond Highway (ceiling).
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
#include "world/road_generation.hpp"
#include "harness_params.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <set>
#include <tuple>
#include <vector>

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

int main()
{
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
        check(seeds_with_highway > 0, "R2 highway tier (road_level 3) is reachable when qualified");
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

    std::printf("%s (%d failure(s))\n", g_fail ? "ROAD GEN AUDIT FAILED" : "ROAD GEN AUDIT OK", g_fail);
    return g_fail ? 1 : 0;
}
