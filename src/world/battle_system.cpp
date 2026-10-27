#include "battle_system.hpp"

#include "logistics.hpp" // road_edge_neighbour: the retreat step (BL-1421)
#include "placement_rules.hpp"
#include "province.hpp"
#include "recipe_registry.hpp"
#include "stance.hpp"
#include "terrain_combat.hpp"
#include "unit_roster.hpp"
#include "world.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// The terrain pair (ruling 2, settled 2026-08-19)
// ---------------------------------------------------------------------------
// The envelope spans several tiles and the battle reads ONE pair. It comes from
// the DEFENDER-side tile with the highest terrain_defence in the envelope: the
// defender is presumed to anchor on the best ground, which is what defending
// means, and terrain_defence already multiplies the defender only.
//
// Since BL-519 that reads all three axes, and the split earns its keep here —
// a forested crag can now out-defend the bare rock beside it, which the single
// composition slot could not express. Ties break on ASCENDING TILE ID, so the
// choice is a pure function of the tile set and not of the order units were
// walked in.
struct terrain_pick
{
    terrain_substrate sub     = terrain_substrate::sedimentary;
    terrain_cover     cov     = terrain_cover::grass;
    std::uint8_t      density = 150u;
    terrain_landform  lf      = terrain_landform::plains;
    entity_id         tile    = null_entity; ///< the picked tile (BL-1145: the card frames it)
    bool              found   = false;
};

terrain_pick defender_ground(const world& w, const std::vector<entity_id>& defender_units)
{
    // Ascending tile id, so the argmax's tie-break is exact rather than
    // whatever order the unit list happened to arrive in. The unit lists are
    // already ascending by unit id, which is NOT the same ordering.
    std::vector<entity_id> tiles;
    tiles.reserve(defender_units.size());
    for (const entity_id uid : defender_units)
    {
        const auto it = w.units.find(uid);
        if (it != w.units.end() && it->second.position != null_entity)
            tiles.push_back(it->second.position);
    }
    std::sort(tiles.begin(), tiles.end());
    tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());

    terrain_pick best;
    int best_defence = -1;
    for (const entity_id tid : tiles)
    {
        const auto tit = w.tiles.find(tid);
        if (tit == w.tiles.end())
            continue;
        const tile_component& tc = tit->second;
        const int d = terrain_defence(tc.substrate, tc.cover, tc.cover_density, tc.landform);
        if (d > best_defence) // strict >, so the first (lowest) id wins a tie
        {
            best_defence  = d;
            best.sub      = tc.substrate;
            best.cov      = tc.cover;
            best.density  = tc.cover_density;
            best.lf       = tc.landform;
            best.tile     = tid;
            best.found    = true;
        }
    }
    return best;
}


// ---------------------------------------------------------------------------
// Combatants (BL-1421)
// ---------------------------------------------------------------------------
// The resolver is world-free: it fights `land_combatant`s. This pass builds them
// from the units a battle snapshotted at open, steps, and writes men and
// readiness back. NO NEW DRAWS anywhere in this file — the battle's whole
// uncertainty budget stays inside step_campaign_battle's two.

/// A side's combatants, in the side's own (ascending unit id) order, every unit
/// with men standing.
std::vector<land_combatant> build_combatants(const world& w, const std::vector<entity_id>& units)
{
    std::vector<land_combatant> out;
    out.reserve(units.size());
    for (const entity_id uid : units)
    {
        const auto it = w.units.find(uid);
        if (it == w.units.end() || it->second.count <= 0)
            continue;
        const unit_component& u = it->second;
        land_combatant c;
        c.unit      = uid;
        c.men       = u.count;
        c.readiness = std::clamp(u.readiness_permille, 0, 1000);
        c.morale    = std::clamp(u.morale_permille, 0, 1000);
        c.supply    = std::clamp(u.supply_factor_permille, 0, 1000);
        c.stats     = unit_land_stats(u);
        out.push_back(c);
    }
    return out;
}

