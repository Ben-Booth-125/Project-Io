// ---------------------------------------------------------------------------
// tree_gate_years_probe — BL-1272 (generation without research), task G1.
//
// THE DATES THAT REPLACED THE GENERATION TREES, AND WHAT THEY OPEN.
// docs/generation/trees/TREES.md: the pre-game history runs no technology
// tree; three things the trees gated open on a calendar year instead, for
// every polity alike, save the two conditions below (Ben, 2026-10-10):
//   (a) sea legs        `sea_legs_year` AND naval capacity (subjection across water)
//   (b) post roads      `history_sim_params::post_roads_year` (the treasury-bought third rung)
//   (c) coastal sea techs `coastal_naval_ramp`, coastal polities only (naval deeds)
// Each date is MEASURED, not picked. This probe prints the dates in force
// and, per curated seed, what they open in the generated world: subjections
// and post roads per span, and the fleets the naval ledger opens Exploration
// with.
//
// THE MEASUREMENT (c05844d9, the 16 library seeds, 2026-10-10). The dates
// were read off the history as it ran WITH the trees, by a probe hook in
// history_sim that reported every key gained / sea tech taken at the Invest
// verb with its year, plus a per-round census of living polities and holders.
// The hook existed only for the measurement; this file is its successor.
//
// THE MEASUREMENT CANNOT BE RE-RUN ON THIS TREE: it needs the trees, which are
// gone. To reproduce it, check out c05844d9 and add to history_sim a global
// `void (*g_tree_gate_probe)(int64_t year, int kind, int a, int b)` called
// (i) after each `apply_tree_effects(q)` at the three Invest purchase sites
// with kind 1 / 2 when the `sea_legs` / `post_roads` bit of `q.tree_keys` is
// newly set, a = q.id; (ii) at the EM coastal sea-tech increment with kind 3,
// b = the new `naval_sea_techs`; and (iii) after the round-top fold with the
// living count (kind 10) and the holders of each gate (kinds 11-15). Then
// generate each library seed with `world_params{seed, epoch_year = 0}`, the
// shipped scripts/world_gen.lua and scripts/works.lua (as this file does),
// and take the median of the recorded years per kind, pooled over seeds.
// That version of this file is the BL-1272 G1 run (c05844d9 plus the hook;
// the hook was never committed).
//
//   gate          acquisitions  pooled median  median of per-seed medians  first
//   sea legs           150          1640              1620                  112
//   post roads         191          1560              1552                  -68
//   sea tech #1        253           -92              -104                 -324
//   sea tech #2         68           152               164                 -148
//   sea tech #3         58           176               176                 -144
//
//   Half the living polities never held any gate on any seed before 1929
//   (sea legs, one seed) / 1833 (post roads, one seed) / -67 (sea tech #1,
//   one seed): TREES.md's "the year by which the median polity held the
//   gating node" is undefined, so the acquisition median is the date.
//   Holders among living polities, pooled, at the last round before each
//   close: 1200 -- 23 sea legs / 32 post roads / 116 >=1 tech / 47 >=2 / 38 3,
//   of 1636; 1660 -- 70 / 110 / 113 / 47 / 38 of 1091; 1960 -- 141 / 173 /
//   110 / 46 / 38 of 971.
//
// Usage:  tree_gate_years_probe [--seeds a,b,c]   (default: the library's 16)
// Build:  bash tools/verify/build_lua_harness.sh tree_gate_years_probe
// Run from the repo root (the Lua loads are relative). Report only.
// ---------------------------------------------------------------------------

#include "world/era_minus_one.hpp"
#include "world/hard_coded_world.hpp"
#include "world/history_sim.hpp"
#include "world/works_roster.hpp"
#include "world/world.hpp"
#include "scripting/lua_state.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    std::vector<uint32_t> seeds = {46, 28, 11, 31, 40, 12, 37, 13, 41, 43, 32, 10, 25, 38, 9, 0};
    for (int a = 1; a < argc; ++a)
        if (std::strcmp(argv[a], "--seeds") == 0 && a + 1 < argc)
        {
            seeds.clear();
            const std::string list = argv[++a];
            std::size_t at = 0;
            while (at <= list.size())
            {
                const std::size_t c = list.find(',', at);
                seeds.push_back(static_cast<uint32_t>(
                    std::strtoul(list.substr(at, c == std::string::npos ? std::string::npos : c - at).c_str(), nullptr, 10)));
                if (c == std::string::npos) break;
                at = c + 1;
            }
        }

    lua_state        lua;
    world_gen_config cfg{};
    works_registry   works;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load("scripts/world_gen.lua");
    cfg.load_from_lua(lua);
    lua.load("scripts/works.lua");
    works.load_from_lua(lua);

    const history_sim_params d{};
    std::printf("dates in force: sea legs %lld, post roads %lld, coastal sea techs %lld / %lld / %lld\n\n",
                static_cast<long long>(d.sea_legs_year), static_cast<long long>(d.post_roads_year),
                static_cast<long long>(d.coastal_naval_ramp[0]), static_cast<long long>(d.coastal_naval_ramp[1]),
                static_cast<long long>(d.coastal_naval_ramp[2]));
    std::printf("  seed | alive 1200 | sea techs held @1200 | fleets opened (hulls) | subjections expl / ind |"
                " post roads expl / ind\n");
    for (uint32_t sd : seeds)
    {
        world_params wp;
        wp.seed       = sd;
        wp.epoch_year = 0;
        generation_report     rep;
        era_minus_one_fixture fx;
        (void)make_hard_coded_world(wp, &rep, cfg, /*progress=*/nullptr, &works, &fx);
        int alive = 0;
        int64_t techs = 0;
        for (const polity& q : fx.pre_exploration_polities)
            if (q.alive) { ++alive; techs += q.naval_sea_techs; }
        std::printf("  %4u | %10d | %20lld | %10lld (%8lld) | %10lld / %-4lld | %10lld / %lld\n", sd, alive,
                    static_cast<long long>(techs),
                    static_cast<long long>(fx.exploration_state.naval_fleets_opened),
                    static_cast<long long>(fx.exploration_state.naval_hulls_opened),
                    static_cast<long long>(fx.exploration_state.subjections_formed),
                    static_cast<long long>(fx.industrialisation_state.subjections_formed),
                    static_cast<long long>(fx.exploration_state.post_roads_built),
                    static_cast<long long>(fx.industrialisation_state.post_roads_built));
        std::fflush(stdout);
    }
    return 0;
}
