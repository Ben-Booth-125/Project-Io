#include "campaign_battle.hpp"

#include "terrain_combat.hpp"

#include <algorithm>
#include <cstdint>

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

/// One draw. Advances the state in place so the CONSUMPTION ORDER is visible at
/// every call site — the property that keeps replay stable (see the header).
uint64_t next_draw(uint64_t& state)
{
    state = splitmix64(state);
    return state;
}

/// A uniform per-mille multiplier in [1000 - swing, 1000 + swing]. Modulo bias
/// over a 64-bit draw into a ~601-wide range is far below the resolution of
/// anything downstream, and — the part that matters here — it is deterministic.
int swing_multiplier(uint64_t& state, int swing)
{
    swing = std::clamp(swing, 0, 1000);
    if (swing == 0) return 1000;
    const uint64_t span = static_cast<uint64_t>(2 * swing + 1);
    return 1000 - swing + static_cast<int>(next_draw(state) % span);
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
/// breakthrough when it attacks, scaled by the unit's carried ammunition and
/// equipment (BL-1452, `land_combatant::carried`).
int64_t side_absorb(const std::vector<land_combatant>& side, bool defending)
{
    int64_t absorb = 0;
    for (const land_combatant& c : side)
        if (land_combatant_fighting(c))
            absorb += static_cast<int64_t>(c.men)
                    * std::max(0, defending ? c.stats.defence : c.stats.breakthrough)
                    * clamp_permille(c.carried) / 1000;
    return absorb;
}

/// What actually lands of @p hits against @p absorb.
int64_t landed_hits(int64_t hits, int64_t absorb, const campaign_battle_params& p)
{
    const int64_t within = std::min(hits, absorb);
    const int64_t beyond = std::max<int64_t>(0, hits - absorb);
    return within * clamp_permille(p.absorbed_land_permille) / 1000 + beyond;
}

/// Kill exactly @p to_remove men across @p targets (indices into @p side),
/// proportional to men, floor shares first and the remainder one man at a time
/// in vector order — ascending unit id by the combatant contract. Integer
/// throughout, so the split cannot depend on a rounding mode. @return removed.
int64_t remove_men(std::vector<land_combatant>& side, const std::vector<std::size_t>& targets,
                   int64_t to_remove)
{
    int64_t total = 0;
    for (const std::size_t k : targets)
        total += side[k].men;
    if (to_remove <= 0 || total <= 0)
        return 0;
    to_remove = std::min(to_remove, total);

    std::vector<int64_t> hit(targets.size(), 0);
    int64_t assigned = 0;
    for (std::size_t t = 0; t < targets.size(); ++t)
    {
        hit[t]    = to_remove * side[targets[t]].men / total;
        assigned += hit[t];
    }
    // Bounded: to_remove <= total, so while assigned < to_remove some unit has room.
    while (assigned < to_remove)
    {
        bool progressed = false;
        for (std::size_t t = 0; t < targets.size() && assigned < to_remove; ++t)
            if (hit[t] < side[targets[t]].men)
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

/// Land @p landed milli-hits on @p side. The HP share kills men across the
/// side's fighting units (or, if none is fighting, across every unit still
/// standing — the overrun); the rest spends each fighting unit's readiness by
/// its men's share of the hits.
void apply_landed(std::vector<land_combatant>& side, int64_t landed, const campaign_battle_params& p)
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

    // Readiness first, read against the men standing BEFORE this round's deaths,
    // so the two halves of the same hits are measured against the same force.
    if (!overrun)
    {
        int64_t men = 0;
        for (const std::size_t k : targets)
            men += side[k].men;
        const int64_t ready_hits = landed * (1000 - clamp_permille(p.hp_share_permille)) / 1000;
        for (const std::size_t k : targets)
        {
            land_combatant& c = side[k];
            const int64_t own = ready_hits * c.men / men;
            const int64_t cost = static_cast<int64_t>(c.men) * std::max(1, p.readiness_cost);
            const int64_t loss = own * 1000 / cost;
            c.readiness = static_cast<int>(std::max<int64_t>(0, c.readiness - loss));
        }
    }

    const int64_t hp_hits = overrun ? landed : landed * clamp_permille(p.hp_share_permille) / 1000;
    remove_men(side, targets, hp_hits / std::max(1, p.hp_cost));
}

} // namespace

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

    // FIXED DRAW ORDER: attacker, then defender, exactly two per round — drawn
    // even when a side cannot fight, so the stream position is a function of
    // rounds fought alone.
    const int atk_swing = swing_multiplier(st.rng_state, params.swing_permille);
    const int def_swing = swing_multiplier(st.rng_state, params.swing_permille);

    // BOTH SIDES' HITS FROM THE STATE BEFORE THE ROUND, applied together: neither
    // side's losses this round weaken what it deals this round.
    // THE GROUND DIVIDES THE ATTACKER'S HITS: x 1000 / (1000 + terrain_defence x
    // scale / 1000), so at the default scale a mountain halves them. On the hits
    // rather than the absorb, so the ground counts in every fight — a raise to the
    // absorb would matter only where hits already exceed it.
    const int64_t terrain_d = terrain_defence(st.terrain_sub, st.terrain_cov,
                                              st.terrain_density, st.terrain_lf);
    const int64_t ground = 1000 + std::max<int64_t>(0, terrain_d) * params.terrain_attack_scale / 1000;
    const int64_t atk_hits = side_hits(attacker, side_hardness(defender)) * atk_swing / 1000 * 1000 / ground;
    const int64_t def_hits = side_hits(defender, side_hardness(attacker)) * def_swing / 1000;
    const int64_t on_def = landed_hits(atk_hits, side_absorb(defender, true), params);
    const int64_t on_atk = landed_hits(def_hits, side_absorb(attacker, false), params);

    const bool atk_could_fight = fighting_men(attacker) > 0;
    const bool def_could_fight = fighting_men(defender) > 0;

    apply_landed(defender, on_def, params);
    apply_landed(attacker, on_atk, params);

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

    if (atk_broken)                     st.end = campaign_battle_end::attacker_broken;
    else if (def_broken)                st.end = campaign_battle_end::defender_broken;
    else if (st.rounds_fought >= std::max(1, params.max_rounds))
                                        st.end = campaign_battle_end::stalemate;

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
    return clamp_permille(params.withdraw_base_permille +
                          st.rounds_fought * params.withdraw_per_round_permille +
                          behind * params.withdraw_pursuit_scale / 1000);
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
            // Stalemate (or read mid-fight): the side with more readiness left
            // holds, defender on an exact tie.
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
                                                std::vector<land_combatant>*    defender_out)
{
    campaign_battle_state st = begin_campaign_battle(id, attacker, defender, terrain_sub,
                                                     terrain_cov, terrain_density, terrain_lf);

    const bool scripted = (withdrawer != withdrawing_side::none) && (withdraw_after_round > 0);

    while (st.end == campaign_battle_end::in_progress)
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
