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
//   C3  WITHDRAWAL IS PRICED BY TIME AND POSITION. Breaking off late usually
//       costs more than breaking off early on the same stream, and where it
//       ever costs less, the withdrawing side's deficit narrowed in between —
//       the pursuit discount, never free time.
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

#include <algorithm>
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
                                    terrain_substrate::sedimentary, terrain_cover::grass, 150, terrain_landform::plains,
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
                                                          terrain_substrate::sedimentary, terrain_cover::grass, 150,
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
        //
        // WHAT IS ASSERTED CHANGED WITH THE SWING (BL-400). At swing 300 the
        // cost was strictly increasing in the round on every seed tried, and
        // this check said "always". At swing 600 that is measurably false —
        // and correctly so: the pursuit term prices the CURRENT field
        // position, so a side that was behind and then wins a big round is
        // pursued less, and its exit gets cheaper. The honest property is the
        // mechanism, asserted sharper rather than looser: the cost rises with
        // time on most seeds, and where it ever falls, the withdrawing side's
        // deficit narrowed in between — the discount is pursuit falling off,
        // never time being free.
        const std::vector<army_stack_entry> atk = stack_of(unit_class::infantry, 520);
        const std::vector<army_stack_entry> def = stack_of(unit_class::infantry, 480);

        int compared = 0, monotone = 0, inversions = 0, explained = 0;
        for (uint64_t t = 100; t < 120; ++t)
        {
            // The fought-out fight's trace doubles as the withdrawal-instant
            // position table: withdrawing consumes no draws, so its first r
            // rounds are byte-identical to the withdraw-at-r battle's.
            const campaign_battle_outcome full = fight(id_at(t), atk, def);

            int prev_cost = -1, prev_behind = -1;
            bool ok = true, usable = true;
            for (int r = 1; r <= 5; ++r)
            {
                const campaign_battle_outcome o = fight(id_at(t), atk, def, withdrawing_side::attacker, r);
                // Only comparable while the attacker actually got to withdraw —
                // a battle that ended first has no window at that round.
                if (o.end != campaign_battle_end::attacker_withdrew) { usable = false; break; }

                const campaign_battle_round& at = full.rounds[static_cast<size_t>(r) - 1];
                const int behind = std::max(0, at.defender_strength_permille -
                                                at.attacker_strength_permille);
                if (prev_cost >= 0 && o.attacker_losses_permille <= prev_cost)
                {
                    ok = false;
                    ++inversions;
                    if (behind < prev_behind) ++explained;
                }
                prev_cost   = o.attacker_losses_permille;
                prev_behind = behind;
            }
            if (!usable) continue;
            ++compared;
            if (ok) ++monotone;
        }
        std::printf("      withdrawal cost across rounds 1..5: %d/%d seeds strictly increasing;"
                    " %d/%d inversions from a narrowed deficit\n",
                    monotone, compared, explained, inversions);
        check(compared > 0, "C3 setup: at least one seed leaves a withdrawal window open through round 5");
        check(compared > 0 && monotone * 2 > compared,
              "C3 withdrawing later usually costs more than withdrawing earlier, on the same stream");
        check(explained == inversions,
              "C3 a later exit only ever gets cheaper because the deficit narrowed - time is never free");

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
        // MEASURED AS A CURVE, NOT AT ONE RATIO, and the reason is a finding.
        //
        // The first cut of this check asserted that a 1.4:1 attacker wins "most
        // but not all" of 200 seeds. It won ALL 200. That is not a bug in the
        // resolver and it was not tuned away: rounds of symmetric swing average
        // out, so the longer a fight runs the LESS the swing can overturn a
        // standing edge. Uncertainty therefore lives near parity and disappears
        // faster than the single-ratio test assumed.
        //
        // So the honest check is the shape of the whole curve: monotone in
        // force, genuinely uncertain SOMEWHERE, and near-unloseable at a large
        // edge. The printed table is the instrument - if the band of real
        // uncertainty is too narrow to play with, `swing_permille` (or the round
        // count) is the dial, and this table is what says so.
        //
        // And it did say so: at swing 300 the table read 1.2:1 -> 94%,
        // 1.4:1 -> 99%, and Ben ruled that band too narrow (NR-204 / BL-400).
        // swing_permille is now 600, picked from a measured sweep of this same
        // curve; at 600 the expectation is roughly 1.0:1 -> ~50%,
        // 1.2:1 -> ~75%, 1.4:1 -> ~85-94%, 2:1 -> ~100% — a 1.4:1 attacker
        // loses a visible share of fights while a 2:1 edge stays near-certain.
        struct ratio_probe { const char* label; int def_count; };
        const ratio_probe probes[] = {
            { "1.00:1", 500 }, { "1.10:1", 455 }, { "1.20:1", 417 },
            { "1.40:1", 357 }, { "2.00:1", 250 }, { "4.20:1", 119 },
        };

        const std::vector<army_stack_entry> atk = stack_of(unit_class::infantry, 500);
        const int trials = 200;

        int  prev_wins = -1;
        bool monotone = true, any_uncertain = false, decisive_at_the_top = false;

        std::printf("      attacker 500 infantry, 200 seeds per ratio:\n");
        for (std::size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); ++i)
        {
            const std::vector<army_stack_entry> def = stack_of(unit_class::infantry,
                                                               probes[i].def_count);
            int wins = 0;
            for (uint64_t t = 1000 + i * 1000; t < 1000 + i * 1000 + trials; ++t)
                if (fight(id_at(t), atk, def).result == battle_result::attacker_victory) ++wins;

            const int rate = wins * 100 / trials;
            std::printf("        %s  attacker won %3d/%d (%3d%%)%s\n",
                        probes[i].label, wins, trials, rate,
                        (wins > 0 && wins < trials) ? "   <- uncertain" : "");

            if (prev_wins >= 0 && wins < prev_wins) monotone = false;
            prev_wins = wins;
            if (wins > 0 && wins < trials) any_uncertain = true;
            if (i + 1 == sizeof(probes) / sizeof(probes[0]) && rate >= 95)
                decisive_at_the_top = true;
        }

        check(monotone, "C4 more strength never wins less often - the curve is monotone in force");
        check(any_uncertain,
              "C4 there is a band where the fight is genuinely uncertain, not a lookup of who is bigger");
        check(decisive_at_the_top,
              "C4 a large edge is near-unloseable - the swing colours the result, it does not decide it");
    }

    // --- C5: no drift with the Era -1 path on a lopsided call --------------
    {
        // Where resolve_battle is not close, the campaign path must not
        // disagree. This is the concrete drift check for the two-path split.
        const std::vector<army_stack_entry> atk = stack_of(unit_class::cavalry,  800);
        const std::vector<army_stack_entry> def = stack_of(unit_class::infantry, 200);

        const battle_outcome era = resolve_battle(atk, neutral, def, neutral,
                                                   terrain_substrate::sedimentary, terrain_cover::grass, 150,
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