/// Write a side's men and readiness back to its units. @return men removed.
/// CONSERVATION: the men the resolver removed are exactly the men the units
/// lose — the count is copied, never re-derived — so the pass cannot mint or
/// destroy men. `battle_engagement_harness` asserts it.
int write_back(world& w, const std::vector<land_combatant>& side)
{
    int removed = 0;
    for (const land_combatant& c : side)
    {
        const auto it = w.units.find(c.unit);
        if (it == w.units.end())
            continue;
        removed += std::max(0, it->second.count - c.men);
        it->second.count              = std::max(0, c.men);
        it->second.readiness_permille = std::clamp(c.readiness, 0, 1000);
    }
    return removed;
}

/// True iff a side can START a fight: at least one unit with men and readiness.
/// A defender need only have men standing — spent defenders are overrun.
bool side_can_attack(const std::vector<land_combatant>& side)
{
    // Fighting AND able to hit: a side of `none`-class units (a ship ashore) has
    // nothing to attack with, and must not open a fight it can only lose.
    for (const land_combatant& c : side)
        if (land_combatant_fighting(c) && (c.stats.soft > 0 || c.stats.hard > 0))
            return true;
    return false;
}

bool side_has_men(const std::vector<land_combatant>& side)
{
    for (const land_combatant& c : side)
        if (c.men > 0)
            return true;
    return false;
}

void move_morale(world& w, const std::vector<entity_id>& units, int delta)
{
    for (const entity_id uid : units)
    {
        const auto it = w.units.find(uid);
        if (it != w.units.end())
            it->second.morale_permille = std::clamp(it->second.morale_permille + delta, 0, 1000);
    }
}

/// Where a retreating unit goes: ONE TILE AWAY FROM THE ENEMY (MILITARY.md § Land —
/// the unit's stats). Among the neighbours a march can step to — the four sides a
/// cost walk crosses (`road_side_is_crossable`), never all six — land only and never
/// a tile an enemy of the unit's owner stands on, preferring ground outside the
/// battle's province, then the unit's own nation's ground; ties to the lowest tile id.
/// `null_entity` if no neighbour qualifies — the unit stays where it is, out of the
/// fight.
entity_id retreat_tile(world& w, const unit_component& u, uint32_t province,
                       const std::set<entity_id>& enemy_tiles)
{
    const auto here = w.tiles.find(u.position);
    if (here == w.tiles.end())
        return null_entity;
    const int row = here->second.grid_y;

    entity_id best      = null_entity;
    int       best_rank = -1;
    for (int side = 0; side < 6; ++side)
    {
        if (!road_side_is_crossable(side, row))
            continue;
        const entity_id n = road_edge_neighbour(w, u.position, side);
        if (n == null_entity || enemy_tiles.count(n) != 0)
            continue;
        const auto tit = w.tiles.find(n);
        if (tit == w.tiles.end() || is_water(tit->second.substrate))
            continue;
        const auto nat  = w.tile_to_nation.find(n);
        const int  rank = (w.provinces.province_of(n) != province ? 2 : 0)
                        + (nat != w.tile_to_nation.end() && nat->second == u.owner ? 1 : 0);
        if (rank > best_rank || (rank == best_rank && n < best))
        {
            best      = n;
            best_rank = rank;
        }
    }
    return best;
}

/// Every tile an ENEMY OF @p owner stands on, across every live battle @p owner is
/// in — a unit can be in more than one fight in one province, and a retreat must not
/// walk onto any of its foes.
std::set<entity_id> enemy_tiles_of(const world& w, entity_id owner)
{
    std::set<entity_id> out;
    auto add = [&](const std::vector<entity_id>& units) {
        for (const entity_id uid : units)
        {
            const auto it = w.units.find(uid);
            if (it != w.units.end() && it->second.count > 0 && it->second.position != null_entity)
                out.insert(it->second.position);
        }
    };
    for (const active_battle& b : w.battles)
    {
        if (b.attacker == owner) add(b.defender_units);
        if (b.defender == owner) add(b.attacker_units);
    }
    return out;
}

