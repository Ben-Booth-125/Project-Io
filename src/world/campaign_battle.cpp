#include "campaign_battle.hpp"

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

/// The force a side still has on the field: its committed stack scaled by the
/// strength it has left. Entries that scale to zero drop out, which is how a
/// worn-down stack loses its smallest contingents first.
std::vector<army_stack_entry> scaled_stack(const std::vector<army_stack_entry>& base, int strength)
{
    std::vector<army_stack_entry> out;
    out.reserve(base.size());
    for (const army_stack_entry& e : base)
    {
        army_stack_entry s = e;
        s.count = static_cast<int>(static_cast<std::int64_t>(std::max(0, e.count)) * strength / 1000);
        if (s.count > 0) out.push_back(s);
    }
    return out;
}

/// Apply a per-mille LOSS to a strength value, multiplicatively — so attrition
/// across a long fight compounds instead of summing past zero.
int apply_loss(int strength, int loss_permille)
{
    return clamp_permille(strength * (1000 - clamp_permille(loss_permille)) / 1000);
}

} // namespace

uint64_t campaign_battle_seed(const campaign_battle_identity& id)
{
    // Fold each field through its own mix so no two of them can cancel. The
    // attacker/defender pair is packed in role order, not sorted: swapping the
    // roles is a different battle and must be a different stream.
    uint64_t h = splitmix64(static_cast<uint64_t>(id.world_seed) * 0x9E3779B97F4A7C15ull);
    h = splitmix64(h ^ ((static_cast<uint64_t>(id.attacker) << 32) | static_cast<uint64_t>(id.defender)));
    h = splitmix64(h ^ (static_cast<uint64_t>(id.tile_index) * 0xD1342543DE82EF95ull));
    h = splitmix64(h ^ id.tick);
    return h;
}

campaign_battle_state begin_campaign_battle(const campaign_battle_identity&      id,
                                            const std::vector<army_stack_entry>& attacker,
                                            const doctrine_row&                  attacker_doctrine,
                                            const std::vector<army_stack_entry>& defender,
                                            const doctrine_row&                  defender_doctrine,
                                            terrain_composition                  terrain_comp,
                                            terrain_landform                     terrain_lf,
                                            season                               battle_season,
                                            int                                  attacker_supply,
                                            int                                  defender_supply)
{
    campaign_battle_state st;
    st.attacker          = attacker;
    st.defender          = defender;
    st.attacker_doctrine = attacker_doctrine;
    st.defender_doctrine = defender_doctrine;
    st.terrain_comp      = terrain_comp;
    st.terrain_lf        = terrain_lf;
    st.battle_season     = battle_season;
    st.attacker_supply   = clamp_permille(attacker_supply);
    st.defender_supply   = clamp_permille(defender_supply);
    st.stream_seed       = campaign_battle_seed(id);
    st.rng_state         = st.stream_seed;
    st.end               = campaign_battle_end::in_progress;
    return st;
}

