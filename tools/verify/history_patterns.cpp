// ---------------------------------------------------------------------------
// history_patterns — the five history-pattern detectors, read per seed
// (BL-1286 HISTORY_PATTERN_DETECTORS; GENERATION_STRATEGY.md § History patterns)
// ---------------------------------------------------------------------------
// Builds each seed's shipped world and reads, off the recorded history alone
// (the era_timelapse records + the post-sim settlement), whether each of the
// five patterns was hit. THE DETECTORS ARE THE SRC MODULE'S
// (src/world/history_pattern_detectors.{hpp,cpp}, BL-1286) — this harness calls them,
// so the gate, the report's pattern tags and the nation card read one
// implementation. The readings, and which are ruled and which provisional, are
// documented there:
//
//   RF  The rise and fall (Empires, always): the front-runner at 200 BCE peaks
//       by 400 CE at >= the floor and < the 500 per mille cap, then halves
//       within 300 years.
//   LC  The late culture (Culture -> Exploration, always): the most remote
//       founding culture at the Culture close, lagging, bound in Exploration.
//   WW  The world war (Industrialisation, always): 1900-1930, the top powers
//       drawn in.
//   CW  Civil war (Empires, lean): a successor with >= a quarter of what its
//       parent kept (3+ regions), the two then fighting within 50 years.
//   RV  Revolution (Industrialisation, lean): a recorded takeover — no
//       revolution event exists yet, so 0 by reading; the old successor-takes-
//       parent reading is printed beside it as a proxy, never a hit.
//
// Per seed it also prints the pooled population at 1200 (BL-1290's movement
// reading), the nations carrying each pattern tag on the generation report, and
// checks the report's tags against the module's fold re-run here (FAIL, exit 1,
// if they disagree). Beside them, BL-1278's measure-first: the natural nation
// count, the living polities at the last close, and how many nations hold a
// specialist (on a budget world, the PLANNED charters the carve ledger records).
//
// Otherwise a census: a pattern missing on a seed prints miss, not FAIL (the
// gates are BL-1287/1288/1289's, built on these).
//
//   bash tools/verify/build_lua_harness.sh history_patterns
//   build_gen/verify/history_patterns.exe [--seeds 0,11,46] [--csv out.csv]

#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/era_minus_one.hpp"
#include "world/era_timelapse.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_pattern_detectors.hpp"
#include "world/world.hpp"
#include "world/works_roster.hpp"
#include "world/world_gen_config.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
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

std::vector<uint32_t> library_seeds(const char* path)
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

