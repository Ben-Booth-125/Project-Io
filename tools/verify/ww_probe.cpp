// ww_probe -- BL-1289 diagnostic. Builds each seed's shipped world and reads the
// world war off the Industrialisation span's own state and record: how it
// formed (spark, sides, escalations), who fought, what it cost (people by
// mechanism, regions razed, roads demoted, industry and port razed, treasury
// spent), and the world's population across the span's recorded steps, so the
// war's dip and the 1930-1960 redevelopment read side by side.
//   bash tools/verify/build_lua_harness.sh ww_probe
//   build_gen/verify/ww_probe.exe --seeds 0,28,46,12
// It also re-runs the span from the 1660 handoff with the playback on and off
// (at the shipped open and two years later) and FAILS unless the two agree.
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

        // THE TOP-POWER READ IS ON ITS OWN CLOCK (BL-1289 fix round): the span
        // re-run from generation's 1660 handoff with the playback ON and OFF,
        // at the shipped open and at an open two years later, must fight the
        // same war and close on the same people, works and chests. The ON run
        // at the shipped open must also be generation's own war.
        if (fx.industrialisation_ran)
        {
            const exploration_output& eo = fx.exploration_handoff;
            const auto rerun = [&](bool playback, int64_t open_shift, std::vector<region>& close) {
                history_sim_params dp = fx.industrialisation_params;
                dp.record_playback         = playback;
                dp.start_year             += open_shift;
                dp.industry_open_year     += open_shift;
                dp.resume_polities         = &eo.polities;
                dp.resume_grudges          = &eo.grudges;
                dp.resume_contacts         = &eo.contacts;
                dp.resume_corridors        = &eo.surviving_corridors;
                dp.resume_sea_legs         = &eo.sea_legs;
                dp.resume_dated_objects    = &eo.dated_objects;
                dp.resume_civilisations    = &eo.civilisations;
                dp.resume_universal_creeds = &eo.universal_creeds;
                settlement_state ss;
                ss.regions = fx.industrialisation_open_regions;
                creed_state cs;
                cs.cultures = eo.cultures;
                history_sim_state out = run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, dp,
                                                        fx.industrialisation_seed, nullptr, fx.works, nullptr);
                close = ss.regions;
                return out;
            };
            const auto same_war = [](const history_sim_state& a, const history_sim_state& b) {
                return a.world_war_year == b.world_war_year && a.world_war_peace_year == b.world_war_peace_year
                    && a.world_war_side_a == b.world_war_side_a && a.world_war_side_b == b.world_war_side_b
                    && a.world_war_belligerents == b.world_war_belligerents
                    && a.world_war_top_engaged == b.world_war_top_engaged
                    && a.world_war_regions_razed == b.world_war_regions_razed
                    && a.world_war_roads_demoted == b.world_war_roads_demoted
                    && a.world_war_conscript_dead == b.world_war_conscript_dead
                    && a.world_war_famine_dead == b.world_war_famine_dead && a.world_war_fled == b.world_war_fled
                    && a.world_war_treasury_spent == b.world_war_treasury_spent;
            };
            const auto same_close = [](const std::vector<region>& a, const std::vector<region>& b) {
                if (a.size() != b.size()) return false;
                for (std::size_t i = 0; i < a.size(); ++i)
                    if (a[i].population != b[i].population || a[i].industry_points != b[i].industry_points
                        || a[i].treasury != b[i].treasury || a[i].nation != b[i].nation)
                        return false;
                return true;
            };
            for (const int64_t shift : {int64_t{0}, int64_t{2}})
            {
                std::vector<region> c_on, c_off;
                const history_sim_state on  = rerun(true, shift, c_on);
                const history_sim_state off = rerun(false, shift, c_off);
                const bool agree = same_war(on, off) && same_close(c_on, c_off);
                const bool gen   = shift != 0 || same_war(on, hs);
                std::printf("    playback on/off at open +%lld: war %d-%d / %d-%d, %s%s\n",
                            static_cast<long long>(shift), on.world_war_year, on.world_war_peace_year,
                            off.world_war_year, off.world_war_peace_year, agree ? "AGREE" : "DISAGREE",
                            gen ? "" : " (the ON re-run is not generation's war)");
                if (!agree || !gen) ++failures;
            }
        }
        std::fflush(stdout);
    }
    std::printf("ww_probe: %d failure(s) (a seed without a war, or playback on/off disagreeing)\n", failures);
    return failures == 0 ? 0 : 1;
}