/// Retreat every unit of @p units with men standing that is spent (readiness 0)
/// or, when @p all, every one of them: each moves one tile away from its enemies,
/// clears its order, loses morale ONCE, and leaves EVERY battle's unit list — a unit
/// in two fights retreats from both, not twice. Dead units stay listed for the upkeep
/// pass's orphan cleanup. The enemy tiles are read once, before anyone moves, so the
/// order units retreat in cannot change where they go. @return units retreated.
int retreat_side(world& w, std::vector<entity_id>& units, entity_id owner, uint32_t province,
                 bool all, const campaign_battle_params& p)
{
    std::vector<entity_id> leaving;
    for (const entity_id uid : units)
    {
        const auto it = w.units.find(uid);
        if (it != w.units.end() && it->second.count > 0
            && (all || it->second.readiness_permille <= 0))
            leaving.push_back(uid);
    }
    if (leaving.empty())
        return 0;

    const std::set<entity_id> enemy_tiles = enemy_tiles_of(w, owner);
    for (const entity_id uid : leaving)
    {
        unit_component& u = w.units.at(uid);
        if (const entity_id to = retreat_tile(w, u, province, enemy_tiles); to != null_entity)
            u.position = to;
        u.order           = movement_order{};
        u.morale_permille = std::clamp(u.morale_permille - p.morale_loss, 0, 1000);
    }

    const std::set<entity_id> gone(leaving.begin(), leaving.end());
    auto strip = [&](std::vector<entity_id>& v) {
        v.erase(std::remove_if(v.begin(), v.end(), [&](entity_id x) { return gone.count(x) != 0; }),
                v.end());
    };
    for (active_battle& other : w.battles)
    {
        strip(other.attacker_units);
        strip(other.defender_units);
    }
    strip(units); // a no-op when @p units is one of w.battles' own lists; kept for a caller's copy
    return static_cast<int>(leaving.size());
}

/// Out of contact, every unit recovers: readiness toward its morale (scaled by its
/// supply, never above it), morale toward its supply. Each unit's step reads only
/// itself, so the visit order cannot matter. A fully supplied unit at full
/// readiness and morale is untouched, so a world that never fights is unmoved.
void recover_out_of_contact(world& w, const campaign_battle_params& p)
{
    std::set<entity_id> in_contact;
    for (const active_battle& b : w.battles)
    {
        if (b.state.end != campaign_battle_end::in_progress)
            continue;
        in_contact.insert(b.attacker_units.begin(), b.attacker_units.end());
        in_contact.insert(b.defender_units.begin(), b.defender_units.end());
    }
    for (auto& [uid, u] : w.units)
    {
        if (u.count <= 0 || in_contact.count(uid) != 0)
            continue;
        const int supply = std::clamp(u.supply_factor_permille, 0, 1000);
        int morale = std::clamp(u.morale_permille, 0, 1000);
        if (morale < supply)      morale += std::min(p.morale_recovery_permille, supply - morale);
        else if (morale > supply) morale -= std::min(p.morale_recovery_permille, morale - supply);
        u.morale_permille = morale;

        int ready = std::clamp(u.readiness_permille, 0, 1000);
        if (ready < morale)
            ready = std::min(morale, ready + p.readiness_recovery_permille * supply / 1000);
        else
            ready = morale;
        u.readiness_permille = ready;
    }
}

/// A candidate engagement found by discovery, before any battle is opened.
struct candidate
{
    uint32_t  province = 0;
    entity_id attacker = null_entity;
    entity_id defender = null_entity;
};

bool candidate_less(const candidate& a, const candidate& b)
{
    if (a.province != b.province) return a.province < b.province;
    if (a.attacker != b.attacker) return a.attacker < b.attacker;
    return a.defender < b.defender;
}

} // namespace

