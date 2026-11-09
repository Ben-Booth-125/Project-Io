// verify_world_supply_probe.cpp -- BL-1544 (every army starves in the verify world). A
// MEASUREMENT: builds the verify world app-free (setup_world + load_economy + the roster half
// of verify_lay_roster; the flags add that function's tail, or variants of it) or the app's
// start world (--real), steps it as verify.econ_step does, and reads what the Logistic Point
// resolver grants each unit and why. A MIRROR of an app-side function, so it can drift from it.
//
//   build_gen/verify/verify_world_supply_probe.exe [--real] [--seed-lp] [--seed-lp-only]
//                                                  [--seed-lp-fold] [--quarters N]
//     (none)          the world with no roads pass and no seeding: the BL-1544 defect
//     --seed-lp       site roads, then seeding: what verify_lay_roster now does
//     --seed-lp-only  seeding with no site roads
//     --seed-lp-fold  --seed-lp, then fold_to_ground
//
// Build: bash tools/verify/build_lua_harness.sh verify_world_supply_probe. Run from the repo root.

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/campaign_settle.hpp"
#include "world/components.hpp"
#include "world/corporation_generation.hpp"
#include "world/economy_system.hpp"
#include "world/state_industry.hpp"
#include "world/logistic_points.hpp"
#include "world/road_generation.hpp"
#include "world/stockpile_budget.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>

namespace {

void sources(const world& w, const char* when)
{
    int hubs = 0, stations = 0, ports = 0, running = 0, on_ground = 0;
    for (const auto& [bid, b] : w.buildings)
    {
        lp_network net;
        if (!lp_source_network(b.type, net)) continue;
        hubs += net == lp_network::road; stations += net == lp_network::rail; ports += net == lp_network::sea;
        running += lp_source_running(b) ? 1 : 0;
        on_ground += w.tile_to_nation.count(b.tile) != 0 ? 1 : 0;
    }
    int nation_units = 0, other_units = 0, on_home = 0;
    for (const auto& [uid, u] : w.units)
    {
        (w.nations.count(u.owner) != 0 ? nation_units : other_units) += 1;
        const auto it = w.tile_to_nation.find(u.position);
        on_home += it != w.tile_to_nation.end() && it->second == u.owner ? 1 : 0;
    }
    std::printf("SOURCES %-8s hubs %d stations %d ports %d (running %d, on a nation's ground %d) | buildings %zu | "
                "units: nation-owned %d other %d on-own-ground %d\n",
                when, hubs, stations, ports, running, on_ground, w.buildings.size(), nation_units, other_units, on_home);
}

void census(const world& w, const lp_tick& lp, const char* when)
{
    int units = 0, men = 0, s0 = 0, spart = 0, sfull = 0, no_row = 0;
    long long food = 0;
    for (const auto& [uid, u] : w.units)
    {
        if (u.count <= 0) continue;
        ++units; men += u.count; food += u.food_permille;
        s0 += u.lp_share_permille <= 0; spart += u.lp_share_permille > 0 && u.lp_share_permille < 1000;
        sfull += u.lp_share_permille >= 1000;
    }
    int rows = 0, zero_need = 0, past = 0, cover0 = 0, cover_part = 0, pool_short = 0, fed = 0;
    for (const lp_supply_row& r : lp.supplies)
    {
        ++rows;
        if (r.need <= 0.0f) { ++zero_need; continue; }
        if (r.paid_tile == null_entity) ++past;
        else if (r.cover <= 0.0f) ++cover0;
        else if (r.cover < 1.0f) ++cover_part;
        else if (r.share_permille < 1000) ++pool_short;
        else ++fed;
    }
    float pool = 0.0f, granted = 0.0f, claim = 0.0f;
    int nrows = 0, nsrc = 0;
    for (const auto& n : lp.nations)
    {
        ++nrows; pool += n.pool; granted += n.supply_granted; claim += n.supply_claim;
        nsrc += n.pool > 0.0f ? 1 : 0;
    }
    (void)no_row;
    std::printf("CENSUS %-8s units %d men %d meanfood %lld | stored share: 0=%d part=%d full=%d | "
                "lp rows %d: need0 %d past-draw %d cover0 %d cover-part %d full-but-pool-short %d fed %d | "
                "pools: %d nations (%d >0) made %.1f claimed %.1f granted %.1f\n",
                when, units, men, units ? food / units : 0, s0, spart, sfull, rows, zero_need, past, cover0,
                cover_part, pool_short, fed, nrows, nsrc, pool, claim, granted);
}

} // namespace

int main(int argc, char** argv)
{
    bool real = false, seed_lp = false, fold = false, no_roads = false;
    int quarters = 6;
    for (int i = 1; i < argc; ++i)
    {
        if (!std::strcmp(argv[i], "--real")) real = true;
        else if (!std::strcmp(argv[i], "--seed-lp")) seed_lp = true;
        else if (!std::strcmp(argv[i], "--seed-lp-only")) seed_lp = no_roads = true;
        else if (!std::strcmp(argv[i], "--seed-lp-fold")) seed_lp = fold = true;
        else if (!std::strcmp(argv[i], "--quarters") && i + 1 < argc) quarters = std::atoi(argv[++i]);
    }
    lua_state lua;
    auto s = std::make_unique<app_start_world>();
    world_params p{};
    int first_step = k_campaign_settle_ticks;
    if (real)
    {
        build_app_start_world(lua, p, *s);
        sources(s->w, "gen");
        for (int k = 0; k < k_app_validation_ticks; ++k)
            (void)run_settle_tick(s->w, s->reg, k, 0, /*spectating=*/true);
        clear_research_at_handover(s->w);
    }
    else
    {
        build_app_base_world(lua, p, *s);
        world& w = s->w;
        const seed_candidate_spend scs =
            spend_stockpile_on_seed_candidate(w, s->reg, p.seed, s->cfg.corporation_count);
        std::printf("ROSTER spent=%d\n", scs.spent ? 1 : 0);
        if (!scs.spent)
        {
            enforce_chain_feasible_roster(w, s->reg, p.seed ^ 0x4A71012u);
            generate_background_firms(w, s->reg, 0x8A21F00Du);
        }
        place_opening_stock(w);
        if (seed_lp)
        {
            const site_road_stats st = no_roads ? site_road_stats{} : lay_site_roads_every_body(w);
            std::printf("VARIANT site roads: %d sites %d unroaded -> %d joined\n", st.sites, st.sites_roadless, st.sites_joined);
            const lp_seed_report r = seed_logistics_sources(w);
            print_lp_seed_report(r);
            if (fold) (void)fold_to_ground(w);
        }
    }
    world& w = s->w;
    sources(w, "start");
    census(w, resolve_logistic_points(w, collect_supply_claims(w, s->reg)), "start");
    for (int q = 1; q <= quarters; ++q)
    {
        ++w.current_day_tick;
        settle_tick_result r = run_settle_tick(w, s->reg, first_step + q - 1, real ? q * 90 : 0, /*spectating=*/false);
        char label[16];
        std::snprintf(label, sizeof(label), "q%d", q);
        census(w, r.report.logistic_points, label);
    }
    return 0;
}
