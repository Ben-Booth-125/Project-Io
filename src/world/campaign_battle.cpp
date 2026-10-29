#include "campaign_battle.hpp"

#include "terrain_combat.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace {

int clamp_permille(int v) { return std::clamp(v, 0, 1000); }

/// splitmix64 — the same shape as continents.cpp / creeds.cpp / body_names.cpp
/// use, kept local for the same reason they do: a battle's stream must not be
/// reachable from, or perturbable by, anything else in the world.
uint64_t splitmix64(uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

/// Men standing among a side's FIGHTING units.
int64_t fighting_men(const std::vector<land_combatant>& side)
{
    int64_t n = 0;
    for (const land_combatant& c : side)
        if (land_combatant_fighting(c))
            n += c.men;
    return n;
}

/// The men-weighted hardness of a side's fighting units, per-mille. A side with
/// none fighting is read as soft — its spent units are caught in the open.
int64_t side_hardness(const std::vector<land_combatant>& side)
{
    int64_t men = 0, hard = 0;
    for (const land_combatant& c : side)
        if (land_combatant_fighting(c))
        {
            men  += c.men;
            hard += static_cast<int64_t>(c.men) * std::clamp(c.stats.hardness, 0, 1000);
        }
    return men > 0 ? hard / men : 0;
}

/// Milli-hits a side deals against a target of @p target_hardness: per unit,
/// men x (soft x (1 - h) + hard x h), scaled by the unit's morale and supply.
/// Before the swing.
int64_t side_hits(const std::vector<land_combatant>& side, int64_t target_hardness)
{
    int64_t hits = 0;
    for (const land_combatant& c : side)
    {
        if (!land_combatant_fighting(c))
            continue;
        const int64_t per_man = (static_cast<int64_t>(std::max(0, c.stats.soft)) * (1000 - target_hardness)
                               + static_cast<int64_t>(std::max(0, c.stats.hard)) * target_hardness) / 1000;
        hits += static_cast<int64_t>(c.men) * per_man
              * clamp_permille(c.morale) / 1000 * clamp_permille(c.supply) / 1000;
    }
    return hits;
}

/// Milli-hits a side can absorb: men x defence when it defends, men x
/// breakthrough when it attacks.
int64_t side_absorb(const std::vector<land_combatant>& side, bool defending)
{
    int64_t absorb = 0;
    for (const land_combatant& c : side)
        if (land_combatant_fighting(c))
            absorb += static_cast<int64_t>(c.men)
                    * std::max(0, defending ? c.stats.defence : c.stats.breakthrough);
    return absorb;
}

/// BL-1453 retune (Ben, 2026-10-29: "a battle is weeks long, not days"): THE DAY'S
/// DAMAGE SCALE on @p landed milli-hits a side takes — `day_damage_permille`, raised
/// by the rout term when the dealer's raw hits @p dealt exceed the taker's own
/// @p taken by more than `rout_ratio_permille` (x (ratio / line)^3, capped).
int64_t day_scaled(int64_t landed, int64_t dealt, int64_t taken, const campaign_battle_params& p)
{
    if (landed <= 0)
        return landed;
    int64_t scale = std::max(0, p.day_damage_permille); // per-mille
    const int64_t line = std::max(1, p.rout_ratio_permille);
    if (dealt > 0)
    {
        // ratio x 1000 / line, per-mille; a taker who deals nothing is out-hit at the cap.
        const int64_t over = taken > 0 ? dealt * 1000 / taken * 1000 / line
                                       : std::numeric_limits<int64_t>::max() / 4;
        if (over > 1000)
        {
            const int64_t cap = std::max<int64_t>(1000, p.rout_cap_permille);
            const int64_t o = std::min<int64_t>(over, 100000); // keeps the cube in range
            const int64_t cube = std::min<int64_t>(o * o / 1000 * o / 1000, cap);
            scale = scale * cube / 1000;
        }
    }
    return landed * scale / 1000;
}

/// What actually lands of @p hits against @p absorb.
int64_t landed_hits(int64_t hits, int64_t absorb, const campaign_battle_params& p)
{
    const int64_t within = std::min(hits, absorb);
    const int64_t beyond = std::max<int64_t>(0, hits - absorb);
    return within * clamp_permille(p.absorbed_land_permille) / 1000 + beyond;
}

/// BL-1453: a target's weight in the split of a side's landed hits — its men x the
/// share of it the enemy sees, so every hit on a unit is scaled by its own visible
/// share (MILITARY.md § Recon). With every unit fully seen this is men x 1000, and
/// the split is the men-proportional one exactly.
int64_t seen_men(const land_combatant& c)
{
    return static_cast<int64_t>(c.men) * clamp_permille(c.visible);
}

/// Kill exactly @p to_remove men across @p targets (indices into @p side),
/// proportional to men seen (`seen_men`), floor shares first and the remainder one
/// man at a time in vector order — ascending unit id by the combatant contract.
/// Integer throughout, so the split cannot depend on a rounding mode. @return removed.
int64_t remove_men(std::vector<land_combatant>& side, const std::vector<std::size_t>& targets,
                   int64_t to_remove)
{
    int64_t total = 0, weight = 0;
    for (const std::size_t k : targets)
    {
        total  += side[k].men;
        weight += seen_men(side[k]);
    }
    if (to_remove <= 0 || total <= 0 || weight <= 0)
        return 0;
    to_remove = std::min(to_remove, total);

    std::vector<int64_t> hit(targets.size(), 0);
    int64_t assigned = 0;
    for (std::size_t t = 0; t < targets.size(); ++t)
    {
        hit[t]    = std::min<int64_t>(to_remove * seen_men(side[targets[t]]) / weight,
                                      side[targets[t]].men);
        assigned += hit[t];
    }
    // Bounded: to_remove <= total, so while assigned < to_remove some unit has room.
    while (assigned < to_remove)
    {
        bool progressed = false;
        for (std::size_t t = 0; t < targets.size() && assigned < to_remove; ++t)
            if (hit[t] < side[targets[t]].men && seen_men(side[targets[t]]) > 0)
            {
                ++hit[t];
                ++assigned;
                progressed = true;
            }
        if (!progressed)
            break;
    }
    int64_t removed = 0;
    for (std::size_t t = 0; t < targets.size(); ++t)
    {
        land_combatant& c = side[targets[t]];
        const int take = static_cast<int>(std::min<int64_t>(hit[t], c.men));
        c.men   -= take;
        removed += take;
    }
    return removed;
}

/// BL-1453 (MILITARY.md § Recon): THE VISIBLE-SHARE TERM. @p landed milli-hits on
/// @p side, scaled by the share of it the enemy sees — the men-weighted mean of its
/// units' `visible` over the units the hits fall on (`apply_landed`'s targets: the
/// fighting units, or every unit standing when none is). Each unit then takes its
/// share in proportion to its men seen, so every hit on a unit is scaled by its
/// own visible share. 1000 everywhere returns @p landed unchanged.
int64_t visible_landed(const std::vector<land_combatant>& side, int64_t landed)
{
    if (landed <= 0)
        return landed;
    bool any_fighting = false;
    for (const land_combatant& c : side)
        any_fighting = any_fighting || land_combatant_fighting(c);
    int64_t men = 0, seen = 0;
    for (const land_combatant& c : side)
        if (any_fighting ? land_combatant_fighting(c) : c.men > 0)
        {
            men  += c.men;
            seen += seen_men(c);
        }
    return men > 0 ? landed * seen / (men * 1000) : landed;
}

/// Land @p landed milli-hits on @p side. The HP share kills men across the
/// side's fighting units (or, if none is fighting, across every unit still
/// standing — the overrun); the rest spends each fighting unit's readiness by
/// its share of the hits (men seen). @p hp_scale_permille scales the men share
/// (an entrenched day's, `entrench_casualty_permille`).
///
/// THE CARRY (BL-1453, cold review): each unit's sub-unit remainder of readiness
/// and of men is carried in its `ready_carry` / `hp_carry` and added to the next
/// day's, so a side that lands any hits always wears the other down; nothing is
/// floored away and nothing is floored up.
void apply_landed(std::vector<land_combatant>& side, int64_t landed, int64_t hp_scale_permille,
                  const campaign_battle_params& p)
{
    if (landed <= 0)
        return;

    std::vector<std::size_t> fighting, standing;
    for (std::size_t k = 0; k < side.size(); ++k)
    {
        if (land_combatant_fighting(side[k])) fighting.push_back(k);
        if (side[k].men > 0)                  standing.push_back(k);
    }
    const bool overrun = fighting.empty();
    const std::vector<std::size_t>& targets = overrun ? standing : fighting;
    if (targets.empty())
        return;

    // The overrun: the hits land in full, as men, across every unit standing.
    if (overrun)
    {
        remove_men(side, targets, landed / std::max(1, p.hp_cost));
        return;
    }

    int64_t seen = 0;
    for (const std::size_t k : targets)
        seen += seen_men(side[k]);
    if (seen <= 0)
        return;
    const int64_t share      = clamp_permille(p.hp_share_permille);
    const int64_t ready_hits = landed * (1000 - share) / 1000;
    const int64_t hp_hits    = landed * share / 1000 * std::clamp<int64_t>(hp_scale_permille, 0, 1000) / 1000;
    const int64_t hp_cost    = std::max(1, p.hp_cost);

    // Readiness first, read against the men standing BEFORE this round's deaths,
    // so the two halves of the same hits are measured against the same force.
    for (const std::size_t k : targets)
    {
        land_combatant& c = side[k];
        const int64_t own  = ready_hits * seen_men(c) / seen;
        const int64_t cost = static_cast<int64_t>(c.men) * std::max(1, p.readiness_cost);
        const int64_t num  = own * 1000 + std::max<int64_t>(0, c.ready_carry);
        const int64_t loss = num / cost;
        c.ready_carry = num % cost;
        c.readiness   = static_cast<int>(std::max<int64_t>(0, c.readiness - loss));
        if (c.readiness == 0)
            c.ready_carry = 0; // spent: nothing left to carry against
    }
    for (const std::size_t k : targets)
    {
        land_combatant& c = side[k];
        const int64_t own  = hp_hits * seen_men(c) / seen + std::max<int64_t>(0, c.hp_carry);
        const int64_t dead = std::min<int64_t>(own / hp_cost, c.men);
        c.hp_carry = dead < c.men ? own - dead * hp_cost : 0;
        c.men     -= static_cast<int>(dead);
    }
}

} // namespace

