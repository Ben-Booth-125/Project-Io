// ---------------------------------------------------------------------------
// late_culture_probe — the late culture's own reading, per seed (BL-1288)
// ---------------------------------------------------------------------------
// EXPLORATION.md sec A colony is a subject (THE LATE CULTURE IS TARGETED, Ben
// 2026-10-10/11). Builds each seed's shipped world with the era fixture, then
// REPLAYS the Empires span from the fixture and the Exploration span from its
// pre-Exploration capture, and prints the sim's own late-culture reading (the
// choice and its remoteness by reach, the next culture, the realm it carries
// to 1200, its lag, the press levels, who bound it and when) beside the
// diffusion's totals and the world's materials ladder at the close.
//
// THE REPLAY IS A CHECK, not a convenience: each replayed span must reproduce
// the generated one (battles / conquests / foundings, and the late-culture
// reading against the record's own event), or the seed prints REPLAY DIVERGES
// and the harness exits 1 -- a backstop that a fixture cannot reproduce would
// be a defect in the seam, never a quirk of the probe.
//
//   bash tools/verify/build_lua_harness.sh late_culture_probe
//   build_gen/verify/late_culture_probe.exe [--seeds 0,11,46]

#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/era_minus_one.hpp"
#include "world/era_timelapse.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_pattern_detectors.hpp"
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
#include <map>
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

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds;
    int diffusion = -1, horizon = -1, step = -1; // overrides for the Empires REPLAY (a measuring ladder)
    bool trace = false;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--trace") == 0) trace = true;
        else if (std::strcmp(argv[i], "--diffusion") == 0 && i + 1 < argc) diffusion = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--horizon") == 0 && i + 1 < argc) horizon = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--isolation-step") == 0 && i + 1 < argc) step = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
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

    int diverged = 0, hits = 0;
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
        if (!fx.ran || home == nullptr) { std::printf("seed %u: no era\n", seed); continue; }

        // --- the Empires span, replayed from the fixture
        settlement_state ss = fx.settlement;
        creed_state      cs = fx.creeds;
        history_sim_params hp = fx.params;
        bool overridden = diffusion >= 0 || horizon >= 0 || step >= 0;
        for (int i = 1; i < argc; ++i) if (std::strcmp(argv[i], "--isolation-step-q") == 0) overridden = true;
        if (diffusion >= 0) hp.contact_diffusion_q = diffusion;
        if (horizon >= 0)   hp.contact_diffusion_horizon = horizon;
        if (step >= 0)      hp.late_isolation_step = step;
        int step_q = -1;
        for (int i = 1; i < argc; ++i) if (std::strcmp(argv[i], "--isolation-step-q") == 0 && i + 1 < argc) step_q = std::atoi(argv[i + 1]);
        if (step_q >= 0)    hp.late_isolation_step_q = step_q;
        const history_sim_state e = run_history_sim(ss, &cs, fx.terrain.view(), fx.gw, fx.gh, hp,
                                                    fx.seed, nullptr, fx.works, nullptr);
        const history_pattern_reading pr = detect_history_patterns(home->prehistory_timelapse, home->exploration_timelapse,
                                                                   home->industrialisation_timelapse, home->settlement,
                                                                   home_grid_width);
        const bool emp_same = overridden || e.battles == fx.battles && e.conquests == fx.conquests && e.foundings == fx.foundings
                           && e.late_culture == pr.lc_culture;
        // The world's materials ladder at the close.
        std::vector<int> held(e.polities.size(), 0);
        for (const region& g : ss.regions)
            if (g.nation >= 0 && static_cast<std::size_t>(g.nation) < held.size()) ++held[static_cast<std::size_t>(g.nation)];
        int bands[7] = {0};
        int living = 0;
        int lc_bands[7] = {0};
        for (std::size_t pi = 0; pi < e.polities.size(); ++pi)
            if (held[pi] > 0)
            {
                ++living;
                const int b = std::max(1, std::min(6, e.polities[pi].capacity[static_cast<int>(sim_domain::materials)]));
                ++bands[b];
                const int bs = e.polities[pi].birth_seat;
                if (bs >= 0 && static_cast<std::size_t>(bs) < ss.regions.size()
                    && ss.regions[static_cast<std::size_t>(bs)].founding_culture == e.late_culture)
                    ++lc_bands[b];
            }
        int lc_births = 0;
        for (const polity& q : e.polities)
            if (q.birth_seat >= 0 && static_cast<std::size_t>(q.birth_seat) < ss.regions.size()
                && ss.regions[static_cast<std::size_t>(q.birth_seat)].founding_culture == e.late_culture)
                ++lc_births;
        std::string lc_ladder;
        for (int b = 1; b <= 6; ++b) lc_ladder += (b > 1 ? "/" : "") + std::to_string(lc_bands[b]);

        // --- the Exploration span, replayed from the pre-Exploration capture
        history_sim_state x;
        bool exp_same = true;
        if (fx.exploration_ran)
        {
            history_sim_params ep = fx.exploration_params;
            ep.resume_polities  = &fx.pre_exploration_polities;
            ep.resume_grudges   = &fx.pre_exploration_grudges;
            ep.resume_contacts  = &fx.pre_exploration_contacts;
            ep.resume_corridors = &fx.pre_exploration_corridors;
            ep.resume_civilisations    = &fx.pre_exploration_civilisations;
            ep.resume_universal_creeds = &fx.pre_exploration_universal_creeds;
            settlement_state xs = fx.pre_exploration_settlement;
            creed_state      xc = fx.pre_exploration_creeds;
            x = run_history_sim(xs, &xc, fx.terrain.view(), fx.gw, fx.gh, ep, fx.exploration_seed, nullptr,
                                fx.works, nullptr);
            // The span names the culture only where the Empires close left a
            // realm of it to flag (`polity::late_isolation`); either way the
            // replay must agree with the record's binding count.
            exp_same = (x.late_culture == pr.lc_culture || x.late_culture < 0) && x.late_culture_bound == pr.lc_bound;
        }
        if (trace && e.rise_fall_realm >= 0 && static_cast<std::size_t>(e.rise_fall_realm) < e.polities.size())
        {
            const int bs = e.polities[static_cast<std::size_t>(e.rise_fall_realm)].birth_seat;
            std::printf("    rise-and-fall realm %d start %d peak %d at %d fall %d at %d levels %d/%d room %d/%d boxed %d why '%s'; late culture level %d\n",
                        e.rise_fall_realm, e.rise_fall_start_pm, e.rise_fall_peak_pm, e.rise_fall_peak_year, e.rise_fall_fall_pm,
                        e.rise_fall_fall_year, e.rise_fall_rise_level, e.rise_fall_fall_level, e.rise_fall_room_regions,
                        e.rise_fall_room_total, e.rise_fall_boxed_realm, e.rise_fall_why.c_str(), e.late_culture_level);
            std::printf("    rise-and-fall realm %d born of culture %d (late culture %d)\n", e.rise_fall_realm,
                        bs >= 0 && static_cast<std::size_t>(bs) < ss.regions.size() ? ss.regions[static_cast<std::size_t>(bs)].founding_culture : -9,
                        e.late_culture);
        }
        if (trace && e.late_culture >= 0)
        {
            // The late culture's ground through both records: every owner
            // change on a region it founded, with the new owner's birth culture.
            for (const era_timelapse* t : {&home->prehistory_timelapse, &home->exploration_timelapse})
            {
                std::map<int, int> born; // polity -> founding culture of its birth seat
                for (const era_timelapse* u : {&home->prehistory_timelapse, &home->exploration_timelapse})
                    for (const lapse_event& ev : u->events)
                        if ((ev.kind == static_cast<uint8_t>(lapse_event_kind::founded) || ev.kind == static_cast<uint8_t>(lapse_event_kind::broke_away)
                             || ev.kind == static_cast<uint8_t>(lapse_event_kind::schism) || ev.kind == static_cast<uint8_t>(lapse_event_kind::inherited))
                            && ev.polity != lapse_event_none && !born.count(ev.polity) && ev.region < home->settlement.regions.size())
                            born[ev.polity] = home->settlement.regions[ev.region].founding_culture;
                int shown = 0;
                for (const owner_change& oc : t->changes)
                {
                    if (oc.region >= home->settlement.regions.size()) continue;
                    if (home->settlement.regions[oc.region].founding_culture != e.late_culture) continue;
                    if (++shown > 40) break;
                    const auto it = born.find(oc.owner);
                    std::printf("    %s %5d region %4u -> polity %5u (culture %d)\n", t == &home->prehistory_timelapse ? "E" : "X",
                                oc.year, static_cast<unsigned>(oc.region), static_cast<unsigned>(oc.owner), it == born.end() ? -9 : it->second);
                }
                for (const lapse_event& ev : t->events)
                    if (ev.kind == static_cast<uint8_t>(lapse_event_kind::capital_moved) && ev.region < home->settlement.regions.size()
                        && home->settlement.regions[ev.region].founding_culture == e.late_culture)
                        std::printf("    %s %5d polity %u re-seats onto region %u\n", t == &home->prehistory_timelapse ? "E" : "X",
                                    ev.year, static_cast<unsigned>(ev.polity), static_cast<unsigned>(ev.region));
                for (const lapse_event& ev : t->events)
                    if (ev.kind == static_cast<uint8_t>(lapse_event_kind::realm_ended) && born.count(ev.polity) && born[ev.polity] == e.late_culture)
                        std::printf("    %s %5d realm %u ended, killer %u (culture %d)\n", t == &home->prehistory_timelapse ? "E" : "X",
                                    ev.year, static_cast<unsigned>(ev.polity), static_cast<unsigned>(ev.other),
                                    born.count(ev.other) ? born[ev.other] : -9);
            }
        }
        if (!emp_same || !exp_same) ++diverged;
        hits += pr.lc_hit;

        std::printf("seed %-3u LC %s culture %d (of %d seated, %d regions) reach %d gap2 %d contact %d next %d@%d/%d | 1200: %d realm%s, %d regions, lag %d%%, level %d%s%s"
                    " | 1660: %d realms (%d regions), bound %d (first %d by %d), level %d%s%s; subjections %lld, lc walk %lld nocontact %lld subj %lld treas %lld sphere %lld elig %lld | diffusion +%lld progress, %d bands; ladder at 1200 b1..6 %d/%d/%d/%d/%d/%d of %d (late culture %s, %d realms born)%s\n",
                    seed, pr.lc_hit ? "HIT " : "miss", e.late_culture, e.late_culture_seated, e.late_culture_regions,
                    e.late_culture_reach >= late_culture_unreached ? -1 : e.late_culture_reach, e.late_culture_gap2,
                    e.late_culture_contact,
                    e.late_culture_next, e.late_culture_next_reach >= late_culture_unreached ? -1 : e.late_culture_next_reach,
                    e.late_culture_next_contact,
                    e.late_culture_realms, e.late_culture_realms == 1 ? "" : "s", e.late_culture_realm_regions,
                    e.late_culture_lag_x100, e.late_culture_level, e.late_culture_missed ? " MISS: " : "",
                    e.late_culture_why.c_str(),
                    x.late_culture_realms, x.late_culture_realm_regions, x.late_culture_bound, x.late_culture_bound_year, x.late_culture_bound_by, x.late_culture_level,
                    x.late_culture_missed ? " MISS: " : "", x.late_culture_why.c_str(),
                    static_cast<long long>(x.subjections_formed), static_cast<long long>(x.late_culture_diag[0]),
                    static_cast<long long>(x.late_culture_diag[1]), static_cast<long long>(x.late_culture_diag[2]),
                    static_cast<long long>(x.late_culture_diag[3]), static_cast<long long>(x.late_culture_diag[4]),
                    static_cast<long long>(x.late_culture_diag[5]),
                    static_cast<long long>(e.contact_diffusion_progress), e.contact_diffusion_bands,
                    bands[1], bands[2], bands[3], bands[4], bands[5], bands[6], living, lc_ladder.c_str(), lc_births,
                    (emp_same && exp_same) ? "" : "  REPLAY DIVERGES");
        std::fflush(stdout);
    }
    std::printf("\nlate_culture_probe: %d/%zu hit, %d replay divergence%s\n", hits, seeds.size(), diverged,
                diverged == 1 ? "" : "s");
    return diverged == 0 ? 0 : 1;
}
