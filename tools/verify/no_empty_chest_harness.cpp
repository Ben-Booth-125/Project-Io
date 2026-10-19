// ---------------------------------------------------------------------------
// no_empty_chest_harness — BL-1386's rules, checked on shipped worlds
// ---------------------------------------------------------------------------
// NATION_GENERATION.md § Pass 7 and EXPLORATION.md § Force persists / the
// tribute clause (Ben, 2026-10-19): no realm closes the history empty. Per
// seed, on the shipped arc with the app's data layer, built and finished as
// the app builds it, this asserts the rules hold where they run:
//
//   R1 the army never takes more than `standing_army_earn_share_q` of the
//      round's earn (every polity-round the sim read, both resumed spans);
//   R2 tribute is drawn from the subject's EARN, before the endowment floor's
//      lift, and never exceeds its share of it;
//   R3 the founding reserve never mints: no major's treasury after the
//      reserve exceeds the chest it held before the foundings;
//   R4 `lp_seed_read_at_gate` restores every `state_idle` flag it toggles.
//
// Build: bash tools/verify/build_lua_harness.sh no_empty_chest_harness
// Run:   build_gen/verify/no_empty_chest_harness.exe [--seeds 28,0]
// ---------------------------------------------------------------------------

#include "harness_params.hpp"
#include "world/components.hpp"
#include "world/era_minus_one.hpp"
#include "world/finish_campaign_world.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/logistic_points.hpp"
#include "world/state_industry.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"
#include "world/works_roster.hpp"
#include "world/world_gen_config.hpp"
#include "scripting/lua_state.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

int g_fail = 0, g_pass = 0;

void check(bool ok, const char* name, const std::string& detail)
{
    std::printf("  %s  %s  %s\n", ok ? "PASS" : "FAIL", name, detail.c_str());
    (ok ? g_pass : g_fail) += 1;
}

std::vector<uint32_t> parse_seeds(const char* s)
{
    std::vector<uint32_t> out;
    std::string cur;
    for (const char* p = s;; ++p)
    {
        if (*p == ',' || *p == '\0')
        {
            if (!cur.empty()) out.push_back(static_cast<uint32_t>(std::strtoul(cur.c_str(), nullptr, 10)));
            cur.clear();
            if (*p == '\0') break;
        }
        else cur.push_back(*p);
    }
    return out;
}

} // namespace

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds{ 28, 0 };
    for (int i = 1; i < argc; ++i)
        if (!std::strcmp(argv[i], "--seeds") && i + 1 < argc) seeds = parse_seeds(argv[++i]);

    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    recipe_registry  reg;
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    reg.load_from_lua(lua);

    for (const uint32_t seed : seeds)
    {
        std::printf("seed %u\n", seed);
        world_params p = arc_params(world_arc::shipped);
        p.seed = seed;
        generation_report     rep{};
        era_minus_one_fixture fx{};
        world w = make_hard_coded_world(p, &rep, cfg, nullptr, &works, &fx);

        // R1 / R2 -- the sim's own reads, both resumed spans.
        int64_t army_rounds = 0, army_over = 0, trib_paid = 0, trib_over = 0;
        for (const history_sim_state* hs : { &fx.exploration_state, &fx.industrialisation_state })
        {
            army_rounds += hs->army_share_rounds;
            army_over   += hs->army_over_share;
            trib_paid   += hs->tribute_payments_on_earn;
            trib_over   += hs->tribute_over_earn;
        }
        check(army_rounds > 0 && army_over == 0, "R1 the army takes at most its share of the earn",
              std::to_string(army_over) + " of " + std::to_string(army_rounds) + " polity-rounds over the share (" +
                  std::to_string(p.standing_army_earn_share_q) + " per mille)");
        check(trib_over == 0, "R2 tribute never exceeds its share of the pre-floor earn",
              std::to_string(trib_over) + " of " + std::to_string(trib_paid) + " payments over it");

        // R3 -- the reserve, inside the finish.
        recipe_registry seed_reg = reg;
        const finish_campaign_result fr = finish_campaign_world(w, rep, seed_reg, p, cfg, nullptr);
        check(fr.reserve.minted == 0, "R3 the founding reserve never mints",
              std::to_string(fr.reserve.minted) + " nation(s) above their pre-founding chest; " +
                  std::to_string(fr.reserve.nations_reserved) + " reserved");

        // R4 -- the LP reading at the gate leaves every flag as it found it.
        std::map<entity_id, bool> before;
        for (const auto& [bid, b] : w.buildings) before[bid] = b.state_idle;
        lp_seed_report lp{};
        lp_seed_read_at_gate(w, seed_reg, lp);
        int changed = 0;
        for (const auto& [bid, b] : w.buildings)
        {
            const auto it = before.find(bid);
            if (it == before.end() || it->second != b.state_idle) ++changed;
        }
        check(changed == 0 && before.size() == w.buildings.size(), "R4 lp_seed_read_at_gate restores every flag",
              std::to_string(changed) + " flag(s) changed over " + std::to_string(before.size()) + " buildings (" +
                  std::to_string(lp.gate_idled_sources) + " gate-idled sources read)");

        // R4b -- the same, where the gate idles: every state nation's chest
        // emptied for the read (so the reading must toggle flags), then put back.
        std::map<entity_id, float> chests;
        for (auto& [nid, nc] : w.nations)
            if (!nc.state_assets.empty()) { chests[nid] = nc.treasury; nc.treasury = 0.0f; }
        const std::vector<entity_id> would_idle = state_gate_would_idle(w, seed_reg);
        lp_seed_report lp0{};
        lp_seed_read_at_gate(w, seed_reg, lp0);
        int changed0 = 0;
        for (const auto& [bid, b] : w.buildings)
        {
            const auto it = before.find(bid);
            if (it == before.end() || it->second != b.state_idle) ++changed0;
        }
        for (const auto& [nid, t] : chests) w.nations.at(nid).treasury = t;
        check(changed0 == 0 && !would_idle.empty(), "R4b ...and where the gate idles (every state chest emptied)",
              std::to_string(changed0) + " flag(s) changed; the gate would idle " + std::to_string(would_idle.size()) +
                  " building(s)");
        std::fflush(stdout);
    }
    std::printf("\nno_empty_chest_harness: %d pass, %d fail -- %s\n", g_pass, g_fail, g_fail == 0 ? "ALL PASS" : "FAIL");
    return g_fail == 0 ? 0 : 1;
}