int visible_share_permille(int64_t recon_milli, int target_stealth)
{
    if (target_stealth <= 0)
        return 1000;
    return static_cast<int>(std::clamp<int64_t>(std::max<int64_t>(0, recon_milli) / target_stealth, 0, 1000));
}

int side_readiness(const std::vector<land_combatant>& side)
{
    int64_t men = 0, ready = 0;
    for (const land_combatant& c : side)
        if (c.men > 0)
        {
            men   += c.men;
            ready += static_cast<int64_t>(c.men) * clamp_permille(c.readiness);
        }
    return men > 0 ? static_cast<int>(ready / men) : 0;
}

bool battle_entrenched(int64_t a, int64_t b, const campaign_battle_params& params)
{
    if (a <= 0 || b <= 0)
        return false;
    const int64_t hi = std::max(a, b), lo = std::min(a, b);
    return hi * 1000 / lo <= std::max(1000, params.entrench_ratio_permille);
}

uint64_t campaign_battle_seed(const campaign_battle_identity& id)
{
    // Fold each field through its own mix so no two of them can cancel. The
    // attacker/defender pair is packed in role order, not sorted: swapping the
    // roles is a different battle and must be a different stream.
    uint64_t h = splitmix64(static_cast<uint64_t>(id.world_seed) * 0x9E3779B97F4A7C15ull);
    h = splitmix64(h ^ ((static_cast<uint64_t>(id.attacker) << 32) | static_cast<uint64_t>(id.defender)));
    // The DEFENDED TILE (2026-10-27), in the slot the province held — same mix,
    // same position, so the fold's shape is unchanged and only what it means moved.
    h = splitmix64(h ^ (static_cast<uint64_t>(id.tile) * 0xD1342543DE82EF95ull));
    h = splitmix64(h ^ id.tick);
    return h;
}

