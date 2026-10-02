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

#include "scripting/lua_state.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
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
// road_level 3 on the shipped field (R2s-a), and the shipped worlds must carry a
// Highway somewhere the rule qualifies one (R2s-b). If no shipped world qualifies
// a link, R2s-b FAILS and the per-seed lines say why — never weakened to pass.
struct shipped_highway_row
{
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
    for (const uint32_t seed : seeds)
    {
        const shipped_highway_row r = read_shipped_highways(lua, seed);
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
        if (r.qualifying > 0 && r.shipped_tiers[3] > 0) ++seeds_qualifying_with_highway;
    }
    std::printf("      (R2s pooled: qualifying links %d, at Highway %d; seeds carrying a qualifying"
                " Highway %d of %zu; re-laid tiles above the shipped field %d)\n",
                qualifying, qualifying_ok, seeds_qualifying_with_highway, seeds.size(), above);
    check(above == 0, "R2s0 the re-laid national lattice sits within the shipped field");
    check(qualifying_ok == qualifying,
          "R2s-a on the shipped worlds every qualifying link (two City+, pct >= 0.80) is Highway");
    check(seeds_qualifying_with_highway > 0,
          "R2s-b the highway tier (road_level 3) is reached on the shipped worlds where the rule"
          " qualifies one");
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
