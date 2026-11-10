// ---------------------------------------------------------------------------
// population_growth_census — POPULATION_GROWTH_RATE (Ben, 2026-11-10), the
// calibration instrument for POPULATION.md § Growth, decline and razing
// ("Growth is a rate, anchored on Earth's history").
// ---------------------------------------------------------------------------
// On the shipped start (`build_app_start_world`), run the settle, then N play
// ticks (quarters, default 40 = ten years), and print per seed:
//
//   - the pooled world headcount at the handoff and at the end, and its
//     annual growth (compound, over N/4 years);
//   - the same split by the owning nation's qualification fraction at the
//     handoff (buckets <0.2, 0.2-0.5, 0.5-0.8, >=0.8), heads-weighted;
//   - the mean share of live centres on a FAILING streak (growth_accumulator
//     < 0: below the staples or habitability floor) and the mean share whose
//     headcount fell this tick;
//   - promotions (a centre's scale rising) over the run.
//
// Every figure is printed with its absolute numerators on a `ROW` line so a
// parallel run (par_seeds.sh) pools them by summing, never by averaging rates.
// It reads only world state, so it builds and runs unchanged on the tree
// before the growth-rate rule and after it — that is what makes it a
// before/after instrument.
//
// Usage (repo root): population_growth_census [--seeds a,b] [--ticks N]
// Lua harness: bash tools/verify/build_lua_harness.sh population_growth_census
// ---------------------------------------------------------------------------

#include "scripting/lua_state.hpp"
#include "harness_params.hpp"
#include "world/campaign_settle.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int k_buckets = 4;
const char* const k_bucket_name[k_buckets] = {"q<0.2", "q0.2-0.5", "q0.5-0.8", "q>=0.8"};

int bucket_of(float q)
{
    if (q < 0.2f) return 0;
    if (q < 0.5f) return 1;
    if (q < 0.8f) return 2;
    return 3;
}

entity_id nation_of_centre(const world& w, entity_id cid)
{
    const auto ti = w.population_centre_tile.find(cid);
    if (ti == w.population_centre_tile.end())
        return null_entity;
    const auto tn = w.tile_to_nation.find(ti->second);
    return (tn != w.tile_to_nation.end()) ? tn->second : null_entity;
}

double cagr(double p0, double p1, double years)
{
    if (!(p0 > 0.0) || !(p1 > 0.0) || !(years > 0.0))
        return 0.0;
    return std::pow(p1 / p0, 1.0 / years) - 1.0;
}

} // namespace

int main(int argc, char** argv)
{
    int ticks = 40;
    std::vector<uint32_t> seeds = {0};
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--ticks") && i + 1 < argc) ticks = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc)
        {
            seeds.clear();
            std::stringstream ss(argv[++i]);
            std::string t;
            while (std::getline(ss, t, ',')) if (!t.empty()) seeds.push_back(std::strtoul(t.c_str(), nullptr, 10));
        }
    }
    const double years = ticks / 4.0;
    std::printf("population_growth_census — %d play quarters (%.1f years) after the settle\n", ticks, years);
    for (const uint32_t seed : seeds)
    {
        lua_state lua;
        world_params p;
        p.seed = seed;
        auto start = std::make_unique<app_start_world>();
        build_app_start_world(lua, p, *start);
        world& w = start->w;
        const recipe_registry& reg = start->reg;
        run_settle(w, reg, k_campaign_settle_ticks);

        // The handoff: each nation's qualification fixes its bucket for the run.
        std::map<entity_id, int> nation_bucket;
        std::vector<entity_id> nids;
        for (const auto& [nid, nc] : w.nations) { (void)nc; nids.push_back(nid); }
        std::sort(nids.begin(), nids.end());
        int nations_in[k_buckets] = {};
        for (const entity_id nid : nids)
        {
            const int b = bucket_of(w.nations.at(nid).qualification);
            nation_bucket[nid] = b;
            ++nations_in[b];
        }
        auto heads_by_bucket = [&](double out[k_buckets + 1]) {
            for (int b = 0; b <= k_buckets; ++b) out[b] = 0.0;
            for (const auto& [cid, pc] : w.population_centres)
            {
                if (pc.razed) continue;
                const entity_id n = nation_of_centre(w, cid);
                const auto it = nation_bucket.find(n);
                const int b = (it != nation_bucket.end()) ? it->second : k_buckets; // stateless
                out[b] += pc.population;
            }
        };
        double p0[k_buckets + 1], p1[k_buckets + 1];
        heads_by_bucket(p0);

        std::map<entity_id, int> scale_prev, pop_prev;
        for (const auto& [cid, pc] : w.population_centres) { scale_prev[cid] = pc.scale; pop_prev[cid] = pc.population; }
        long long promotions = 0, demotions = 0;
        long long failing_sum = 0, shrank_sum = 0, live_sum = 0;

        for (int i = 0; i < ticks; ++i)
        {
            const int step = k_campaign_settle_ticks + i;
            (void)run_settle_tick(w, reg, step, step, /*spectating=*/false);
            std::vector<entity_id> cids;
            for (const auto& [cid, pc] : w.population_centres) { (void)pc; cids.push_back(cid); }
            std::sort(cids.begin(), cids.end());
            for (const entity_id cid : cids)
            {
                const population_centre_component& pc = w.population_centres.at(cid);
                if (pc.razed) continue;
                ++live_sum;
                if (pc.growth_accumulator < 0) ++failing_sum;
                const auto sp = scale_prev.find(cid);
                if (sp != scale_prev.end())
                {
                    if (pc.scale > sp->second) promotions += pc.scale - sp->second;
                    if (pc.scale < sp->second) demotions += sp->second - pc.scale;
                }
                const auto pp = pop_prev.find(cid);
                if (pp != pop_prev.end() && pc.population < pp->second) ++shrank_sum;
                scale_prev[cid] = pc.scale;
                pop_prev[cid]   = pc.population;
            }
        }
        heads_by_bucket(p1);

        double w0 = 0.0, w1 = 0.0;
        for (int b = 0; b <= k_buckets; ++b) { w0 += p0[b]; w1 += p1[b]; }
        std::printf("seed %u: heads %.0fk -> %.0fk, world %.2f%%/yr | failing %.1f%% shrank %.1f%% of centre-ticks | promotions %lld demotions %lld (%.1f / decade)\n",
                    seed, w0, w1, 100.0 * cagr(w0, w1, years),
                    live_sum ? 100.0 * failing_sum / live_sum : 0.0,
                    live_sum ? 100.0 * shrank_sum / live_sum : 0.0,
                    promotions, demotions, promotions * 10.0 / years);
        for (int b = 0; b < k_buckets; ++b)
            std::printf("    %-9s nations %3d heads %9.0fk -> %9.0fk  %.2f%%/yr\n",
                        k_bucket_name[b], nations_in[b], p0[b], p1[b], 100.0 * cagr(p0[b], p1[b], years));
        // Machine-readable: absolute numerators only.
        std::printf("ROW seed=%u years=%.2f w0=%.0f w1=%.0f", seed, years, w0, w1);
        for (int b = 0; b < k_buckets; ++b)
            std::printf(" b%d_n=%d b%d_0=%.0f b%d_1=%.0f", b, nations_in[b], b, p0[b], b, p1[b]);
        std::printf(" live=%lld failing=%lld shrank=%lld prom=%lld dem=%lld\n",
                    live_sum, failing_sum, shrank_sum, promotions, demotions);
        std::fflush(stdout);
    }
    return 0;
}