struct row
{
    uint32_t seed = 0;
    double   build_s = 0;
    history_pattern_reading pr;   ///< the src module's reading (world/history_pattern_detectors.hpp)
    int64_t pop_1200 = 0;         ///< pooled population at the Empires close (BL-1290's movement reading)
    int tagged[static_cast<int>(history_pattern::count)] = {0}; ///< nations carrying each tag
    int unfolded = 0;             ///< tags that reached no nation (a realm ended before the fold)
    bool tags_agree = true;       ///< the report's tags == the module's fold, re-run here
    // BL-1278
    int nations = 0, nations_with_specialist = 0, specialists = 0, living_polities = 0;
};

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    const char* csv = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            std::stringstream ss(argv[++i]);
            std::string tok;
            while (std::getline(ss, tok, ',')) if (!tok.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
        }
        else if (std::strcmp(argv[i], "--csv") == 0 && i + 1 < argc) csv = argv[++i];
    }
    if (seeds.empty()) seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty()) { std::printf("FATAL  no seeds (docs/generation/seed_library.json)\n"); return 1; }

    // The shipped data layer, in app order (app::ensure_works_loaded): a build
    // without the works table runs Industrialisation with no scale credit.
    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);

    std::vector<row> rows;
    for (const uint32_t seed : seeds)
    {
        world_params p = arc_params(world_arc::shipped);
        p.seed = seed;
        generation_report     rep{};
        era_minus_one_fixture fx{};
        const auto t0 = std::chrono::steady_clock::now();
        world w = make_hard_coded_world(p, &rep, cfg, nullptr, &works, &fx);
        row r;
        r.seed    = seed;
        r.build_s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();

        const generation_report::body_entry* home = nullptr;
        for (const generation_report::body_entry& b : rep.bodies) if (b.is_homeworld) { home = &b; break; }
        if (home == nullptr) { std::printf("seed %u: no homeworld entry\n", seed); continue; }

        const era_timelapse& emp = home->prehistory_timelapse;
        const era_timelapse& exp = home->exploration_timelapse;
        const era_timelapse& ind = home->industrialisation_timelapse;
        std::printf("seed %u records: empires %zu steps %zu events, exploration %zu/%zu, industrialisation %zu/%zu\n",
                    seed, emp.steps.size(), emp.events.size(), exp.steps.size(), exp.events.size(),
                    ind.steps.size(), ind.events.size());
        // THE ONE IMPLEMENTATION (BL-1286): the src module's detectors, over
        // the same records the report carries and the nation card reads.
        r.pr = detect_history_patterns(emp, exp, ind, home->settlement, home_grid_width); // the settlement grid (fill_history_pattern_tags reads the same)
        if (!emp.steps.empty())
        {
            const timelapse_step& last = emp.steps.back();
            for (int i = 0; i < last.sample_count; ++i)
                r.pop_1200 += emp.samples[static_cast<std::size_t>(last.first_sample + i)].population;
        }
        // The tags as the report carries them, against the module's fold re-run here.
        const std::vector<uint8_t> refold = fold_history_pattern_tags(
            r.pr, emp, exp, ind, home->nation_polity, home->nation_absorbed_first, home->nation_absorbed_count,
            home->nation_absorbed, &r.unfolded);
        r.tags_agree = refold == home->nation_pattern_tags;
        for (const uint8_t m : home->nation_pattern_tags)
            for (int b = 0; b < static_cast<int>(history_pattern::count); ++b)
                r.tagged[b] += (m >> b) & 1;

        // BL-1278's measure-first.
        // A budget world charters its specialists in finish_campaign_world,
        // after this build; the carve ledger carries the PLAN it will spend
        // (plan_charters_by_nation). A roster world has them on the world now.
        std::set<entity_id> with_spec;
        for (const auto& kv : w.nations) { (void)kv; ++r.nations; }
        if (rep.carve_competitors_from_budget)
        {
            for (const auto& c : rep.carve_competitors)
                if (c.planned_specialists > 0) { r.specialists += static_cast<int>(c.planned_specialists); with_spec.insert(c.nation); }
        }
        else
            for (const auto& kv : w.corporations)
                if (!kv.second.is_background && kv.second.home_nation != null_entity)
                { ++r.specialists; with_spec.insert(kv.second.home_nation); }
        r.nations_with_specialist = static_cast<int>(with_spec.size());
        const era_timelapse& last = !ind.steps.empty() ? ind : (!exp.steps.empty() ? exp : emp);
        if (!last.steps.empty()) r.living_polities = last.steps.back().sample_count;

        const history_pattern_reading& q = r.pr;
        std::printf("seed %-3u (%.0f s)  RF %s realm %d%s front %d‰ peak %d‰ at %d x%.2f, %d seats, fall %dy to %d‰, %d split%s%s, levels %d/%d  |"
                    "  LC %s culture %d gap %d (next %d) lag %d%% bound %d  |"
                    "  WW %s %d/%d in %d-%d, %d moments  |  CW %d/%d  RV %d (proxy %d)  |"
                    "  pop@1200 %lld  |  tags RF %d LC %d WW %d CW %d RV %d, unfolded %d%s  |"
                    "  nations %d, %d w/ specialist, polities %d\n",
                    seed, r.build_s, q.rf_hit ? "HIT " : "miss", q.rf_polity,
                    q.rf_boxed >= 0 ? (" (boxed " + std::to_string(q.rf_boxed) + " gave way)").c_str() : "",
                    q.rf_front_q, q.rf_peak_q, q.rf_peak_year, q.rf_margin_x100 / 100.0, q.rf_captures,
                    q.rf_fall_years, q.rf_fall_q, q.rf_breakaways, q.rf_breakaways == 1 ? "" : "s",
                    q.rf_ended ? " ended" : "", q.rf_rise_level, q.rf_fall_level,
                    q.lc_hit ? "HIT " : "miss", q.lc_culture, q.lc_gap, q.lc_second_gap, q.lc_lag_x100, q.lc_bound,
                    q.ww_hit ? "HIT " : "miss", q.ww_belligerents, hp_ww_top_k, hp_ww_window_start, hp_ww_window_end,
                    q.ww_moments, q.cw_count, q.cw_splits, q.rv_count, q.rv_proxy,
                    static_cast<long long>(r.pop_1200),
                    r.tagged[0], r.tagged[1], r.tagged[2], r.tagged[3], r.tagged[4], r.unfolded,
                    r.tags_agree ? "" : "  TAGS DISAGREE",
                    r.nations, r.nations_with_specialist, r.living_polities);
        if (rep.handoff_invalid)
            std::printf("seed %u: HANDOFF VIOLATION %s\n", seed, rep.handoff_violation.c_str());
        std::fflush(stdout);
        rows.push_back(r);
    }

    int rf = 0, lc = 0, ww = 0, cw = 0, rv = 0, hegemon = 0, disagree = 0;
    long long pop = 0;
    for (const row& r : rows)
    {
        rf += r.pr.rf_hit; lc += r.pr.lc_hit; ww += r.pr.ww_hit; cw += r.pr.cw_count > 0; rv += r.pr.rv_count > 0;
        hegemon += r.pr.rf_peak_q >= hp_rf_hegemony_q;
        disagree += !r.tags_agree;
        pop += static_cast<long long>(r.pop_1200);
    }
    const int n = static_cast<int>(rows.size());
    std::printf("\nhistory_patterns over %d seeds (world/history_pattern_detectors.hpp readings):\n", n);
    std::printf("  rise and fall  %2d/%d   (targets: always)   %d seeds peaked at hegemony\n", rf, n, hegemon);
    std::printf("  late culture   %2d/%d   (targets: always)\n", lc, n);
    std::printf("  world war      %2d/%d   (targets: always)\n", ww, n);
    std::printf("  civil war      %2d/%d   (lean)\n", cw, n);
    std::printf("  revolution     %2d/%d   (lean)\n", rv, n);
    std::printf("pooled population at 1200: %lld\n", pop);
    if (disagree > 0)
        std::printf("FAIL  %d seed(s): the report's pattern tags disagree with the module's fold\n", disagree);

    if (csv != nullptr)
    {
        std::ofstream o(csv);
        o << "seed,rf_hit,rf_polity,rf_front_q,rf_peak_q,rf_peak_year,rf_fall_years,rf_breakaways,"
             "lc_hit,lc_culture,lc_gap,lc_second_gap,lc_lag_x100,lc_bound,"
             "ww_hit,ww_belligerents,ww_moments,cw_count,cw_splits,rv_count,rv_proxy,pop_1200,unfolded,"
             "nations,nations_with_specialist,specialists,living_polities\n";
        for (const row& r : rows)
        {
            const history_pattern_reading& q = r.pr;
            o << r.seed << ',' << q.rf_hit << ',' << q.rf_polity << ',' << q.rf_front_q << ',' << q.rf_peak_q << ','
              << q.rf_peak_year << ',' << q.rf_fall_years << ',' << q.rf_breakaways << ','
              << q.lc_hit << ',' << q.lc_culture << ',' << q.lc_gap << ',' << q.lc_second_gap << ','
              << q.lc_lag_x100 << ',' << q.lc_bound << ','
              << q.ww_hit << ',' << q.ww_belligerents << ',' << q.ww_moments << ',' << q.cw_count << ','
              << q.cw_splits << ',' << q.rv_count << ',' << q.rv_proxy << ',' << r.pop_1200 << ',' << r.unfolded << ','
              << r.nations << ',' << r.nations_with_specialist << ',' << r.specialists << ',' << r.living_polities << '\n';
        }
    }
    return disagree == 0 ? 0 : 1;
}
