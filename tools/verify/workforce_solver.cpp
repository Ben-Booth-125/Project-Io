// Headless verification harness for the player workforce auto-solver (BL-181).
// No SDL / Lua / ImGui — exercises solve_workforce_target (economy_system.hpp)
// directly over hand-built worlds and hand-built registries.
//
// The solver's model is LINEAR in the workforce target (output, wages, and the
// labour part of maintenance all scale with it), so at fixed prices its optimum
// would be degenerate bang-bang (0 or 200). The interior optimum exists only
// because of the local market PRICE RESPONSE — more output raises supply, which
// lowers the clearing price via base*sqrt(demand/supply). Property 4 below is the
// load-bearing check on exactly that: the same building solved against a scarce
// market versus a glutted one.
//
// Build (from repo root, after sourcing vcvars64): see tools/verify/README.md.

#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdio>

static constexpr std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

static int g_failures = 0;

static void check(bool ok, const char* name, const char* detail)
{
    std::printf("%s  %s", ok ? "PASS" : "FAIL", name);
    if (detail && detail[0] != '\0')
        std::printf("  [%s]", detail);
    std::printf("\n");
    if (!ok)
        ++g_failures;
}

// ---------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------

// A registry with a single extraction economics profile. The wage/maintenance
// pair is the lever the loss-making case (property 5) turns.
static recipe_registry make_registry(float base_rate, float maintenance, float base_wage)
{
    recipe_registry reg;
    reg.set_thresholds(/*t_full=*/1.0f, /*t_idle=*/0.2f);
    building_economics ex;
    ex.base_rate   = base_rate;
    ex.maintenance = maintenance;
    ex.base_wage   = base_wage;
    reg.set_economics(building_type::extraction_site, ex);
    return reg;
}

// One body, one market, one player-owned iron-ore extraction site.
//   richness  -> the tile's deposit richness (0 = a tile that cannot yield)
//   supply    -> the market's standing supply of iron_ore
//   demand    -> the market's standing demand for iron_ore
//   assigned  -> the building's workforce_assigned
// The building id is returned via `out_building`.
struct fixture
{
    world     w;
    entity_id building = null_entity;
};

static fixture make_fixture(float richness, float supply, float demand,
                            float assigned, float base_price)
{
    fixture f;
    world&  w = f.w;

    const entity_id body = w.create_entity();
    w.bodies[body]       = body_component{};
    w.bodies[body].name  = "SolverTestWorld";

    const entity_id market = w.create_entity();
    {
        market_component mc;
        mc.body = body;
        mc.supply.fill(0.0f);
        mc.demand.fill(0.0f);
        mc.base_price.fill(0.0f);
        mc.base_price[ri(resource_type::iron_ore)] = base_price;
        mc.supply[ri(resource_type::iron_ore)]     = supply;
        mc.demand[ri(resource_type::iron_ore)]     = demand;
        mc.price = mc.base_price;
        w.markets[market] = mc;
    }

    const entity_id tile = w.create_entity();
    {
        tile_component tc{};
        tc.body        = body;
        tc.composition = terrain_composition::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)]   = richness;
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e8f;
        tc.hazard_level = 0.0f;
        w.tiles[tile] = tc;
    }

    f.building = w.create_entity();
    {
        building_component b{};
        b.tile               = tile;
        b.type               = building_type::extraction_site;
        b.workforce_assigned = assigned;
        b.workforce_target   = 100; // the incumbent target the solver reprices against
        b.target_resource    = resource_type::iron_ore;
        w.buildings[f.building] = b;
    }

    const entity_id corp = w.create_entity();
    {
        corporation_component cc;
        cc.name             = "PlayerCorp";
        cc.starting_capital = 1000.0f;
        cc.balance          = 1000.0f;
        cc.assets.push_back(f.building);
        w.corporations[corp] = cc;
    }
    w.player_entity = corp;

    return f;
}

