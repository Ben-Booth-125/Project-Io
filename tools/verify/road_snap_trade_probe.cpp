// ---------------------------------------------------------------------------
// road_snap_trade_probe -- BL-1252 (no parallel roads): WHY the snap moves trade.
// A MEASUREMENT, not a gate; exits 0 unless a world fails to build.
// ---------------------------------------------------------------------------
// Builds the shipped start world (`build_app_start_world`, the app's order) per seed
// twice: SNAP OFF (g_road_probe_no_snap, the pre-BL-1252 roads) and SNAP ON at the
// given run (g_road_probe_snap_run). On each, before any tick, reads the quantities a
// convoy pays on (supply_system's dispatch: haul = rate x path.cost x (1 - node
// discount); travel ticks = convoy_travel_ticks, also from path.cost):
//
//   ROADS     land road tiles by tier; 4-connected road components; the largest's share.
//   LANES     each market centre to its kMarketNeighbours nearest market centres by grid
//             distance (the main trade lanes), priced centre -> centre with
//             intra_body_path: summed cost, summed travel ticks, mean path tiles, the
//             share of path land tiles on road, and pairs unreachable.
//   CATCH     every population centre to its own market's centre (market_for_tile):
//             the haul a producer pays to its shelf, summed the same way.
//   REACH     land tiles within reach of a supply anchor (body_reach_field finite), and
//             the mean reach cost over them.
//   Per lane, both variants' cost side by side: how many rose, fell, or broke, and the
//   ten largest rises named.
//
// Usage:  road_snap_trade_probe.exe [--snap-run N] [seed ...]   (default 43; repo root)
// Build:  bash tools/verify/build_lua_harness.sh road_snap_trade_probe

#include "harness_params.hpp"
#include "world/logistics.hpp"
#include "world/market_clearing.hpp"
#include "world/road_generation.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr int kMarketNeighbours = 3;

struct lane { entity_id a, b; double cost; int ticks; int tiles; int land; int roaded; bool ok; };

struct reading
{
    long long tier[4] = {};
    int components = 0, largest = 0;
    std::vector<lane> lanes;
    double catch_cost = 0; long long catch_ticks = 0, catch_n = 0, catch_unreach = 0;
    long long reach_tiles = 0; double reach_mean = 0;
    int markets = 0;
    std::set<entity_id> centres;
};

