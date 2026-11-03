// war_census.cpp — BL-1461 (rivals declare war). A MEASUREMENT with one band check:
// on a generated campaign world (the app's start, its 12-tick settle, the seat, then
// live quarters as the app steps them), what the rivals' war pass does by econ tick 40.
//
// Per seed it prints: wars declared, wars at peace (closed), nations at war at the end
// and at the most, the nation count, cessions, imposes, answers, offers, accepts and
// refuses, and declarations against the player's nation. The band (requirement group
// `rivals-declare-war` R2) is a POOLED reading across the seeds run: at least one war
// declared somewhere, and no seed with every nation at war at any tick. A shard sees
// only its seeds, so a shard's "at least one" verdict is reported but only the merged
// table settles it — the per-seed rows are the data.
//
//   build_gen/verify/war_census.exe [--seeds a,b,c] [--ticks T] [--replay] [--trace]
//
//   --seeds   the worlds (default: the 16 of docs/generation/seed_library.json)
//   --ticks   the last econ tick stepped (default 40: settle 0..11, live 12..T)
//   --replay  run EVERY seed given twice from scratch and require the war tables, the
//             trace and the world's state hash to match (R1: replayable)
//   --trace   print every declaration candidate the pass weighed
//
// Build: bash tools/verify/build_lua_harness.sh war_census. Run from the repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/corp_ai.hpp"
#include "world/orbital_system.hpp"
#include "world/spawn_seat.hpp"
#include "world/survey_system.hpp"
#include "world/war.hpp"
#include "world/world.hpp"
#include "world/world_save.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

const std::vector<std::uint32_t> k_library = { 46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0 };

struct seed_result
{
    std::uint32_t seed = 0;
    int nations = 0;
    int declared = 0, at_peace = 0, at_war_end = 0, at_war_max = 0, cessions = 0, imposes = 0;
    int answers = 0, offers = 0, accepts = 0, refuses = 0, on_player = 0;
    int bunkers = 0, bunkers_done = 0, player_bunkers = 0; ///< BL-1426: rivals' bunkers (built or building), completed, the player's (ceded ones included)
    int built_for_player = 0; ///< BL-1426: a bunker that first appeared in the player's assets - a build for it
    long long pair_ticks = 0, grievance_pair_ticks = 0; ///< BL-1479: ordered nation pairs x live ticks, and those whose Trust <= -25
    int grievance_declares = 0; ///< BL-1479: declarations made with grievance holding
    int imposed_on_player = 0;  ///< BL-1480: peaces a rival imposed on the player's nation
    std::uint64_t hash = 0;
    std::string war_bytes; ///< the war tables, for --replay
    std::vector<war_ai_trace_row> trace;
};

int nations_at_war(const world& w)
{
    std::set<entity_id> s;
    for (const auto& [key, war] : w.wars) { s.insert(war.declarer); s.insert(war.defender); }
    return static_cast<int>(s.size());
}

std::string war_tables(const world& w)
{
    std::ostringstream o;
    for (const auto& [key, war] : w.wars)
    {
        o << "W " << war.declarer << ' ' << war.defender << ' ' << war.answered << ' ' << war.opened_tick;
        for (const uint32_t g : war.declarer_goals) o << " d" << g;
        for (const uint32_t g : war.defender_goals) o << " a" << g;
        o << '\n';
    }
    for (const war_history_entry& h : w.war_history)
    {
        o << "H " << h.declarer << ' ' << h.defender << ' ' << h.opened_tick << ' ' << h.closed_tick << ' '
          << static_cast<int>(h.end) << ' ' << h.gainer << ' ' << h.final_score;
        for (const uint32_t g : h.ceded) o << " c" << g;
        o << '\n';
    }
    for (const auto& [key, off] : w.peace_offers)
        o << "O " << key.first << ' ' << key.second << ' ' << off.offerer << ' '
          << static_cast<int>(off.terms.kind) << '\n';
    return o.str();
}

