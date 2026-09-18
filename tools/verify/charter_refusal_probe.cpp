// charter_refusal_probe — the charter spend's REFUSAL CONTRACT, case by case (BL-1039).
//
// `charter_spend_refusal` (src/world/charter_budget.hpp) is the one gate a charter
// budget passes before any mutation: a refused spend is today's world. Its cases
// are cheap to state and expensive to reach through a world build (a full
// player_seed_sweep row is minutes and ~3 GB), so this probe asks the header
// directly — no world, no Lua, milliseconds. It exists because the BL-1039 fix
// round added two refusals no sweep row ever reaches: a per-good cap set under
// the lifted rule (unread there), and a capital rate whose product with the
// richest centre's points leaves the float balance's range (never clamped).
//
// Run:   node tools/verify/build_harness.js charter_refusal_probe
//        ./build_gen/verify/charter_refusal_probe.exe
// Exit:  0 every case as expected, 1 otherwise.

#include "world/charter_budget.hpp"

#include <cstdio>
#include <limits>
#include <map>

namespace {

int g_fail = 0;

void expect(const char* name, const charter_budget& b, const charter_spend_params& s, bool refused)
{
    const char* why = charter_spend_refusal(b, s);
    const bool got = why != nullptr;
    std::printf("  [%s] %-62s %s%s%s\n", got == refused ? "PASS" : "FAIL", name,
                got ? "refused: " : "accepted", got ? why : "", "");
    if (got != refused)
        ++g_fail;
}

/// A spend every rule accepts: the synthetic numbers (firm 1, specialist 4,
/// c 8, guard 200) under the fixed rule and the draw.
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
    const charter_budget huge(std::map<entity_id, std::int32_t>{
        { 1, std::numeric_limits<std::int32_t>::max() } });

    std::printf("charter_refusal_probe — charter_spend_refusal, case by case\n");

    // --- the empty budget is never refused, whatever the params ---
    expect("empty budget, default (unpriced) params", empty, charter_spend_params{}, false);

    // --- no shipped default ---
    expect("default params on a non-empty budget", small, charter_spend_params{}, true);
    expect("base spend (fixed, draw)", small, base(), false);
    { auto s = base(); s.max_firms_per_body = 0;    expect("guard 0", small, s, true); }
    { auto s = base(); s.per_resource_firm_cap = 0; expect("fixed rule, c 0", small, s, true); }

    // --- lifted: c is unread, so a set c is refused (fix round) ---
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

    // --- the capital rate ---
    { auto s = base(); s.capital_per_point = 100.0f; expect("draw with a rate set (unread)", small, s, true); }
    { auto s = base(); s.capital_rule = charter_capital_rule::unspent_points;
      expect("unspent rule, rate 0", small, s, true); }
    { auto s = base(); s.capital_rule = charter_capital_rule::unspent_points;
      s.capital_per_point = std::numeric_limits<float>::infinity();
      expect("unspent rule, rate +inf", small, s, true); }
    { auto s = base(); s.capital_rule = charter_capital_rule::unspent_points; s.capital_per_point = 100.0f;
      expect("unspent rule, rate 100", small, s, false); }

    // --- the capital's range (fix round): refused WHOLE, never clamped ---
    // A finite rate whose product with the richest centre's points is past
    // FLT_MAX: the narrowing cast would write an infinite balance.
    { auto s = base(); s.capital_rule = charter_capital_rule::unspent_points;
      s.capital_per_point = std::numeric_limits<float>::max() / 16.0f;
      expect("unspent rule, INT32_MAX points x FLT_MAX/16 rate", huge, s, true); }
    { auto s = base(); s.capital_rule = charter_capital_rule::unspent_points;
      s.capital_per_point = std::numeric_limits<float>::max() / 64.0f;
      expect("unspent rule, 40 points x FLT_MAX/64 rate (in range)", small, s, false); }
    { auto s = base(); s.capital_rule = charter_capital_rule::unspent_points;
      s.capital_per_point = std::numeric_limits<float>::max() / 32.0f;
      expect("unspent rule, 40 points x FLT_MAX/32 rate (out of range)", small, s, true); }
    { auto s = base(); s.capital_rule = charter_capital_rule::unspent_points; s.capital_per_point = 1.0e6f;
      expect("unspent rule, INT32_MAX points x 1e6 rate (in range)", huge, s, false); }

    // --- the wide product and the domain test themselves ---
    {
        const double w = charter_unspent_capital_wide(9, 100.0f);
        const bool ok = w == 900.0 && charter_capital_in_balance_domain(w)
                     && !charter_capital_in_balance_domain(-1.0)
                     && !charter_capital_in_balance_domain(std::numeric_limits<double>::infinity())
                     && !charter_capital_in_balance_domain(
                            static_cast<double>(std::numeric_limits<float>::max()) * 2.0)
                     && charter_capital_in_balance_domain(
                            static_cast<double>(std::numeric_limits<float>::max()));
        std::printf("  [%s] %-62s\n", ok ? "PASS" : "FAIL", "wide product and balance-domain test");
        if (!ok)
            ++g_fail;
    }

    std::printf("%s (%d failing)\n", g_fail == 0 ? "ALL PASS" : "FAILED", g_fail);
    return g_fail == 0 ? 0 : 1;
}
