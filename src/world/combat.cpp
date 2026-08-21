#include "combat.hpp"

#include "terrain_combat.hpp"

#include <algorithm>
#include <cstdint>

namespace {

int clamp_permille(int v) { return std::clamp(v, 0, 1000); }

/// Baseline power per unit of a class before any per-type, doctrine or
/// terrain modifier is applied. Illustrative first-cut values, not gospel —
/// the roster table (BL-274) is where real era tuning happens; this is just
/// enough spread to make the class matrix legible. `naval` is 0: naval is
/// strategic-only in this cut (file header), so it never contributes tactical
/// power regardless of what the matrix below says about it.
int class_base_power(unit_class c)
{
    switch (c)
    {
        case unit_class::infantry: return 100;
        case unit_class::cavalry:  return 130;
        case unit_class::ranged:   return  90;
        case unit_class::siege:    return 150;
        case unit_class::naval:    return   0;
    }
    return 0;
}

/// The class x class matchup matrix, per-mille (1000 = neutral). Rows are the
/// attacking class, columns are the defending class — matchup_mult(a, d) is
/// how effective class `a` is when it is the one initiating against `d`.
/// resolve_battle looks BOTH sides up (matchup_mult(atk,def) for the
/// attacker's own power, matchup_mult(def,atk) for the defender's own power),
/// so the table must be internally consistent as a cycle, not just plausible
/// read one row at a time: infantry beats ranged (closes the distance), ranged
/// beats cavalry (volleys stop a charge), cavalry beats infantry (broken by
/// the charge) — a rock-paper-scissors core — with siege uniformly weak
/// against everything else in the open field, because this cut has no
/// fortification to make siege's real job — reducing a position — meaningful;
/// that stays in siege_stance's doctrine modifiers (assault/invest) rather
/// than the matrix.
int matchup_mult(unit_class attacker, unit_class defender)
{
    static constexpr int table[unit_class_count][unit_class_count] = {
        // defender:  infantry  cavalry  ranged  siege  naval
        /* infantry */ { 1000,     700,   1300,  1400,  1000 },
        /* cavalry  */ { 1300,    1000,    700,  1300,  1000 },
        /* ranged   */ {  700,    1300,   1000,  1100,  1000 },
        /* siege    */ {  600,     600,    700,  1000,  1000 },
        /* naval    */ { 1000,    1000,   1000,  1000,  1000 }, // unused: naval never enters tactical power
    };
    return table[static_cast<int>(attacker)][static_cast<int>(defender)];
}

/// Winter is the only season that changes anything in this cut: it sharpens
/// the terrain_attrition contribution (bad roads, no forage). Everything else
/// is 1000 (neutral) — spring/summer/autumn are not distinguished yet.
int season_attrition_multiplier(season s)
{
    return s == season::winter ? 1500 : 1000;
}

/// Sum of (count * per-unit power) across a stack, and the total non-naval
/// unit count, in one pass. Naval entries are skipped entirely — see file
/// header § naval — so they add neither power nor weight to the matchup
/// average computed from `total_count`.
struct stack_totals
{
    std::int64_t power = 0;
    std::int64_t count = 0;
};

stack_totals sum_stack(const std::vector<army_stack_entry>& stack)
{
    stack_totals t;
    for (const army_stack_entry& e : stack)
    {
        if (e.cls == unit_class::naval || e.count <= 0)
            continue;
        const int per_unit = std::max(1, class_base_power(e.cls) + e.type_power_mod);
        t.power += static_cast<std::int64_t>(e.count) * per_unit;
        t.count += e.count;
    }
    return t;
}

/// Weighted average matchup multiplier of `attacker` against the composition
/// of `defender`, per-mille. Weighted by unit COUNT on both sides (a class
/// that makes up half of a huge defending army matters more than one making
/// up half of a token garrison). Returns 1000 (neutral) if either side has no
/// non-naval units, so an empty/all-naval stack never divides by zero.
int weighted_matchup(const std::vector<army_stack_entry>& attacker,
                      const std::vector<army_stack_entry>& defender)
{
    std::int64_t attacker_count = 0, defender_count = 0;
    for (const army_stack_entry& e : attacker)
        if (e.cls != unit_class::naval && e.count > 0) attacker_count += e.count;
    for (const army_stack_entry& e : defender)
        if (e.cls != unit_class::naval && e.count > 0) defender_count += e.count;

    if (attacker_count == 0 || defender_count == 0)
        return 1000;

    std::int64_t weighted_sum = 0; // sum of count_a * count_d * matchup_mult, before the final division
    for (const army_stack_entry& a : attacker)
    {
        if (a.cls == unit_class::naval || a.count <= 0) continue;
        for (const army_stack_entry& d : defender)
        {
            if (d.cls == unit_class::naval || d.count <= 0) continue;
            weighted_sum += static_cast<std::int64_t>(a.count) * d.count * matchup_mult(a.cls, d.cls);
        }
    }
    // Single division at the end (not per-pair) so rounding happens once.
    return static_cast<int>(weighted_sum / (attacker_count * defender_count));
}

/// Apply a per-mille multiplier to a power value, floored at 0. The one
/// arithmetic primitive every modifier step below goes through, so the
/// rounding rule (integer division, truncate toward zero) is stated once.
std::int64_t apply_permille(std::int64_t power, int permille)
{
    return std::max<std::int64_t>(0, power * std::max(0, permille) / 1000);
}

} // namespace

