// ---------------------------------------------------------------------------
// civil_war_probe — the civil-war famine and flight, measured (BL-1290)
// ---------------------------------------------------------------------------
// MILITARY_HISTORY.md sec Armies are distinct from population (Ben,
// 2026-10-12): while a region is fought over in a civil war it loses a share of
// its people to famine and a further share flees to safe regions of the same
// people; both bounded per year so no region is emptied, both recorded.
//
// Per seed, builds the shipped world with the Era -1 fixture, then REPLAYS the
// Empires span from the fixture twice:
//   ON   the params generation ran on — must reproduce the recorded Empires
//        events exactly (replay == generation), and reports the mechanism's
//        tallies (halves recognised, strikes, famine dead, fled, no refuge);
//   OFF  the same params with the famine and flight at 0 — the pre-BL-1290
//        Empires span, so the pooled population at 1200 can be read before
//        and after on the same ground.
// Checks (FAIL lines, exit 1): the replay reproduces the record; every
// civil_war_flight names a valid destination region; no held land region the
// mechanism struck stands EMPTY at the close; famine and flight never both
// zero where a strike was counted with people on the ground.
//
//   bash tools/verify/build_lua_harness.sh civil_war_probe
//   build_gen/verify/civil_war_probe.exe [--seeds 0,11,46]

#include "harness_params.hpp"
#include "world/era_minus_one.hpp"
#include "world/era_timelapse.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/world.hpp"
#include "world/works_roster.hpp"
#include "world/world_gen_config.hpp"
#include "scripting/lua_state.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
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

bool same_events(const std::vector<lapse_event>& a, const std::vector<lapse_event>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].year != b[i].year || a[i].kind != b[i].kind || a[i].region != b[i].region
            || a[i].polity != b[i].polity || a[i].other != b[i].other) return false;
    return true;
}

int living(const history_sim_state& s)
{
    int n = 0;
    for (const polity& q : s.polities) n += q.alive ? 1 : 0;
    return n;
}

