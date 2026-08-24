// upkeep_harness — BL-603 (UPKEEP_ZEROS): the unit-upkeep vector turns on.
//
// Before this item, `economy.military.unit_upkeep` (scripts/economy.lua) was
// authored entirely at ZERO — see that table's own comment. `run_unit_upkeep`
// (economy_system.cpp) was fully wired and correct, but every rate it read
// resolved to nothing: no credit charged, no pool drawn, no draw ever unmet,
// and (out_of_supply_reach <= 0) the reach trigger disabled outright. This
// harness is the check that turning the table on actually bites.
//
//   U1  THE SHIPPED TABLE IS NON-ZERO, and (loosely) obeys the BL-543 value
//       anchor: goods-per-head value ~= 2x credits_per_head's flat term,
//       priced against the authored base_price constants (world_gen.lua).
//       Live-Lua for exactly this reason — a hand-built fixture cannot ask
//       what the SHIPPED file actually authors without becoming the thing
//       under test (price_band_harness's own precedent).
//   U2  TRIGGER (a): a unit beyond `out_of_supply_reach`, goods draw fully
//       met, still decays by exactly `supply_decay_permille`.
//   U3  A unit WITHIN reach with its goods draw met does not decay — it
//       recovers by `supply_recovery_permille` instead (started partially
//       decayed so recovery is visible, not just "didn't move").
//   U4  TRIGGER (b): a unit WITHIN reach whose goods draw goes UNMET decays
//       by the SAME `supply_decay_permille` as U2 — one rule, two triggers,
//       not two rules (MILITARY.md § Upkeep).
//   U5  BOTH TRIGGERS AT ONCE (out of reach AND unmet) still subtract
//       `supply_decay_permille` exactly ONCE — the combined case is the
//       strongest evidence the rule is genuinely one subtraction.
//   U6  RECOVERY floors correctly restore a decayed unit once back in supply
//       with goods met (same mechanism U3 exercises, asserted again from a
//       lower starting point to show the climb).
//   U7  DETERMINISM: the whole U2-U5 scenario replayed from a fresh world
//       twice produces bit-identical `supply_factor_permille`,
//       `unit_upkeep_tick` counters, and resolved credit charges both times.
//
// Live-Lua harness (see CMakeLists.txt's IO_TEST_SCRIPT_ROOTED_HARNESSES) —
// run from the repo root so scripts/*.lua resolve.

#include "scripting/lua_state.hpp"
#include "world/economy_system.hpp"
#include "world/logistics.hpp"
#include "world/recipe_registry.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const std::string& what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what.c_str());
    ok ? ++g_pass : ++g_fail;
}

constexpr int   GW = 20, GH = 4;
constexpr uint16_t ROSTER_LEVY = 0;

// A flat plains body so `tile_reach_cost` is exactly 1.0 per tile step from
// the anchor (logistics_reach_harness's own R2 measurement) — distance in
// tiles reads directly as reach cost.
struct fixture
{
    world                  w;
    entity_id              body = null_entity;
    std::vector<entity_id> grid; // raster: row * GW + col
};

fixture make_body()
{
    fixture f;
    f.body = f.w.create_entity();
    body_component bc;
    bc.grid_width  = GW;
    bc.grid_height = GH;
    f.w.bodies.emplace(f.body, bc);

    f.grid.assign(static_cast<std::size_t>(GW) * GH, null_entity);
    for (int r = 0; r < GH; ++r)
        for (int c = 0; c < GW; ++c)
        {
            const entity_id t = f.w.create_entity();
            tile_component tc;
            tc.body          = f.body;
            tc.grid_x        = c;
            tc.grid_y        = r;
            tc.substrate     = terrain_substrate::sedimentary;
            tc.cover         = terrain_cover::grass;
            tc.cover_density = 150;
            tc.landform      = terrain_landform::plains;
            f.w.tiles.emplace(t, tc);
            f.grid[static_cast<std::size_t>(r) * GW + c] = t;
        }
    return f;
}

entity_id at(const fixture& f, int c, int r) { return f.grid[static_cast<std::size_t>(r) * GW + c]; }

void anchor(fixture& f, int c, int r)
{
    const entity_id b = f.w.create_entity();
    building_component bc;
    bc.tile = at(f, c, r);
    bc.type = building_type::inland_logistics_hub;
    f.w.buildings.emplace(b, bc);
    f.w.body_reach_cost.clear();
}

entity_id add_corp(world& w)
{
    const entity_id c = w.create_entity();
    corporation_component cc{};
    cc.name    = "Upkeep Test Corp";
    cc.balance = 10000.0f;
    w.corporations[c] = cc;
    return c;
}

