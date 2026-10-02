// ---------------------------------------------------------------------------
// centre_decline_trace — BL-1163 (play villages decline), the diagnosis
// instrument
// ---------------------------------------------------------------------------
// On the shipped start (`build_app_start_world`), run the settle and then play
// ticks, and every --every ticks print, per settled body, the inputs the growth
// pass (economy_system.cpp, BL-048B/BL-078/BL-616) gates a centre on:
//
//   conditions_met = body habitability >= 0.5  AND  met_ratio >= threshold
//
// where met_ratio is the growth-basket-weighted mean of min(1, supply/demand)
// over the body's markets, read from the markets the LAST clear left (exactly
// what the next tick's growth pass reads). Alongside: centres, heads (k),
// centres on a negative streak, and each basket good's supply/demand.
//
// Usage (repo root): centre_decline_trace [--seeds a,b] [--ticks N] [--every K]
// Lua harness: bash tools/verify/build_lua_harness.sh centre_decline_trace
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/resource_names.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

void sample(const world& w, const recipe_registry& reg, int tick, const economy_report* rep = nullptr)
{
    if (rep)
    {
        int rows = 0, active = 0, idle = 0, starved = 0, ext = 0, proc = 0;
        float eff = 0, grant = 0; int ng = 0, contended = 0;
        for (const building_report& b : rep->buildings)
        {
            ++rows; if (b.active) ++active; if (b.idle) ++idle;
            if (b.type == building_type::extraction_site) ++ext; else ++proc;
            eff += b.effective_workforce;
        }
        for (const auto& [bid, g] : rep->building_labour) { (void)bid; grant += g; ++ng; if (g < 0.99f) ++contended; }
        int live = 0, decom = 0, building = 0;
        for (const auto& [bid, b] : w.buildings) { (void)bid; if (b.decommissioned) ++decom; else if (b.ticks_remaining > 0) ++building; else ++live; }
        std::printf("  t%4d econ | corps %3zu | buildings live %5d under-construction %4d decommissioned %5d | rows %5d (ext %d proc %d) active %5d idle %5d | eff-workforce %.0f | grant mean %.3f, %d below 0.99\n",
                    tick, w.corporations.size(), live, building, decom, rows, ext, proc, active, idle, eff,
                    ng ? grant / ng : 0.0f, contended);
        (void)starved;
    }
    const growth_params& gp = reg.growth();
    std::map<entity_id, std::array<float, resource_count>> sup, dem;
    std::vector<entity_id> mids;
    for (const auto& kv : w.markets) mids.push_back(kv.first);
    std::sort(mids.begin(), mids.end());
    for (const entity_id m : mids)
    {
        const market_component& mc = w.markets.at(m);
        auto& s = sup[mc.body];
        auto& d = dem[mc.body];
        for (std::size_t r = 0; r < resource_count; ++r) { s[r] += mc.supply[r]; d[r] += mc.demand[r]; }
    }
    struct acc { int n = 0, pop = 0, neg = 0, v = 0; float hs = 0, hw = 0; };
    std::map<entity_id, acc> by_body;
    std::vector<entity_id> cids;
    for (const auto& kv : w.population_centres) cids.push_back(kv.first);
    std::sort(cids.begin(), cids.end());
    for (const entity_id c : cids)
    {
        const auto& pc = w.population_centres.at(c);
        const auto ti = w.population_centre_tile.find(c);
        if (ti == w.population_centre_tile.end()) continue;
        const auto tc = w.tiles.find(ti->second);
        if (tc == w.tiles.end()) continue;
        acc& a = by_body[tc->second.body];
        if (pc.razed) continue;
        ++a.n; a.pop += pc.population;
        if (pc.growth_accumulator < 0) ++a.neg;
        if (pc.scale == 1) ++a.v;
        const float ws = static_cast<float>(std::max(1, pc.scale));
        a.hs += pc.habitability * ws; a.hw += ws;
    }
    for (const auto& [body, a] : by_body)
    {
        float met = 1.0f, ma = 0, mw = 0;
        std::string goods;
        const auto di = dem.find(body);
        for (std::size_t r = 0; r < resource_count; ++r)
        {
            const float bw = gp.demand_basket[r];
            if (bw <= 0.0f) continue;
            const float s = (di != dem.end()) ? sup[body][r] : 0.0f;
            const float d = (di != dem.end()) ? di->second[r] : 0.0f;
            char buf[96];
            std::snprintf(buf, sizeof buf, " %s %.0f/%.0f", resource_names::name_of(static_cast<resource_type>(r)).c_str(), s, d);
            goods += buf;
            if (d <= 0.0f) continue;
            ma += bw * std::min(1.0f, s / d); mw += bw;
        }
        if (mw > 0) met = ma / mw;
        std::printf("  t%4d body %llu | centres %4d villages %4d heads %7dk declining %4d | hab %.3f met %.3f |%s\n",
                    tick, static_cast<unsigned long long>(body), a.n, a.v, a.pop, a.neg,
                    a.hw > 0 ? a.hs / a.hw : 1.0f, met, goods.c_str());
    }
    std::fflush(stdout);
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 440, every = 20;
    std::vector<uint32_t> seeds = {0};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--every") && i + 1 < argc) every = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        {
            seeds.clear();
            std::stringstream ss(argv[++i]);
            std::string t;
            while (std::getline(ss, t, ',')) if (!t.empty()) seeds.push_back(std::strtoul(t.c_str(), nullptr, 10));
        }
    }
    std::printf("centre_decline_trace — BL-1163; threshold read from registry\n");
    for (const uint32_t seed : seeds)
    {
        lua_state lua;
        world_params p;
        p.seed = seed;
        auto start = std::make_unique<app_start_world>();
        build_app_start_world(lua, p, *start);
        world& w = start->w;
        const recipe_registry& reg = start->reg;
        std::printf("seed %u (threshold %.2f)\n", seed, reg.growth().growth_met_threshold);
        sample(w, reg, -1);
        run_settle(w, reg, k_campaign_settle_ticks);
        sample(w, reg, k_campaign_settle_ticks);
        for (int i = 0; i < ticks; ++i)
        {
            const int step = k_campaign_settle_ticks + i;
            const settle_tick_result res = run_settle_tick(w, reg, step, step, /*spectating=*/false);
            if ((step + 1) % every == 0) sample(w, reg, step + 1, &res.report);
        }
    }
    return 0;
}
