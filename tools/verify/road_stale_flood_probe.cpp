// ---------------------------------------------------------------------------
// road_stale_flood_probe -- BL-1119 (roads tree and detour): what the stale flood
// reuse costs the road field. A MEASUREMENT, not a gate; exits 0 unless the build
// or the replay check fails.
// ---------------------------------------------------------------------------
// Within a pass, `generate_roads` and `stamp_history_roads` reuse cost floods and
// pair answers and never refresh them after a stamp, so a route is priced on the
// road field as it stood when its destination's flood was FIRST built — not, as
// LOGISTICS.md § 4 says, "on the field as it stands". Round 4 maximised the reuse
// (the history pass's 10x speed-up). This probe replays both passes on each built
// world twice:
//
//   STALE  the shipped passes (g_road_probe_fresh_floods off), the BL-1252 snap
//          included;
//   FRESH  stamp_edge clears every logistics cache after any stamp that raised a
//          tile (g_road_probe_fresh_floods on), so every later route is priced on
//          the field as it stands -- without the snap: the BL-1119 reference.
//   or, with --compare unsnapped, UNSNAPPED in FRESH's place: the shipped passes
//          with the BL-1252 snap off (g_road_probe_no_snap), the field before it --
//          the snap's effect, and its time, read in one run on one machine.
//
// and reports, per seed and pooled:
//   TILES     distinct land tiles on the laid routes, by pass class (backbone =
//             tree + loop, spur, border, history), and the road field's total.
//   PARALLEL  a laid route's PARALLEL RUN: a maximal run of consecutive LAND tiles
//             of route R, each having within Chebyshev distance d (columns wrap)
//             a tile of some OTHER laid route that is not itself on R. Village
//             streets that no route crosses are not "other road". Counted for
//             (d, K) in {(1,4), (1,8), (2,6), (2,12)} as runs of length >= K and
//             the tiles in them. Every parallel pair is counted from both sides
//             (once on R, once on S); a perpendicular crossing makes a run of at
//             most ~2d+1 tiles, which the K values sit above.
//   CHANGED   routes whose laid tiles differ between STALE and FRESH (backbone and
//             history by (from, to); spurs by village; border by nation pair), and
//             for those, both variants' routes priced on each variant's final field.
//   GAP       every laid route priced along its laid tiles on its own variant's
//             final field, less the cheapest route between the same two tiles on
//             that field (caches cleared): what staleness leaves on the table.
//   TIME      wall seconds of each pass, Release. Machine load is not controlled.
//
// The REPLAY CHECK: STALE generate_roads + stamp_history_roads + lay_market_roads
// must reproduce the built world's road field tile for tile, or the replay is not
// the pass the build ran.
//
// Usage:  road_stale_flood_probe.exe [--examples DIR] [--compare fresh|unsnapped] [seed ...]
//         (default: the 16 curated seeds; run from the repo root). DIR receives one
//         tile list per variant: the longest d=1 parallel run found (each tile tagged
//         with the laid routes holding it, by lay order).
// Build:  bash tools/verify/build_lua_harness.sh road_stale_flood_probe

#include "harness_params.hpp"
#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/logistics.hpp"
#include "world/river_generation.hpp"
#include "world/road_generation.hpp"
#include "world/works_roster.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace {

using clk = std::chrono::steady_clock;
double secs(clk::time_point a, clk::time_point b) { return std::chrono::duration<double>(b - a).count(); }

enum cls { kBackbone = 0, kSpur = 1, kBorder = 2, kHistory = 3, kClasses = 4 };
const char* cls_name[kClasses] = { "backbone", "spur", "border", "history" };

struct route
{
    cls       c;
    entity_id from, to;
    std::vector<entity_id> path; // lo -> hi as the traces store it
};

struct variant
{
    std::vector<route> routes;
    std::map<entity_id, std::uint8_t> field; // road tiles after the two passes
    double gen_s = 0.0, hist_s = 0.0;
    long long floods_gen = 0, floods_hist = 0;
    road_generation_stats st{};
    history_road_stats    hs{};
};

