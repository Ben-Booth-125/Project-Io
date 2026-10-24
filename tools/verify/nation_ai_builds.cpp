// ---------------------------------------------------------------------------
// nation_ai_builds — BL-1401 (nation AI actor), requirement group `nation-ai-actor`
// ---------------------------------------------------------------------------
// docs/ai/AI_OPPONENT.md § 11, "Every AI nation builds its own economy" (Ben,
// 2026-10-23); docs/politics/NATIONS.md § What a nation owns / § Own ground. On the
// SHIPPED start (harness_params.hpp's app mirror), per seed: the app's validation
// settle, the seat, then a LIVE window of k_live_ticks quarters with spectating OFF
// (a seated session, as the app plays it). Over the live window:
//
//   R1  EVERY NON-PLAYER NATION BUILDS. At least one MAJOR nation and at least one
//       MINOR nation (no champion) placed a production building (extraction site
//       or processing facility) that joined its own state_assets. Production is
//       counted because only the build scorer places it — the network's nodes
//       have other builders.
//   R2  NEVER THE PLAYER'S NATION. No building of any kind joins the player's
//       nation's state_assets, and no decision or agency entry in the world
//       history log names the player's nation.
//   R2g OWN GROUND. Every building a nation owns at the end of the window that was
//       placed after generation (through the settle and the live window: the
//       scorer's builds and every other builder's) stands on that nation's ground.
//       Generation's own holdings are counted and printed beside it as a NOTE:
//       they are generation's placement, not this item's (BL-1401) subject.
//
// Build: cmd //c tools\verify\build_lua_harness.bat nation_ai_builds
// Run:   build_gen/verify/nation_ai_builds.exe [seed ...]   (default: 0 28 46)
// Exits non-zero on any FAIL. Every row is REQUIRED: a row whose fixture is missing
// on a seed FAILS — no finding stands in for it.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/spawn_seat.hpp"
#include "world/survey_system.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr int k_live_ticks = 40;

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* row, const std::string& what)
{
    std::printf("  [%s] %s %s\n", ok ? "PASS" : "FAIL", row, what.c_str());
    if (ok) ++g_pass; else ++g_fail;
}

