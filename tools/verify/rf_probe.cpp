// rf_probe -- BL-1287 diagnostic. Re-runs the Empires span from generation's
// fixture with battle tracing on and reads what the rise-and-fall realm did,
// per 40-year window: its regions, its campaigns and defences (men, supply,
// odds), the verbs that outbid its campaigns, seats won and lost, breakaways.
// RF_EVENTS=<year> also lists every event touching the realm before that year.
//   bash tools/verify/build_lua_harness.sh rf_probe
//   build_gen/verify/rf_probe.exe --seeds 13
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
#include <map>
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
            while (std::getline(ss, tok, ',')) if (!tok.empty()) seeds.push_back(static_cast<uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
        }
    lua_state lua; world_gen_config cfg{}; works_registry works;
    lua.load("scripts/world_gen.lua"); cfg.load_from_lua(lua);
    lua.load("scripts/works.lua"); works.load_from_lua(lua);
    for (const uint32_t seed : seeds)
    {
        world_params p = arc_params(world_arc::shipped);
        p.seed = seed;
        generation_report rep{}; era_minus_one_fixture fx{};
        make_hard_coded_world(p, &rep, cfg, nullptr, &works, &fx);
        settlement_state ss = fx.settlement;
        history_sim_params hp = fx.params;
        hp.trace_battles = true;
        const history_sim_state hs = run_history_sim(ss, &fx.creeds, fx.terrain.view(), fx.gw, fx.gh, hp, fx.seed,
                                                     nullptr, fx.works, nullptr);
        const int F = hs.rise_fall_realm;
        std::printf("seed %u: realm %d start %d peak %d in %d fall %d levels %d/%d room %d/%d missed %d (%s) battles %lld (fixture %lld)\n",
                    seed, F, hs.rise_fall_start_pm, hs.rise_fall_peak_pm, hs.rise_fall_peak_year, hs.rise_fall_fall_year,
                    hs.rise_fall_rise_level, hs.rise_fall_fall_level, hs.rise_fall_room_regions, hs.rise_fall_room_total, hs.rise_fall_missed ? 1 : 0, hs.rise_fall_why.c_str(),
                    (long long)hs.battles, (long long)fx.battles);
        struct W { int atk = 0, won = 0, took = 0, def = 0, def_lost = 0, contests = 0, contest_lost = 0, secede = 0; int regions = -1, total = 0; int64_t dam = 0, ddm = 0; int dwon = 0; std::map<int,int> who; int seats_won = 0, seats_lost = 0; int64_t men = 0, dmen = 0; int sup = 0, pwin = 0; };
        std::map<int, W> win;
        for (const battle_trace& b : hs.battle_traces)
        {
            const int k = (b.year - (-400)) / 40;
            if (b.attacker == F) { W& w = win[k]; ++w.atk; w.won += b.attacker_won; w.took += b.conquered; w.men += b.attacker_men; w.dmen += b.defender_men; w.sup += b.attacker_supply_q; w.pwin += b.p_win_q; }
            if (b.defender == F) { W& w = win[k]; ++w.def; w.def_lost += b.conquered; w.dam += b.attacker_men; w.ddm += b.defender_men; w.dwon += b.attacker_won; ++w.who[b.attacker]; }
        }
        for (const verb_contest_trace& v : hs.verb_contests)
            if (v.polity == F) { W& w = win[(v.year + 400) / 40]; ++w.contests; w.contest_lost += v.winner != sim_verb::campaign; }
        for (const lapse_event& e : hs.events)
        {
            if ((e.kind == static_cast<uint8_t>(lapse_event_kind::broke_away) || e.kind == static_cast<uint8_t>(lapse_event_kind::schism)) && e.other == F)
                ++win[(e.year + 400) / 40].secede;
            if (e.kind == static_cast<uint8_t>(lapse_event_kind::seat_captured) && e.polity == F) ++win[(e.year + 400) / 40].seats_won;
            if (e.kind == static_cast<uint8_t>(lapse_event_kind::seat_captured) && e.other == F) ++win[(e.year + 400) / 40].seats_lost;
            if (std::getenv("RF_EVENTS") && (e.polity == F || e.other == F) && e.year < std::atoi(std::getenv("RF_EVENTS")))
                std::printf("    ev %d kind %d region %d polity %d other %d\n", e.year, e.kind, e.region, e.polity, e.other);
        }
        for (const timelapse_step& st : hs.steps)
        {
            if ((st.year + 400) % 40 != 0) continue;
            W& w = win[(st.year + 400) / 40];
            w.total = 0;
            for (int i = 0; i < st.sample_count; ++i)
            {
                const polity_sample& s = hs.samples[static_cast<std::size_t>(st.first_sample + i)];
                w.total += s.regions;
                if (s.polity == F) w.regions = s.regions;
            }
        }
        { int hist[8] = {0}; int64_t cs = 0, ws = 0; int n = 0;
          for (const verb_contest_trace& v : hs.verb_contests) if (v.polity == F && v.winner != sim_verb::campaign && v.year >= -200 && v.year < 400) { ++hist[static_cast<int>(v.winner) & 7]; cs += v.campaign_score; ws += v.winner_score; ++n; }
          std::printf("  lost contests -200..400: %d  (settle %d invest %d consolidate %d work %d supply %d organise %d)  mean campaign %lld vs winner %lld\n", n, hist[1], hist[3], hist[4], hist[5], hist[6], hist[7], (long long)(n ? cs / n : 0), (long long)(n ? ws / n : 0)); }
        { int sch = 0, brk = 0, cap = 0;
          for (const lapse_event& e : hs.events)
          {
              if (e.year < -200 || e.year > 400) continue;
              if (e.kind == static_cast<uint8_t>(lapse_event_kind::schism) && e.other == F) ++sch;
              if (e.kind == static_cast<uint8_t>(lapse_event_kind::broke_away) && e.other == F) ++brk;
              if (e.kind == static_cast<uint8_t>(lapse_event_kind::seat_captured) && e.polity == F) ++cap;
          }
          std::printf("  -200..400: seats taken %d, schisms %d, breakaways %d\n", cap, sch, brk); }
        for (const auto& kv : win)
        {
            const W& w = kv.second;
            if (kv.first > 25) break;
            std::printf("  %5d  regions %3d/%4d  atk %2d won %2d took %2d (men %lld vs %lld, sup %d, p %d) | def %2d lost %2d | contests %2d lost-to-verb %2d | seceded %d | vsF atk %lld def %lld won %d by %d realms | seats +%d -%d\n",
                        -400 + kv.first * 40, w.regions, w.total, w.atk, w.won, w.took,
                        (long long)(w.atk ? w.men / w.atk : 0), (long long)(w.atk ? w.dmen / w.atk : 0),
                        w.atk ? w.sup / w.atk : 0, w.atk ? w.pwin / w.atk : 0,
                        w.def, w.def_lost, w.contests, w.contest_lost, w.secede, (long long)(w.def ? w.dam / w.def : 0), (long long)(w.def ? w.ddm / w.def : 0), w.dwon, (int)w.who.size(), w.seats_won, w.seats_lost);
        }
        std::fflush(stdout);
    }
    return 0;
}
