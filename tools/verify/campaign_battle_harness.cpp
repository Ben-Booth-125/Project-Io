// ---------------------------------------------------------------------------
// Headless campaign battle resolver harness (BL-315; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises src/world/campaign_battle.cpp — the SECOND combat path, the one
// that fights unit tokens standing on a tile over a short span, with a
// withdrawal window. The Era -1 path (combat.cpp's resolve_battle) is left
// alone; it is only used here as the shared power-scoring kernel, which is the
// property that keeps the two from drifting.
//
//   C1  REPLAY DETERMINISM. The same identity and the same inputs produce a
//       byte-identical outcome, per-round records included, across two runs.
//   C2  THE RANDOMNESS IS REAL. Different battle identities produce different
//       fights — the swing reaches the outcome, it is not decoration.
//   C3  WITHDRAWAL IS PRICED BY TIME. Breaking off early costs strictly less
//       than breaking off late, on the same battle and the same stream.
//   C4  STRENGTH USUALLY WINS. A decisively stronger force wins MOST battles
//       over a spread of seeds — not all of them. Both halves are asserted:
//       an unbeatable favourite would mean the uncertainty is fake.
//   C5  NO DRIFT ON THE OBVIOUS CASE. Where the Era -1 resolver calls a fight
//       lopsided, the campaign resolver agrees on the winner.
//
// C4's rate threshold is a PREDICTION, not a tuned-to-pass number: the measured
// rate is printed on every run, and a rate outside the expectation is a finding
// to explain rather than a threshold to lower.
//
// The process exits non-zero if any assertion FAILs.

#include "world/campaign_battle.hpp"
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

/// Field equality including the per-round trace — "byte-identical outcome" in
/// the only sense that matters, i.e. every number a caller could read.
bool same_outcome(const campaign_battle_outcome& a, const campaign_battle_outcome& b)
{
    if (a.end != b.end || a.result != b.result || a.rounds_fought != b.rounds_fought) return false;
    if (a.attacker_losses_permille != b.attacker_losses_permille) return false;
    if (a.defender_losses_permille != b.defender_losses_permille) return false;
    if (a.attacker_strength_permille != b.attacker_strength_permille) return false;
    if (a.defender_strength_permille != b.defender_strength_permille) return false;
    if (a.decisiveness != b.decisiveness || a.stream_seed != b.stream_seed) return false;
    if (a.rounds.size() != b.rounds.size()) return false;
    for (size_t i = 0; i < a.rounds.size(); ++i)
    {
        const campaign_battle_round& x = a.rounds[i];
        const campaign_battle_round& y = b.rounds[i];
        if (x.round != y.round || x.attacker_power != y.attacker_power ||
            x.defender_power != y.defender_power || x.round_winner != y.round_winner ||
            x.attacker_strength_permille != y.attacker_strength_permille ||
            x.defender_strength_permille != y.defender_strength_permille)
            return false;
    }
    return true;
}

const doctrine_row neutral{};

/// Every battle below is fought on open grassland plains, where terrain_defence
/// is zero — so nothing but headcount, the matchup matrix and the seeded swing
/// decides it, and a strength-vs-outcome claim means what it says.
campaign_battle_outcome fight(const campaign_battle_identity& id,
                              const std::vector<army_stack_entry>& atk,
                              const std::vector<army_stack_entry>& def,
                              withdrawing_side withdrawer = withdrawing_side::none,
                              int withdraw_after_round = 0)
{
    return resolve_campaign_battle(id, atk, neutral, def, neutral,
                                    terrain_composition::grassland, terrain_landform::plains,
                                    season::summer, 1000, 1000,
                                    withdrawer, withdraw_after_round);
}

campaign_battle_identity id_at(uint64_t tick)
{
    campaign_battle_identity id;
    id.attacker   = 11;
    id.defender   = 22;
    id.tile_index = 4242;
    id.tick       = tick;
    id.world_seed = 0xBEEF;
    return id;
}

const char* end_name(campaign_battle_end e)
{
    switch (e)
    {
        case campaign_battle_end::in_progress:       return "in-progress";
        case campaign_battle_end::attacker_broken:   return "attacker broken";
        case campaign_battle_end::defender_broken:   return "defender broken";
        case campaign_battle_end::attacker_withdrew: return "attacker withdrew";
        case campaign_battle_end::defender_withdrew: return "defender withdrew";
        case campaign_battle_end::stalemate:         return "stalemate";
    }
    return "?";
}

} // namespace