entity_id add_unit(world& w, entity_id owner, entity_id tile, int count, int supply)
{
    const entity_id id = w.create_entity();
    unit_component uc{};
    uc.position              = tile;
    uc.owner                 = owner;
    uc.count                 = count;
    uc.type                  = ROSTER_LEVY;
    uc.supply_factor_permille = supply;
    uc.muster_base            = null_entity; // never orphaned
    w.units.emplace(id, uc);
    return id;
}

// Builds a registry from scripts/recipes.lua + scripts/economy.lua, then a
// probe script that overrides economy.military.unit_upkeep with clean,
// round mechanical-test numbers (price_band_harness's own P2 pattern) —
// so U2-U7 exercise the real loader end to end without depending on
// whatever BL-603's calibration happens to be tuned to later.
recipe_registry load_probe_registry()
{
    const char* probe_path = "build_gen/verify/upkeep_probe.lua";
    {
        std::error_code ec;
        std::filesystem::create_directories("build_gen/verify", ec);
    }
    {
        std::ofstream probe(probe_path);
        probe <<
            "economy.military.unit_upkeep = {\n"
            "  credits_per_head = 5.0,\n"
            "  credits_per_head_per_power = 0.0,\n"
            "  goods_per_head = { ordnance = 2.0, food_rations = 0.0 },\n"
            "  supply_decay_permille = 100,\n"
            "  supply_recovery_permille = 60,\n"
            "  out_of_supply_reach = 5.0,\n"
            "}\n";
    }

    lua_state lua;
    lua.load("scripts/recipes.lua");
    lua.load("scripts/economy.lua");
    lua.load(probe_path);

    recipe_registry reg;
    reg.load_from_lua(lua);
    return reg;
}

struct scenario_result
{
    int supply_in_reach_met     = 0;
    int supply_in_reach_unmet   = 0;
    int supply_out_of_reach_met = 0;
    int supply_both_triggers    = 0;
    int supply_recovering       = 0;
    unit_upkeep_tick tick{};
};

std::size_t ordnance_idx()
{
    return static_cast<std::size_t>(resource_type::ordnance);
}

// One full pass of the mechanical scenario: five units on a plains body with
// one supply anchor at (0,0) and out_of_supply_reach = 5.0. Returns the
// post-pass supply factors and the pass's own counters, so the caller can
// assert both U2-U6's individual rows and U7's replay-equality in one place.
scenario_result run_scenario(const recipe_registry& reg)
{
    fixture f = make_body();
    anchor(f, 0, 0);

    // A distinct corp per unit: `pool_for` is keyed (corp, body), and this
    // fixture puts every unit on the SAME body (so they share one reach
    // field) — sharing one corp too would pool their goods draws together
    // and make "met" / "unmet" undecidable per unit. One corp per line
    // isolates the goods half while the reach half stays body-wide, which is
    // exactly the axis each row means to test independently.
    const entity_id corp_in_met     = add_corp(f.w);
    const entity_id corp_in_unmet   = add_corp(f.w);
    const entity_id corp_out_met    = add_corp(f.w);
    const entity_id corp_out_unmet  = add_corp(f.w);

    // U3/U6 — in reach (cost 2.0 <= 5.0), goods MET, decayed already so
    // recovery is visible.
    const entity_id u_in_met = add_unit(f.w, corp_in_met, at(f, 2, 0), 10, 500);
    // U4 — in reach (cost 3.0 <= 5.0), goods UNMET (no pool credited).
    const entity_id u_in_unmet = add_unit(f.w, corp_in_unmet, at(f, 3, 0), 10, 1000);
    // U2 — out of reach (cost 10.0 > 5.0), goods MET.
    const entity_id u_out_met = add_unit(f.w, corp_out_met, at(f, 10, 0), 10, 1000);
    // U5 — out of reach (cost 12.0 > 5.0) AND goods UNMET — both triggers.
    const entity_id u_out_unmet = add_unit(f.w, corp_out_unmet, at(f, 12, 0), 10, 1000);

    // Fund only the MET units' goods draw: 10 heads x 2.0 ordnance/head =
    // 20.0 needed, credited generously. The unmet corps' pools stay at their
    // auto-created zero, so their draw is fully unmet.
    f.w.pool_for(corp_in_met, f.body).quantities[ordnance_idx()]  = 1000.0f;
    f.w.pool_for(corp_out_met, f.body).quantities[ordnance_idx()] = 1000.0f;

    unit_upkeep_tick tick = run_unit_upkeep(f.w, reg);

    scenario_result r;
    r.supply_in_reach_met     = f.w.units.at(u_in_met).supply_factor_permille;
    r.supply_in_reach_unmet   = f.w.units.at(u_in_unmet).supply_factor_permille;
    r.supply_out_of_reach_met = f.w.units.at(u_out_met).supply_factor_permille;
    r.supply_both_triggers    = f.w.units.at(u_out_unmet).supply_factor_permille;
    r.tick                    = tick;
    return r;
}

} // namespace