std::map<entity_id, std::uint8_t> road_field(const world& w, entity_id body)
{
    std::map<entity_id, std::uint8_t> f;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == body && tc.road_level != 0) f[tid] = tc.road_level;
    return f;
}

void clear_roads(world& w, entity_id body)
{
    for (auto& [tid, tc] : w.tiles)
        if (tc.body == body) tc.road_level = 0;
    invalidate_logistics_caches(w);
}

void set_field(world& w, entity_id body, const std::map<entity_id, std::uint8_t>& f)
{
    for (auto& [tid, tc] : w.tiles)
        if (tc.body == body)
        {
            const auto it = f.find(tid);
            tc.road_level = it == f.end() ? 0 : it->second;
        }
    invalidate_logistics_caches(w);
}

/// The sea lanes are stamped AFTER the two road passes in generation and they halve a
/// laned water step, so the replay runs with them lifted and puts them back for the
/// market pass, which generation runs after them.
std::map<entity_id, std::uint8_t> lane_field(const world& w, entity_id body)
{
    std::map<entity_id, std::uint8_t> f;
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == body && tc.lane_level != 0) f[tid] = tc.lane_level;
    return f;
}

void set_lanes(world& w, entity_id body, const std::map<entity_id, std::uint8_t>& f)
{
    for (auto& [tid, tc] : w.tiles)
        if (tc.body == body)
        {
            const auto it = f.find(tid);
            tc.lane_level = it == f.end() ? 0 : it->second;
        }
    invalidate_logistics_caches(w);
}

std::vector<history_road_node> history_nodes(const generation_report& rep, entity_id body)
{
    std::vector<history_road_node> nodes;
    for (const auto& b : rep.bodies)
        if (b.id == body)
            for (const region& p : b.settlement.regions)
                nodes.push_back(history_road_node{ p.col, p.row, p.work_reach_mod });
    return nodes;
}

/// The comparator V[1] (BL-1252): FRESH floods (the default), or the shipped passes with
/// the snap off -- the pre-BL-1252 field, for a same-machine before/after.
bool g_compare_unsnapped = false;

variant run_variant(world& w, entity_id body, const std::vector<history_road_node>& nodes,
                    const era_minus_one_fixture& fx, bool fresh)
{
    variant v;
    clear_roads(w, body);
    g_road_probe_fresh_floods = fresh && !g_compare_unsnapped;
    g_road_probe_no_snap      = fresh && g_compare_unsnapped;
    road_generation_trace tr;
    const clk::time_point t0 = clk::now();
    generate_roads(w, body, nullptr, kVillageSpurFloorHeads, &v.st, &tr);
    v.gen_s = secs(t0, clk::now());
    for (const auto& r : tr.routes)
    {
        const cls c = (r.k == road_generation_trace::kind::spur)     ? kSpur
                    : (r.k == road_generation_trace::kind::border)   ? kBorder
                                                                     : kBackbone;
        v.routes.push_back({ c, r.from, r.to, r.path });
    }
    if (!fx.setup_corridors.empty() && !nodes.empty())
    {
        history_road_trace ht;
        const clk::time_point h0 = clk::now();
        stamp_history_roads(w, body, nodes, fx.setup_corridors, nullptr, &v.hs, &ht);
        v.hist_s = secs(h0, clk::now());
        for (const auto& r : ht.routes) v.routes.push_back({ kHistory, r.from, r.to, r.path });
    }
    g_road_probe_fresh_floods = false;
    g_road_probe_no_snap      = false;
    v.field = road_field(w, body);
    return v;
}

// --- geometry -----------------------------------------------------------------
struct geo
{
    int gw = 0, gh = 0;
    const std::vector<entity_id>* grid = nullptr;
};

bool is_land(const world& w, entity_id t)
{
    const auto it = w.tiles.find(t);
    return it != w.tiles.end() && !is_water(it->second.substrate);
}

