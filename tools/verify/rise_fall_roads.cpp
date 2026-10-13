// ---------------------------------------------------------------------------
// rise_fall_roads — does the rise and fall's ruin outlive its span? (BL-1287)
// ---------------------------------------------------------------------------
// The fall acts on the map (CIVILISATION.md sec How an empire actually falls,
// Ben 2026-10-12/13): the falling realm's roads and works decay on the ground
// and stay decayed for whoever holds it next. This instrument reads, per seed,
// how much of that ruin is still standing at the 1960 world, and where the
// campaign's input-starved processors stand relative to the fallen realm's old
// ground. It changes nothing; it reads generation's own fixture.
//
//   R  the realm that rose (the detector's reading), its peak, and its ground
//      AT THE PEAK (the Empires record's owner slice that year).
//   C  the corridor record at 1200 (`pre_exploration_corridors`, what
//      Exploration resumes) and at 1960 (`setup_corridors`, what the campaign
//      world's roads are stamped from), split into corridors touching the
//      fallen ground and the rest: the tier mix, and how many sit BELOW the
//      rung their own traffic earned (tier < the tier `uses` alone reaches --
//      the decay's mark; a refused promotion can also leave one rung short,
//      which the "rest" column reads as the base rate).
//   W  `region::work_reach_mod` (the works' reach relief) at 1200 and at 1960
//      on the fallen ground and elsewhere: mean, and regions holding any.
//   A  the counterfactual at 1200 only: the Empires span re-run from the
//      fixture with the fall's two decays switched off (`fall_road_decay_q`,
//      `fall_works_decay_per_year_q` = 0). The history diverges from there, so
//      this is a reading of the decay's own size, never a matched world.
//   P  with --processors FILE (market_viability --dump-processors at the
//      handoff): every processor placed on its nearest history region (the
//      1960 settlement, `region_distance`, ties to the lower index), and the
//      starved share on the fallen ground against the rest.
//
//   bash tools/verify/build_lua_harness.sh rise_fall_roads
//   build_gen/verify/market_viability.exe --seeds 13 --dump-processors build_gen/verify/out/procs13.txt
//   build_gen/verify/rise_fall_roads.exe --seeds 13 --processors build_gen/verify/out/procs13.txt

#include "harness_params.hpp"
#include "world/era_minus_one.hpp"
#include "world/era_timelapse.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_pattern_detectors.hpp"
#include "world/history_sim.hpp"
#include "world/world.hpp"
#include "scripting/lua_state.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

int rung_of_uses(int uses, const history_sim_params& p)
{
    if (uses >= p.road_tier2_uses) return 2; // tier 3 is bought, never walked
    if (uses >= p.road_tier1_uses) return 1;
    return 0;
}

struct corridor_split
{
    int n[2] = {0, 0};          ///< [0] touching fallen ground, [1] the rest
    int tier[2][4] = {};
    int below[2] = {0, 0};
};

corridor_split split_corridors(const std::vector<history_corridor>& cs, const std::vector<char>& fallen,
                               const history_sim_params& p)
{
    corridor_split s;
    for (const history_corridor& c : cs)
    {
        const bool f = (c.a < fallen.size() && fallen[c.a]) || (c.b < fallen.size() && fallen[c.b]);
        const int k = f ? 0 : 1;
        ++s.n[k];
        ++s.tier[k][c.tier < 4 ? c.tier : 3];
        if (c.tier < rung_of_uses(c.uses, p)) ++s.below[k];
    }
    return s;
}

void print_split(const char* label, const corridor_split& s)
{
    for (int k = 0; k < 2; ++k)
        std::printf("    %-5s %-7s corridors %4d  tier0 %4d tier1 %4d tier2 %4d tier3 %3d  below their traffic rung %4d (%.1f%%)\n",
                    label, k == 0 ? "fallen" : "rest", s.n[k], s.tier[k][0], s.tier[k][1], s.tier[k][2], s.tier[k][3],
                    s.below[k], s.n[k] ? 100.0 * s.below[k] / s.n[k] : 0.0);
}

