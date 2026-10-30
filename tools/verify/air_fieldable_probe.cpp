// air_fieldable_probe — why the player's air rows read "Not fieldable now" (sprint 60 live
// click, 2026-10-30). Prints, per seed, the player's actor, whether it is a nation, the
// roster gate input, and which air rows available_air_rows returns. Reports; does not gate.
//
//   build_gen/verify/air_fieldable_probe.exe [--seed N]

#include "harness_params.hpp"
#include "scripting/lua_state.hpp"
#include "world/air_combat.hpp"
#include "world/unit_roster.hpp"
#include "world/world.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>

int main(int argc, char** argv)
{
    std::uint32_t seed = 0;
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--seed") == 0 && i + 1 < argc)
            seed = static_cast<std::uint32_t>(std::atoi(argv[++i]));

    lua_state lua;
    world_params p;
    p.seed = seed;
    auto start = std::make_unique<app_start_world>();
    build_app_start_world(lua, p, *start);
    const world& w = start->w;

    const entity_id actor  = player_actor(w);
    const entity_id nation = player_nation(w);
    std::printf("seed %u: actor %llu, player_nation %llu, actor is a nation: %s\n", seed,
                static_cast<unsigned long long>(actor), static_cast<unsigned long long>(nation),
                w.nations.count(actor) ? "yes" : "no");
    const campaign_roster_gate_input g = campaign_gate_input(w, actor);
    std::printf("  gate input: ore %d farm %d energy %d port %d\n", g.ore_q, g.farm_q, g.energy_q, g.port_q);
    const auto rows = available_air_rows(w, actor);
    std::printf("  available air rows: %zu of %zu\n", rows.size(), air_roster_table().size());
    for (std::size_t i = 0; i < air_roster_table().size(); ++i)
    {
        const roster_gate& r = air_roster_table()[i].gate;
        std::printf("  row %zu %-28s gate ore %d energy %d -> %s\n", i, air_roster_table()[i].name, r.ore_q,
                    r.energy_q, std::find(rows.begin(), rows.end(), static_cast<uint16_t>(i)) != rows.end()
                                    ? "fieldable" : "NOT");
    }
    return 0;
}