campaign_battle_state begin_campaign_battle(const campaign_battle_identity&    id,
                                            const std::vector<land_combatant>& attacker,
                                            const std::vector<land_combatant>& defender,
                                            terrain_substrate                  terrain_sub,
                                            terrain_cover                      terrain_cov,
                                            std::uint8_t                       terrain_density,
                                            terrain_landform                   terrain_lf)
{
    auto mean_supply = [](const std::vector<land_combatant>& side) {
        int64_t sum = 0;
        for (const land_combatant& c : side) sum += clamp_permille(c.supply);
        return side.empty() ? 1000 : static_cast<int>(sum / static_cast<int64_t>(side.size()));
    };

    campaign_battle_state st;
    st.terrain_sub     = terrain_sub;
    st.terrain_cov     = terrain_cov;
    st.terrain_density = terrain_density;
    st.terrain_lf      = terrain_lf;
    st.battle_season   = season::summer; // read by nothing since BL-1421; kept for the stream shape
    st.attacker_supply = mean_supply(attacker);
    st.defender_supply = mean_supply(defender);
    st.attacker_strength_permille = side_readiness(attacker);
    st.defender_strength_permille = side_readiness(defender);
    st.stream_seed     = campaign_battle_seed(id);
    st.rng_state       = st.stream_seed;
    st.end             = campaign_battle_end::in_progress;
    return st;
}