/// The laid route priced tile by tile along its tiles in TRAVEL order (from -> to) on
/// the world's current field: the flood's own directed edge (logistics.cpp §
/// flood_edge_cost), hop a -> b = mean of the two node weights x a's river discount on
/// the side it leaves by.
double price_path(const world& w, const geo& g, const route& r)
{
    std::vector<entity_id> seq = r.path;
    if (r.from > r.to) std::reverse(seq.begin(), seq.end());
    double c = 0.0;
    for (std::size_t i = 0; i + 1 < seq.size(); ++i)
    {
        const tile_component& a = w.tiles.at(seq[i]);
        const tile_component& b = w.tiles.at(seq[i + 1]);
        int dc = b.grid_x - a.grid_x;
        if (dc > 1) dc -= g.gw;
        if (dc < -1) dc += g.gw;
        const int dr   = b.grid_y - a.grid_y;
        const int side = hex_side_for_offset(dc, dr, (a.grid_y & 1) != 0);
        const float rm = side >= 0 ? river_edge_discount(a, side) : 1.0f;
        c += 0.5f * (tile_traversal_cost(a) + tile_traversal_cost(b)) * rm;
    }
    return c;
}

// --- parallel runs ---------------------------------------------------------------
struct par_cfg { int d; int k; };
const par_cfg kPar[] = { { 1, 4 }, { 1, 8 }, { 2, 6 }, { 2, 12 } };
constexpr int kParN = 4;

/// Runs on R's side, by R's class; and unordered route PAIRS with a qualifying run on
/// either side, by the pair's make-up (both national, national + history, both history).
struct par_count
{
    long long runs[kClasses] = {}, tiles[kClasses] = {};
    long long pairs_nn = 0, pairs_nh = 0, pairs_hh = 0;
};

struct example
{
    int len = 0;
    uint32_t seed = 0;
    int route_idx = -1, other_idx = -1;
    std::vector<entity_id> run;
};

struct par_result
{
    par_count by[kParN];
    example longest; // d = 1
};