battle_phase read_battle_phase(const active_battle& b, int attacker_swing, int defender_swing,
                               const campaign_battle_params& params)
{
    // Order matters: the later readings are the more urgent ones, so they win.
    if (b.state.end != campaign_battle_end::in_progress)
    {
        if (b.state.end == campaign_battle_end::attacker_withdrew
         || b.state.end == campaign_battle_end::defender_withdrew)
            return battle_phase::broke_off;
        return battle_phase::concluded;
    }

    // WAVERING is read against the authored readiness mark rather than a fixed
    // number, so the word stays true if it is ever tuned (BL-1421: the strength
    // reading is side readiness, and a side breaks only when none is left).
    // Near enough that leaving is the live question, which is the whole reason
    // the phase exists.
    if (b.state.attacker_strength_permille <= params.wavering_readiness
     || b.state.defender_strength_permille <= params.wavering_readiness)
        return battle_phase::wavering;

    // CRISIS is a big swing across this tick's batch on either side. Quoted
    // against the batch, not the round, because a surface speaks once per batch.
    const int worst = std::max(-attacker_swing, -defender_swing);
    if (worst >= 150)
        return battle_phase::crisis;

    // The opening is the first batch. After that both sides are committed —
    // which is a statement about the withdrawal price, not a mood: by round 4 the
    // per-round term alone has tripled.
    return (b.state.rounds_fought <= std::max(1, params.rounds_per_tick))
               ? battle_phase::opening
               : battle_phase::committed;
}

withdrawal_price quote_withdrawal(const active_battle& b, withdrawing_side side,
                                  const campaign_battle_params& params)
{
    withdrawal_price q;
    if (side == withdrawing_side::none)
        return q;

    // The SAME arithmetic withdraw_campaign_battle applies, separated into its
    // three terms. Kept beside the resolver's own copy deliberately rather than
    // in the card: a second copy in a surface is a second place for the quoted
    // price to drift from the price actually charged, and a player who is shown
    // one number and charged another has been lied to about the only decision
    // the fight contains.
    const bool attacker_leaving = (side == withdrawing_side::attacker);
    const int own   = attacker_leaving ? b.state.attacker_strength_permille
                                       : b.state.defender_strength_permille;
    const int enemy = attacker_leaving ? b.state.defender_strength_permille
                                       : b.state.attacker_strength_permille;
    const int behind = std::max(0, enemy - own);

    q.base      = params.withdraw_base_permille;
    q.per_round = b.state.rounds_fought * params.withdraw_per_round_permille;
    q.pursuit   = behind * params.withdraw_pursuit_scale / 1000;
    // One price: the resolver's own total, which is what withdraw_campaign_battle
    // charges (in men since BL-1421).
    q.total     = withdrawal_cost_permille(b.state, side, params);
    return q;
}

bool battle_ground(const world& w, const std::vector<entity_id>& defender_units,
                   terrain_substrate& sub, terrain_cover& cov, std::uint8_t& density,
                   terrain_landform& lf)
{
    const terrain_pick g = defender_ground(w, defender_units);
    if (!g.found)
        return false;
    sub = g.sub; cov = g.cov; density = g.density; lf = g.lf;
    return true;
}

bool unit_in_battle(const world& w, entity_id unit)
{
    for (const active_battle& b : w.battles)
    {
        if (b.state.end != campaign_battle_end::in_progress)
            continue;
        if (std::find(b.attacker_units.begin(), b.attacker_units.end(), unit)
            != b.attacker_units.end())
            return true;
        if (std::find(b.defender_units.begin(), b.defender_units.end(), unit)
            != b.defender_units.end())
            return true;
    }
    return false;
}

const active_battle* find_battle(const world& w, entity_id corp, uint32_t province)
{
    for (const active_battle& b : w.battles)
        if (b.province == province && (b.attacker == corp || b.defender == corp))
            return &b;
    return nullptr;
}

const active_battle* first_battle_in(const world& w, uint32_t province)
{
    const active_battle* fallback = nullptr;
    for (const active_battle& b : w.battles)
    {
        if (b.province != province)
            continue;
        if (b.attacker == w.player_entity || b.defender == w.player_entity)
            return &b;                    // yours first — it is the one you can act on
        if (fallback == nullptr)
            fallback = &b;                // else the lowest in sorted order
    }
    return fallback;
}