void print_works(const char* label, const std::vector<region>& rs, const std::vector<char>& fallen)
{
    int64_t sum[2] = {0, 0};
    int n[2] = {0, 0}, any[2] = {0, 0};
    for (std::size_t i = 0; i < rs.size(); ++i)
    {
        const int k = (i < fallen.size() && fallen[i]) ? 0 : 1;
        ++n[k];
        sum[k] += rs[i].work_reach_mod;
        if (rs[i].work_reach_mod > 0) ++any[k];
    }
    for (int k = 0; k < 2; ++k)
        std::printf("    %-5s %-7s regions %4d  work_reach_mod mean %6.1f  holding any %4d\n",
                    label, k == 0 ? "fallen" : "rest", n[k], n[k] ? static_cast<double>(sum[k]) / n[k] : 0.0, any[k]);
}

std::vector<uint32_t> parse_list(const char* s)
{
    std::vector<uint32_t> out;
    std::stringstream ss(s);
    std::string tok;
    while (std::getline(ss, tok, ',')) if (!tok.empty()) out.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    const char* procs = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_list(argv[++i]);
        else if (!std::strcmp(argv[i], "--processors") && i + 1 < argc) procs = argv[++i];
    }
    if (seeds.empty()) { std::printf("usage: rise_fall_roads --seeds a,b [--processors FILE]\n"); return 1; }

    for (const uint32_t seed : seeds)
    {
        // The app's generation inputs (market_viability's world, history and all).
        lua_state lua;
        world_gen_config cfg{};
        works_registry works;
        load_app_generation_inputs(lua, cfg, works);
        world_params p;
        p.seed = seed;
        generation_report rep{};
        era_minus_one_fixture fx{};
        const world w = make_hard_coded_world(p, &rep, cfg, nullptr, &works, &fx);
        (void)w;
        const generation_report::body_entry* home = nullptr;
        for (const auto& b : rep.bodies) if (b.is_homeworld) { home = &b; break; }
        if (home == nullptr || !fx.ran) { std::printf("seed %u: no history\n", seed); continue; }

        // R -- the realm and its ground at the peak.
        const history_pattern_reading pr = detect_history_patterns(home->prehistory_timelapse, home->exploration_timelapse,
                                                                   home->industrialisation_timelapse, home->settlement, fx.gw);
        const std::vector<uint16_t> slice = owner_slice_at(home->prehistory_timelapse, pr.rf_peak_year);
        std::vector<char> fallen(std::max<std::size_t>(slice.size(), home->settlement.regions.size()), 0);
        int fallen_n = 0;
        for (std::size_t i = 0; i < slice.size(); ++i)
            if (pr.rf_polity >= 0 && slice[i] == static_cast<uint16_t>(pr.rf_polity)) { fallen[i] = 1; ++fallen_n; }
        std::printf("seed %u: RF %s realm %d peak %d per mille in %d, fall %d y, %d regions at the peak\n",
                    seed, pr.rf_hit ? "HIT" : "miss", pr.rf_polity, pr.rf_peak_q, pr.rf_peak_year, pr.rf_fall_years, fallen_n);

        // C -- the corridor record at 1200 and at 1960.
        std::printf("  C corridors (fallen = an end on the realm's peak ground)\n");
        print_split("1200", split_corridors(fx.pre_exploration_corridors, fallen, fx.params));
        print_split("1960", split_corridors(fx.setup_corridors, fallen, fx.params));
        // The same corridors, followed: of those below their rung at 1200 on the
        // fallen ground, how many are still below their (new) rung at 1960.
        {
            std::map<std::pair<int, int>, const history_corridor*> at1960;
            for (const history_corridor& c : fx.setup_corridors) at1960[{c.a, c.b}] = &c;
            int ruined = 0, still = 0, gone = 0, risen = 0;
            for (const history_corridor& c : fx.pre_exploration_corridors)
            {
                const bool f = (c.a < fallen.size() && fallen[c.a]) || (c.b < fallen.size() && fallen[c.b]);
                if (!f || c.tier >= rung_of_uses(c.uses, fx.params)) continue;
                ++ruined;
                const auto it = at1960.find({c.a, c.b});
                if (it == at1960.end()) { ++gone; continue; }
                if (it->second->tier > c.tier) ++risen;
                if (it->second->tier < rung_of_uses(it->second->uses, fx.params)) ++still;
            }
            std::printf("    fallen corridors below their rung at 1200: %d -> at 1960: still below %d, re-walked above their 1200 tier %d, dropped from the record %d\n",
                        ruined, still, risen, gone);
        }

        // W -- the works' reach relief at 1200 and 1960.
        std::printf("  W works reach relief\n");
        print_works("1200", fx.pre_exploration_settlement.regions, fallen);
        print_works("1960", home->settlement.regions, fallen);

        // A -- the Empires span without the fall's decays (a divergent history).
        {
            settlement_state ss = fx.settlement;
            history_sim_params hp = fx.params;
            hp.fall_road_decay_q = 0;
            hp.fall_works_decay_per_year_q = 0;
            const history_sim_state hs = run_history_sim(ss, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh, hp, fx.seed,
                                                         nullptr, fx.works, nullptr);
            std::vector<char> f2(ss.regions.size(), 0);
            const std::vector<uint16_t> s2 = owner_slice_at(hs, hs.rise_fall_peak_year);
            for (std::size_t i = 0; i < s2.size() && i < f2.size(); ++i)
                if (hs.rise_fall_realm >= 0 && s2[i] == static_cast<uint16_t>(hs.rise_fall_realm)) f2[i] = 1;
            std::printf("  A without the decays (divergent history): realm %d peak %d in %d, fall year %d, missed %d (%s)\n",
                        hs.rise_fall_realm, hs.rise_fall_peak_pm, hs.rise_fall_peak_year, hs.rise_fall_fall_year,
                        hs.rise_fall_missed ? 1 : 0, hs.rise_fall_why.c_str());
            print_split("1200", split_corridors(hs.supply_corridors, f2, hp));
            print_works("1200", ss.regions, f2);
        }

        // P -- the processors on the history's regions.
        if (procs != nullptr)
        {
            std::ifstream in(procs);
            uint32_t s; int x, y, starved, good; std::string state;
            int tot[2] = {0, 0}, st[2] = {0, 0};
            std::map<int, int> starved_good[2];
            const std::vector<region>& rs = home->settlement.regions;
            while (in >> s >> x >> y >> state >> starved >> good)
            {
                if (s != seed) continue;
                region probe; probe.col = x; probe.row = y;
                int best = -1, best_d = 1 << 30;
                for (std::size_t i = 0; i < rs.size(); ++i)
                {
                    const int d = region_distance(probe, rs[i], fx.gw);
                    if (d < best_d) { best_d = d; best = static_cast<int>(i); }
                }
                const int k = (best >= 0 && static_cast<std::size_t>(best) < fallen.size() && fallen[static_cast<std::size_t>(best)]) ? 0 : 1;
                ++tot[k];
                if (starved) { ++st[k]; ++starved_good[k][good]; }
            }
            std::printf("  P processors at the handoff on the history's regions\n");
            for (int k = 0; k < 2; ++k)
            {
                std::printf("    %-7s processors %4d  starved %4d (%.1f%%)  by input:", k == 0 ? "fallen" : "rest", tot[k], st[k],
                            tot[k] ? 100.0 * st[k] / tot[k] : 0.0);
                for (const auto& kv : starved_good[k]) std::printf(" #%d x%d", kv.first, kv.second);
                std::printf("\n");
            }
        }
        std::fflush(stdout);
    }
    return 0;
}
