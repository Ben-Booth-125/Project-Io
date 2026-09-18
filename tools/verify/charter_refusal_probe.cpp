// charter_refusal_probe — the charter spend's CONTRACT, case by case (BL-1039).
//
// Two things in src/world/charter_budget.hpp are cheap to state and expensive to
// reach through a world build (a full player_seed_sweep row is minutes and ~3 GB),
// so this probe asks the header directly — no world, no Lua, milliseconds:
//
//  1. `charter_spend_refusal`, the one gate a charter budget passes before any
//     mutation (a refused spend is today's world): every no-default number
//     refused where a rule reads it, and refused where set and unread — a
//     per-good cap under the lifted rule, a ceiling under a legacy rule — which
//     no sweep row ever reaches.
//  2. The square-root rule's ANCHOR (Ben, 2026-09-18: B and B_ref in the same
//     units): at B = B_ref = c x |G| x firm price the cap is c EXACTLY, over a
//     grid of c, |G| and firm prices; the rule steps up only past (k/c)^2 B_ref;
//     and B's per-centre term (`charter_centre_firm_points`) nets out the
//     specialist price in whole firm charters.
//
// Run:   node tools/verify/build_harness.js charter_refusal_probe
//        ./build_gen/verify/charter_refusal_probe.exe
// Exit:  0 every case as expected, 1 otherwise.

#include "world/charter_budget.hpp"

#include <cstdint>
#include <cstdio>
#include <map>

namespace {

int g_fail = 0;

void expect(const char* name, const charter_budget& b, const charter_spend_params& s, bool refused)
{
    const char* why = charter_spend_refusal(b, s);
    const bool got = why != nullptr;
    std::printf("  [%s] %-62s %s%s\n", got == refused ? "PASS" : "FAIL", name,
                got ? "refused: " : "accepted", got ? why : "");
    if (got != refused)
        ++g_fail;
}

void expect_eq(const char* name, long long got, long long want)
{
    const bool ok = got == want;
    std::printf("  [%s] %-62s got %lld, want %lld\n", ok ? "PASS" : "FAIL", name, got, want);
    if (!ok)
        ++g_fail;
}

/// A spend every rule accepts: the synthetic numbers (firm 1, specialist 4,
/// c 8, guard 200) under the fixed rule.
charter_spend_params base()
{
    charter_spend_params s;
    s.firm_price_points        = 1;
    s.specialist_firm_charters = 4;
    s.window_radius            = 4;
    s.resource_cap_rule        = charter_cap_rule::fixed;
    s.per_resource_firm_cap    = 8;
    s.max_firms_per_body       = 200;
    return s;
}

} // namespace