std::string fmt(const char* f, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::vector<entity_id> sorted_nations(const world& w)
{
    std::vector<entity_id> ids;
    for (const auto& kv : w.nations) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    return ids;
}

bool is_production(building_type t)
{
    return t == building_type::extraction_site || t == building_type::processing_facility;
}

void run_seed(lua_state& lua, std::uint32_t seed)
{
    std::printf("\n=== seed %u ===\n", static_cast<unsigned>(seed));
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;
    auto start = std::make_unique<app_start_world>();
    try { build_app_start_world(lua, params, *start); }
    catch (const std::exception& e)
    {
        check(false, "R0", fmt("generation threw: %s", e.what()));
        return;
    }
    world& w = start->w;
    const recipe_registry& reg = start->reg;

    // Every building generation left: R2g's subject is everything placed after it.
    std::set<entity_id> generated;
    for (const auto& kv : w.buildings) generated.insert(kv.first);

    run_app_validation_settle(w, reg);
    const spawn_seat_result seat = seat_player_corporation(w, seed, start->land.search.winner_score);
    check(seat.seated != null_entity, "R0", "a corporation is seated (the session is a played one)");
    if (seat.seated == null_entity) return;
    const entity_id pn = player_nation(w);
    check(pn != null_entity && w.nations.count(pn) == 1, "R0",
          fmt("the seat has a home nation (%llu)", static_cast<unsigned long long>(pn)));
    if (pn == null_entity || w.nations.count(pn) == 0) return;

    const std::vector<entity_id> nations = sorted_nations(w);
    // The buildings each nation owns at the seat.
    std::map<entity_id, std::set<entity_id>> at_seat;
    for (const entity_id n : nations)
        at_seat[n] = std::set<entity_id>(w.nations.at(n).state_assets.begin(),
                                         w.nations.at(n).state_assets.end());
    const std::size_t log_at_seat = w.history_log.size();

    std::set<entity_id> player_added;                    // R2
    std::map<entity_id, int> production_added;           // R1, per nation
    for (int live = 1; live <= k_live_ticks; ++live)
    {
        const int day = live * 90;
        advance_orbits(w, 90.0);
        advance_surveys(w, 90);
        w.current_day_tick = day;
        (void)run_settle_tick(w, reg, k_app_validation_ticks + (live - 1), day, /*spectating=*/false);
        for (const entity_id n : nations)
        {
            std::set<entity_id>& seen = at_seat[n];
            for (const entity_id b : w.nations.at(n).state_assets)
            {
                if (seen.count(b)) continue;
                seen.insert(b);
                if (n == pn) { player_added.insert(b); continue; }
                const auto bit = w.buildings.find(b);
                if (bit != w.buildings.end() && is_production(bit->second.type))
                    ++production_added[n];
            }
        }
    }

    // ---- R1 -------------------------------------------------------------------
    int majors = 0, minors = 0, major_built = 0, minor_built = 0, major_n = 0, minor_n = 0;
    for (const entity_id n : nations)
    {
        if (n == pn) continue;
        const bool major = is_major_nation(w, n);
        (major ? majors : minors) += 1;
        const int added = production_added.count(n) ? production_added.at(n) : 0;
        if (added > 0)
        {
            (major ? major_built : minor_built) += 1;
            (major ? major_n : minor_n) += added;
        }
    }
    std::printf("  non-player nations: %d major (%d built, %d production buildings), "
                "%d minor (%d built, %d production buildings)\n",
                majors, major_built, major_n, minors, minor_built, minor_n);
    check(major_built >= 1, "R1", fmt("at least one major nation placed a production building it owns "
                                      "(%d of %d majors)", major_built, majors));
    check(minor_built >= 1, "R1", fmt("at least one minor nation placed a production building it owns "
                                      "(%d of %d minors)", minor_built, minors));

    // ---- R2 -------------------------------------------------------------------
    check(player_added.empty(), "R2",
          fmt("no building joined the player's nation %llu in %d seated ticks (%zu joined)",
              static_cast<unsigned long long>(pn), k_live_ticks, player_added.size()));
    std::size_t named = 0;
    for (std::size_t i = log_at_seat; i < w.history_log.size(); ++i)
    {
        const world_history_entry& e = w.history_log[i];
        if ((e.topic == history_topic::decision || e.topic == history_topic::agency) && e.corp == pn)
            ++named;
    }
    check(named == 0, "R2", fmt("no decision or agency entry names the player's nation (%zu do, of %zu "
                                "entries logged since the seat)", named, w.history_log.size() - log_at_seat));

    // ---- R2g ------------------------------------------------------------------
    std::size_t owned = 0, abroad = 0, gen_owned = 0, gen_abroad = 0;
    for (const entity_id n : nations)
        for (const entity_id b : w.nations.at(n).state_assets)
        {
            const auto bit = w.buildings.find(b);
            if (bit == w.buildings.end()) continue;
            const bool from_gen = generated.count(b) != 0;
            ++(from_gen ? gen_owned : owned);
            const auto tn = w.tile_to_nation.find(bit->second.tile);
            if (tn != w.tile_to_nation.end() && tn->second == n) continue;
            ++(from_gen ? gen_abroad : abroad);
            std::printf("    abroad (%s): building %llu (type %d) of nation %llu on tile %llu\n",
                        from_gen ? "generation" : "placed in play", static_cast<unsigned long long>(b),
                        static_cast<int>(bit->second.type), static_cast<unsigned long long>(n),
                        static_cast<unsigned long long>(bit->second.tile));
        }
    check(owned > 0 && abroad == 0, "R2g",
          fmt("every nation-owned building placed since generation stands on its owner's ground "
              "(%zu of %zu abroad)", abroad, owned));
    std::printf("  NOTE generation's nation holdings: %zu of %zu abroad (generation's placement, "
                "not BL-1401's subject)\n", gen_abroad, gen_owned);
    // Printed for a run-to-run comparison (R3): two runs of one seed print one digest.
    std::printf("  state_hash after the live window: %016llX\n",
                static_cast<unsigned long long>(w.state_hash(0)));
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<std::uint32_t> seeds;
    for (int i = 1; i < argc; ++i)
        seeds.push_back(static_cast<std::uint32_t>(std::strtoul(argv[i], nullptr, 10)));
    if (seeds.empty())
        seeds = {0, 28, 46};

    lua_state lua;
    for (const std::uint32_t s : seeds)
        run_seed(lua, s);

    std::printf("\nnation_ai_builds: %d pass, %d fail -- %s\n", g_pass, g_fail,
                g_fail == 0 ? "ALL PASS" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}