bool step_campaign_battle(campaign_battle_state&        st,
                          std::vector<land_combatant>&  attacker,
                          std::vector<land_combatant>&  defender,
                          const campaign_battle_params& params)
{
    if (st.end != campaign_battle_end::in_progress)
        return false;

    // NO DRAW (Ben, 2026-10-29): the round is the deterministic arithmetic alone.

    // BOTH SIDES' HITS FROM THE STATE BEFORE THE ROUND, applied together: neither
    // side's losses this round weaken what it deals this round.
    // THE GROUND DIVIDES THE ATTACKER'S HITS: x 1000 / (1000 + terrain_defence x
    // scale / 1000), so at the default scale a mountain halves them. On the hits
    // rather than the absorb, so the ground counts in every fight — a raise to the
    // absorb would matter only where hits already exceed it.
    const int64_t terrain_d = terrain_defence(st.terrain_sub, st.terrain_cov,
                                              st.terrain_density, st.terrain_lf);
    const int64_t ground = 1000 + std::max<int64_t>(0, terrain_d) * params.terrain_attack_scale / 1000;
    const int64_t atk_hits = side_hits(attacker, side_hardness(defender)) * 1000 / ground;
    const int64_t def_hits = side_hits(defender, side_hardness(attacker));
    // The rout and entrenchment terms read the raw hits (the ground included).
    const int64_t atk_raw = atk_hits;
    const int64_t def_raw = def_hits;
    // LIKE FORCES ENTRENCH (Ben, 2026-10-29): near parity the day's damage drops
    // further, and each side recovers from its supplies after it (below).
    const bool dug_in = battle_entrenched(atk_raw, def_raw, params);
    const int64_t casualties = dug_in ? std::clamp(params.entrench_casualty_permille, 0, 1000) : 1000;
    const int64_t on_def = day_scaled(visible_landed(defender, landed_hits(atk_hits, side_absorb(defender, true), params)),
                                      atk_raw, def_raw, params);
    const int64_t on_atk = day_scaled(visible_landed(attacker, landed_hits(def_hits, side_absorb(attacker, false), params)),
                                      def_raw, atk_raw, params);

    const bool atk_could_fight = fighting_men(attacker) > 0;
    const bool def_could_fight = fighting_men(defender) > 0;

    apply_landed(defender, on_def, casualties, params);
    apply_landed(attacker, on_atk, casualties, params);

    // ENTRENCHED RECOVERY: every unit still fighting after the day's damage regains
    // its supplies' share of readiness, never above its morale. Per round, a day's
    // worth spread over `rounds_per_day`. A spent unit (readiness 0) is out of the
    // fight and recovers nothing here.
    if (dug_in)
    {
        const int rpd = std::max(1, params.rounds_per_day);
        for (auto* side : {&attacker, &defender})
            for (land_combatant& c : *side)
                if (land_combatant_fighting(c) && c.readiness < c.morale)
                    c.readiness = std::min(clamp_permille(c.morale),
                                           c.readiness + std::max(0, c.entrench_recovery) / rpd);
    }

    st.attacker_strength_permille = side_readiness(attacker);
    st.defender_strength_permille = side_readiness(defender);
    ++st.rounds_fought;

    campaign_battle_round rec;
    rec.round                      = st.rounds_fought;
    rec.attacker_power             = static_cast<int>(std::min<int64_t>(on_def, INT32_MAX));
    rec.defender_power             = static_cast<int>(std::min<int64_t>(on_atk, INT32_MAX));
    rec.round_winner               = on_def > on_atk ? battle_result::attacker_victory
                                                     : battle_result::defender_victory;
    rec.attacker_strength_permille = st.attacker_strength_permille;
    rec.defender_strength_permille = st.defender_strength_permille;
    st.rounds.push_back(rec);

    // A side breaks when no unit of it is left fighting — checked AFTER the
    // round, so a side that breaks still took (and dealt) the round's damage. A
    // side that could not fight at the start of the round is broken by it. Both
    // at once is decided in the defender's favour, as every tie on both paths.
    const bool atk_broken = !atk_could_fight || fighting_men(attacker) == 0;
    const bool def_broken = !def_could_fight || fighting_men(defender) == 0;

    // NO ROUND CAP (Ben, 2026-10-29): nothing else ends a fight here. A side breaks
    // or withdraws (withdraw_campaign_battle); `stalemate` is produced by nothing.
    if (atk_broken)                     st.end = campaign_battle_end::attacker_broken;
    else if (def_broken)                st.end = campaign_battle_end::defender_broken;

    return st.end == campaign_battle_end::in_progress;
}

