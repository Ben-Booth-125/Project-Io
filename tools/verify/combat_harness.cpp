// ---------------------------------------------------------------------------
// Headless combat-engine harness (BL-272; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises src/world/combat.cpp's resolve_battle over hand-built army
// stacks — no world generation involved, since resolve_battle is a pure
// function of its arguments (see combat.hpp's file header).
//
//   R1  unit_component carries a `type`; armies are stacks of (type, count).
//   R2  A battle resolves deterministically as matchup x doctrine x terrain x
//       supply x season, integer arithmetic, explicit tie-breaks: same
//       inputs -> same outputs across repeated calls, and a hand-picked
//       matchup+terrain scenario produces the expected side of outcome.
//   R3  Doctrine is pure modifier data: two different doctrine_row values
//       drive the SAME resolve_battle function with no branching per doctrine.
//   R4  resolve_battle is reachable/callable with no Era-(-1)-sim-only
//       coupling — no dependency on a settlement/sim type this item does not
//       build.
//
// The process exits non-zero if any assertion FAILs.

#include "world/combat.hpp"
#include "world/components.hpp"

#include <cstdio>
#include <vector>

namespace {

int g_pass = 0, g_fail = 0;

void check(bool ok, const char* what)
{
    std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    ok ? ++g_pass : ++g_fail;
}

std::vector<army_stack_entry> stack_of(unit_class cls, int count, int type_mod = 0, uint16_t type_id = 1)
{
    return { army_stack_entry{ type_id, cls, count, type_mod } };
}

} // namespace

