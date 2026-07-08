// Headless harness (BL-079): the scoped background-corp agency. A NON-player corp
// with a persistently loss-making building has it idled (decommissioned) by the
// agency after a sustained loss streak; a PLAYER corp with an identical building is
// never auto-acted on. Deterministic. Hand-builds a minimal world (no Lua / SDL /
// ImGui); kept outside src/ so the CMake glob ignores it.

#include "world/budget_system.hpp"
#include "world/components.hpp"
#include "world/economy_system.hpp"
#include "world/market_clearing.hpp"
#include "world/recipe_registry.hpp"
#include "world/world.hpp"

#include <cstdio>

namespace {

int g_failures = 0;
void check(bool ok, const char* label)
{
    std::printf("  [%s] %s\n", ok ? "PASS" : "FAIL", label);
    if (!ok) ++g_failures;
}
std::size_t ri(resource_type r) { return static_cast<std::size_t>(r); }

// A scene with two identical loss-making extraction corps on one body: one player,
// one background. Only `is_player` differs between them.
struct scene { world w; entity_id bg_bld; entity_id pl_bld; };

scene make_scene()
{
    scene s;
    world& w = s.w;

    const entity_id body = w.create_entity();
    w.bodies[body] = body_component{};

    const entity_id market = w.create_entity();
    {
        market_component mc;
        mc.body = body;
        mc.base_price[ri(resource_type::iron_ore)] = 2.5f;
        mc.price = mc.base_price;
        w.markets[market] = mc;
    }

    // Two identical low-deposit tiles (tiny output -> revenue far below opex -> loss).
    auto make_loss_building = [&](bool is_player, const char* name) -> entity_id {
        const entity_id tile = w.create_entity();
        tile_component tc{};
        tc.body = body;
        tc.composition = terrain_composition::rocky;
        tc.resource_deposit[ri(resource_type::iron_ore)]   = 0.02f;  // tiny richness
        tc.resource_remaining[ri(resource_type::iron_ore)] = 1.0e6f; // ample reserve (loss, not exhaustion)
        w.tiles[tile] = tc;

        const entity_id bld = w.create_entity();
        building_component b{};
        b.tile = tile;
        b.type = building_type::extraction_site;
        b.workforce_assigned = 0.5f;
        b.target_resource = resource_type::iron_ore;
        w.buildings[bld] = b;

        const entity_id corp = w.create_entity();
        corporation_component cc;
        cc.name = name;
        cc.is_player = is_player;
        cc.starting_capital = 1000.0f;
        cc.balance = 1000.0f;
        cc.assets.push_back(bld);
        w.corporations[corp] = cc;
        return bld;
    };

    s.bg_bld = make_loss_building(/*is_player=*/false, "Background Co");
    s.pl_bld = make_loss_building(/*is_player=*/true,  "Player Co");
    return s;
}

// Registry: extraction with real opex, so a tiny-output site runs at a clear loss.
recipe_registry make_registry()
{
    recipe_registry reg;
    building_economics ex;
    ex.base_rate = 20.0f; ex.maintenance = 5.0f; ex.base_wage = 8.0f;
    reg.set_economics(building_type::extraction_site, ex);
    return reg;
}

bool run_and_report_idled(int ticks)
{
    scene s = make_scene();
    const recipe_registry reg = make_registry();
    for (int t = 1; t <= ticks; ++t)
    {
        const economy_report rep = run_economy_step(s.w, reg);
        const auto flows = clear_markets(s.w, reg, rep, {});
        apply_budget(s.w, reg, flows, rep.workforce_contention, nullptr);
    }
    const bool bg_idled = s.w.buildings[s.bg_bld].decommissioned;
    const bool pl_idled = s.w.buildings[s.pl_bld].decommissioned;
    std::printf("  after %d ticks: background idled=%s, player idled=%s\n",
                ticks, bg_idled ? "YES" : "no", pl_idled ? "YES" : "no");
    // Encode both facts into asserts by the caller; return bg_idled for determinism.
    check(bg_idled, "BL-079 R1: background corp idles its persistent loss-maker");
    check(!pl_idled, "BL-079 R1: the player's identical building is NEVER auto-idled");
    return bg_idled;
}

} // namespace

int main()
{
    std::printf("BL-079 scoped-agency harness\n");

    // Before the streak threshold (8 ticks) the background building is still running.
    {
        scene s = make_scene();
        const recipe_registry reg = make_registry();
        for (int t = 1; t <= 3; ++t)
        {
            const economy_report rep = run_economy_step(s.w, reg);
            const auto flows = clear_markets(s.w, reg, rep, {});
            apply_budget(s.w, reg, flows, rep.workforce_contention, nullptr);
        }
        check(!s.w.buildings[s.bg_bld].decommissioned,
              "BL-079 R1: agency does not idle early (narrow — requires a sustained streak)");
    }

    // After the streak threshold the background loser is idled; the player is not.
    const bool run1 = run_and_report_idled(12);

    // Determinism: an identical run reproduces the same outcome.
    const bool run2 = run_and_report_idled(12);
    check(run1 == run2, "BL-079 R2: the agency outcome is deterministic across identical runs");

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