int withdrawal_cost_permille(const campaign_battle_state& st, withdrawing_side side,
                             const campaign_battle_params& params)
{
    if (side == withdrawing_side::none)
        return 0;
    const bool attacker_leaving = (side == withdrawing_side::attacker);
    const int own   = attacker_leaving ? st.attacker_strength_permille : st.defender_strength_permille;
    const int enemy = attacker_leaving ? st.defender_strength_permille : st.attacker_strength_permille;

    // The three terms of the cost, per the header's contract. Both the
    // per-round and the pursuit term grow as the fight runs on, and the rounds
    // already fought have separately cost their own attrition — so a late
    // break-off is dear twice over. Nothing here is clamped to produce that
    // ordering; it falls out of the shape.
    const int behind = std::max(0, enemy - own);
    // The per-round term holds at its cap (BL-1453 fix round): a battle has no
    // round cap now, and an uncapped term would reach the whole force.
    const int64_t per_round = std::min<int64_t>(
        int64_t{st.rounds_fought} * params.withdraw_per_round_permille,
        std::max(0, params.withdraw_per_round_cap_permille));
    return clamp_permille(static_cast<int>(params.withdraw_base_permille + per_round +
                                           behind * params.withdraw_pursuit_scale / 1000));
}

int withdraw_campaign_battle(campaign_battle_state&        st,
                             withdrawing_side              side,
                             std::vector<land_combatant>&  leaving,
                             const campaign_battle_params& params)
{
    if (st.end != campaign_battle_end::in_progress || side == withdrawing_side::none)
        return 0;

    const int parting = withdrawal_cost_permille(st, side, params);

    // The parting cost lands IN MEN (BL-1421), on every unit still standing.
    std::vector<std::size_t> standing;
    int64_t men = 0;
    for (std::size_t k = 0; k < leaving.size(); ++k)
        if (leaving[k].men > 0)
        {
            standing.push_back(k);
            men += leaving[k].men;
        }
    const int removed = static_cast<int>(remove_men(leaving, standing, men * parting / 1000));

    const bool attacker_leaving = (side == withdrawing_side::attacker);
    if (attacker_leaving) st.attacker_strength_permille = side_readiness(leaving);
    else                  st.defender_strength_permille = side_readiness(leaving);

    st.end = attacker_leaving ? campaign_battle_end::attacker_withdrew
                              : campaign_battle_end::defender_withdrew;
    return removed;
}