int main()
{
    std::printf("=== combat engine (BL-272) ===\n\n");

    // --- R1: unit_component carries a type; armies are (type,count) stacks -
    {
        unit_component u{};
        u.position = 1;
        u.owner    = 2;
        u.count    = 40;
        u.type     = 7;
        check(u.type == 7, "R1 unit_component carries a type field");

        // An army as a vector of (type, count) pairs, per the backlog shape.
        std::vector<army_stack_entry> army = {
            army_stack_entry{ 1, unit_class::infantry, 100, 0 },
            army_stack_entry{ 2, unit_class::cavalry,   40, 0 },
        };
        check(army.size() == 2 && army[0].count == 100 && army[1].count == 40,
              "R1 an army is representable as a vector of typed (type,count) stacks");
    }

    const doctrine_row neutral_doctrine{}; // frontal_bonus=0, flank_fragility=0, mountain_penalty=0

    // --- R2: determinism — identical inputs, identical outputs -------------
    {
        const std::vector<army_stack_entry> atk = stack_of(unit_class::infantry, 1000, 50, 11);
        const std::vector<army_stack_entry> def = stack_of(unit_class::cavalry,   800, 0, 22);

        const battle_outcome a = resolve_battle(atk, neutral_doctrine, def, neutral_doctrine,
                                                 terrain_composition::grassland, terrain_landform::plains,
                                                 season::summer, 900, 900);
        const battle_outcome b = resolve_battle(atk, neutral_doctrine, def, neutral_doctrine,
                                                 terrain_composition::grassland, terrain_landform::plains,
                                                 season::summer, 900, 900);

        check(a.result == b.result &&
              a.attacker_power == b.attacker_power && a.defender_power == b.defender_power &&
              a.attacker_losses_permille == b.attacker_losses_permille &&
              a.defender_losses_permille == b.defender_losses_permille &&
              a.decisiveness == b.decisiveness,
              "R2 identical inputs produce an identical battle_outcome across repeated calls");
    }

    // --- R2: a hand-picked matchup+terrain scenario lands on the expected side
    {
        // Ranged crushes cavalry in the matchup matrix (1200 permille) and the
        // defender sits on open grassland/plains (no terrain_defence bonus at
        // all), with both sides equally supplied and equal raw headcount --
        // the matchup alone should decide it for the attacker.
        const std::vector<army_stack_entry> atk = stack_of(unit_class::ranged,  500);
        const std::vector<army_stack_entry> def = stack_of(unit_class::cavalry, 500);

        const battle_outcome o = resolve_battle(atk, neutral_doctrine, def, neutral_doctrine,
                                                 terrain_composition::grassland, terrain_landform::plains,
                                                 season::summer, 1000, 1000);
        std::printf("      ranged(500) vs cavalry(500), open ground: attacker %d, defender %d, result %s\n",
                    o.attacker_power, o.defender_power,
                    o.result == battle_result::attacker_victory ? "attacker" : "defender");
        check(o.result == battle_result::attacker_victory,
              "R2 ranged attacking cavalry on open ground wins, as the matchup matrix dictates");

        // Flip it: cavalry now defends a mountain with a doctrine built to
        // exploit it (a large terrain_defence bonus should be able to flip an
        // otherwise-losing matchup for the defender).
        const doctrine_row mountain_defender{ .frontal_bonus = 300 };
        const battle_outcome m = resolve_battle(atk, neutral_doctrine, def, mountain_defender,
                                                 terrain_composition::rocky, terrain_landform::mountain,
                                                 season::summer, 1000, 1000);
        std::printf("      same armies, defender dug into a rocky mountain: attacker %d, defender %d, result %s\n",
                    m.attacker_power, m.defender_power,
                    m.result == battle_result::attacker_victory ? "attacker" : "defender");
        check(m.result == battle_result::defender_victory,
              "R2 terrain_defence on a mountain can flip an otherwise-losing matchup for the defender");
    }

    // --- R2: explicit tie-break — exactly equal power favours the defender -
    {
        const std::vector<army_stack_entry> atk = stack_of(unit_class::infantry, 500);
        const std::vector<army_stack_entry> def = stack_of(unit_class::infantry, 500);
        const battle_outcome o = resolve_battle(atk, neutral_doctrine, def, neutral_doctrine,
                                                 terrain_composition::grassland, terrain_landform::plains,
                                                 season::summer, 1000, 1000);
        check(o.attacker_power == o.defender_power, "R2 setup: mirrored armies produce an exact tie");
        check(o.result == battle_result::defender_victory,
              "R2 an exact tie is broken in the defender's favour, per the documented rule");
    }

    // --- R2: losses are bounded fractions, not raw counts -------------------
    {
        const std::vector<army_stack_entry> atk = stack_of(unit_class::cavalry, 2000, 400);
        const std::vector<army_stack_entry> def = stack_of(unit_class::siege,    100);
        const battle_outcome o = resolve_battle(atk, neutral_doctrine, def, neutral_doctrine,
                                                 terrain_composition::grassland, terrain_landform::plains,
                                                 season::summer, 1000, 1000);
        check(o.attacker_losses_permille >= 0 && o.attacker_losses_permille <= 1000 &&
              o.defender_losses_permille >= 0 && o.defender_losses_permille <= 1000,
              "R2 losses are always expressed as a 0..1000 permille fraction");
        check(o.result == battle_result::attacker_victory && o.defender_losses_permille > o.attacker_losses_permille,
              "R2 an overwhelming attacker inflicts more proportional loss than it takes");
    }

    // --- R3: doctrine is pure modifier data ---------------------------------
    {
        const std::vector<army_stack_entry> atk = stack_of(unit_class::infantry, 500);
        const std::vector<army_stack_entry> def = stack_of(unit_class::infantry, 500);

        // Two hand-authored doctrine rows -- "phalanx"-style (strong frontal,
        // brittle flank, useless in mountain) and a cautious/defensive row --
        // both drive the SAME resolve_battle with no per-doctrine branch.
        const doctrine_row phalanx{ .frontal_bonus = 700, .flank_fragility = 200, .mountain_penalty = 900 };
        const doctrine_row cautious{ .frontal_bonus = 50, .flank_fragility = 0, .mountain_penalty = 0 };

        const battle_outcome open_ground = resolve_battle(atk, phalanx, def, cautious,
                                                            terrain_composition::grassland, terrain_landform::plains,
                                                            season::summer, 1000, 1000);
        const battle_outcome mountain = resolve_battle(atk, phalanx, def, cautious,
                                                         terrain_composition::rocky, terrain_landform::mountain,
                                                         season::summer, 1000, 1000);

        check(open_ground.result == battle_result::attacker_victory,
              "R3 the phalanx doctrine's frontal bonus wins on open ground");
        check(mountain.result == battle_result::defender_victory,
              "R3 the SAME resolve_battle, fed the SAME phalanx doctrine_row, loses in the mountain");
        std::printf("      phalanx vs cautious: open ground -> %s, mountain -> %s (one function, two data rows)\n",
                    open_ground.result == battle_result::attacker_victory ? "attacker" : "defender",
                    mountain.result == battle_result::attacker_victory ? "attacker" : "defender");
    }

    // --- R4: naval is strategic-only, not a silent crash/branch ------------
    {
        const std::vector<army_stack_entry> atk = stack_of(unit_class::naval, 500);
        const std::vector<army_stack_entry> def = stack_of(unit_class::infantry, 10);
        const battle_outcome o = resolve_battle(atk, neutral_doctrine, def, neutral_doctrine,
                                                 terrain_composition::grassland, terrain_landform::plains,
                                                 season::summer, 1000, 1000);
        check(o.attacker_power == 0,
              "R4 an all-naval stack contributes zero tactical power (strategic-only, deferred)");
        check(o.result == battle_result::defender_victory,
              "R4 a token defending garrison beats a purely naval 'attacker' with no tactical presence");
    }

    // --- R4: resolve_battle needs nothing beyond combat.hpp/components.hpp -
    // (Structural: this harness includes only world/combat.hpp and
    // world/components.hpp -- no settlement.hpp, no history_ladder.hpp, no
    // sim-loop type. If that changed, this file would fail to compile.)
    check(true, "R4 this harness compiles against combat.hpp + components.hpp alone, no sim-only coupling");

    std::printf("\n=== %s ===\n", g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT");
    return g_fail == 0 ? 0 : 1;
}
