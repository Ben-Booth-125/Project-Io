// ---------------------------------------------------------------------------
// industry_siting — WHERE the opening industry stands (a READING, no gate).
// ---------------------------------------------------------------------------
// docs/generation/CORPORATION_GENERATION.md § Pass 3 (Ben, 2026-10-26): processing
// and trade charters anchor at the capital, then the market centres by chain
// distance; extractors rank deposits by richness x Logistic Point cover; buildings
// stack on every tile up to its cap, the cluster filling a tile before the next.
//
// On the SHIPPED start (harness_params.hpp's app mirror), per seed:
//   PROC   processors: total, and the share within N grid steps (wrapped
//          Chebyshev) of their own nation's capital, of any of their nation's
//          market centres (the capital's own included), and of either.
//   STACK  tiles by building count — every building, then producers only
//          (extraction sites and processors).
//   COVER  the mean Logistic Point coverage of the extraction sites and of the
//          processors at the opening resolve, and how many read below full.
//
// Build: cmd //c tools\verify\build_lua_harness.bat industry_siting
// Run:   build_gen/verify/industry_siting.exe [--n N] [seed ...]   (default: 0; N 6)

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/logistic_points.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <vector>

namespace {

int wrapped_cheb(const world& w, entity_id a, entity_id b)
{
    const tile_component& ta = w.tiles.at(a);
    const tile_component& tb = w.tiles.at(b);
    if (ta.body != tb.body)
        return 1 << 30;
    const int gw = w.bodies.at(ta.body).grid_width;
    int dx = std::abs(ta.grid_x - tb.grid_x);
    if (gw > 0) dx = std::min(dx, gw - dx);
    return std::max(dx, std::abs(ta.grid_y - tb.grid_y));
}

void run_seed(lua_state& lua, std::uint32_t seed, int n)
{
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;
    app_start_world out;
    build_app_start_world(lua, params, out);
    world& w = out.w;

    // Each nation's market centres: markets whose centre tile is its ground.
    std::map<entity_id, std::vector<entity_id>> centres;
    for (const auto& [mid, mc] : w.markets)
    {
        const auto tn = w.tile_to_nation.find(mc.centre_tile);
        if (tn != w.tile_to_nation.end())
            centres[tn->second].push_back(mc.centre_tile);
    }

    int proc = 0, near_cap = 0, near_mc = 0, near_either = 0;
    std::map<entity_id, int> all_on, prod_on;
    std::vector<entity_id> ids;
    for (const auto& [bid, b] : w.buildings)
        ids.push_back(bid);
    std::sort(ids.begin(), ids.end());
    for (const entity_id bid : ids)
    {
        const building_component& b = w.buildings.at(bid);
        ++all_on[b.tile];
        const bool producer = b.type == building_type::processing_facility
                           || b.type == building_type::extraction_site;
        if (producer)
            ++prod_on[b.tile];
        if (b.type != building_type::processing_facility)
            continue;
        ++proc;
        const auto tn = w.tile_to_nation.find(b.tile);
        if (tn == w.tile_to_nation.end() || w.nations.count(tn->second) == 0)
            continue;
        const entity_id cap = w.nations.at(tn->second).capital_tile;
        const bool c = cap != null_entity && w.tiles.count(cap) != 0 && wrapped_cheb(w, b.tile, cap) <= n;
        bool m = false;
        for (const entity_id ct : centres[tn->second])
            if (wrapped_cheb(w, b.tile, ct) <= n) { m = true; break; }
        near_cap += c ? 1 : 0;
        near_mc += m ? 1 : 0;
        near_either += (c || m) ? 1 : 0;
    }
    const auto pct = [&](int k) { return proc > 0 ? 100.0 * k / proc : 0.0; };
    std::printf("PROC seed=%u n=%d processors=%d near_capital=%d (%.1f%%) near_market_centre=%d (%.1f%%) "
                "near_either=%d (%.1f%%)\n",
                static_cast<unsigned>(seed), n, proc, near_cap, pct(near_cap), near_mc, pct(near_mc),
                near_either, pct(near_either));

    const auto hist = [&](const char* label, const std::map<entity_id, int>& on) {
        std::map<int, int> h;
        for (const auto& [t, k] : on) ++h[k];
        std::printf("STACK seed=%u %s tiles=%zu", static_cast<unsigned>(seed), label, on.size());
        for (const auto& [k, c] : h) std::printf(" %d:%d", k, c);
        std::printf("\n");
    };
    hist("all", all_on);
    hist("producers", prod_on);

    // EXTR: how extraction spreads — sites, the (tile, target) deposits they
    // work, and the share standing alone on their deposit (the spread share).
    {
        std::map<std::pair<entity_id, int>, int> per_deposit;
        int sites = 0;
        for (const entity_id bid : ids)
        {
            const building_component& b = w.buildings.at(bid);
            if (b.type != building_type::extraction_site)
                continue;
            ++sites;
            ++per_deposit[{ b.tile, static_cast<int>(b.target_resource) }];
        }
        int alone = 0;
        std::map<int, int> h;
        for (const auto& [k, c] : per_deposit)
        {
            ++h[c];
            if (c == 1) ++alone;
        }
        std::printf("EXTR seed=%u sites=%d deposits=%zu alone=%d spread_share=%.1f%% stacks",
                    static_cast<unsigned>(seed), sites, per_deposit.size(), alone,
                    sites > 0 ? 100.0 * alone / sites : 0.0);
        for (const auto& [k, c] : h) std::printf(" %d:%d", k, c);
        std::printf("\n");
    }

    const lp_tick t = resolve_logistic_points(w);
    double ext_sum = 0.0, proc_sum = 0.0;
    int ext_n = 0, proc_n = 0, ext_low = 0, proc_low = 0;
    for (const lp_building_row& r : t.buildings)
    {
        const auto bit = w.buildings.find(r.building);
        if (bit == w.buildings.end())
            continue;
        if (bit->second.type == building_type::extraction_site)
        {
            ext_sum += r.coverage; ++ext_n; ext_low += r.coverage < 1.0f ? 1 : 0;
        }
        else if (bit->second.type == building_type::processing_facility)
        {
            proc_sum += r.coverage; ++proc_n; proc_low += r.coverage < 1.0f ? 1 : 0;
        }
    }
    std::printf("COVER seed=%u extraction=%d mean=%.3f below_full=%d processors=%d mean=%.3f below_full=%d\n",
                static_cast<unsigned>(seed), ext_n, ext_n ? ext_sum / ext_n : 0.0, ext_low, proc_n,
                proc_n ? proc_sum / proc_n : 0.0, proc_low);
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    int n = 6;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--n") == 0 && i + 1 < argc)
        {
            n = std::atoi(argv[++i]);
            continue;
        }
        seeds.push_back(static_cast<std::uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    }
    if (seeds.empty())
        seeds.push_back(0);
    lua_state lua;
    for (const std::uint32_t s : seeds)
        run_seed(lua, s, n);
    return 0;
}