reading read_world(world& w)
{
    reading r;
    const entity_id body = w.home_body;
    const auto& bc = w.bodies.at(body);
    const int gw = bc.grid_width, gh = bc.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    std::vector<char> road(static_cast<std::size_t>(gw) * gh, 0);
    for (int i = 0; i < gw * gh; ++i)
    {
        const auto it = w.tiles.find(grid[static_cast<std::size_t>(i)]);
        if (it == w.tiles.end() || is_water(it->second.substrate)) continue;
        const int lv = std::min<int>(3, it->second.road_level);
        ++r.tier[lv];
        road[static_cast<std::size_t>(i)] = lv > 0;
    }
    // 4-connected components of the road raster, columns wrapping.
    std::vector<int> comp(road.size(), -1);
    for (int i = 0; i < gw * gh; ++i)
    {
        if (!road[static_cast<std::size_t>(i)] || comp[static_cast<std::size_t>(i)] >= 0) continue;
        int size = 0;
        std::vector<int> st{ i };
        comp[static_cast<std::size_t>(i)] = r.components;
        while (!st.empty())
        {
            const int u = st.back(); st.pop_back(); ++size;
            const int x = u % gw, y = u / gw;
            const int nb[4] = { y > 0 ? u - gw : -1, y + 1 < gh ? u + gw : -1,
                                y * gw + (x + gw - 1) % gw, y * gw + (x + 1) % gw };
            for (const int v : nb)
                if (v >= 0 && road[static_cast<std::size_t>(v)] && comp[static_cast<std::size_t>(v)] < 0)
                { comp[static_cast<std::size_t>(v)] = r.components; st.push_back(v); }
        }
        r.largest = std::max(r.largest, size);
        ++r.components;
    }
    // Market centres on the home body, ascending id.
    std::vector<std::pair<entity_id, entity_id>> mk; // (market, centre tile)
    for (const auto& [mid, m] : w.markets)
        if (m.body == body && m.centre_tile != null_entity) mk.push_back({ mid, m.centre_tile });
    std::sort(mk.begin(), mk.end());
    r.markets = static_cast<int>(mk.size());
    for (const auto& [m, t] : mk) r.centres.insert(t);
    const auto xy = [&](entity_id t) { const auto& tc = w.tiles.at(t); return std::make_pair(tc.grid_x, tc.grid_y); };
    const auto d2 = [&](entity_id a, entity_id b) {
        auto [ax, ay] = xy(a); auto [bx, by] = xy(b);
        int dx = std::abs(ax - bx); dx = std::min(dx, gw - dx);
        return static_cast<long long>(dx) * dx + static_cast<long long>(ay - by) * (ay - by);
    };
    std::set<std::pair<entity_id, entity_id>> pairs;
    for (const auto& [ma, ta] : mk)
    {
        std::vector<std::pair<long long, entity_id>> near;
        for (const auto& [mb, tb] : mk) if (mb != ma) near.push_back({ d2(ta, tb), mb });
        std::sort(near.begin(), near.end());
        for (int k = 0; k < kMarketNeighbours && k < static_cast<int>(near.size()); ++k)
            pairs.insert({ std::min(ma, near[k].second), std::max(ma, near[k].second) });
    }
    const auto lay = [&](entity_id from, entity_id to) {
        lane l{ from, to, 0, 0, 0, 0, 0, false };
        const logistics_path& p = intra_body_path(w, body, from, to);
        if (!p.reachable) return l;
        l.ok = true; l.cost = p.cost; l.ticks = convoy_travel_ticks(w, body, p);
        l.tiles = static_cast<int>(p.tiles.size());
        for (const entity_id t : p.tiles)
        {
            const auto& tc = w.tiles.at(t);
            if (is_water(tc.substrate)) continue;
            ++l.land; l.roaded += tc.road_level > 0;
        }
        return l;
    };
    for (const auto& [ma, mb] : pairs)
        r.lanes.push_back(lay(w.markets.at(ma).centre_tile, w.markets.at(mb).centre_tile));
    // Catchment hauls: every live centre on the body to its market's centre.
    std::vector<std::pair<entity_id, entity_id>> cs(w.population_centre_tile.begin(), w.population_centre_tile.end());
    std::sort(cs.begin(), cs.end());
    for (const auto& [c, t] : cs)
    {
        if (w.tiles.at(t).body != body) continue;
        const entity_id m = market_for_tile(w, t);
        const auto mit = w.markets.find(m);
        if (mit == w.markets.end() || mit->second.centre_tile == null_entity || mit->second.centre_tile == t) continue;
        const lane l = lay(t, mit->second.centre_tile);
        if (!l.ok) { ++r.catch_unreach; continue; }
        r.catch_cost += l.cost; r.catch_ticks += l.ticks; ++r.catch_n;
    }
    const std::vector<float>& reach = body_reach_field(w, body);
    double sum = 0;
    for (std::size_t i = 0; i < reach.size(); ++i)
        if (std::isfinite(reach[i]) && reach[i] < 1e29f) { ++r.reach_tiles; sum += reach[i]; }
    r.reach_mean = r.reach_tiles ? sum / static_cast<double>(r.reach_tiles) : 0.0;
    return r;
}

