// ---------------------------------------------------------------------------
// nation_floor_probe — BL-1402 diagnostic (reported, no assertions)
// ---------------------------------------------------------------------------
// After the fold a nation's build scorer reads one solvency floor over its whole
// industry (corp_reserve_floor: floor_wage_mult x the quarterly wage bill). This
// prints, per non-player nation on the shipped start and again after a short
// live window, its treasury, its uncommitted treasury and that floor, so a seed
// whose nations do not build can be read as money or as a defect.
//
// Build: cmd //c tools\verify\build_lua_harness.bat nation_floor_probe
// Run:   build_gen/verify/nation_floor_probe.exe [seed] [live_ticks]   (default 46, 20)

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/corp_ai.hpp"
#include "world/state_industry.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <map>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

void print_nations(const world& w, const recipe_registry& reg, const char* when)
{
    std::printf("--- %s ---\n", when);
    std::vector<entity_id> ids;
    for (const auto& kv : w.nations) ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    const corp_ai_params p{};
    int above = 0, n = 0;
    for (const entity_id nid : ids)
    {
        if (nid == player_nation(w)) continue;
        const nation_component& nc = w.nations.at(nid);
        const float floor = corp_reserve_floor(w, reg, nid, p);
        const float room = state_uncommitted_treasury(w, nid);
        ++n;
        if (nc.treasury > floor) ++above;
        if (is_major_nation(w, nid) || nc.treasury > floor)
            std::printf("  %s %llu: buildings %zu  treasury %.1f  uncommitted %.1f  floor %.1f  %s\n",
                        is_major_nation(w, nid) ? "MAJOR" : "minor", static_cast<unsigned long long>(nid),
                        nc.state_assets.size(), static_cast<double>(nc.treasury), static_cast<double>(room),
                        static_cast<double>(floor), nc.treasury > floor ? "ABOVE" : "below");
    }
    std::printf("  %d of %d non-player nations above their floor\n", above, n);
}

} // namespace

int main(int argc, char** argv)
{
    const std::uint32_t seed = argc > 1 ? static_cast<std::uint32_t>(std::strtoul(argv[1], nullptr, 10)) : 46u;
    const int ticks = argc > 2 ? std::atoi(argv[2]) : 20;
    lua_state lua;
    world_params params = arc_params(world_arc::shipped);
    params.seed = seed;
    app_start_world out;
    build_app_start_world(lua, params, out);
    print_nations(out.w, out.reg, "at the start (after the settle)");
    std::vector<extraction_trace_row> trace;
    corp_extraction_trace_sink() = &trace;
    run_app_live_window(out.w, out.reg, k_app_validation_ticks, ticks);
    corp_extraction_trace_sink() = nullptr;
    print_nations(out.w, out.reg, "after the live window");
    // Extraction candidates the scorer weighed for each MAJOR, by outcome.
    std::map<entity_id, std::map<int, int>> by;
    for (const extraction_trace_row& r : trace)
        if (out.w.nations.count(r.corp) && is_major_nation(out.w, r.corp))
            ++by[r.corp][static_cast<int>(r.outcome)];
    std::printf("--- extraction trace for majors (outcome index: count) ---\n");
    for (const auto& [nid, m] : by)
    {
        std::printf("  major %llu:", static_cast<unsigned long long>(nid));
        for (const auto& [o, n] : m) std::printf(" [%d]=%d", o, n);
        std::printf("\n");
    }
    if (by.empty()) std::printf("  (no extraction candidate was traced for any major)\n");
    // What the scorer DECIDED (the last 256 decisions world-wide), per major.
    std::map<entity_id, std::map<std::string, int>> dec;
    for (const corp_decision& d : out.w.ai_decisions.entries)
        if (out.w.nations.count(d.corp) && is_major_nation(out.w, d.corp))
            ++dec[d.corp][std::string(corp_verb_label(d.command.verb)) + "/" + corp_decision_reason_label(d.reason)];
    std::printf("--- decisions by majors (verb/reason: count) ---\n");
    for (const auto& [nid, m] : dec)
    {
        std::printf("  major %llu:", static_cast<unsigned long long>(nid));
        for (const auto& [k, n] : m) std::printf(" %s=%d", k.c_str(), n);
        std::printf("\n");
    }
    if (dec.empty()) std::printf("  (no decision by any major in the ring)\n");
    return 0;
}