bool request_withdraw(world& w, entity_id corp, uint32_t province, entity_id against)
{
    // MUTATES NOTHING on rejection. This verb is reachable from the AI-facing
    // seam (--serve, the MCP server), where the standing rule is that a whole
    // command is rejected rather than partially applied.
    for (active_battle& b : w.battles)
    {
        if (b.province != province)
            continue;
        if (b.state.end != campaign_battle_end::in_progress)
            continue;
        if (b.attacker == corp)
        {
            if (against != null_entity && b.defender != against) continue;
            b.withdraw_requested = withdrawing_side::attacker;
            return true;
        }
        if (b.defender == corp)
        {
            if (against != null_entity && b.attacker != against) continue;
            b.withdraw_requested = withdrawing_side::defender;
            return true;
        }
    }
    return false;
}


bool open_battle(world& w, int tick, uint32_t province, entity_id attacker, entity_id defender)
{
    // Already fighting? Same identity rule discovery uses: a pair fights at
    // most one battle per province at a time, in EITHER role order.
    for (const active_battle& b : w.battles)
    {
        if (b.province != province)
            continue;
        if ((b.attacker == attacker && b.defender == defender)
         || (b.attacker == defender && b.defender == attacker))
            return false;
    }

    // Gather each side's units standing in `province`, ascending unit id —
    // self-contained (unlike run_battles' discovery, this does not require a
    // pre-built by-owner-by-province map), so a caller — either trigger, or a
    // harness with no such map to hand — can call it directly.
    std::vector<entity_id> attacker_units, defender_units;
    {
        std::vector<entity_id> ids;
        ids.reserve(w.units.size());
        for (const auto& kv : w.units)
            ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end());
        for (const entity_id uid : ids)
        {
            const unit_component& u = w.units.at(uid);
            if (u.count <= 0 || u.owner == null_entity || u.position == null_entity)
                continue;
            if (w.provinces.province_of(u.position) != province)
                continue;
            if (u.owner == attacker)
                attacker_units.push_back(uid);
            else if (u.owner == defender)
                defender_units.push_back(uid);
        }
    }
    if (attacker_units.empty() || defender_units.empty())
        return false;

    // A spent attacker starts nothing; a defender need only have men standing.
    const std::vector<land_combatant> atk = build_combatants(w, attacker_units);
    const std::vector<land_combatant> def = build_combatants(w, defender_units);
    if (!side_can_attack(atk) || !side_has_men(def))
        return false;

    const terrain_pick g = defender_ground(w, defender_units);

    campaign_battle_identity id;
    id.attacker   = attacker;
    id.defender   = defender;
    id.province   = province;
    id.tick       = static_cast<uint64_t>(tick);
    id.world_seed = w.provinces.seed;

    active_battle nb;
    nb.province        = province;
    nb.attacker        = attacker;
    nb.defender        = defender;
    nb.attacker_units  = attacker_units;
    nb.defender_units  = defender_units;
    nb.ground_tile     = g.tile; // the ground the fight is ON, fixed at open (BL-1145 review)
    nb.state = begin_campaign_battle(id, atk, def, g.sub, g.cov, g.density, g.lf);

    w.battles.push_back(std::move(nb));
    std::sort(w.battles.begin(), w.battles.end(), battle_order_less);
    return true;
}