int64_t pooled(const std::vector<region>& rs)
{
    int64_t t = 0;
    for (const region& r : rs) t += r.population;
    return t;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            std::stringstream ss(argv[++i]);
            std::string tok;
            while (std::getline(ss, tok, ',')) if (!tok.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
        }
    if (seeds.empty()) seeds = library_seeds("docs/generation/seed_library.json");
    if (seeds.empty()) { std::printf("FATAL  no seeds\n"); return 1; }

    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);

    int fails = 0;
    int64_t sum_on = 0, sum_off = 0, sum_dead = 0, sum_fled = 0;
    for (const uint32_t seed : seeds)
    {
        world_params p = arc_params(world_arc::shipped);
        p.seed = seed;
        generation_report     rep{};
        era_minus_one_fixture fx{};
        world w = make_hard_coded_world(p, &rep, cfg, nullptr, &works, &fx);
        (void)w;
        const generation_report::body_entry* home = nullptr;
        for (const generation_report::body_entry& b : rep.bodies) if (b.is_homeworld) { home = &b; break; }
        if (home == nullptr || !fx.ran) { std::printf("FAIL seed %u: no homeworld / era did not run\n", seed); ++fails; continue; }
        const era_timelapse& emp = home->prehistory_timelapse;

        settlement_state ss_on = fx.settlement;
        const history_sim_state on = run_history_sim(ss_on, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh,
                                                     fx.params, fx.seed, nullptr, fx.works, nullptr);
        history_sim_params off_p = fx.params;
        off_p.civil_war_famine_q = 0;
        off_p.civil_war_flight_q = 0;
        settlement_state ss_off = fx.settlement;
        const history_sim_state off = run_history_sim(ss_off, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh,
                                                      off_p, fx.seed, nullptr, fx.works, nullptr);

        const bool replay_ok = same_events(on.events, emp.events);
        if (!replay_ok) { std::printf("FAIL seed %u: Empires replay does not reproduce the recorded events (%zu vs %zu)\n", seed, on.events.size(), emp.events.size()); ++fails; }

        int famines = 0, flights = 0, bad_dest = 0;
        std::set<int> struck;
        for (const lapse_event& e : on.events)
        {
            if (e.kind == static_cast<uint8_t>(lapse_event_kind::civil_war_famine)) { ++famines; struck.insert(e.region); }
            if (e.kind == static_cast<uint8_t>(lapse_event_kind::civil_war_flight))
            {
                ++flights; struck.insert(e.region);
                if (e.other == lapse_event_none || e.other >= ss_on.regions.size() || e.other == e.region) ++bad_dest;
            }
        }
        if (bad_dest > 0) { std::printf("FAIL seed %u: %d flights name no valid destination\n", seed, bad_dest); ++fails; }
        int emptied = 0;
        int64_t min_pop = -1, min_fill_q = -1;
        for (const int r : struck)
        {
            if (r < 0 || static_cast<std::size_t>(r) >= ss_on.regions.size()) continue;
            const region& rg = ss_on.regions[static_cast<std::size_t>(r)];
            if (rg.population <= 0) ++emptied;
            if (min_pop < 0 || rg.population < min_pop) min_pop = rg.population;
            const int64_t k = region_carrying_capacity(rg.farm_q, rg.work_capacity_mod);
            const int64_t q = k > 0 ? rg.population * 1000 / k : 1000;
            if (min_fill_q < 0 || q < min_fill_q) min_fill_q = q;
        }
        // The same reading over every settled land region of the OFF run, for scale.
        int64_t off_min_fill_q = -1;
        for (const region& rg : ss_off.regions)
        {
            if (rg.domain != region_domain::land || rg.population <= 0) continue;
            const int64_t k = region_carrying_capacity(rg.farm_q, rg.work_capacity_mod);
            const int64_t q = k > 0 ? rg.population * 1000 / k : 1000;
            if (off_min_fill_q < 0 || q < off_min_fill_q) off_min_fill_q = q;
        }
        if (emptied > 0) { std::printf("FAIL seed %u: %d struck regions stand empty at 1200\n", seed, emptied); ++fails; }
        if (on.civil_war_strikes > 0 && famines == 0 && flights == 0) { std::printf("FAIL seed %u: strikes counted, nothing recorded\n", seed); ++fails; }

        const int64_t pop_on = pooled(ss_on.regions), pop_off = pooled(ss_off.regions);
        sum_on += pop_on; sum_off += pop_off; sum_dead += on.civil_war_famine_dead; sum_fled += on.civil_war_fled;
        std::printf("seed %-3u replay %s | halves %lld strikes %lld (no refuge %lld) famine %d ev %lld dead | flight %d ev %lld fled | "
                    "struck regions %zu, min pop %lld, min fill %lld‰ (off world min %lld‰) | pooled pop 1200 off %lld on %lld (%+.2f%%) | battles off %lld on %lld | secessions off %lld on %lld | living realms off %d on %d\n",
                    seed, replay_ok ? "ok" : "MISMATCH",
                    static_cast<long long>(on.civil_wars), static_cast<long long>(on.civil_war_strikes),
                    static_cast<long long>(on.civil_war_no_refuge), famines,
                    static_cast<long long>(on.civil_war_famine_dead), flights, static_cast<long long>(on.civil_war_fled),
                    struck.size(), static_cast<long long>(min_pop), static_cast<long long>(min_fill_q), static_cast<long long>(off_min_fill_q),
                    static_cast<long long>(pop_off), static_cast<long long>(pop_on),
                    pop_off > 0 ? 100.0 * static_cast<double>(pop_on - pop_off) / static_cast<double>(pop_off) : 0.0,
                    static_cast<long long>(off.battles), static_cast<long long>(on.battles),
                    static_cast<long long>(off.secessions), static_cast<long long>(on.secessions), living(off), living(on));
        std::fflush(stdout);
    }
    std::printf("\ncivil_war_probe over %zu seeds: pooled pop 1200 off %lld on %lld (%+.2f%%), famine dead %lld, fled %lld, %d FAIL\n",
                seeds.size(), static_cast<long long>(sum_off), static_cast<long long>(sum_on),
                sum_off > 0 ? 100.0 * static_cast<double>(sum_on - sum_off) / static_cast<double>(sum_off) : 0.0,
                static_cast<long long>(sum_dead), static_cast<long long>(sum_fled), fails);
    return fails == 0 ? 0 : 1;
}
