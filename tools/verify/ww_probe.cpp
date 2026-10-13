// ww_probe -- BL-1289 diagnostic. Builds each seed's shipped world and reads the
// world war off the Industrialisation span's own state and record: how it
// formed (spark, sides, escalations), who fought, what it cost (people by
// mechanism, regions razed, roads demoted, industry and port razed, treasury
// spent), and the world's population across the span's recorded steps, so the
// war's dip and the 1930-1960 redevelopment read side by side.
//   bash tools/verify/build_lua_harness.sh ww_probe
//   build_gen/verify/ww_probe.exe --seeds 0,28,46,12
#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/era_minus_one.hpp"
#include "world/era_timelapse.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/world.hpp"
#include "world/works_roster.hpp"
#include "world/world_gen_config.hpp"
#include "scripting/lua_state.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            std::stringstream ss(argv[++i]);
            std::string tok;
            while (std::getline(ss, tok, ','))
                if (!tok.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
        }
    if (seeds.empty()) seeds = {0, 28, 46, 12};
    lua_state lua; world_gen_config cfg{}; works_registry works;
    lua.load("scripts/world_gen.lua"); cfg.load_from_lua(lua);
    lua.load("scripts/works.lua"); works.load_from_lua(lua);
    int failures = 0;
    for (const uint32_t seed : seeds)
    {
        world_params p = arc_params(world_arc::shipped);
        p.seed = seed;
        generation_report rep{}; era_minus_one_fixture fx{};
        make_hard_coded_world(p, &rep, cfg, nullptr, &works, &fx);
        const history_sim_state& hs = fx.industrialisation_state;
        int living = 0;
        if (!hs.steps.empty()) living = hs.steps.back().sample_count;
        std::printf("seed %u: war %d-%d spark %d over region %d, %d vs %d, %d realms (%d of the top), %d escalations%s%s\n",
                    seed, hs.world_war_year, hs.world_war_peace_year, hs.world_war_spark, hs.world_war_contested,
                    hs.world_war_side_a, hs.world_war_side_b, hs.world_war_belligerents, hs.world_war_top_engaged,
                    hs.world_war_escalations, hs.world_war_missed ? "  MISSED: " : "", hs.world_war_why.c_str());
        std::printf("    regions %zu (razed %lld, front-rounds %lld), roads demoted %lld, points razed %lld scale + %lld treasury, port razed %lld q\n",
                    fx.industrialisation_handoff.regions.size(), static_cast<long long>(hs.world_war_regions_razed),
                    static_cast<long long>(hs.world_war_front_rounds), static_cast<long long>(hs.world_war_roads_demoted),
                    static_cast<long long>(hs.world_war_points_razed_scale),
                    static_cast<long long>(hs.world_war_points_razed_treasury),
                    static_cast<long long>(hs.world_war_port_razed_q));
        std::printf("    people: conscripts dead %lld, blockade famine %lld, fled %lld (no refuge %lld); treasury spent %lld; points from scale %lld treasury %lld\n",
                    static_cast<long long>(hs.world_war_conscript_dead), static_cast<long long>(hs.world_war_famine_dead),
                    static_cast<long long>(hs.world_war_fled), static_cast<long long>(hs.world_war_no_refuge),
                    static_cast<long long>(hs.world_war_treasury_spent),
                    static_cast<long long>(hs.industry_points_from_scale),
                    static_cast<long long>(hs.industry_points_from_treasury));
        std::printf("    population by step:");
        for (const timelapse_step& st : hs.steps)
        {
            if (st.year < 1860) continue;
            int64_t pop = 0, ind = 0;
            for (int i = 0; i < st.sample_count; ++i)
            {
                pop += hs.samples[static_cast<std::size_t>(st.first_sample + i)].population;
                ind += hs.samples[static_cast<std::size_t>(st.first_sample + i)].industry_points;
            }
            std::printf("  %d: %lld (ind %lld)", st.year, static_cast<long long>(pop), static_cast<long long>(ind));
        }
        std::printf("\n    living polities at close %d; handoff %s %s\n", living,
                    rep.handoff_invalid ? "INVALID" : "ok", rep.handoff_violation.c_str());
        // SELF-CHECKS: no region emptied by the war; a war hit.
        int emptied = 0;
        for (const region& r : fx.industrialisation_handoff.regions)
            if (r.domain == region_domain::land && r.nation >= 0 && r.population <= 0) ++emptied;
        if (emptied > 0) std::printf("    note: %d held land regions stand empty at the close\n", emptied);
        if (hs.world_war_missed || hs.world_war_year == 0) ++failures;
        std::fflush(stdout);
    }
    std::printf("ww_probe: %d seed(s) without a war\n", failures);
    return failures == 0 ? 0 : 1;
}