int main()
{
    const charter_budget empty;
    const charter_budget small(std::map<entity_id, std::int32_t>{ { 1, 40 }, { 2, 7 } });

    std::printf("charter_refusal_probe — charter_spend_refusal, case by case\n");

    // --- the empty budget is never refused, whatever the params ---
    expect("empty budget, default (unpriced) params", empty, charter_spend_params{}, false);

    // --- no shipped default ---
    expect("default params on a non-empty budget", small, charter_spend_params{}, true);
    expect("base spend (fixed)", small, base(), false);
    { auto s = base(); s.max_firms_per_body = 0;    expect("guard 0", small, s, true); }
    { auto s = base(); s.per_resource_firm_cap = 0; expect("fixed rule, c 0", small, s, true); }

    // --- lifted: c is unread, so a set c is refused ---
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::lifted;
      expect("lifted rule, c 8 set (unread)", small, s, true); }
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::lifted; s.per_resource_firm_cap = 0;
      expect("lifted rule, c 0", small, s, false); }

    // --- the density ceiling: sqrt only, 0 < ceiling < guard ---
    { auto s = base(); s.density_ceiling = 120; expect("fixed rule with a ceiling set", small, s, true); }
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::sqrt_capital;
      expect("sqrt rule, no ceiling", small, s, true); }
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::sqrt_capital; s.density_ceiling = 120;
      expect("sqrt rule, ceiling 120", small, s, false); }
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::sqrt_capital; s.density_ceiling = 200;
      expect("sqrt rule, ceiling at the guard", small, s, true); }
    { auto s = base(); s.resource_cap_rule = charter_cap_rule::sqrt_capital; s.density_ceiling = 120;
      s.per_resource_firm_cap = 0;
      expect("sqrt rule, c 0", small, s, true); }

    // --- the anchor: cap(B_ref) == c exactly, over a grid ---
    std::printf("\nthe square-root rule's anchor — cap(B_ref = c x |G| x fp) == c\n");
    {
        int cases = 0, wrong = 0;
        for (std::int32_t c = 1; c <= 32; ++c)
            for (int g = 1; g <= 38; ++g)
                for (const std::int32_t fp : { 1, 2, 3, 7, 100, 12345 })
                {
                    const std::int64_t bref = static_cast<std::int64_t>(c) * g * fp;
                    ++cases;
                    if (charter_sqrt_per_good_cap(c, bref, g, fp) != c)
                        ++wrong;
                    // One point short of the reference still keeps c (the floor).
                    if (charter_sqrt_per_good_cap(c, bref - 1, g, fp) != c)
                        ++wrong;
                }
        char name[96];
        std::snprintf(name, sizeof name, "cap(B_ref) == c and cap(B_ref - 1) == c over %d cases", cases);
        expect_eq(name, wrong, 0);
    }
    // The step: cap reaches k exactly at B = (k/c)^2 B_ref (k^2 |G| fp / c when
    // it divides), and not one point before.
    {
        const std::int32_t c = 8, fp = 1;
        const int g = 11;                                    // the library seeds' |G|
        const std::int64_t bref = static_cast<std::int64_t>(c) * g * fp;   // 88
        expect_eq("c 8, |G| 11: B_ref 88", bref, 88);
        // k = 16 at B = 4 B_ref (352), 15 one point before.
        expect_eq("cap(4 x B_ref = 352)", charter_sqrt_per_good_cap(c, 4 * bref, g, fp), 16);
        expect_eq("cap(351)", charter_sqrt_per_good_cap(c, 4 * bref - 1, g, fp), 15);
        // The smoke row's B: 4x on seed 0 is 348 points over 11 specialists at 4.
        expect_eq("cap(348 - 11 x 4 = 304)", charter_sqrt_per_good_cap(c, 304, g, fp), 14);
        // Below the reference: the floor holds.
        expect_eq("cap(1)", charter_sqrt_per_good_cap(c, 1, g, fp), 8);
        expect_eq("cap with |G| 0 keeps c", charter_sqrt_per_good_cap(c, 1000, 0, fp), 8);
    }

    // --- B's per-centre term: points net of the specialist price, whole firms ---
    std::printf("\nB's per-centre term — charter_centre_firm_points\n");
    {
        auto s = base();                                     // firm 1, specialist 4
        expect_eq("40 points, affords a specialist: 36", charter_centre_firm_points(40, s), 36);
        expect_eq("4 points, exactly a specialist: 0", charter_centre_firm_points(4, s), 0);
        expect_eq("3 points, no specialist: 3", charter_centre_firm_points(3, s), 3);
        s.firm_price_points = 3;                             // specialist 12
        expect_eq("fp 3, 40 points: (40 - 12) / 3 = 9 firms = 27", charter_centre_firm_points(40, s), 27);
        expect_eq("fp 3, 11 points, no specialist: 3 firms = 9", charter_centre_firm_points(11, s), 9);
        expect_eq("0 points", charter_centre_firm_points(0, s), 0);
    }

    std::printf("\n%s (%d failing)\n", g_fail == 0 ? "ALL PASS" : "FAILED", g_fail);
    return g_fail == 0 ? 0 : 1;
}