bool step_campaign_battle(campaign_battle_state& st, const campaign_battle_params& params)
{
    if (st.end != campaign_battle_end::in_progress)
        return false;

    const std::vector<army_stack_entry> atk = scaled_stack(st.attacker, st.attacker_strength_permille);
    const std::vector<army_stack_entry> def = scaled_stack(st.defender, st.defender_strength_permille);

    // POWER SCORING IS THE ERA -1 ENGINE'S, unchanged — matchup x doctrine x
    // terrain x supply x season. Only the two power numbers are read; the
    // one-shot loss and decisiveness model below it belongs to a war-year, not
    // to a round, so this file computes its own (see header § what is shared).
    const battle_outcome scored = resolve_battle(atk, st.attacker_doctrine,
                                                  def, st.defender_doctrine,
                                                  st.terrain_comp, st.terrain_lf,
                                                  st.battle_season,
                                                  st.attacker_supply, st.defender_supply);

    // FIXED DRAW ORDER: attacker, then defender, exactly two per round.
    const int atk_swing = swing_multiplier(st.rng_state, params.swing_permille);
    const int def_swing = swing_multiplier(st.rng_state, params.swing_permille);

    const std::int64_t atk_power = static_cast<std::int64_t>(scored.attacker_power) * atk_swing / 1000;
    const std::int64_t def_power = static_cast<std::int64_t>(scored.defender_power) * def_swing / 1000;

    // Same tie-break as resolve_battle: the defender holds an inconclusive round.
    const bool attacker_won = atk_power > def_power;

    const std::int64_t winner_power = std::max(atk_power, def_power);
    const std::int64_t loser_power  = std::min(atk_power, def_power);
    const int margin = winner_power > 0
        ? clamp_permille(static_cast<int>((winner_power - loser_power) * 1000 / winner_power))
        : 0;

    const int loser_loss = clamp_permille(params.round_loser_loss_base +
                                           margin * params.round_loser_loss_margin / 1000);
    const int winner_loss = clamp_permille(std::max(params.round_winner_loss_floor,
                                                     params.round_winner_loss_base -
                                                     margin * params.round_winner_loss_margin / 1000));

    if (attacker_won)
    {
        st.attacker_strength_permille = apply_loss(st.attacker_strength_permille, winner_loss);
        st.defender_strength_permille = apply_loss(st.defender_strength_permille, loser_loss);
    }
    else
    {
        st.attacker_strength_permille = apply_loss(st.attacker_strength_permille, loser_loss);
        st.defender_strength_permille = apply_loss(st.defender_strength_permille, winner_loss);
    }

    ++st.rounds_fought;

    campaign_battle_round rec;
    rec.round                      = st.rounds_fought;
    rec.attacker_power             = static_cast<int>(std::min<std::int64_t>(atk_power, INT32_MAX));
    rec.defender_power             = static_cast<int>(std::min<std::int64_t>(def_power, INT32_MAX));
    rec.round_winner               = attacker_won ? battle_result::attacker_victory
                                                   : battle_result::defender_victory;
    rec.attacker_strength_permille = st.attacker_strength_permille;
    rec.defender_strength_permille = st.defender_strength_permille;
    st.rounds.push_back(rec);

    // Breaking is checked AFTER the round resolves, so a side that routs still
    // took (and dealt) that round's damage. Both breaking at once is decided in
    // the defender's favour, consistent with every other tie in the two paths.
    const bool atk_broken = st.attacker_strength_permille <= params.rout_threshold;
    const bool def_broken = st.defender_strength_permille <= params.rout_threshold;

    if (atk_broken && !def_broken)      st.end = campaign_battle_end::attacker_broken;
    else if (def_broken && !atk_broken) st.end = campaign_battle_end::defender_broken;
    else if (atk_broken && def_broken)  st.end = campaign_battle_end::attacker_broken;
    else if (st.rounds_fought >= std::max(1, params.max_rounds))
                                         st.end = campaign_battle_end::stalemate;

    return st.end == campaign_battle_end::in_progress;
}

void withdraw_campaign_battle(campaign_battle_state&        st,
                              withdrawing_side              side,
                              const campaign_battle_params& params)
{
    if (st.end != campaign_battle_end::in_progress || side == withdrawing_side::none)
        return;

    const bool attacker_leaving = (side == withdrawing_side::attacker);
    const int own   = attacker_leaving ? st.attacker_strength_permille : st.defender_strength_permille;
    const int enemy = attacker_leaving ? st.defender_strength_permille : st.attacker_strength_permille;

    // The three terms of the cost, per the header's contract. Both the
    // per-round and the pursuit term grow as the fight runs on, and the rounds
    // already fought have separately cost their own attrition — so a late
    // break-off is dear twice over. Nothing here is clamped to produce that
    // ordering; it falls out of the shape.
    const int behind  = std::max(0, enemy - own);
    const int parting = clamp_permille(params.withdraw_base_permille +
                                        st.rounds_fought * params.withdraw_per_round_permille +
                                        behind * params.withdraw_pursuit_scale / 1000);

    const int after = apply_loss(own, parting);
    if (attacker_leaving) st.attacker_strength_permille = after;
    else                  st.defender_strength_permille = after;

    st.end = attacker_leaving ? campaign_battle_end::attacker_withdrew
                              : campaign_battle_end::defender_withdrew;
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
            // Stalemate (or read mid-fight): the stronger remaining force holds,
            // defender on an exact tie.
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

campaign_battle_outcome resolve_campaign_battle(const campaign_battle_identity&      id,
                                                const std::vector<army_stack_entry>& attacker,
                                                const doctrine_row&                  attacker_doctrine,
                                                const std::vector<army_stack_entry>& defender,
                                                const doctrine_row&                  defender_doctrine,
                                                terrain_composition                  terrain_comp,
                                                terrain_landform                     terrain_lf,
                                                season                               battle_season,
                                                int                                  attacker_supply,
                                                int                                  defender_supply,
                                                withdrawing_side                     withdrawer,
                                                int                                  withdraw_after_round,
                                                const campaign_battle_params&        params)
{
    campaign_battle_state st = begin_campaign_battle(id, attacker, attacker_doctrine,
                                                      defender, defender_doctrine,
                                                      terrain_comp, terrain_lf, battle_season,
                                                      attacker_supply, defender_supply);

    const bool scripted = (withdrawer != withdrawing_side::none) && (withdraw_after_round > 0);

    while (st.end == campaign_battle_end::in_progress)
    {
        // The window is checked BEFORE each round, so "withdraw after round N"
        // means N rounds were actually fought — the same instant a watching
        // player would get the choice.
        if (scripted && st.rounds_fought >= withdraw_after_round)
        {
            withdraw_campaign_battle(st, withdrawer, params);
            break;
        }
        step_campaign_battle(st, params);
    }

    return campaign_battle_result(st);
}