battle_outcome resolve_battle(const std::vector<army_stack_entry>& attacker,
                               const doctrine_row&                 attacker_doctrine,
                               const std::vector<army_stack_entry>& defender,
                               const doctrine_row&                 defender_doctrine,
                               terrain_substrate                    terrain_sub,
                               terrain_cover                        terrain_cov,
                               std::uint8_t                         terrain_density,
                               terrain_landform                    terrain_lf,
                               season                               battle_season,
                               int                                  attacker_supply,
                               int                                  defender_supply)
{
    attacker_supply = clamp_permille(attacker_supply);
    defender_supply = clamp_permille(defender_supply);

    const stack_totals atk_totals = sum_stack(attacker);
    const stack_totals def_totals = sum_stack(defender);

    const int atk_matchup = weighted_matchup(attacker, defender);
    const int def_matchup = weighted_matchup(defender, attacker);

    std::int64_t atk_power = apply_permille(atk_totals.power, atk_matchup);
    std::int64_t def_power = apply_permille(def_totals.power, def_matchup);

    // --- Doctrine: frontal bonus, flank fragility, mountain penalty --------
    atk_power = apply_permille(atk_power, 1000 + attacker_doctrine.frontal_bonus);
    def_power = apply_permille(def_power, 1000 + defender_doctrine.frontal_bonus);

    atk_power = apply_permille(atk_power, 1000 - clamp_permille(attacker_doctrine.flank_fragility));
    def_power = apply_permille(def_power, 1000 - clamp_permille(defender_doctrine.flank_fragility));

    if (terrain_lf == terrain_landform::mountain)
    {
        atk_power = apply_permille(atk_power, 1000 - clamp_permille(attacker_doctrine.mountain_penalty));
        def_power = apply_permille(def_power, 1000 - clamp_permille(defender_doctrine.mountain_penalty));
    }

    // --- Terrain: defence favours the defender, attrition costs both -------
    def_power = apply_permille(def_power,
                               1000 + terrain_defence(terrain_sub, terrain_cov,
                                                      terrain_density, terrain_lf));

    const int attrition = terrain_attrition(terrain_sub, terrain_cov, terrain_density, terrain_lf) *
                           season_attrition_multiplier(battle_season) / 1000;
    // Supply mitigates attrition: fully supplied (1000) cancels it out entirely,
    // cut off (0) takes the full attrition hit.
    const int atk_attrition_hit = attrition * (1000 - attacker_supply) / 1000;
    const int def_attrition_hit = attrition * (1000 - defender_supply) / 1000;
    atk_power = apply_permille(atk_power, 1000 - clamp_permille(atk_attrition_hit));
    def_power = apply_permille(def_power, 1000 - clamp_permille(def_attrition_hit));

    battle_outcome out;
    out.attacker_power = static_cast<int>(std::min<std::int64_t>(atk_power, INT32_MAX));
    out.defender_power = static_cast<int>(std::min<std::int64_t>(def_power, INT32_MAX));

    // TIE-BREAK: strictly greater required to win; the defender holds ground
    // on an exact tie (see combat.hpp resolve_battle comment).
    out.result = (atk_power > def_power) ? battle_result::attacker_victory
                                          : battle_result::defender_victory;

    const std::int64_t winner_power = std::max(atk_power, def_power);
    const std::int64_t loser_power  = std::min(atk_power, def_power);
    const int decisiveness = winner_power > 0
        ? clamp_permille(static_cast<int>((winner_power - loser_power) * 1000 / winner_power))
        : 0;
    out.decisiveness = decisiveness;

    const int loser_loss  = clamp_permille(400 + decisiveness * 6 / 10);
    const int winner_loss = clamp_permille(200 - decisiveness * 2 / 10);

    if (out.result == battle_result::attacker_victory)
    {
        out.attacker_losses_permille = winner_loss;
        out.defender_losses_permille = loser_loss;
    }
    else
    {
        out.attacker_losses_permille = loser_loss;
        out.defender_losses_permille = winner_loss;
    }

    return out;
}