/// THE MEASURE. For an ordered pair of laid routes (R, S): R's land tiles in path order;
/// tile i is BESIDE S when it is not itself on S and some tile of S that is not on R lies
/// within Chebyshev distance d (columns wrap). A parallel run is a maximal stretch of
/// consecutive tiles beside S; a tile R shares with S ends the run (a shared trunk is
/// reuse, not a parallel road). Counted when the run reaches K tiles.
par_result parallel_runs(const world& w, const geo& g, const std::vector<route>& routes, uint32_t seed)
{
    const int n = static_cast<int>(routes.size());
    std::vector<std::set<entity_id>> landset(static_cast<std::size_t>(n));
    std::vector<std::vector<entity_id>> land(static_cast<std::size_t>(n));
    std::map<entity_id, std::vector<int>> on; // tile -> routes holding it (land)
    for (int i = 0; i < n; ++i)
        for (const entity_id t : routes[i].path)
            if (is_land(w, t))
            {
                land[i].push_back(t);
                if (landset[i].insert(t).second) on[t].push_back(i);
            }
    std::set<std::pair<int, int>> qual[kParN];
    par_result res;
    auto around = [&](entity_id t, int d, auto&& fn) {
        const tile_component& tc = w.tiles.at(t);
        for (int dy = -d; dy <= d; ++dy)
        {
            const int y = tc.grid_y + dy;
            if (y < 0 || y >= g.gh) continue;
            for (int dx = -d; dx <= d; ++dx)
            {
                if (dx == 0 && dy == 0) continue;
                const int x = ((tc.grid_x + dx) % g.gw + g.gw) % g.gw;
                const entity_id u = (*g.grid)[static_cast<std::size_t>(y) * g.gw + x];
                if (u != null_entity) fn(u);
            }
        }
    };
    for (int ri = 0; ri < n; ++ri)
    {
        const std::vector<entity_id>& L = land[ri];
        for (int pi = 0; pi < kParN; ++pi)
        {
            const int d = kPar[pi].d;
            std::set<int> cands;
            for (const entity_id t : L)
                around(t, d, [&](entity_id u) {
                    if (landset[ri].count(u)) return;
                    const auto it = on.find(u);
                    if (it == on.end()) return;
                    for (const int s : it->second) if (s != ri) cands.insert(s);
                });
            for (const int si : cands)
            {
                const std::set<entity_id>& SS = landset[si];
                std::vector<char> beside(L.size(), 0);
                for (std::size_t i = 0; i < L.size(); ++i)
                {
                    if (SS.count(L[i])) continue;
                    bool b = false;
                    around(L[i], d, [&](entity_id u) {
                        if (!b && SS.count(u) && !landset[ri].count(u)) b = true;
                    });
                    beside[i] = b ? 1 : 0;
                }
                std::size_t i = 0;
                while (i < L.size())
                {
                    if (!beside[i]) { ++i; continue; }
                    std::size_t j = i;
                    while (j < L.size() && beside[j]) ++j;
                    const int len = static_cast<int>(j - i);
                    if (len >= kPar[pi].k)
                    {
                        ++res.by[pi].runs[routes[ri].c];
                        res.by[pi].tiles[routes[ri].c] += len;
                        qual[pi].insert({ std::min(ri, si), std::max(ri, si) });
                    }
                    if (pi == 0 && len > res.longest.len)
                    {
                        res.longest.len = len;
                        res.longest.seed = seed;
                        res.longest.route_idx = ri;
                        res.longest.other_idx = si;
                        res.longest.run.assign(L.begin() + static_cast<long>(i),
                                               L.begin() + static_cast<long>(j));
                    }
                    i = j;
                }
            }
        }
    }
    for (int pi = 0; pi < kParN; ++pi)
        for (const auto& [a, b] : qual[pi])
        {
            const int h = (routes[a].c == kHistory) + (routes[b].c == kHistory);
            ++(h == 0 ? res.by[pi].pairs_nn : h == 1 ? res.by[pi].pairs_nh : res.by[pi].pairs_hh);
        }
    return res;
}

std::string xy(const world& w, entity_id t)
{
    const auto it = w.tiles.find(t);
    if (it == w.tiles.end()) return "?";
    return "(" + std::to_string(it->second.grid_x) + "," + std::to_string(it->second.grid_y) + ")";
}

std::string centre_at(const world& w, entity_id t)
{
    for (const auto& [c, tile] : w.population_centre_tile)
        if (tile == t)
        {
            const auto p = w.population_centres.find(c);
            return "centre scale " + std::to_string(p != w.population_centres.end() ? p->second.scale : -1);
        }
    return "no centre";
}

/// The key a route is matched across variants by.
std::tuple<int, entity_id, entity_id> route_key(const world& w, const route& r)
{
    if (r.c == kSpur) return { kSpur, r.from, 0 };
    if (r.c == kBorder)
    {
        const auto fa = w.tile_to_nation.find(r.from), fb = w.tile_to_nation.find(r.to);
        const entity_id na = fa != w.tile_to_nation.end() ? fa->second : null_entity;
        const entity_id nb = fb != w.tile_to_nation.end() ? fb->second : null_entity;
        return { kBorder, std::min(na, nb), std::max(na, nb) };
    }
    return { r.c, r.from, r.to };
}

const char* vname(int v)
{
    return v == 0 ? "STALE" : g_compare_unsnapped ? "UNSNAPPED" : "FRESH";
}

