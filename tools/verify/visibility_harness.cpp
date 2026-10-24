// Headless harness for the Visibility model (BL-068) — no SDL / Lua / ImGui.
// Builds a tiny world by hand and asserts the read-side ownership accessors that
// the competitor information-asymmetry rule branches on:
//   R: owner_nation_of resolves a building to its owning corporation (the reverse of
//      corporation_component::assets), and null_entity for an unowned building.
//   R: is_player_owned is true only for the player corp's buildings (the uniform
//      branch point — every non-player corp is treated identically as a rival).
//
// Build (from repo root):
//   g++ -std=c++20 -I src tools/verify/visibility_harness.cpp src/world/world.cpp \
//       -o visibility_harness
//   ./visibility_harness

#include "world/components.hpp"
#include "world/world.hpp"

#include <cstdio>

static int g_failures = 0;

static void check(bool cond, const char* what)
{
    if (cond) std::printf("  PASS  %s\n", what);
    else { std::printf("  FAIL  %s\n", what); ++g_failures; }
}

int main()
{
    std::printf("Visibility model harness (BL-068)\n");

    world w;

    // A player corporation and two rivals, each owning one building.
    const entity_id player_corp = w.create_entity();
    const entity_id rival_a     = w.create_entity();
    const entity_id rival_b     = w.create_entity();

    auto add_building = [&w](entity_id corp) {
        const entity_id b = w.create_entity();
        w.buildings[b] = building_component{};
        w.nations[corp].state_assets.push_back(b);
        return b;
    };

    { nation_component cc{}; cc.name = "Player Co"; w.nations[player_corp] = cc; }
    { nation_component cc{}; cc.name = "Rival A";   w.nations[rival_a]     = cc; }
    { nation_component cc{}; cc.name = "Rival B";   w.nations[rival_b]     = cc; }
    w.player_entity = player_corp; // BL-1407: the player IS its nation (was cc.is_player)

    const entity_id player_bld = add_building(player_corp);
    const entity_id rival_a_bld = add_building(rival_a);
    const entity_id rival_b_bld = add_building(rival_b);

    // An unowned building (claimed by no corp's assets list).
    const entity_id orphan_bld = w.create_entity();
    w.buildings[orphan_bld] = building_component{};

    // --- R: owner_nation_of resolves the forward assets list in reverse ---
    std::printf("\nR — owner_nation_of resolves ownership\n");
    check(owner_nation_of(w, player_bld)  == player_corp, "player building -> player corp");
    check(owner_nation_of(w, rival_a_bld) == rival_a,     "rival A building -> rival A corp");
    check(owner_nation_of(w, rival_b_bld) == rival_b,     "rival B building -> rival B corp");
    check(owner_nation_of(w, orphan_bld)  == null_entity, "unowned building -> null_entity");
    check(owner_nation_of(w, null_entity) == null_entity, "null building -> null_entity");

    // --- R: is_player_owned is the single uniform branch point ---
    std::printf("\nR — is_player_owned branch (uniform rival treatment)\n");
    check( is_player_owned(w, player_bld),  "player building is player-owned");
    check(!is_player_owned(w, rival_a_bld), "rival A building is NOT player-owned");
    check(!is_player_owned(w, rival_b_bld), "rival B building is NOT player-owned");
    check(!is_player_owned(w, orphan_bld),  "unowned building is NOT player-owned");
    check(!is_player_owned(w, null_entity), "null building is NOT player-owned");

    std::printf("\n%s  (%d failure%s)\n", g_failures ? "FAIL" : "PASS",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures ? 1 : 0;
}