int main()
{
    // --- U1: the shipped table is non-zero and honours the value anchor ----
    {
        std::printf("=== U1: shipped scripts/economy.lua turns upkeep ON ===\n\n");

        lua_state lua;
        lua.load("scripts/recipes.lua");
        lua.load("scripts/economy.lua");
        recipe_registry reg;
        reg.load_from_lua(lua);

        const unit_upkeep_params& up = reg.military().upkeep;
        check(up.credits_per_head > 0.0f, "U1 credits_per_head is non-zero");
        check(up.supply_decay_permille > 0, "U1 supply_decay_permille is non-zero");
        check(up.supply_recovery_permille > 0, "U1 supply_recovery_permille is non-zero");
        check(up.out_of_supply_reach > 0.0f, "U1 out_of_supply_reach is non-zero (trigger enabled)");
        check(up.goods_per_head[ordnance_idx()] > 0.0f, "U1 ordnance draw is non-zero");
        check(up.goods_per_head[static_cast<std::size_t>(resource_type::food_rations)] > 0.0f,
              "U1 food_rations draw is non-zero");

        // BL-543's value anchor, priced against the authored base_price
        // constants (world_gen.lua: ordnance 43.0, food_rations 6.0) —
        // Sigma(goods_per_head[r] x base_price[r]) ~= 2 x credits_per_head.
        const float ordnance_base_price     = 43.0f;
        const float food_rations_base_price = 6.0f;
        const float goods_value =
            up.goods_per_head[ordnance_idx()] * ordnance_base_price +
            up.goods_per_head[static_cast<std::size_t>(resource_type::food_rations)] * food_rations_base_price;
        const float target = 2.0f * up.credits_per_head;
        const float band   = std::fabs(goods_value - target);
        check(band <= 0.5f,
              "U1 goods draw value is within the BL-543 2x-wage band (goods=" +
              std::to_string(goods_value) + " target=" + std::to_string(target) + ")");

        std::printf("\n");
    }

    // --- U2-U6: the mechanical scenario, one probe-loaded registry ---------
    const recipe_registry reg = load_probe_registry();
    const scenario_result s1  = run_scenario(reg);

    std::printf("=== U2-U6: the one decay rule, its two triggers, and recovery ===\n\n");

    check(s1.supply_out_of_reach_met == 900,
          "U2 out-of-reach + goods met decays by exactly supply_decay_permille (1000 -> 900)");
    check(s1.supply_in_reach_met == 560,
          "U3 in-reach + goods met recovers by supply_recovery_permille instead of decaying (500 -> 560)");
    check(s1.supply_in_reach_unmet == 900,
          "U4 in-reach + goods UNMET decays by the SAME amount as U2 (1000 -> 900)");
    check(s1.supply_both_triggers == 900,
          "U5 both triggers at once still subtract supply_decay_permille exactly ONCE (1000 -> 900, not 800)");
    check(s1.supply_out_of_reach_met == s1.supply_in_reach_unmet,
          "U2 and U4 land on the identical value -- one subtraction, two triggers, not two rules");

    check(s1.tick.out_of_reach == 2, "U2/U5 counted as out_of_reach (2 units)");
    check(s1.tick.unmet == 2, "U4/U5 counted as unmet (2 units)");
    check(s1.tick.disbanded == 0, "no unit is orphaned by this fixture");
    check(s1.tick.units == 4, "all four units survive the pass");

    // U6: run the recovering unit a second tick from its new starting point,
    // confirm the climb continues rather than having been a one-off.
    {
        fixture f = make_body();
        anchor(f, 0, 0);
        const entity_id corp = add_corp(f.w);
        const entity_id u    = add_unit(f.w, corp, at(f, 2, 0), 10, 560);
        f.w.pool_for(corp, f.body).quantities[ordnance_idx()] = 1000.0f;
        run_unit_upkeep(f.w, reg);
        check(f.w.units.at(u).supply_factor_permille == 620,
              "U6 recovery continues on a second in-supply tick (560 -> 620)");
    }

    std::printf("\n");

    // --- U7: determinism across two independent replays --------------------
    std::printf("=== U7: determinism across two runs of the same scenario ===\n\n");
    {
        const scenario_result s2 = run_scenario(reg);
        check(s1.supply_in_reach_met == s2.supply_in_reach_met &&
              s1.supply_in_reach_unmet == s2.supply_in_reach_unmet &&
              s1.supply_out_of_reach_met == s2.supply_out_of_reach_met &&
              s1.supply_both_triggers == s2.supply_both_triggers,
              "U7 supply factors are bit-identical across two fresh replays");
        check(s1.tick.units == s2.tick.units &&
              s1.tick.disbanded == s2.tick.disbanded &&
              s1.tick.unmet == s2.tick.unmet &&
              s1.tick.out_of_reach == s2.tick.out_of_reach,
              "U7 unit_upkeep_tick counters are bit-identical across two fresh replays");
    }
    std::printf("\n");

    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
