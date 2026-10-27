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
// The ground (ruling 2; Ben, 2026-10-27)
// ---------------------------------------------------------------------------
// A battle is fought over ONE tile — the defended one — and reads that tile's
// terrain. The 2026-08-19 envelope picked the defender's best tile in a
// province; with the footprint now the tile, there is nothing to pick.
struct terrain_pick
{
    terrain_substrate sub     = terrain_substrate::sedimentary;
    terrain_cover     cov     = terrain_cover::grass;
    std::uint8_t      density = 150u;
    terrain_landform  lf      = terrain_landform::plains;
    entity_id         tile    = null_entity; ///< the defended tile (BL-1145: the card frames it)
    bool              found   = false;
};

terrain_pick tile_ground(const world& w, entity_id tile)
{
    terrain_pick g;
    const auto tit = w.tiles.find(tile);
    if (tit == w.tiles.end())
        return g;
    const tile_component& tc = tit->second;
    g.sub = tc.substrate; g.cov = tc.cover; g.density = tc.cover_density; g.lf = tc.landform;
    g.tile = tile;
    g.found = true;
    return g;
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
/// a tile ANOTHER NATION'S units stand on, whatever its stance (one nation per tile,
/// Ben 2026-10-27), preferring ground outside the battle's province, then the
/// unit's own nation's ground; ties to the lowest tile id.
/// `null_entity` if no neighbour qualifies — the unit stays where it is, out of the
/// fight.
entity_id retreat_tile(world& w, const unit_component& u, uint32_t province,
                       const std::set<entity_id>& foreign_tiles)
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
        if (n == null_entity || foreign_tiles.count(n) != 0)
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

/// Retreat every unit of @p units with men standing that is spent (readiness 0)
/// or, when @p all, every one of them: each moves one tile away from its enemies,
/// clears its order, loses morale ONCE, and leaves EVERY battle's unit list — a unit
/// in two fights retreats from both, not twice. Dead units stay listed for the upkeep
/// pass's orphan cleanup. The foreign-held tiles are read once, before anyone moves, so
/// the order units retreat in cannot change where they go (a side is one nation, so
/// its own units sharing a tile is a stack, never a breach). @return units retreated.
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

    const std::set<entity_id> foreign = foreign_held_tiles(w, owner);
    for (const entity_id uid : leaving)
    {
        unit_component& u = w.units.at(uid);
        if (const entity_id to = retreat_tile(w, u, province, foreign); to != null_entity)
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

/// The six hex neighbours of @p tile, ascending and unique — "adjacent" for the
/// join rule. All six sides, not the four a march crosses: a stack beside the
/// tile is beside it whichever way the grid runs.
std::vector<entity_id> hex_neighbours(const world& w, entity_id tile)
{
    std::vector<entity_id> out;
    for (int side = 0; side < 6; ++side)
        if (const entity_id n = road_edge_neighbour(w, tile, side); n != null_entity && n != tile)
            out.push_back(n);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
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

bool battle_ground(const world& w, entity_id tile,
                   terrain_substrate& sub, terrain_cover& cov, std::uint8_t& density,
                   terrain_landform& lf)
{
    const terrain_pick g = tile_ground(w, tile);
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

const active_battle* find_battle(const world& w, entity_id corp, entity_id tile)
{
    for (const active_battle& b : w.battles)
        if (b.tile == tile && (b.attacker == corp || b.defender == corp))
            return &b;
    return nullptr;
}

const active_battle* first_battle_at(const world& w, entity_id tile)
{
    const active_battle* fallback = nullptr;
    for (const active_battle& b : w.battles)
    {
        if (b.tile != tile)
            continue;
        if (b.attacker == w.player_entity || b.defender == w.player_entity)
            return &b;
        if (fallback == nullptr)
            fallback = &b;
    }
    return fallback;
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

bool request_withdraw(world& w, entity_id corp, entity_id tile, entity_id against)
{
    // MUTATES NOTHING on rejection. This verb is reachable from the AI-facing
    // seam (--serve, the MCP server), where the standing rule is that a whole
    // command is rejected rather than partially applied.
    for (active_battle& b : w.battles)
    {
        if (b.tile != tile)
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

std::set<entity_id> units_in_live_battles(const world& w)
{
    std::set<entity_id> out;
    for (const active_battle& b : w.battles)
    {
        if (b.state.end != campaign_battle_end::in_progress)
            continue;
        out.insert(b.attacker_units.begin(), b.attacker_units.end());
        out.insert(b.defender_units.begin(), b.defender_units.end());
    }
    return out;
}

std::set<entity_id> foreign_held_tiles(const world& w, entity_id owner)
{
    std::set<entity_id> out;
    for (const auto& [uid, u] : w.units) // a set: the walk order cannot matter
        if (u.count > 0 && u.position != null_entity && u.owner != owner)
            out.insert(u.position);
    return out;
}

std::vector<entity_id> march_path(world& w, entity_id body, entity_id from, entity_id dest,
                                  const std::set<entity_id>& blocked)
{
    std::vector<entity_id> path;
    {
        const logistics_path& p = intra_body_path(w, body, from, dest);
        if (p.reachable && !p.tiles.empty())
        {
            path = p.tiles; // copied before any call that could touch the cache
            if (path.front() != from)
                std::reverse(path.begin(), path.end());
        }
    }
    bool clear = !path.empty();
    for (std::size_t i = 1; clear && i + 1 < path.size(); ++i)
        clear = blocked.count(path[i]) == 0;
    if (clear || blocked.empty())
        return path;

    const logistics_path p = intra_body_path_avoiding(w, body, from, dest, blocked);
    path.clear();
    if (p.reachable && !p.tiles.empty())
    {
        path = p.tiles;
        if (path.front() != from)
            std::reverse(path.begin(), path.end());
    }
    return path;
}

bool open_tile_battle(world& w, int tick, entity_id tile, entity_id attacker, entity_id defender,
                      const std::vector<entity_id>& attacking_units)
{
    if (attacker == defender || w.tiles.find(tile) == w.tiles.end())
        return false;
    // Already fighting over this tile? A pair fights at most one battle per tile
    // at a time, in EITHER role order.
    for (const active_battle& b : w.battles)
    {
        if (b.tile != tile)
            continue;
        if ((b.attacker == attacker && b.defender == defender)
         || (b.attacker == defender && b.defender == attacker))
            return false;
    }

    const std::set<entity_id> busy = units_in_live_battles(w);
    const std::vector<entity_id> beside = hex_neighbours(w, tile);
    const auto adjacent = [&](entity_id t) {
        return std::binary_search(beside.begin(), beside.end(), t);
    };

    std::vector<entity_id> stack; // the attacking stack, ascending
    for (const entity_id uid : attacking_units)
    {
        const auto it = w.units.find(uid);
        if (it != w.units.end() && it->second.owner == attacker && it->second.count > 0
            && it->second.position != null_entity && busy.count(uid) == 0)
            stack.push_back(uid);
    }
    std::sort(stack.begin(), stack.end());
    stack.erase(std::unique(stack.begin(), stack.end()), stack.end());
    if (stack.empty())
        return false;

    // Each side, ascending unit id. The defending stack on the tile fights whatever
    // else it is in; a stack beside the tile joins only if it is in no live battle.
    std::vector<entity_id> ids;
    ids.reserve(w.units.size());
    for (const auto& kv : w.units)
        ids.push_back(kv.first);
    std::sort(ids.begin(), ids.end());
    std::vector<entity_id> attacker_units, defender_units;
    for (const entity_id uid : ids)
    {
        const unit_component& u = w.units.at(uid);
        if (u.count <= 0 || u.position == null_entity)
            continue;
        if (u.owner == attacker)
        {
            if (std::binary_search(stack.begin(), stack.end(), uid)
                || (adjacent(u.position) && busy.count(uid) == 0))
                attacker_units.push_back(uid);
        }
        else if (u.owner == defender)
        {
            if (u.position == tile || (adjacent(u.position) && busy.count(uid) == 0))
                defender_units.push_back(uid);
        }
    }
    bool held = false;
    for (const entity_id uid : defender_units)
        held = held || w.units.at(uid).position == tile;
    if (!held)
        return false; // nobody of the defender's stands on the tile: nothing to attack

    // A spent attacker starts nothing; a defender need only have men standing.
    const std::vector<land_combatant> atk = build_combatants(w, attacker_units);
    const std::vector<land_combatant> def = build_combatants(w, defender_units);
    if (!side_can_attack(atk) || !side_has_men(def))
        return false;

    const terrain_pick g = tile_ground(w, tile);

    campaign_battle_identity id;
    id.attacker   = attacker;
    id.defender   = defender;
    id.tile       = tile;
    id.tick       = static_cast<uint64_t>(tick);
    // `world` carries no seed of its own; the partition's is the campaign seed it
    // was built from and is stable for the world's lifetime.
    id.world_seed = w.provinces.seed;

    active_battle nb;
    nb.tile            = tile;
    nb.province        = w.provinces.province_of(tile);
    nb.attacker        = attacker;
    nb.defender        = defender;
    nb.attacker_units  = attacker_units;
    nb.defender_units  = defender_units;
    nb.ground_tile     = tile;
    nb.attacking_units = stack;
    nb.state = begin_campaign_battle(id, atk, def, g.sub, g.cov, g.density, g.lf);

    w.battles.push_back(std::move(nb));
    std::sort(w.battles.begin(), w.battles.end(), battle_order_less);
    return true;
}

int open_tile_battles(world& w, int tick, std::vector<tile_attack> attacks)
{
    std::sort(attacks.begin(), attacks.end(), [](const tile_attack& a, const tile_attack& b) {
        if (a.tile != b.tile) return a.tile < b.tile;
        if (a.attacker != b.attacker) return a.attacker < b.attacker;
        if (a.defender != b.defender) return a.defender < b.defender;
        return a.unit < b.unit;
    });
    int opened = 0;
    for (std::size_t i = 0; i < attacks.size();)
    {
        std::size_t j = i;
        std::vector<entity_id> stack;
        while (j < attacks.size() && attacks[j].tile == attacks[i].tile
               && attacks[j].attacker == attacks[i].attacker
               && attacks[j].defender == attacks[i].defender)
            stack.push_back(attacks[j++].unit);
        if (open_tile_battle(w, tick, attacks[i].tile, attacks[i].attacker, attacks[i].defender, stack))
            ++opened;
        i = j;
    }
    return opened;
}

battle_tick run_battles(world& w, const recipe_registry& reg, int tick)
{
    battle_tick out;
    (void)reg; // the resolver's tuning is campaign_battle_params, not the registry
    const campaign_battle_params params{};

    (void)tick; // a battle's stream was seeded when it opened (open_tile_battle)

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
            d.tile     = b.tile;
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

                // THE ADVANCE (Ben, 2026-10-27): when the attacker holds the field, the
                // stack whose march made the attack steps onto the defended tile — the
                // units of it still in the fight and standing. Joined stacks stay where
                // they stood. Never onto a tile another nation still holds: a defender
                // with nowhere to retreat to stays put, and the advance waits on it.
                if (atk_won && !b.attacking_units.empty())
                {
                    bool foreign = false;
                    for (const auto& [uid, u] : w.units) // a test, so the walk order cannot matter
                        foreign = foreign || (u.count > 0 && u.position == b.tile && u.owner != b.attacker);
                    if (!foreign)
                        for (const entity_id uid : b.attacking_units)
                        {
                            if (std::find(b.attacker_units.begin(), b.attacker_units.end(), uid)
                                == b.attacker_units.end())
                                continue; // it retreated: it is not the one who took the tile
                            const auto it = w.units.find(uid);
                            if (it == w.units.end() || it->second.count <= 0)
                                continue;
                            it->second.position = b.tile;
                            it->second.order    = movement_order{};
                        }
                }
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