int main()
{
    std::printf("WORKFORCE_SOLVER RESULT\n\n");

    // The baseline economics: base_rate 20, maintenance 5, base_wage 30. At the
    // fixture's richness 2.0 / assigned 1.0 this gives k = 40 units per full
    // workforce tier, against a per-tier cost of ~35 — close enough that the price
    // response, not the linear terms, decides the answer.
    const recipe_registry reg = make_registry(/*base_rate=*/20.0f, /*maintenance=*/5.0f,
                                              /*base_wage=*/30.0f);

    // --- Property 1: determinism ------------------------------------------------
    {
        fixture f = make_fixture(/*richness=*/2.0f, /*supply=*/60.0f, /*demand=*/100.0f,
                                 /*assigned=*/1.0f, /*base_price=*/1.0f);
        const building_component& b = f.w.buildings.at(f.building);
        const int a = solve_workforce_target(f.w, reg, b, 1.0f);
        const int c = solve_workforce_target(f.w, reg, b, 1.0f);
        char d[128];
        std::snprintf(d, sizeof d, "%d == %d", a, c);
        check(a == c, "1. determinism: identical state -> identical target", d);
    }

    // --- Property 2: range and grid ---------------------------------------------
    {
        bool all_ok = true;
        char d[160];
        d[0] = '\0';
        // Sweep a spread of market states so the grid check sees varied answers.
        const float supplies[] = {1.0f, 20.0f, 60.0f, 500.0f, 100000.0f};
        for (const float s : supplies)
        {
            fixture f = make_fixture(2.0f, s, 100.0f, 1.0f, 1.0f);
            const int v = solve_workforce_target(f.w, reg, f.w.buildings.at(f.building), 1.0f);
            if (v < 0 || v > 200 || (v % 10) != 0)
            {
                all_ok = false;
                std::snprintf(d, sizeof d, "supply %.0f -> %d", s, v);
                break;
            }
        }
        check(all_ok, "2. range/grid: result in [0,200] and a multiple of 10", d);
    }

    // --- Property 3: a building that cannot yield solves to 0 -------------------
    {
        // (a) zero deposit richness
        fixture f0 = make_fixture(/*richness=*/0.0f, 60.0f, 100.0f, 1.0f, 1.0f);
        const int v0 = solve_workforce_target(f0.w, reg, f0.w.buildings.at(f0.building), 1.0f);
        char d0[128];
        std::snprintf(d0, sizeof d0, "richness 0 -> %d", v0);
        check(v0 == 0, "3a. zero-yield (deposit richness 0) solves to 0", d0);

        // (b) no workforce assigned
        fixture f1 = make_fixture(2.0f, 60.0f, 100.0f, /*assigned=*/0.0f, 1.0f);
        const int v1 = solve_workforce_target(f1.w, reg, f1.w.buildings.at(f1.building), 1.0f);
        char d1[128];
        std::snprintf(d1, sizeof d1, "workforce_assigned 0 -> %d", v1);
        check(v1 == 0, "3b. zero-yield (workforce_assigned 0) solves to 0", d1);
    }

    // --- Property 4: the price response (load-bearing) ---------------------------
    // The SAME building, the SAME economics, against two market states. Only the
    // market's standing supply of the output differs. If the solver were blind to
    // the price response both would return the identical bang-bang answer.
    {
        fixture scarce  = make_fixture(2.0f, /*supply=*/5.0f,      /*demand=*/400.0f, 1.0f, 1.0f);
        fixture glutted = make_fixture(2.0f, /*supply=*/100000.0f, /*demand=*/100.0f, 1.0f, 1.0f);

        const int vs = solve_workforce_target(scarce.w,  reg, scarce.w.buildings.at(scarce.building),  1.0f);
        const int vg = solve_workforce_target(glutted.w, reg, glutted.w.buildings.at(glutted.building), 1.0f);

        char d[160];
        std::snprintf(d, sizeof d, "scarce -> %d, glutted -> %d", vs, vg);
        check(vs >= vg, "4a. price response: solved(scarce) >= solved(glutted)", d);
        check(vs >  vg, "4b. price response: solved(scarce) >  solved(glutted) (strict)", d);
    }

    // --- Property 4c: the price response actually produces an INTERIOR optimum ---
    // A market whose price falls through the band as this building's output grows
    // (base 1.0, demand 100, standing supply 60): revenue is concave in the target
    // while cost is linear, so the best target is strictly between the two rails.
    // This is the check that distinguishes a real price response from bang-bang.
    {
        fixture f = make_fixture(2.0f, /*supply=*/60.0f, /*demand=*/100.0f, 1.0f, /*base_price=*/1.0f);
        const int v = solve_workforce_target(f.w, reg, f.w.buildings.at(f.building), 1.0f);
        char d[128];
        std::snprintf(d, sizeof d, "solved -> %d (expected strictly between 0 and 200)", v);
        check(v > 0 && v < 200, "4c. price response yields an interior optimum (not bang-bang)", d);
    }

    // --- Property 5: an obviously loss-making building solves to 0 ---------------
    // Output pinned at the price floor (a glutted market) plus a punitive wage:
    // every extra tier costs strictly more than it earns, at every tier.
    {
        const recipe_registry loss_reg = make_registry(/*base_rate=*/20.0f,
                                                       /*maintenance=*/50.0f,
                                                       /*base_wage=*/200.0f);
        fixture f = make_fixture(2.0f, /*supply=*/100000.0f, /*demand=*/1.0f, 1.0f, /*base_price=*/1.0f);
        const int v = solve_workforce_target(f.w, loss_reg, f.w.buildings.at(f.building), 1.0f);
        char d[128];
        std::snprintf(d, sizeof d, "floored price + punitive wage -> %d", v);
        check(v == 0, "5. loss-making building solves to 0", d);
    }

    std::printf("\n%s  (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