battle_tick run_battles(world& w, const recipe_registry& reg, int tick)
{
    battle_tick out;
    (void)reg; // the resolver's tuning is campaign_battle_params, not the registry
    const campaign_battle_params params{};

    // -----------------------------------------------------------------------
    // 1. DISCOVERY
    // -----------------------------------------------------------------------
    // Walked from `corp_hostile_pairs`, which is a std::set and therefore
    // already ordered — a deliberate choice over walking every province, since
    // the hostile set is small and every candidate must contain a hostile pair
    // by definition. The RESULT is sorted by (province, attacker, defender)
    // regardless, because that is the order the ruling names and because the
    // unit map this joins against is unordered.
    if (!w.corp_hostile_pairs.empty() && !w.units.empty())
    {
        // owner -> province -> its unit ids, ascending. Built once per tick.
        std::map<entity_id, std::map<uint32_t, std::vector<entity_id>>> by_owner;
        {
            std::vector<entity_id> uids;
            uids.reserve(w.units.size());
            for (const auto& kv : w.units)
                uids.push_back(kv.first);
            std::sort(uids.begin(), uids.end()); // ascending: the remainder rule depends on it
            for (const entity_id uid : uids)
            {
                const unit_component& u = w.units.at(uid);
                if (u.count <= 0 || u.owner == null_entity || u.position == null_entity)
                    continue;
                const uint32_t prov = w.provinces.province_of(u.position);
                if (prov == 0)
                    continue; // unpartitioned ground (ocean, off-body) frames no fight
                by_owner[u.owner][prov].push_back(uid);
            }
        }

        std::vector<candidate> candidates;
        for (const auto& [from, to] : w.corp_hostile_pairs)
        {
            const auto fit = by_owner.find(from);
            const auto tit = by_owner.find(to);
            if (fit == by_owner.end() || tit == by_owner.end())
                continue;
            for (const auto& [prov, fu] : fit->second)
            {
                if (tit->second.find(prov) == tit->second.end())
                    continue;
                // DIRECTED HOSTILITY SUFFICES — the ambush property BL-448 paid
                // two tables for. The declarer attacks; the target is drawn in
                // regardless of what it thinks of the declarer.
                (void)fu;
                candidates.push_back({ prov, from, to });
            }
        }

        // Mutual hostility would otherwise open the pair twice, once per
        // direction, with the roles swapped — and those are DIFFERENT streams
        // (the seed folds role order deliberately). One battle per pair per
        // province: the direction whose ATTACKER SORTS LOWER wins, which is a
        // property of the ids rather than of who declared first.
        //
        // That rule is enforced by the `exists` scan below and by this sort, NOT
        // by a dedup pass. A std::unique over unordered-by-pair equality was
        // tried and removed: std::unique only collapses ADJACENT equals, and
        // with a third nation C where A < C < B the sorted order is
        // (P,A,B), (P,A,C), (P,B,A) — the mutual pair is not adjacent, so the
        // dedup silently did nothing in exactly the case it was written for. The
        // sort is what makes (P,A,B) reach `exists` before (P,B,A); the scan is
        // what refuses the second. One mechanism, and it works for any number of
        // nations.
        std::sort(candidates.begin(), candidates.end(), candidate_less);

        for (const candidate& c : candidates)
        {
            bool exists = false;
            for (const active_battle& b : w.battles)
            {
                if (b.province != c.province) continue;
                if ((b.attacker == c.attacker && b.defender == c.defender)
                 || (b.attacker == c.defender && b.defender == c.attacker))
                {
                    exists = true;
                    break;
                }
            }
            if (exists)
                continue;

            active_battle nb;
            nb.province = c.province;
            nb.attacker = c.attacker;
            nb.defender = c.defender;
            nb.attacker_units = by_owner[c.attacker][c.province];
            nb.defender_units = by_owner[c.defender][c.province];
            if (nb.attacker_units.empty() || nb.defender_units.empty())
                continue;

            // A spent attacker starts nothing; a defender need only have men
            // standing — spent defenders are overrun (step_campaign_battle).
            const std::vector<land_combatant> atk = build_combatants(w, nb.attacker_units);
            const std::vector<land_combatant> def = build_combatants(w, nb.defender_units);
            if (!side_can_attack(atk) || !side_has_men(def))
                continue;

            const terrain_pick g = defender_ground(w, nb.defender_units);
            nb.ground_tile = g.tile; // the ground the fight is ON, fixed at open (BL-1145 review)

            campaign_battle_identity id;
            id.attacker   = c.attacker;
            id.defender   = c.defender;
            id.province   = c.province;
            id.tick       = static_cast<uint64_t>(tick);
            // `world` carries no seed of its own; the partition's is the campaign
            // seed it was built from and is stable for the world's lifetime, which
            // is exactly what campaign_battle_identity asks of this field ("so two
            // worlds with equal ids still differ").
            id.world_seed = w.provinces.seed;

            nb.state = begin_campaign_battle(id, atk, def, g.sub, g.cov, g.density, g.lf);

            w.battles.push_back(std::move(nb));
            ++out.opened;
        }
        std::sort(w.battles.begin(), w.battles.end(), battle_order_less);
    }

    // -----------------------------------------------------------------------
    // 2. STEP — several rounds per tick (ruling 3)
    // -----------------------------------------------------------------------
    // `w.battles` is kept sorted, so this walk is order-independent even though
    // discovery joined against unordered containers.
    for (active_battle& b : w.battles)
    {
        if (b.state.end != campaign_battle_end::in_progress)
            continue;

        // The side's units AS THEY STAND NOW: men lost, readiness spent and supply
        // moved since the last batch all carry into this one.
        std::vector<land_combatant> atk = build_combatants(w, b.attacker_units);
        std::vector<land_combatant> def = build_combatants(w, b.defender_units);

        const int before_a = b.state.attacker_strength_permille;
        const int before_d = b.state.defender_strength_permille;
        const int rounds_before = b.state.rounds_fought;

        // WITHDRAWAL IS HONOURED AT THE TICK BOUNDARY, before the next round
        // batch — that is what makes the window a real window. The price is the
        // resolver's own three-term cost, charged in men; this pass adds no
        // second rule.
        withdrawing_side left = withdrawing_side::none;
        if (b.withdraw_requested != withdrawing_side::none)
        {
            left = b.withdraw_requested;
            withdraw_campaign_battle(b.state, left,
                                     left == withdrawing_side::attacker ? atk : def, params);
            b.withdraw_requested = withdrawing_side::none;
            ++out.withdrew;
        }

        const int budget = std::max(1, params.rounds_per_tick);
        while (b.state.end == campaign_battle_end::in_progress
               && b.state.rounds_fought - rounds_before < budget)
            step_campaign_battle(b.state, atk, def, params);
        const int fought = b.state.rounds_fought - rounds_before;
        if (fought > 0)
        {
            ++out.stepped;
            out.rounds += fought;
        }

        // -------------------------------------------------------------------
        // 3. PER-UNIT HITS, then RETREATS
        // -------------------------------------------------------------------
        // Men and readiness land on the units as the batch fought them, so a
        // battle weakens units while it runs — which is what makes a mid-fight
        // withdrawal a real decision.
        const int lost_a = write_back(w, atk);
        const int lost_d = write_back(w, def);
        out.losses += lost_a + lost_d;

        // A spent unit leaves the fight one tile away from its enemies; a side that
        // withdrew leaves whole.
        retreat_side(w, b.attacker_units, b.attacker, b.province,
                     left == withdrawing_side::attacker, params);
        retreat_side(w, b.defender_units, b.defender, b.province,
                     left == withdrawing_side::defender, params);

        // The strength reading follows the units STILL IN the fight, so the next
        // withdrawal price and phase are read against the force that is there, not
        // against the spent units that just left it. A concluded battle keeps the
        // reading its last round ended on — that is what decided its field.
        if (b.state.end == campaign_battle_end::in_progress)
        {
            b.state.attacker_strength_permille = side_readiness(build_combatants(w, b.attacker_units));
            b.state.defender_strength_permille = side_readiness(build_combatants(w, b.defender_units));
        }

        if (b.state.end != campaign_battle_end::in_progress)
            ++out.concluded;

        // -------------------------------------------------------------------
        // 3b. THE DISPATCH RECORD (BL-468 / BL-469)
        // -------------------------------------------------------------------
        // Emitted HERE, before the retire pass below erases a concluded battle.
        // That ordering is the whole reason this is a report rather than a read:
        // the aftermath line — who held the field and what it cost — is the one a
        // player most needs, and it describes a record that no longer exists by
        // the time any surface runs.
        //
        // Nothing here consumes a draw. The text is a pure function of this record
        // plus the battle's own stream seed, so the dispatch layer cannot move the
        // simulation however it is written.
        if (fought > 0 || b.state.end != campaign_battle_end::in_progress || lost_a || lost_d)
        {
            const int swing_a = b.state.attacker_strength_permille - before_a;
            const int swing_d = b.state.defender_strength_permille - before_d;

            battle_dispatch d;
            d.province = b.province;
            d.attacker = b.attacker;
            d.defender = b.defender;
            d.phase    = read_battle_phase(b, swing_a, swing_d, params);
            d.rounds_fought    = b.state.rounds_fought;
            d.rounds_this_tick = fought;
            d.max_rounds       = params.max_rounds;
            d.attacker_strength_permille = b.state.attacker_strength_permille;
            d.defender_strength_permille = b.state.defender_strength_permille;
            d.attacker_swing_permille = swing_a;
            d.defender_swing_permille = swing_d;
            d.attacker_men_lost = lost_a;
            d.defender_men_lost = lost_d;
            d.end               = b.state.end;
            d.stream_seed       = b.state.stream_seed;
            if (b.state.end == campaign_battle_end::attacker_withdrew)
                d.withdrew = withdrawing_side::attacker;
            else if (b.state.end == campaign_battle_end::defender_withdrew)
                d.withdrew = withdrawing_side::defender;
            // WHO HOLDS THE FIELD: the resolver's own rule, read through
            // campaign_battle_result so the dispatch and the resolver cannot
            // disagree about who won.
            if (b.state.end != campaign_battle_end::in_progress)
                d.field_held_by = campaign_battle_result(b.state).result == battle_result::attacker_victory
                                      ? b.attacker : b.defender;

            // BL-569 (province holder): a DECISIVE close moves the holder to
            // whoever field_held_by already names — every ending except
            // stalemate, mirroring MILITARY.md's "moved by a decisive
            // battle's field_held_by, left untouched by a stalemate". Reuses
            // `province_partition::find` (province.hpp) to translate the
            // province id into `province_holder`'s positional index; a world
            // whose holder vector was never seeded (empty, or a fixture that
            // skipped generation) is left untouched rather than indexed
            // out of bounds.
            if (b.state.end != campaign_battle_end::in_progress
                && b.state.end != campaign_battle_end::stalemate)
            {
                if (const province* pr = w.provinces.find(b.province))
                {
                    const auto idx = static_cast<std::size_t>(pr - w.provinces.provinces.data());
                    if (idx < w.province_holder.size())
                        w.province_holder[idx] = d.field_held_by;
                }

                // MORALE ON THE RESULT (MILITARY.md § Land — the unit's stats): the
                // field's holders gain, the units the other side still has in the
                // fight lose. Units that retreated already paid their loss.
                const bool atk_won = (d.field_held_by == b.attacker);
                move_morale(w, atk_won ? b.attacker_units : b.defender_units,  params.morale_win);
                move_morale(w, atk_won ? b.defender_units : b.attacker_units, -params.morale_loss);
            }

            out.dispatches.push_back(d);
        }
    }

    // -----------------------------------------------------------------------
    // 4. RETIRE
    // -----------------------------------------------------------------------
    // The trace is discarded with the record: it is retained only for the span
    // of the fight, so nothing but the record itself ever crosses a tick
    // boundary. Zero-count units are NOT disbanded here — `run_unit_upkeep`'s
    // existing orphan cleanup runs last in the economy step and does it, and a
    // second disband mechanism is exactly what BL-467's aftermath note rules out.
    w.battles.erase(
        std::remove_if(w.battles.begin(), w.battles.end(),
                       [](const active_battle& b) {
                           return b.state.end != campaign_battle_end::in_progress;
                       }),
        w.battles.end());

    // -----------------------------------------------------------------------
    // 5. RECOVERY (BL-1421) — every unit not in a live fight
    // -----------------------------------------------------------------------
    recover_out_of_contact(w, params);

    return out;
}