int main()
{
    std::printf("=== campaign battle resolver (BL-315) ===\n\n");

    // --- C1: replay determinism -------------------------------------------
    {
        const std::vector<army_stack_entry> atk = stack_of(unit_class::infantry, 500, 20, 11);
        const std::vector<army_stack_entry> def = stack_of(unit_class::cavalry,  400,  0, 22);

        const campaign_battle_outcome a = fight(id_at(9000), atk, def);
        const campaign_battle_outcome b = fight(id_at(9000), atk, def);
        check(same_outcome(a, b),
              "C1 the same identity and inputs replay to an identical outcome, per-round trace included");

        // The stepped API and the scripted wrapper must resolve identically —
        // otherwise a player-driven fight and a headless one diverge.
        campaign_battle_state st = begin_campaign_battle(id_at(9000), atk, neutral, def, neutral,
                                                          terrain_composition::grassland,
                                                          terrain_landform::plains,
                                                          season::summer, 1000, 1000);
        while (step_campaign_battle(st)) {}
        check(same_outcome(campaign_battle_result(st), a),
              "C1 stepping the battle round-by-round gives the same result as the scripted wrapper");

        std::printf("      %s after %d rounds; attacker %d/1000 left, defender %d/1000\n",
                    end_name(a.end), a.rounds_fought,
                    a.attacker_strength_permille, a.defender_strength_permille);
    }

    // --- C1: the seed really is folded from the identity -------------------
    {
        check(campaign_battle_seed(id_at(1)) != campaign_battle_seed(id_at(2)),
              "C1 a different tick folds to a different stream seed");

        campaign_battle_identity swapped = id_at(1);
        swapped.attacker = 22; swapped.defender = 11;
        check(campaign_battle_seed(id_at(1)) != campaign_battle_seed(swapped),
              "C1 swapping attacker and defender is a different battle, so a different stream");
    }

    // --- C2: different seeds give different fights -------------------------
    {
        // Near-even armies, where the swing has the most room to matter.
        const std::vector<army_stack_entry> atk = stack_of(unit_class::infantry, 500);
        const std::vector<army_stack_entry> def = stack_of(unit_class::infantry, 480);

        int distinct = 0, attacker_wins = 0;
        const campaign_battle_outcome first = fight(id_at(1), atk, def);
        for (uint64_t t = 1; t <= 64; ++t)
        {
            const campaign_battle_outcome o = fight(id_at(t), atk, def);
            if (!same_outcome(o, first)) ++distinct;
            if (o.result == battle_result::attacker_victory) ++attacker_wins;
        }
        std::printf("      64 near-even battles: %d differ from the first, attacker took %d\n",
                    distinct, attacker_wins);
        check(distinct > 0,
              "C2 different battle identities produce different fights - the randomness reaches the outcome");
        check(attacker_wins > 0 && attacker_wins < 64,
              "C2 a near-even fight is genuinely uncertain: both sides win some of the 64");
    }

    // --- C3: withdrawing early costs less than withdrawing late ------------
    {
        // A slightly-ahead attacker, so it survives long enough for a late
        // window to exist at all and the comparison is like-for-like.
        const std::vector<army_stack_entry> atk = stack_of(unit_class::infantry, 520);
        const std::vector<army_stack_entry> def = stack_of(unit_class::infantry, 480);

        int compared = 0, monotone = 0;
        for (uint64_t t = 100; t < 120; ++t)
        {
            int prev_cost = -1;
            bool ok = true, usable = true;
            for (int r = 1; r <= 5; ++r)
            {
                const campaign_battle_outcome o = fight(id_at(t), atk, def, withdrawing_side::attacker, r);
                // Only comparable while the attacker actually got to withdraw —
                // a battle that ended first has no window at that round.
                if (o.end != campaign_battle_end::attacker_withdrew) { usable = false; break; }
                if (prev_cost >= 0 && o.attacker_losses_permille <= prev_cost) ok = false;
                prev_cost = o.attacker_losses_permille;
            }
            if (!usable) continue;
            ++compared;
            if (ok) ++monotone;
        }
        std::printf("      withdrawal cost across rounds 1..5: %d/%d seeds strictly increasing\n",
                    monotone, compared);
        check(compared > 0, "C3 setup: at least one seed leaves a withdrawal window open through round 5");
        check(compared > 0 && monotone == compared,
              "C3 withdrawing later always costs more than withdrawing earlier, on the same stream");

        // And the headline comparison, stated plainly.
        const campaign_battle_outcome early = fight(id_at(100), atk, def, withdrawing_side::attacker, 1);
        const campaign_battle_outcome late  = fight(id_at(100), atk, def, withdrawing_side::attacker, 5);
        std::printf("      seed 100: withdraw after 1 round costs %d/1000, after 5 rounds costs %d/1000\n",
                    early.attacker_losses_permille, late.attacker_losses_permille);
        check(early.attacker_losses_permille < late.attacker_losses_permille,
              "C3 a cheap early exit and an expensive late one - committing force is a real decision");

        // Withdrawal is not a defeat flag: the side that stayed holds the field.
        check(early.end == campaign_battle_end::attacker_withdrew &&
              early.result == battle_result::defender_victory,
              "C3 withdrawal is its own end state, and the side that stayed holds the ground");
    }

    // --- C4: a decisively stronger force USUALLY wins ----------------------
    {
        // A ~1.4:1 edge on open ground: enough that strength should tell over
        // six rounds, not so much that the +/-30% swing cannot ever upset it.
        const std::vector<army_stack_entry> atk = stack_of(unit_class::infantry, 500);
        const std::vector<army_stack_entry> def = stack_of(unit_class::infantry, 357);

        const int trials = 200;
        int wins = 0;
        for (uint64_t t = 1000; t < 1000 + trials; ++t)
            if (fight(id_at(t), atk, def).result == battle_result::attacker_victory) ++wins;

        const int rate = wins * 100 / trials;
        std::printf("      1.4:1 attacker over %d seeds: won %d (%d%%)\n", trials, wins, rate);
        check(rate >= 70,
              "C4 a decisively stronger force wins the large majority of battles (>=70%)");
        check(wins < trials,
              "C4 ... but not all of them - the fight is uncertain, not a lookup of who is bigger");

        // A far larger edge should be near-unloseable; printed so the shape of
        // the strength->victory curve is visible rather than asserted blind.
        const std::vector<army_stack_entry> huge = stack_of(unit_class::infantry, 1500);
        int huge_wins = 0;
        for (uint64_t t = 2000; t < 2000 + trials; ++t)
            if (fight(id_at(t), huge, def).result == battle_result::attacker_victory) ++huge_wins;
        std::printf("      4.2:1 attacker over %d seeds: won %d (%d%%)\n",
                    trials, huge_wins, huge_wins * 100 / trials);
        check(huge_wins >= wins,
              "C4 more strength never wins less often - the curve is monotone in force");
    }

    // --- C5: no drift with the Era -1 path on a lopsided call --------------
    {
        // Where resolve_battle is not close, the campaign path must not
        // disagree. This is the concrete drift check for the two-path split.
        const std::vector<army_stack_entry> atk = stack_of(unit_class::cavalry,  800);
        const std::vector<army_stack_entry> def = stack_of(unit_class::infantry, 200);

        const battle_outcome era = resolve_battle(atk, neutral, def, neutral,
                                                   terrain_composition::grassland,
                                                   terrain_landform::plains,
                                                   season::summer, 1000, 1000);
        int agree = 0;
        const int trials = 50;
        for (uint64_t t = 3000; t < 3000 + trials; ++t)
            if (fight(id_at(t), atk, def).result == era.result) ++agree;

        std::printf("      Era -1 called it %s (decisiveness %d); campaign agreed %d/%d\n",
                    era.result == battle_result::attacker_victory ? "attacker" : "defender",
                    era.decisiveness, agree, trials);
        check(era.decisiveness > 500, "C5 setup: the Era -1 resolver calls this matchup lopsided");
        check(agree == trials,
              "C5 the two paths never disagree on a lopsided fight - shared calibration, not two tunings");
    }

    std::printf("\n=== %s (%d pass, %d fail) ===\n",
                g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