campaign_battle_outcome campaign_battle_result(const campaign_battle_state& st)
{
    campaign_battle_outcome out;
    out.end                        = st.end;
    out.rounds_fought              = st.rounds_fought;
    out.attacker_strength_permille = st.attacker_strength_permille;
    out.defender_strength_permille = st.defender_strength_permille;
    out.attacker_losses_permille   = 1000 - st.attacker_strength_permille;
    out.defender_losses_permille   = 1000 - st.defender_strength_permille;
    out.stream_seed                = st.stream_seed;
    out.rounds                     = st.rounds;

    // Who holds the ground. A withdrawal hands the field to whoever stayed,
    // even if they were losing on points — ground is held by being on it.
    switch (st.end)
    {
        case campaign_battle_end::attacker_broken:
        case campaign_battle_end::attacker_withdrew:
            out.result = battle_result::defender_victory;
            break;
        case campaign_battle_end::defender_broken:
        case campaign_battle_end::defender_withdrew:
            out.result = battle_result::attacker_victory;
            break;
        default:
            // Read mid-fight (or a pre-2026-10-29 stalemate): the side with more
            // readiness left holds, defender on an exact tie.
            out.result = st.attacker_strength_permille > st.defender_strength_permille
                             ? battle_result::attacker_victory
                             : battle_result::defender_victory;
            break;
    }

    const int hi = std::max(st.attacker_strength_permille, st.defender_strength_permille);
    const int lo = std::min(st.attacker_strength_permille, st.defender_strength_permille);
    out.decisiveness = hi > 0 ? clamp_permille((hi - lo) * 1000 / hi) : 1000;

    return out;
}

campaign_battle_outcome resolve_campaign_battle(const campaign_battle_identity& id,
                                                std::vector<land_combatant>     attacker,
                                                std::vector<land_combatant>     defender,
                                                terrain_substrate               terrain_sub,
                                                terrain_cover                   terrain_cov,
                                                std::uint8_t                    terrain_density,
                                                terrain_landform                terrain_lf,
                                                withdrawing_side                withdrawer,
                                                int                             withdraw_after_round,
                                                const campaign_battle_params&   params,
                                                std::vector<land_combatant>*    attacker_out,
                                                std::vector<land_combatant>*    defender_out,
                                                int                             round_budget)
{
    campaign_battle_state st = begin_campaign_battle(id, attacker, defender, terrain_sub,
                                                     terrain_cov, terrain_density, terrain_lf);

    const bool scripted = (withdrawer != withdrawing_side::none) && (withdraw_after_round > 0);

    while (st.end == campaign_battle_end::in_progress && st.rounds_fought < std::max(1, round_budget))
    {
        // The window is checked BEFORE each round, so "withdraw after round N"
        // means N rounds were actually fought — the same instant a watching
        // player would get the choice.
        if (scripted && st.rounds_fought >= withdraw_after_round)
        {
            withdraw_campaign_battle(st, withdrawer,
                                     withdrawer == withdrawing_side::attacker ? attacker : defender,
                                     params);
            break;
        }
        step_campaign_battle(st, attacker, defender, params);
    }

    if (attacker_out) *attacker_out = attacker;
    if (defender_out) *defender_out = defender;
    return campaign_battle_result(st);
}