const char* choice_name(war_ai_choice c)
{
    switch (c)
    {
        case war_ai_choice::candidate: return "candidate";
        case war_ai_choice::declare:   return "DECLARE";
        case war_ai_choice::answer:    return "answer";
        case war_ai_choice::offer:     return "offer";
        case war_ai_choice::accept:    return "accept";
        case war_ai_choice::refuse:    return "refuse";
        case war_ai_choice::impose:    return "impose";
    }
    return "?";
}

seed_result run_seed(std::uint32_t seed, int last_tick, bool print_trace)
{
    seed_result r;
    r.seed = seed;
    lua_state lua;
    world_params wp;
    wp.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, wp, *start);
    world& w = start->w;
    const recipe_registry& reg = start->reg;

    std::vector<war_ai_trace_row> trace;
    war_ai_trace_sink() = &trace;

    for (int step = 0; step < k_campaign_settle_ticks; ++step)
        (void)run_settle_tick(w, reg, step, /*day_tick=*/0, /*spectating=*/true);
    clear_research_at_handover(w);
    const bool settle_clean = w.wars.empty() && w.war_history.empty() && trace.empty();
    if (!settle_clean)
        std::printf("  FAIL  seed %u: a war or a war verb in the settle (wars %zu, trace %zu)\n", seed,
                    w.wars.size(), trace.size());
    seat_player_corporation(w, seed, start->land.search.winner_score);
    const entity_id player = player_nation(w);
    r.nations = static_cast<int>(w.nations.size());

    constexpr int k_days = 90;
    std::set<entity_id> rival_bunkers_seen, player_bunkers_flagged; // BL-1426
    for (int step = k_campaign_settle_ticks, k = 1; step <= last_tick; ++step, ++k)
    {
        advance_orbits(w, static_cast<double>(k_days));
        advance_surveys(w, k_days);
        w.current_day_tick = k * k_days;
        (void)run_settle_tick(w, reg, step, k * k_days, /*spectating=*/false);
        r.at_war_max = std::max(r.at_war_max, nations_at_war(w));
        // BL-1479 (NR-1104): how often grievance holds - every ordered pair of distinct
        // nations, its observer's Trust toward the subject at or below the edge.
        for (const auto& [a, na] : w.nations)
            for (const auto& [b, nb] : w.nations)
                if (a != b)
                {
                    ++r.pair_ticks;
                    if (sentiment_toward(w.sentiment, a, b).trust <= k_war_grievance_trust)
                        ++r.grievance_pair_ticks;
                }
        // BL-1426 (cold review): a bunker CEDED to the player is legal; one BUILT for it
        // is not. A cession moves an existing building id, a build mints a new one, so a
        // bunker id the player holds that no rival held at an earlier step was built for
        // it. (One a rival built and ceded inside the same step would read as built for
        // the player - a false alarm, never a miss.)
        for (const auto& [nid, nc] : w.nations)
            for (const entity_id bid : nc.state_assets)
                if (const auto bit = w.buildings.find(bid);
                    bit != w.buildings.end() && bit->second.type == building_type::bunker)
                {
                    if (nid != player)
                        rival_bunkers_seen.insert(bid);
                    else if (rival_bunkers_seen.count(bid) == 0 && player_bunkers_flagged.insert(bid).second)
                        ++r.built_for_player;
                }
    }
    war_ai_trace_sink() = nullptr;

    r.at_war_end = nations_at_war(w);
    r.at_peace   = static_cast<int>(w.war_history.size());
    r.declared   = static_cast<int>(w.wars.size() + w.war_history.size());
    r.cessions   = static_cast<int>(w.province_cessions.size());
    for (const war_history_entry& h : w.war_history)
        r.imposes += (h.end == war_end::imposed_white || h.end == war_end::imposed_cession) ? 1 : 0;
    for (const war_ai_trace_row& t : trace)
        r.imposed_on_player += (t.applied && t.choice == war_ai_choice::impose && t.target == player) ? 1 : 0;
    for (const auto& [key, war] : w.wars) r.on_player += war.defender == player ? 1 : 0;
    for (const war_history_entry& h : w.war_history) r.on_player += h.defender == player ? 1 : 0;
    for (const war_ai_trace_row& t : trace)
    {
        if (!t.applied) continue;
        r.answers += t.choice == war_ai_choice::answer ? 1 : 0;
        r.offers  += t.choice == war_ai_choice::offer ? 1 : 0;
        r.accepts += t.choice == war_ai_choice::accept ? 1 : 0;
        r.refuses += t.choice == war_ai_choice::refuse ? 1 : 0;
        // The trace's g is max(0, -Trust) / limit (limit 100): grievance held at g >= 0.25.
        r.grievance_declares += (t.choice == war_ai_choice::declare
                                 && t.grievance >= -k_war_grievance_trust / 100.0f - 1e-6f) ? 1 : 0;
    }
    // The player's nation is never acted for: no trace row names it as the actor.
    bool player_clean = true;
    for (const war_ai_trace_row& t : trace)
        if (t.nation == player) player_clean = false;
    if (!player_clean)
        std::printf("  FAIL  seed %u: the war pass acted for the player's nation\n", seed);
    for (const war_ai_trace_row& t : trace)
        if (print_trace || t.choice != war_ai_choice::candidate)
            std::printf("    t%-3d %-9s %4u -> %4u u %+.3f g %.3f a %.3f s %+.3f p %.2f x %.1f goals %d%s\n",
                        t.tick, choice_name(t.choice), static_cast<unsigned>(t.nation),
                        static_cast<unsigned>(t.target), static_cast<double>(t.utility),
                        static_cast<double>(t.grievance), static_cast<double>(t.alarm),
                        static_cast<double>(t.edge), static_cast<double>(t.price),
                        static_cast<double>(t.trade), t.goals, t.applied ? "" : " (refused)");
    // BL-1426 (AI_OPPONENT.md § 11, "Rival nations build bunkers on their own ground"):
    // the rivals' bunkers, and the player's - ceded ones are legal; a build for the
    // player is caught step by step above (`built_for_player`, must be 0: no harness
    // here presses the player's build).
    for (const auto& [nid, nc] : w.nations)
        for (const entity_id bid : nc.state_assets)
            if (const auto bit = w.buildings.find(bid);
                bit != w.buildings.end() && bit->second.type == building_type::bunker)
            {
                if (nid == player) { ++r.player_bunkers; continue; }
                ++r.bunkers;
                r.bunkers_done += bit->second.ticks_remaining <= 0 ? 1 : 0;
            }
    if (r.built_for_player != 0)
    {
        std::printf("  FAIL  seed %u: %d bunker(s) built for the player's nation\n", seed, r.built_for_player);
        player_clean = false;
    }
    r.hash      = w.state_hash(0);
    r.war_bytes = war_tables(w);
    r.trace     = std::move(trace);
    if (!settle_clean || !player_clean)
        r.nations = -r.nations; // flagged for the verdict
    return r;
}

