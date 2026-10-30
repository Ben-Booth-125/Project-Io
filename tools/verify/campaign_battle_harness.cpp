// ---------------------------------------------------------------------------
// Headless campaign battle resolver harness (BL-315, BL-1421; no SDL / Lua / ImGui)
// ---------------------------------------------------------------------------
// Exercises src/world/campaign_battle.cpp — the campaign's land round: soft and
// hard attack against hardness, defence and breakthrough, per-unit HP (men) and
// readiness, the seeded swing, and the withdrawal window. The Era -1 path
// (combat.cpp's resolve_battle) is not called by it and is not exercised here.
//
//   C1  REPLAY DETERMINISM. The same identity and the same inputs produce a
//       byte-identical outcome, per-round records included, across two runs.
//   C2  NO RANDOMNESS (Ben, 2026-10-29: "Let's not add battle randomness ...
//       remove the swing too"). RE-SCOPED from "the randomness is real" and "a
//       fight near the edge is genuinely uncertain": the battle's identity no
//       longer reaches the fight — the same forces on the same ground fight the
//       same battle under any seed, and a step consumes no draw.
//   C3  WITHDRAWAL IS PRICED BY TIME AND POSITION, IN MEN. Breaking off late
//       costs more men than breaking off early, and where it ever costs fewer,
//       the withdrawing side's deficit narrowed. (Its twenty seeds are now one
//       fight, since a seed changes nothing; the rows stand.)
//   C4  STRENGTH DECIDES, AS A STEP. Monotone in force; the same ratio always
//       gives the same result (RE-SCOPED from "uncertain somewhere", by the same
//       ruling); a large edge wins; attacking is dear: at parity the defender
//       holds, and a tie goes to the defender.
//   C7  LIKE FORCES ENTRENCH, AND SUPPLY DECIDES THE STALEMATE (Ben, 2026-10-29).
//       Near parity a fully supplied pair holds for a year; a starved side breaks;
//       a rout stays fast; the round budget stops the headless runner.
//   C5  THE CLASSES AND THE GROUND (R3). Armour beats infantry in the open; a
//       mountain costs an attacker; hardness turns soft attack away.
//   C6  READINESS TAKES THE LARGER SHARE (R2). A fight spends readiness faster
//       than it kills men, a spent unit is out of the fight, and a side with
//       none left fighting is broken.
//
// C4's rates are PREDICTIONS, not tuned-to-pass numbers: the measured curve is
// printed on every run, and a rate outside the expectation is a finding to
// explain rather than a threshold to lower.
//
// The process exits non-zero if any assertion FAILs.

#include "world/campaign_battle.hpp"
#include "world/components.hpp"
#include "world/unit_roster.hpp"

#include <array>
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

/// One unit of @p cls, @p men strong, fully ready and supplied.
land_combatant unit_of(entity_id id, land_class cls, int men, int readiness = 1000)
{
    land_combatant c;
    c.unit      = id;
    c.men       = men;
    c.readiness = readiness;
    c.stats     = land_class_stats(cls);
    return c;
}

std::vector<land_combatant> side_of(land_class cls, int men, entity_id first_id = 100)
{
    return { unit_of(first_id, cls, men) };
}

int men_of(const std::vector<land_combatant>& s)
{
    int n = 0;
    for (const land_combatant& c : s) n += c.men;
    return n;
}