void print(const char* tag, const reading& r)
{
    double c = 0; long long tk = 0, tl = 0, land = 0, rd = 0, bad = 0;
    for (const lane& l : r.lanes) { if (!l.ok) { ++bad; continue; } c += l.cost; tk += l.ticks; tl += l.tiles; land += l.land; rd += l.roaded; }
    std::printf("  %-4s ROADS track %lld road %lld highway %lld (land road tiles %lld) | components %d, largest %d\n",
                tag, r.tier[1], r.tier[2], r.tier[3], r.tier[1] + r.tier[2] + r.tier[3], r.components, r.largest);
    std::printf("  %-4s LANES %zu pairs, %lld unreachable | cost %.1f, travel ticks %lld, tiles %lld, land on road %.1f%%\n",
                tag, r.lanes.size(), bad, c, tk, tl, land ? 100.0 * rd / land : 0.0);
    std::printf("  %-4s CATCH %lld centres -> own market centre, %lld unreachable | cost %.1f (mean %.3f), travel ticks %lld\n",
                tag, r.catch_n, r.catch_unreach, r.catch_cost, r.catch_n ? r.catch_cost / r.catch_n : 0.0, r.catch_ticks);
    std::printf("  %-4s REACH %lld tiles reach an anchor, mean reach cost %.3f\n", tag, r.reach_tiles, r.reach_mean);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    int run = 2;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--snap-run" && i + 1 < argc) { run = std::atoi(argv[++i]); continue; }
        seeds.push_back(static_cast<std::uint32_t>(std::strtoul(argv[i], nullptr, 0)));
    }
    if (seeds.empty()) seeds = { 43 };
    for (const std::uint32_t seed : seeds)
    {
        reading R[2];
        for (int v = 0; v < 2; ++v)
        {
            g_road_probe_no_snap  = (v == 0);
            g_road_probe_snap_run = run;
            lua_state lua;
            world_params p;
            p.seed = seed;
            auto start = std::make_unique<app_start_world>();
            try { build_app_start_world(lua, p, *start); }
            catch (const std::exception& e) { std::printf("seed %u build threw: %s\n", seed, e.what()); return 1; }
            R[v] = read_world(start->w);
        }
        g_road_probe_no_snap = false;
        std::printf("seed %u (snap run %d)\n", seed, run);
        print("OFF", R[0]);
        print("ON", R[1]);
        // The market centres themselves: the folds read traversal cost after the national
        // and ancient roads are laid, so the roads can move which markets exist.
        int shared_c = 0;
        for (const entity_id t : R[0].centres) shared_c += R[1].centres.count(t) ? 1 : 0;
        std::printf("  MARKETS off %d, on %d, centres shared %d\n", R[0].markets, R[1].markets, shared_c);
        // Lane by lane, matched on the centre-tile pair; a lane in one variant only is skipped.
        std::map<std::pair<entity_id, entity_id>, std::size_t> on_idx;
        for (std::size_t i = 0; i < R[1].lanes.size(); ++i)
            on_idx[{ std::min(R[1].lanes[i].a, R[1].lanes[i].b), std::max(R[1].lanes[i].a, R[1].lanes[i].b) }] = i;
        int rose = 0, fell = 0, same = 0, broke = 0, mended = 0, tick_up = 0, tick_down = 0, common = 0;
        double ca = 0, cb = 0;
        std::vector<std::pair<double, std::size_t>> rises;
        std::vector<std::size_t> match(R[0].lanes.size(), 0);
        for (std::size_t i = 0; i < R[0].lanes.size(); ++i)
        {
            const lane& a = R[0].lanes[i];
            const auto it = on_idx.find({ std::min(a.a, a.b), std::max(a.a, a.b) });
            if (it == on_idx.end()) continue;
            match[i] = it->second;
            ++common;
            const lane& b = R[1].lanes[it->second];
            if (a.ok && b.ok) { ca += a.cost; cb += b.cost; }
            if (a.ok && !b.ok) { ++broke; continue; }
            if (!a.ok && b.ok) { ++mended; continue; }
            if (!a.ok) continue;
            if (b.cost > a.cost + 1e-3) { ++rose; rises.push_back({ b.cost - a.cost, i }); }
            else if (b.cost < a.cost - 1e-3) ++fell; else ++same;
            tick_up += b.ticks > a.ticks; tick_down += b.ticks < a.ticks;
        }
        std::printf("  LANE DELTA on %d common lanes (ON vs OFF): cost %.1f -> %.1f | %d rose, %d fell, %d same,"
                    " %d broke, %d mended | travel ticks up %d, down %d\n",
                    common, ca, cb, rose, fell, same, broke, mended, tick_up, tick_down);
        std::sort(rises.rbegin(), rises.rend());
        for (std::size_t k = 0; k < rises.size() && k < 10; ++k)
        {
            const lane& a = R[0].lanes[rises[k].second]; const lane& b = R[1].lanes[match[rises[k].second]];
            std::printf("    rise %.2f: cost %.2f -> %.2f, ticks %d -> %d, tiles %d -> %d, land on road %d/%d -> %d/%d\n",
                        rises[k].first, a.cost, b.cost, a.ticks, b.ticks, a.tiles, b.tiles, a.roaded, a.land,
                        b.roaded, b.land);
        }
        std::fflush(stdout);
    }
    return 0;
}