bool same_trace(const std::vector<war_ai_trace_row>& a, const std::vector<war_ai_trace_row>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (a[i].tick != b[i].tick || a[i].nation != b[i].nation || a[i].target != b[i].target
            || a[i].choice != b[i].choice || a[i].utility != b[i].utility || a[i].goals != b[i].goals
            || a[i].applied != b[i].applied)
            return false;
    return true;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    int last_tick = 40;
    bool replay = false, print_trace = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--seeds") == 0 && i + 1 < argc)
        {
            std::string s = argv[++i];
            std::size_t pos = 0;
            while (pos <= s.size())
            {
                const std::size_t c = s.find(',', pos);
                const std::string tok = s.substr(pos, c == std::string::npos ? std::string::npos : c - pos);
                if (!tok.empty()) seeds.push_back(static_cast<std::uint32_t>(std::strtoul(tok.c_str(), nullptr, 10)));
                if (c == std::string::npos) break;
                pos = c + 1;
            }
        }
        else if (std::strcmp(argv[i], "--ticks") == 0 && i + 1 < argc) last_tick = std::atoi(argv[++i]);
        else if (std::strcmp(argv[i], "--replay") == 0) replay = true;
        else if (std::strcmp(argv[i], "--trace") == 0) print_trace = true;
    }
    if (seeds.empty()) seeds = k_library;

    std::printf("=== war census (BL-1461): %zu seed(s), econ ticks 0..%d (settle 0..%d) ===\n", seeds.size(),
                last_tick, k_campaign_settle_ticks - 1);
    int failures = 0, total_declared = 0;
    std::vector<seed_result> rows;
    for (const std::uint32_t seed : seeds)
    {
        std::printf("seed %u\n", seed);
        rows.push_back(run_seed(seed, last_tick, print_trace));
        const seed_result& r = rows.back();
        if (r.nations < 0) { ++failures; rows.back().nations = -r.nations; }
        total_declared += r.declared;
        const bool all_at_war = r.at_war_max >= rows.back().nations && rows.back().nations > 0;
        if (all_at_war) ++failures;
        std::printf("  %s  seed %u: not every nation at war at any tick (max %d of %d)\n",
                    all_at_war ? "FAIL" : "PASS", seed, r.at_war_max, rows.back().nations);
    }

    std::printf("\nWAR CENSUS (econ tick %d)\n", last_tick);
    std::printf("  seed nations declared at_peace at_war_end at_war_max cessions imposes answers offers accepts refuses on_player bunkers built\n");
    for (const seed_result& r : rows)
        std::printf("  %4u %7d %8d %8d %10d %10d %8d %7d %7d %6d %7d %7d %9d %7d %5d\n", r.seed, r.nations, r.declared,
                    r.at_peace, r.at_war_end, r.at_war_max, r.cessions, r.imposes, r.answers, r.offers,
                    r.accepts, r.refuses, r.on_player, r.bunkers, r.bunkers_done);
    std::printf("  TOTAL declared %d across %zu seed(s)\n", total_declared, rows.size());
    {
        int tb = 0, td = 0;
        for (const seed_result& r : rows) { tb += r.bunkers; td += r.bunkers_done; }
        std::printf("  TOTAL rival bunkers %d (%d completed) across %zu seed(s)\n", tb, td, rows.size());
    }
    // A shard (fewer seeds than the library) only reports: the pooled table settles R2.
    const bool full = seeds.size() >= k_library.size();
    std::printf("  %s  at least one war declared across the seeds run%s\n",
                total_declared > 0 ? "PASS" : (full ? "FAIL" : "NOTE"),
                full ? "" : " (a shard: the pooled table across every shard settles it)");
    if (total_declared == 0 && full) ++failures;

    if (replay)
        for (const seed_result& first : rows) // EVERY seed given, each rebuilt from scratch
        {
            const seed_result again = run_seed(first.seed, last_tick, false);
            const bool tables = again.war_bytes == first.war_bytes;
            const bool traces = same_trace(again.trace, first.trace);
            const bool ok = again.hash == first.hash && tables && traces;
            std::printf("  %s  R1 replay: seed %u run twice from scratch - state hash %016llx vs %016llx, "
                        "%d declared, war tables %s, trace %s (%zu rows)\n",
                        ok ? "PASS" : "FAIL", first.seed, static_cast<unsigned long long>(first.hash),
                        static_cast<unsigned long long>(again.hash), first.declared,
                        tables ? "identical" : "DIFFER", traces ? "identical" : "DIFFERS", first.trace.size());
            if (!ok) ++failures;
        }
    std::printf("war_census: %s (%d failure(s))\n", failures == 0 ? "ALL PASS" : "FAILURES", failures);
    return failures == 0 ? 0 : 1;
}