struct pooled
{
    long long tiles[kClasses] = {}, field = 0;
    par_count par[kParN];
    long long changed[kClasses] = {}, only_one[kClasses] = {}, matched[kClasses] = {};
    long long ends[kClasses] = {}; // matched routes whose ENDPOINTS differ (a spur's target, a border's winning probe)
    double cS[kClasses] = {}, cF[kClasses] = {}, cSF[kClasses] = {}, cFF[kClasses] = {};
    double gap[kClasses] = {}, laid[kClasses] = {};
    long long gap_routes[kClasses] = {};
    double gen_s = 0, hist_s = 0;
    long long floods_gen = 0, floods_hist = 0;
    long long snaps_gen = 0, snaps_hist = 0; // BL-1252: stretches the snap re-walked
};

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    std::string ex_dir;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--examples" && i + 1 < argc) { ex_dir = argv[++i]; continue; }
        if (a == "--snap-run" && i + 1 < argc) { g_road_probe_snap_run = std::atoi(argv[++i]); continue; }
        if (a == "--compare" && i + 1 < argc)
        {
            const std::string m = argv[++i];
            if (m == "unsnapped") g_compare_unsnapped = true;
            else if (m != "fresh") { std::fprintf(stderr, "--compare fresh|unsnapped\n"); return 2; }
            continue;
        }
        seeds.push_back(static_cast<uint32_t>(std::strtoul(argv[i], nullptr, 0)));
    }
    if (seeds.empty()) seeds = { 46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0 };

    lua_state lua;
    lua.load("scripts/world_gen.lua");
    world_gen_config cfg{};
    cfg.load_from_lua(lua);
    works_registry works;
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);

    pooled P[2]; // 0 STALE, 1 FRESH
    example best_ex[2];
    std::string best_desc[2];
    bool all_faithful = true;

    for (const uint32_t seed : seeds)
    {
        world_params params{};
        params.seed = seed;
        era_minus_one_fixture fx;
        generation_report rep;
        world w = make_hard_coded_world(params, &rep, cfg, nullptr, &works, &fx);
        const entity_id body = w.home_body;
        const auto bit = w.bodies.find(body);
        geo g;
        g.gw = bit->second.grid_width;
        g.gh = bit->second.grid_height;
        g.grid = &body_tile_grid(w, body);
        const std::map<entity_id, std::uint8_t> built = road_field(w, body);
        const std::vector<history_road_node> nodes = history_nodes(rep, body);

        const std::map<entity_id, std::uint8_t> lanes = lane_field(w, body);
        set_lanes(w, body, {});

        variant V[2];
        V[0] = run_variant(w, body, nodes, fx, false);
        // REPLAY CHECK on the STALE replay: + the market roads == the built field.
        {
            set_lanes(w, body, lanes);
            lay_market_roads(w, body);
            set_lanes(w, body, {});
            const auto replay = road_field(w, body);
            std::size_t diff = 0;
            for (const auto& [t, l] : built) { const auto it = replay.find(t); if (it == replay.end() || it->second != l) ++diff; }
            for (const auto& [t, l] : replay) if (built.find(t) == built.end()) ++diff;
            std::printf("seed %u REPLAY CHECK: built %zu, replay %zu, %zu differ -> %s\n", seed,
                        built.size(), replay.size(), diff, diff == 0 ? "FAITHFUL" : "DIVERGES");
            if (diff != 0) all_faithful = false;
        }
        V[1] = run_variant(w, body, nodes, fx, true);

        // Price every route on each variant's final field; and the cheapest between its ends.
        // cost[v][f][i]: variant v's route i on variant f's field. opt[v][i] on its own field.
        std::vector<double> cost[2][2], opt[2];
        for (int f = 0; f < 2; ++f)
        {
            set_field(w, body, V[f].field);
            for (int v = 0; v < 2; ++v)
            {
                cost[v][f].resize(V[v].routes.size());
                for (std::size_t i = 0; i < V[v].routes.size(); ++i)
                    cost[v][f][i] = price_path(w, g, V[v].routes[i]);
            }
            opt[f].resize(V[f].routes.size());
            for (std::size_t i = 0; i < V[f].routes.size(); ++i)
            {
                const route& r = V[f].routes[i];
                opt[f][i] = intra_body_path(w, body, r.from, r.to).cost;
            }
        }

        for (int v = 0; v < 2; ++v)
        {
            set_field(w, body, V[v].field);
            pooled& p = P[v];
            const variant& X = V[v];
            std::set<entity_id> tiles[kClasses];
            for (const route& r : X.routes)
                for (const entity_id t : r.path)
                    if (is_land(w, t)) tiles[r.c].insert(t);
            long long field_land = 0;
            for (const auto& [t, l] : X.field) if (is_land(w, t)) ++field_land;
            const par_result pr = parallel_runs(w, g, X.routes, seed);
            double gap[kClasses] = {};
            for (std::size_t i = 0; i < X.routes.size(); ++i)
            {
                const cls c = X.routes[i].c;
                gap[c] += cost[v][v][i] - opt[v][i];
                p.gap[c] += cost[v][v][i] - opt[v][i];
                p.laid[c] += cost[v][v][i];
                ++p.gap_routes[c];
            }
            std::printf("seed %u %s TILES backbone %zu spur %zu border %zu history %zu | field %lld | "
                        "gen %.2f s (%lld floods) history %.2f s (%lld floods)\n",
                        seed, vname(v), tiles[0].size(), tiles[1].size(),
                        tiles[2].size(), tiles[3].size(), field_land, X.gen_s, X.st.flood_fields,
                        X.hist_s, X.hs.floods);
            for (int pi = 0; pi < kParN; ++pi)
            {
                std::printf("seed %u %s PARALLEL d%d K%d runs/tiles: backbone %lld/%lld spur %lld/%lld"
                            " border %lld/%lld history %lld/%lld | pairs nat-nat %lld nat-hist %lld"
                            " hist-hist %lld\n",
                            seed, vname(v), kPar[pi].d, kPar[pi].k,
                            pr.by[pi].runs[0], pr.by[pi].tiles[0], pr.by[pi].runs[1], pr.by[pi].tiles[1],
                            pr.by[pi].runs[2], pr.by[pi].tiles[2], pr.by[pi].runs[3], pr.by[pi].tiles[3],
                            pr.by[pi].pairs_nn, pr.by[pi].pairs_nh, pr.by[pi].pairs_hh);
                for (int c = 0; c < kClasses; ++c)
                {
                    p.par[pi].runs[c] += pr.by[pi].runs[c];
                    p.par[pi].tiles[c] += pr.by[pi].tiles[c];
                }
                p.par[pi].pairs_nn += pr.by[pi].pairs_nn;
                p.par[pi].pairs_nh += pr.by[pi].pairs_nh;
                p.par[pi].pairs_hh += pr.by[pi].pairs_hh;
            }
            std::printf("seed %u %s SNAPS gen %lld history %lld\n", seed, vname(v), X.st.snaps, X.hs.snaps);
            p.snaps_gen  += X.st.snaps;
            p.snaps_hist += X.hs.snaps;
            std::printf("seed %u %s GAP (laid on final field - cheapest on it): backbone %.1f spur %.1f"
                        " border %.1f history %.1f\n",
                        seed, vname(v), gap[0], gap[1], gap[2], gap[3]);
            for (int c = 0; c < kClasses; ++c) p.tiles[c] += static_cast<long long>(tiles[c].size());
            p.field += field_land;
            p.gen_s += X.gen_s;
            p.hist_s += X.hist_s;
            p.floods_gen += X.st.flood_fields;
            p.floods_hist += X.hs.floods;
            if (pr.longest.len > best_ex[v].len)
            {
                best_ex[v] = pr.longest;
                const route& R = X.routes[static_cast<std::size_t>(pr.longest.route_idx)];
                std::string d = "seed " + std::to_string(seed) + ": " + cls_name[R.c] + " route "
                              + xy(w, R.from) + " [" + centre_at(w, R.from) + "] -> " + xy(w, R.to)
                              + " [" + centre_at(w, R.to) + "], " + std::to_string(R.path.size())
                              + " tiles";
                if (pr.longest.other_idx >= 0)
                {
                    const route& S = X.routes[static_cast<std::size_t>(pr.longest.other_idx)];
                    d += "\n    beside " + std::string(cls_name[S.c]) + " route " + xy(w, S.from) + " ["
                       + centre_at(w, S.from) + "] -> " + xy(w, S.to) + " [" + centre_at(w, S.to)
                       + "], " + std::to_string(S.path.size()) + " tiles";
                    d += "\n    shared land tiles between the two: ";
                    std::set<entity_id> rs(R.path.begin(), R.path.end());
                    int shared = 0;
                    for (const entity_id t : S.path) if (rs.count(t) && is_land(w, t)) ++shared;
                    d += std::to_string(shared);
                }
                // Which laid routes (by lay order) hold a tile: the run's own and the other's.
                const auto holders = [&](entity_id t) {
                    std::string h = "[";
                    for (std::size_t q = 0; q < X.routes.size(); ++q)
                        if (std::find(X.routes[q].path.begin(), X.routes[q].path.end(), t)
                            != X.routes[q].path.end())
                            h += (h.size() > 1 ? "," : "") + std::to_string(q);
                    return h + "]";
                };
                d += "\n    lay order: route " + std::to_string(pr.longest.route_idx) + ", other "
                   + std::to_string(pr.longest.other_idx);
                d += "\n    run (" + std::to_string(pr.longest.len) + " tiles):";
                for (const entity_id t : pr.longest.run) d += " " + xy(w, t) + holders(t);
                if (pr.longest.other_idx >= 0)
                {
                    const route& S = X.routes[static_cast<std::size_t>(pr.longest.other_idx)];
                    d += "\n    other route tiles near the run:";
                    std::set<entity_id> rs(R.path.begin(), R.path.end());
                    for (const entity_id t : S.path)
                    {
                        if (rs.count(t) || !is_land(w, t)) continue;
                        const auto& a = w.tiles.at(t);
                        bool near = false;
                        for (const entity_id q : pr.longest.run)
                        {
                            const auto& b = w.tiles.at(q);
                            int dx = std::abs(a.grid_x - b.grid_x);
                            dx = std::min(dx, g.gw - dx);
                            if (dx <= 1 && std::abs(a.grid_y - b.grid_y) <= 1) { near = true; break; }
                        }
                        if (near) d += " " + xy(w, t) + holders(t);
                    }
                }
                best_desc[v] = d;
            }
        }

        // CHANGED: match routes across variants.
        std::map<std::tuple<int, entity_id, entity_id>, int> idx[2];
        for (int v = 0; v < 2; ++v)
            for (int i = 0; i < static_cast<int>(V[v].routes.size()); ++i)
                idx[v][route_key(w, V[v].routes[i])] = i;
        long long changed[kClasses] = {}, only[kClasses] = {}, matched[kClasses] = {};
        double sS[kClasses] = {}, sF[kClasses] = {}, sS_onF[kClasses] = {}, sF_onF[kClasses] = {};
        for (const auto& [k, i0] : idx[0])
        {
            const int c = std::get<0>(k);
            const auto it = idx[1].find(k);
            if (it == idx[1].end()) { ++only[c]; continue; }
            ++matched[c];
            const route& a = V[0].routes[static_cast<std::size_t>(i0)];
            const route& b = V[1].routes[static_cast<std::size_t>(it->second)];
            if (a.path == b.path && a.to == b.to && a.from == b.from) continue;
            ++changed[c];
            if (a.to != b.to || a.from != b.from) ++P[0].ends[c];
            sS[c] += cost[0][0][static_cast<std::size_t>(i0)];
            sF[c] += cost[1][0][static_cast<std::size_t>(it->second)];
            sS_onF[c] += cost[0][1][static_cast<std::size_t>(i0)];
            sF_onF[c] += cost[1][1][static_cast<std::size_t>(it->second)];
        }
        for (const auto& [k, i1] : idx[1]) if (idx[0].find(k) == idx[0].end()) ++only[std::get<0>(k)];
        for (int c = 0; c < kClasses; ++c)
        {
            std::printf("seed %u CHANGED %s: %lld of %lld matched routes differ (%lld laid in one variant"
                        " only) | changed routes' cost, STALE route vs FRESH route: on STALE field %.1f vs"
                        " %.1f, on FRESH field %.1f vs %.1f\n",
                        seed, cls_name[c], changed[c], matched[c], only[c], sS[c], sF[c], sS_onF[c], sF_onF[c]);
            P[0].changed[c] += changed[c];
            P[0].cS[c] += sS[c];
            P[0].cF[c] += sF[c];
            P[0].cSF[c] += sS_onF[c];
            P[0].cFF[c] += sF_onF[c];
            P[0].only_one[c] += only[c];
            P[0].matched[c] += matched[c];
        }
        std::fflush(stdout);
    }

    std::printf("\nPOOLED over %zu seeds\n", seeds.size());
    for (int v = 0; v < 2; ++v)
    {
        const pooled& p = P[v];
        std::printf("%s TILES backbone %lld spur %lld border %lld history %lld | field %lld | gen %.1f s "
                    "(%lld floods) history %.1f s (%lld floods)\n",
                    vname(v), p.tiles[0], p.tiles[1], p.tiles[2], p.tiles[3], p.field,
                    p.gen_s, p.floods_gen, p.hist_s, p.floods_hist);
        std::printf("%s SNAPS gen %lld history %lld\n", vname(v), p.snaps_gen, p.snaps_hist);
        for (int pi = 0; pi < kParN; ++pi)
            std::printf("%s PARALLEL d%d K%d runs/tiles: backbone %lld/%lld spur %lld/%lld border %lld/%lld"
                        " history %lld/%lld | pairs nat-nat %lld nat-hist %lld hist-hist %lld\n",
                        vname(v), kPar[pi].d, kPar[pi].k, p.par[pi].runs[0], p.par[pi].tiles[0],
                        p.par[pi].runs[1], p.par[pi].tiles[1], p.par[pi].runs[2], p.par[pi].tiles[2],
                        p.par[pi].runs[3], p.par[pi].tiles[3], p.par[pi].pairs_nn, p.par[pi].pairs_nh,
                        p.par[pi].pairs_hh);
        for (int c = 0; c < kClasses; ++c)
            std::printf("%s GAP %s: %lld routes, laid cost %.1f, gap %.1f (%.2f%%)\n", vname(v),
                        cls_name[c], p.gap_routes[c], p.laid[c], p.gap[c],
                        p.laid[c] > 0 ? 100.0 * p.gap[c] / p.laid[c] : 0.0);
    }
    for (int c = 0; c < kClasses; ++c)
        std::printf("CHANGED %s: %lld of %lld matched differ (%lld with different endpoints), %lld laid in"
                    " one variant only | changed routes, STALE route vs FRESH route: on STALE field %.1f vs"
                    " %.1f, on FRESH field %.1f vs %.1f\n",
                    cls_name[c], P[0].changed[c], P[0].matched[c], P[0].ends[c], P[0].only_one[c],
                    P[0].cS[c], P[0].cF[c], P[0].cSF[c], P[0].cFF[c]);
    for (int v = 0; v < 2; ++v)
    {
        std::printf("\nEXAMPLE %s longest d=1 parallel run: %s\n", vname(v), best_desc[v].c_str());
        if (!ex_dir.empty())
            if (FILE* f = std::fopen((ex_dir + (v ? "/parallel_fresh.txt" : "/parallel_stale.txt")).c_str(), "w"))
            {
                std::fprintf(f, "%s\n", best_desc[v].c_str());
                std::fclose(f);
            }
    }
    std::printf("\nREPLAY %s\n", all_faithful ? "FAITHFUL on every seed" : "DIVERGES on some seed");
    return all_faithful ? 0 : 1;
}
