// site_idle_floor — BL-1183 C4 (site pays the idle floor)
// ---------------------------------------------------------------------------
// Ben, 2026-10-05 (FINANCE.md § Building operating cost): "A site under
// construction pays the idle floor and nothing else." Until it completes, a
// building charges no wages and no labour maintenance -- exactly what a
// decommissioned building is charged.
//
// A hand-built world: one body, one tile, one corp (the player's, so the corp
// AI never acts), one extraction site under construction with no material cost
// (so it advances one tick per tick, run_construction's market-less branch).
// Each tick drives the REAL path: run_economy_step, then apply_budget with the
// report's contention, rows and labour grants -- the order the app runs them.
//
//   S1  every tick the site is under construction, the corp is billed
//       maintenance == maintenance x idle floor, and wages == 0
//   S2  that is the same figure compute_building_opex charges a decommissioned
//       copy of the building (one rule, not two)
//   S3  the tick it completes (run_construction runs first in the step), the
//       budget charges FULL opex: maintenance > the floor, wages > 0
//   S4  on that tick, building_profit's estimate (estimate_building_profit)
//       reports the same maintenance and wages the budget charged
//
// Build: node tools/verify/build_harness.js site_idle_floor
// Run:   build_gen/verify/site_idle_floor.exe
// ---------------------------------------------------------------------------

#include "world/budget_system.hpp"
#include "world/building_profit.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <map>
#include <unordered_map>

namespace {
int g_fail = 0;
void check(bool ok, const char* what, double got, double want)
{
    std::printf("  %s  %s   (got %.4f, want %.4f)\n", ok ? "PASS" : "FAIL", what, got, want);
    if (!ok) ++g_fail;
}
bool near(double a, double b) { return std::fabs(a - b) < 1e-4; }
}

int main()
{
    recipe_registry reg;
    building_economics ex;
    ex.build_cost           = 0.0f;
    ex.build_duration_ticks = 3.0f;
    ex.maintenance          = 10.0f;
    ex.base_wage            = 5.0f;
    ex.base_rate            = 1.0f;
    reg.set_economics(building_type::extraction_site, ex);
    const float floor_ = reg.idle_maintenance_floor();

    world w;
    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};
    const entity_id tile = w.create_entity();
    { tile_component tc{}; tc.body = body; tc.substrate = terrain_substrate::rocky;
      tc.resource_deposit[static_cast<std::size_t>(resource_type::iron_ore)] = 1.0f;
      w.tiles[tile] = tc; }

    const entity_id corp = w.create_entity();
    { corporation_component cc{}; cc.is_player = true; cc.balance = 1.0e6f; w.corporations[corp] = cc; }
    w.player_entity = corp;

    const entity_id site = w.create_entity();
    { building_component b{}; b.tile = tile; b.type = building_type::extraction_site;
      b.target_resource = resource_type::iron_ore; b.workforce_assigned = 0.5f;
      b.ticks_remaining = 3;
      b.workforce_auto = false; // hold the target at 100: the fixture has no market, so the solver would idle it
      w.buildings[site] = b; }
    w.corporations[corp].assets.push_back(site);

    bool completed_seen = false;
    for (int tick = 1; tick <= 5 && !completed_seen; ++tick)
    {
        const int before = w.buildings.at(site).ticks_remaining;
        economy_report rep = run_economy_step(w, reg);
        std::map<entity_id, corp_budget> bd;
        const std::unordered_map<entity_id, corp_cash_flow> flows;
        apply_budget(w, reg, flows, rep.workforce_contention, &bd, &rep.buildings, &rep.building_labour);
        const building_component& b = w.buildings.at(site);
        const corp_budget& cb = bd[corp];
        std::printf("tick %d: ticks_remaining %d -> %d  decom %d wt %d assigned %.2f  maintenance %.4f  wages %.4f\n",
                    tick, before, b.ticks_remaining, b.decommissioned ? 1 : 0, b.workforce_target, b.workforce_assigned, cb.maintenance, cb.wages);
        if (b.ticks_remaining > 0)
        {
            check(near(cb.maintenance, ex.maintenance * floor_), "S1 site under construction: maintenance is the idle floor",
                  cb.maintenance, ex.maintenance * floor_);
            check(near(cb.wages, 0.0), "S1 site under construction: no wages", cb.wages, 0.0);
            building_component decom = b;
            decom.ticks_remaining = 0;
            decom.decommissioned  = true;
            const building_opex od = compute_building_opex(decom, ex, 1.0f, 1.0f, floor_);
            const building_opex os = compute_building_opex(b, ex, 1.0f, 1.0f, floor_);
            check(near(os.maintenance, od.maintenance) && near(os.wages, od.wages),
                  "S2 site opex == decommissioned opex (one rule)", os.maintenance + os.wages,
                  od.maintenance + od.wages);
        }
        else
        {
            completed_seen = true;
            check(cb.maintenance > ex.maintenance * floor_ + 1e-4, "S3 completion tick: full maintenance",
                  cb.maintenance, ex.maintenance);
            check(cb.wages > 0.0f, "S3 completion tick: wages charged", cb.wages, 0.0);
            const building_profit bp = estimate_building_profit(w, reg, rep, site);
            check(bp.has_data, "S4 estimate has a row on the completion tick", bp.has_data ? 1 : 0, 1);
            check(near(bp.maintenance, cb.maintenance) && near(bp.wages, cb.wages),
                  "S4 building_profit estimate == budget charge", bp.maintenance + bp.wages,
                  cb.maintenance + cb.wages);
        }
    }
    check(completed_seen, "the site completed within 5 ticks", completed_seen ? 1 : 0, 1);
    std::printf(g_fail ? "FAILED (%d)\n" : "ALL PASS\n", g_fail);
    return g_fail ? 1 : 0;
}