/// Field equality including the per-round trace — "byte-identical outcome" in
/// the only sense that matters, i.e. every number a caller could read.
bool same_outcome(const campaign_battle_outcome& a, const campaign_battle_outcome& b)
{
    if (a.end != b.end || a.result != b.result || a.rounds_fought != b.rounds_fought) return false;
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

struct ground { terrain_substrate sub; terrain_cover cov; std::uint8_t density; terrain_landform lf; };
constexpr ground k_plains   { terrain_substrate::sedimentary, terrain_cover::grass, 150, terrain_landform::plains };
constexpr ground k_mountain { terrain_substrate::rocky,       terrain_cover::none,    0, terrain_landform::mountain };

campaign_battle_outcome fight(const campaign_battle_identity& id,
                              const std::vector<land_combatant>& atk,
                              const std::vector<land_combatant>& def,
                              withdrawing_side withdrawer = withdrawing_side::none,
                              int withdraw_after_round = 0,
                              const ground& g = k_plains,
                              std::vector<land_combatant>* atk_out = nullptr,
                              std::vector<land_combatant>* def_out = nullptr)
{
    return resolve_campaign_battle(id, atk, def, g.sub, g.cov, g.density, g.lf,
                                   withdrawer, withdraw_after_round, {}, atk_out, def_out);
}

campaign_battle_identity id_at(uint64_t tick)
{
    campaign_battle_identity id;
    id.attacker   = 11;
    id.defender   = 22;
    id.tile       = 4242; // the defended tile (2026-10-27; the province envelope is retired)
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

int win_rate(const std::vector<land_combatant>& atk, const std::vector<land_combatant>& def,
             uint64_t first_tick, int trials, const ground& g = k_plains)
{
    int wins = 0;
    for (uint64_t t = first_tick; t < first_tick + static_cast<uint64_t>(trials); ++t)
        if (fight(id_at(t), atk, def, withdrawing_side::none, 0, g).result == battle_result::attacker_victory)
            ++wins;
    return wins;
}

} // namespace

int main()
{
    std::printf("=== campaign battle resolver (BL-315, BL-1421) ===\n\n");

    // --- C1: replay determinism -------------------------------------------
    {
        const std::vector<land_combatant> atk = { unit_of(101, land_class::mechanised, 500),
                                                  unit_of(102, land_class::artillery,  200) };
        const std::vector<land_combatant> def = side_of(land_class::infantry, 600, 201);

        const campaign_battle_outcome a = fight(id_at(9000), atk, def);
        const campaign_battle_outcome b = fight(id_at(9000), atk, def);
        check(same_outcome(a, b),
              "C1 the same identity and inputs replay to an identical outcome, per-round trace included");

        // The stepped API and the scripted wrapper must resolve identically —
        // otherwise a player-driven fight and a headless one diverge.
        std::vector<land_combatant> sa = atk, sd = def;
        campaign_battle_state st = begin_campaign_battle(id_at(9000), sa, sd, k_plains.sub,
                                                         k_plains.cov, k_plains.density, k_plains.lf);
        while (step_campaign_battle(st, sa, sd)) {}
        check(same_outcome(campaign_battle_result(st), a),
              "C1 stepping the battle round-by-round gives the same result as the scripted wrapper");

        std::printf("      %s after %d rounds; attacker readiness %d/1000, defender %d/1000\n",
                    end_name(a.end), a.rounds_fought,
                    a.attacker_strength_permille, a.defender_strength_permille);

        check(campaign_battle_seed(id_at(1)) != campaign_battle_seed(id_at(2)),
              "C1 a different tick folds to a different seed (it varies the dispatch wording only)");
        campaign_battle_identity swapped = id_at(1);
        swapped.attacker = 22; swapped.defender = 11;
        check(campaign_battle_seed(id_at(1)) != campaign_battle_seed(swapped),
              "C1 swapping attacker and defender folds to a different seed");
    }

    // --- C2: no randomness (Ben, 2026-10-29) ----------------------------------
    {
        // RE-SCOPED by the ruling: the battle's identity (its seed) no longer reaches
        // the fight. Equal fights under 64 identities must be the same fight in every
        // number but the seed itself.
        const std::vector<land_combatant> atk = side_of(land_class::infantry, 750);
        const std::vector<land_combatant> def = side_of(land_class::infantry, 500, 201);

        int distinct = 0, attacker_wins = 0;
        campaign_battle_outcome first = fight(id_at(1), atk, def);
        for (uint64_t t = 1; t <= 64; ++t)
        {
            campaign_battle_outcome o = fight(id_at(t), atk, def);
            o.stream_seed = first.stream_seed; // the one field a seed still sets
            if (!same_outcome(o, first)) ++distinct;
            if (o.result == battle_result::attacker_victory) ++attacker_wins;
        }
        std::printf("      64 battles at 1.5:1 under 64 identities: %d differ from the first, attacker took %d\n",
                    distinct, attacker_wins);
        check(distinct == 0,
              "C2 the battle's identity does not reach the fight: the same forces fight the same battle under any seed");
        check(attacker_wins == 0 || attacker_wins == 64,
              "C2 the same ratio always gives the same result - no draw decides a battle");

        // A step consumes no draw: the state's stream fields are untouched by a round.
        std::vector<land_combatant> a = atk, d = def;
        campaign_battle_state st = begin_campaign_battle(id_at(3), a, d, k_plains.sub, k_plains.cov,
                                                         k_plains.density, k_plains.lf);
        const uint64_t rng0 = st.rng_state, seed0 = st.stream_seed;
        for (int r = 0; r < 10 && step_campaign_battle(st, a, d); ++r) {}
        check(st.rng_state == rng0 && st.stream_seed == seed0, "C2 a step consumes no draw (rng_state is never advanced)");
    }

    // --- C3: withdrawing early costs fewer men than withdrawing late -------
    {
        // A slightly-ahead attacker, so it survives long enough for a late window.
        // The cost is MEN lost by the withdrawal instant plus the parting charge;
        // the pursuit term prices the CURRENT position, so where a later exit is
        // ever cheaper, the deficit must have narrowed in between.
        const std::vector<land_combatant> atk = side_of(land_class::infantry, 1000);
        const std::vector<land_combatant> def = side_of(land_class::infantry,  500, 201);

        int compared = 0, monotone = 0, inversions = 0, explained = 0;
        for (uint64_t t = 100; t < 120; ++t)
        {
            const campaign_battle_outcome full = fight(id_at(t), atk, def);
            int prev_cost = -1, prev_behind = -1;
            bool ok = true, usable = true;
            for (int r = 1; r <= 5; ++r)
            {
                std::vector<land_combatant> ao;
                const campaign_battle_outcome o =
                    fight(id_at(t), atk, def, withdrawing_side::attacker, r, k_plains, &ao);
                if (o.end != campaign_battle_end::attacker_withdrew) { usable = false; break; }
                const int cost = men_of(atk) - men_of(ao);
                const campaign_battle_round& at = full.rounds[static_cast<size_t>(r) - 1];
                const int behind = std::max(0, at.defender_strength_permille - at.attacker_strength_permille);
                if (prev_cost >= 0 && cost <= prev_cost)
                {
                    ok = false;
                    ++inversions;
                    if (behind < prev_behind) ++explained;
                }
                prev_cost   = cost;
                prev_behind = behind;
            }
            if (!usable) continue;
            ++compared;
            if (ok) ++monotone;
        }
        std::printf("      men lost withdrawing after rounds 1..5: %d/%d seeds strictly increasing;"
                    " %d/%d inversions from a narrowed deficit\n",
                    monotone, compared, explained, inversions);
        check(compared > 0, "C3 setup: at least one seed leaves a withdrawal window open through round 5");
        check(compared > 0 && monotone * 2 > compared,
              "C3 withdrawing later costs more men than withdrawing earlier");
        check(explained == inversions,
              "C3 a later exit only ever gets cheaper because the deficit narrowed - time is never free");

        std::vector<land_combatant> early_out, late_out;
        const campaign_battle_outcome early =
            fight(id_at(100), atk, def, withdrawing_side::attacker, 1, k_plains, &early_out);
        const campaign_battle_outcome late =
            fight(id_at(100), atk, def, withdrawing_side::attacker, 5, k_plains, &late_out);
        std::printf("      seed 100: withdraw after 1 round costs %d men, after 5 rounds %d men\n",
                    men_of(atk) - men_of(early_out), men_of(atk) - men_of(late_out));
        check(men_of(early_out) > men_of(late_out),
              "C3 a cheap early exit and an expensive late one - committing force is a real decision");
        check(early.end == campaign_battle_end::attacker_withdrew &&
              early.result == battle_result::defender_victory,
              "C3 withdrawal is its own end state, and the side that stayed holds the ground");

        // A MANY-UNIT side pays exactly the quoted share of its standing men, and
        // the men it reports removed are exactly the men its units lost (R5).
        std::vector<land_combatant> many = { unit_of(101, land_class::infantry, 333),
                                             unit_of(102, land_class::mechanised, 251),
                                             unit_of(103, land_class::artillery, 97) };
        std::vector<land_combatant> foe = side_of(land_class::infantry, 500, 201);
        campaign_battle_state st = begin_campaign_battle(id_at(55), many, foe, k_plains.sub,
                                                         k_plains.cov, k_plains.density, k_plains.lf);
        step_campaign_battle(st, many, foe);
        step_campaign_battle(st, many, foe);
        const int standing = men_of(many);
        const int quote    = withdrawal_cost_permille(st, withdrawing_side::attacker);
        const int removed  = withdraw_campaign_battle(st, withdrawing_side::attacker, many);
        std::printf("      three-unit withdrawal: %d standing, quote %d permille, %d removed\n",
                    standing, quote, removed);
        check(removed == standing * quote / 1000 && men_of(many) == standing - removed,
              "C3 a many-unit side is charged exactly the quoted share of its men, and loses exactly that");
    }

    // --- C4: a decisively stronger force USUALLY wins ----------------------
    {
        // Infantry attacking infantry absorbs with its small breakthrough while
        // the defender absorbs with its defence, so attacking is dear and the
        // uncertain band sits well above parity. Expectation: 1:1 -> ~0%,
        // 2:1 -> a real coin, 4:1 -> near-certain.
        struct ratio_probe { const char* label; int atk_men; };
        const ratio_probe probes[] = {
            { "1.0:1", 500 }, { "1.5:1", 750 }, { "2.0:1", 1000 },
            { "2.5:1", 1250 }, { "3.0:1", 1500 }, { "4.0:1", 2000 },
        };
        const std::vector<land_combatant> def = side_of(land_class::infantry, 500, 201);
        const int trials = 200;

        int  prev_wins = -1, first_wins = -1;
        bool monotone = true, a_step = true, decisive_at_the_top = false;
        std::printf("      defender 500 infantry on plains, 200 identities per ratio:\n");
        for (std::size_t i = 0; i < sizeof(probes) / sizeof(probes[0]); ++i)
        {
            const int wins = win_rate(side_of(land_class::infantry, probes[i].atk_men), def,
                                      1000 + i * 1000, trials);
            const int rate = wins * 100 / trials;
            std::printf("        %s  attacker won %3d/%d (%3d%%)%s\n", probes[i].label, wins, trials, rate,
                        (wins > 0 && wins < trials) ? "   <- uncertain" : "");
            if (prev_wins >= 0 && wins < prev_wins) monotone = false;
            if (first_wins < 0) first_wins = wins;
            prev_wins = wins;
            if (wins > 0 && wins < trials) a_step = false;
            if (i + 1 == sizeof(probes) / sizeof(probes[0]) && rate >= 95) decisive_at_the_top = true;
        }
        check(monotone, "C4 more strength never wins less often - the curve is monotone in force");
        check(a_step,
              "C4 the curve is a step: every ratio gives one result under every identity (no randomness)");
        check(decisive_at_the_top, "C4 a large edge wins");
        check(first_wins == 0, "C4 attacking is dear: at parity the defender holds");

        // A tie goes to the defender: two sides broken in the same round.
        {
            std::vector<land_combatant> a = { unit_of(101, land_class::infantry, 500, 0) };
            std::vector<land_combatant> d = { unit_of(201, land_class::infantry, 500, 0) };
            campaign_battle_state st = begin_campaign_battle(id_at(4), a, d, k_plains.sub, k_plains.cov,
                                                             k_plains.density, k_plains.lf);
            step_campaign_battle(st, a, d);
            check(campaign_battle_result(st).result == battle_result::defender_victory,
                  "C4 a tie goes to the defender (both sides broken in one round)");
        }

        // THE INSTRUMENT: a finer sweep, reporting BOTH the result (which counts
        // a stalemate the attacker leads on readiness) and the DECISIVE rate (the
        // defender broken — what moves a province's holder). Printed, not asserted.
        // No seed changes a fight, so one fight a ratio is the whole curve. Without
        // supply recovery (entrench_recovery 0) — C7 measures the supplied case.
        std::printf("      finer sweep (one fight each, no supply recovery): ratio  end  days\n");
        int days_at[9] = {};
        campaign_battle_end end_at[9] = {};
        for (int atk_men = 500; atk_men <= 1500; atk_men += 125)
        {
            const campaign_battle_outcome o = fight(id_at(20000), side_of(land_class::infantry, atk_men), def);
            std::printf("        %4.2f:1   %-18s %d\n", atk_men / 500.0, end_name(o.end), o.rounds_fought);
            days_at[(atk_men - 500) / 125] = o.rounds_fought;
            end_at[(atk_men - 500) / 125]  = o.end;
        }
        // THE RULED TARGETS (Ben, 2026-10-29; MILITARY.md § Battles), one deterministic
        // fight a ratio, with no supply recovery (a supplied parity pair entrenches and
        // holds: C7). Index k is (500 + 125k):500 - 0 parity, 1 is 1.25:1, 2 is 1.5:1, 8 is 3:1.
        check(end_at[0] == campaign_battle_end::attacker_broken && days_at[0] >= 30 && days_at[0] <= 60,
              "C4 TARGET a parity battle is fought out in 30-60 days");
        check(days_at[2] > days_at[0], "C4 TARGET a 1.5:1 battle runs longer than parity");
        check(end_at[8] == campaign_battle_end::defender_broken && days_at[8] < 10,
              "C4 TARGET a rout (3:1) is fast: under ten days");
        check(end_at[1] == campaign_battle_end::attacker_broken && end_at[2] == campaign_battle_end::defender_broken,
              "C4 TARGET the break sits at about 1.5:1 in men (1.25:1 fails, 1.5:1 breaks the defender)");
    }

    // --- C5: the classes and the ground (R3) -------------------------------
    {
        // Every roster row fights as SOME land class, and only a naval row as none.
        bool mapped = true;
        std::array<int, land_class_count> reached{};
        bool modern_declared = true, modern_one_each = true;
        std::array<int, land_class_count> modern_per_class{};
        for (const roster_row& r : unit_roster_table())
        {
            const land_class c = land_class_for(r);
            if (static_cast<int>(c) >= land_class_count) { mapped = false; continue; }
            if ((c == land_class::none) != (r.cls == unit_class::naval)) mapped = false;
            ++reached[static_cast<std::size_t>(c)];
            if (r.band == roster_band::modern)
            {
                if (r.land == k_land_class_from_cls) modern_declared = false;
                ++modern_per_class[static_cast<std::size_t>(c)];
            }
        }
        check(mapped, "C5 every roster row maps to a land class; only naval rows fight as none");

        // BL-1422: all seven land classes are reachable from some row, and the
        // modern band carries exactly one declared row per land class.
        bool all_seven = true;
        for (int c = 0; c < land_class_count; ++c)
        {
            if (static_cast<land_class>(c) == land_class::none) continue;
            std::printf("      land class %d: %d row(s), %d modern\n", c, reached[static_cast<std::size_t>(c)],
                        modern_per_class[static_cast<std::size_t>(c)]);
            if (reached[static_cast<std::size_t>(c)] == 0) all_seven = false;
            if (modern_per_class[static_cast<std::size_t>(c)] != 1) modern_one_each = false;
        }
        check(all_seven, "C5 all seven land classes are reachable from some roster row");
        check(modern_declared && modern_one_each,
              "C5 the modern band declares exactly one row per land class");
        check(modern_row_for(land_class::infantry) >= 0
                  && campaign_opening_unit_row() == static_cast<uint16_t>(modern_row_for(land_class::infantry)),
              "C5 the campaign opening unit row is the modern infantry row");
        check(roster_band_for_capacity(6) != roster_band::modern,
              "C5 no Era -1 ladder capacity reaches the modern band");

        const std::vector<land_combatant> tanks = side_of(land_class::armour,   500);
        const std::vector<land_combatant> foot  = side_of(land_class::infantry, 500, 201);
        const int open  = win_rate(tanks, foot, 5000, 100);
        const int moun  = win_rate(side_of(land_class::infantry, 1000), foot, 6000, 100, k_mountain);
        const int plain = win_rate(side_of(land_class::infantry, 1000), foot, 6000, 100);
        std::printf("      armour vs infantry 1:1 on plains: %d/100; infantry 2:1 on plains %d/100,"
                    " on a mountain %d/100\n", open, plain, moun);
        check(open >= 90, "C5 armour beats infantry of equal men in the open");
        // RE-SCOPED (no randomness): a win rate is 0 or 100, so the ground's cost is
        // read as the assault going from a win on plains to a loss on a mountain.
        {
            std::vector<land_combatant> ap, am;
            const campaign_battle_outcome op = fight(id_at(6000), side_of(land_class::infantry, 1000), foot,
                                                     withdrawing_side::none, 0, k_plains, &ap);
            const campaign_battle_outcome om = fight(id_at(6000), side_of(land_class::infantry, 1000), foot,
                                                     withdrawing_side::none, 0, k_mountain, &am);
            std::printf("      2:1 assault: plains %s in %d days losing %d men; mountain %s in %d days losing %d men\n",
                        end_name(op.end), op.rounds_fought, 1000 - men_of(ap),
                        end_name(om.end), om.rounds_fought, 1000 - men_of(am));
            check(moun < plain || (om.rounds_fought > op.rounds_fought && men_of(am) < men_of(ap)),
                  "C5 a mountain costs an attacker: the same assault loses, or takes longer and costs more men");
        }

        // Hardness turns soft attack away: artillery (soft-heavy) does far less to
        // armour than to infantry, one round, same men, same stream.
        auto one_round_men_lost = [](const std::vector<land_combatant>& atk,
                                     std::vector<land_combatant> def) {
            std::vector<land_combatant> a = atk;
            campaign_battle_state st = begin_campaign_battle(id_at(7), a, def, k_plains.sub,
                                                             k_plains.cov, k_plains.density, k_plains.lf);
            const int before = men_of(def);
            const int ready  = side_readiness(def);
            step_campaign_battle(st, a, def);
            return (before - men_of(def)) * 1000 + (ready - side_readiness(def));
        };
        const std::vector<land_combatant> guns = side_of(land_class::artillery, 500);
        const int on_foot  = one_round_men_lost(guns, side_of(land_class::infantry, 500, 201));
        const int on_tanks = one_round_men_lost(guns, side_of(land_class::armour,   500, 201));
        check(on_tanks * 2 < on_foot, "C5 hardness turns soft attack away: artillery hurts armour far less");
    }

    // --- C6: readiness takes the larger share (R2) -------------------------
    {
        std::vector<land_combatant> ao, dout;
        const campaign_battle_outcome o =
            fight(id_at(8000), side_of(land_class::infantry, 500), side_of(land_class::infantry, 500, 201),
                  withdrawing_side::none, 0, k_plains, &ao, &dout);
        const int men_lost_permille = (500 - men_of(ao)) * 1000 / 500;
        const int ready_lost        = 1000 - ao.front().readiness;
        std::printf("      parity assault: attacker lost %d/1000 men and %d/1000 readiness (%s)\n",
                    men_lost_permille, ready_lost, end_name(o.end));
        check(ready_lost > men_lost_permille, "C6 a fight spends readiness faster than it kills men");

        // A spent unit deals nothing; a side of spent units is broken in a round.
        std::vector<land_combatant> spent = { unit_of(201, land_class::infantry, 500, 0) };
        std::vector<land_combatant> fresh = side_of(land_class::infantry, 500);
        campaign_battle_state st = begin_campaign_battle(id_at(9), fresh, spent, k_plains.sub,
                                                         k_plains.cov, k_plains.density, k_plains.lf);
        const int fresh_before = men_of(fresh);
        const int spent_before = men_of(spent);
        step_campaign_battle(st, fresh, spent);
        check(st.end == campaign_battle_end::defender_broken,
              "C6 a side with no unit left fighting is broken");
        check(men_of(fresh) == fresh_before && men_of(spent) < spent_before,
              "C6 spent units deal nothing and are overrun - the enemy's hits land on them in full");
    }

    // --- C7: like forces entrench, supply decides the stalemate ----------------
    {
        const campaign_battle_params p{};
        auto supplied = [&](std::vector<land_combatant> s, bool fed) {
            // As the battle pass builds a unit: its supply factor scales its hits, and
            // its recovery is `unit_entrench_recovery_permille`'s (the stand-in reads
            // the same factor). A starved unit is at supply 300.
            for (land_combatant& c : s)
            {
                c.supply            = fed ? 1000 : 300;
                c.entrench_recovery = fed ? p.entrench_recovery_permille : 0;
            }
            return s;
        };
        const std::vector<land_combatant> a500 = side_of(land_class::infantry, 500);
        const std::vector<land_combatant> d500 = side_of(land_class::infantry, 500, 201);
        check(battle_entrenched(1000, 1000) && battle_entrenched(1250, 1000) && !battle_entrenched(1300, 1000)
                  && !battle_entrenched(0, 0),
              "C7 the entrenchment band: within 1.25:1 either way; a fight with no hits is not dug in");

        // Two equal, fully supplied sides: a year, run on the round budget.
        const campaign_battle_outcome year =
            resolve_campaign_battle(id_at(1), supplied(a500, true), supplied(d500, true), k_plains.sub,
                                    k_plains.cov, k_plains.density, k_plains.lf, withdrawing_side::none, 0, p,
                                    nullptr, nullptr, 365);
        std::printf("      equal, both supplied: %s after %d days; readiness %d / %d\n", end_name(year.end),
                    year.rounds_fought, year.attacker_strength_permille, year.defender_strength_permille);
        check(year.end == campaign_battle_end::in_progress && year.rounds_fought == 365,
              "C7 two equal fully supplied sides hold each other for a year (live at day 365)");

        // The attacker starved, the defender fed; then the reverse.
        const campaign_battle_outcome atk_starved =
            resolve_campaign_battle(id_at(1), supplied(a500, false), supplied(d500, true), k_plains.sub,
                                    k_plains.cov, k_plains.density, k_plains.lf, withdrawing_side::none, 0, p,
                                    nullptr, nullptr, 3650);
        const campaign_battle_outcome def_starved =
            resolve_campaign_battle(id_at(1), supplied(a500, true), supplied(d500, false), k_plains.sub,
                                    k_plains.cov, k_plains.density, k_plains.lf, withdrawing_side::none, 0, p,
                                    nullptr, nullptr, 3650);
        std::printf("      attacker starved: %s on day %d; defender starved: %s on day %d\n",
                    end_name(atk_starved.end), atk_starved.rounds_fought,
                    end_name(def_starved.end), def_starved.rounds_fought);
        check(atk_starved.end == campaign_battle_end::attacker_broken,
              "C7 a starved attacker against a fed defender breaks");
        check(def_starved.end == campaign_battle_end::defender_broken,
              "C7 a starved defender against a fed attacker breaks - supply, not the dice, decides the stalemate");

        // A rout stays fast: 3:1, both fed.
        const campaign_battle_outcome rout =
            resolve_campaign_battle(id_at(1), supplied(side_of(land_class::infantry, 1500), true),
                                    supplied(d500, true), k_plains.sub, k_plains.cov, k_plains.density,
                                    k_plains.lf, withdrawing_side::none, 0, p, nullptr, nullptr, 3650);
        std::printf("      3:1, both fed: %s on day %d\n", end_name(rout.end), rout.rounds_fought);
        check(rout.end == campaign_battle_end::defender_broken && rout.rounds_fought <= 10,
              "C7 a rout stays fast (3:1 breaks the defender within ten days)");

        // THE FREEZE (cold review, 2026-10-29): at supply 50 on both sides, 300 men a
        // side, a day lands a fraction of a man and of a per-mille of readiness. With
        // the carry the fight still wears down to a break; without it, it froze at
        // 1000/1000 for 20,000 rounds.
        {
            std::vector<land_combatant> la = side_of(land_class::infantry, 300);
            std::vector<land_combatant> ld = side_of(land_class::infantry, 300, 201);
            for (auto* sd : {&la, &ld})
                for (land_combatant& c : *sd) c.supply = 50;
            const campaign_battle_outcome slow =
                resolve_campaign_battle(id_at(1), la, ld, k_plains.sub, k_plains.cov, k_plains.density,
                                        k_plains.lf, withdrawing_side::none, 0, p, nullptr, nullptr, 20000);
            std::printf("      supply 50 both sides, 300 men: %s after %d days\n", end_name(slow.end), slow.rounds_fought);
            check(slow.end == campaign_battle_end::attacker_broken || slow.end == campaign_battle_end::defender_broken,
                  "C7 a side that lands any hits wears the other down: no freeze by truncation (the carry)");
        }

        // The headless runner's budget: a fight with no hits at all stops on it.
        std::vector<land_combatant> mute_a = { unit_of(101, land_class::none, 500) };
        std::vector<land_combatant> mute_d = { unit_of(201, land_class::none, 500) };
        const campaign_battle_outcome mute =
            resolve_campaign_battle(id_at(1), mute_a, mute_d, k_plains.sub, k_plains.cov, k_plains.density,
                                    k_plains.lf, withdrawing_side::none, 0, p, nullptr, nullptr, 50);
        std::printf("      no hits on either side: %s after %d rounds (budget 50)\n", end_name(mute.end),
                    mute.rounds_fought);
        check(mute.end == campaign_battle_end::in_progress && mute.rounds_fought == 50,
              "C7 a fight that lands nothing runs until withdrawn - the headless runner stops on its budget");
    }

    std::printf("\n=== %s (%d pass, %d fail) ===\n",
                g_fail == 0 ? "ALL PASS" : "FAILURES PRESENT", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
