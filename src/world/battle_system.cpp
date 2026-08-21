#include "battle_system.hpp"

#include "placement_rules.hpp"
#include "province.hpp"
#include "recipe_registry.hpp"
#include "stance.hpp"
#include "terrain_combat.hpp"
#include "unit_roster.hpp"
#include "world.hpp"

#include <algorithm>
#include <map>
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
            best.found    = true;
        }
    }
    return best;
}

// ---------------------------------------------------------------------------
// Per-unit hits
// ---------------------------------------------------------------------------
// The resolver returns per-side STRENGTH, per-mille of what marched in. The map
// needs per-unit counts. This distributes a side's losses across its units
// proportional to count, remainder assigned by ascending entity id.
//
// NO NEW DRAWS. It is a pure function of the round outcome, which is what keeps
// the battle's whole uncertainty budget inside step_campaign_battle's two draws.
// Supply and quality already entered through unit_to_stack_entry, so an
// unsupplied unit is weaker IN THE RESOLVER and its losses bite harder, with no
// second mechanism bolted on here.
//
// CONSERVATION: the distributed total equals the requested total exactly, or the
// pass would quietly mint or destroy men. `battle_engagement_harness` asserts it.
int distribute_losses(world& w, const std::vector<entity_id>& units, int to_remove)
{
    if (to_remove <= 0 || units.empty())
        return 0;

    int total = 0;
    for (const entity_id uid : units)
    {
        const auto it = w.units.find(uid);
        if (it != w.units.end() && it->second.count > 0)
            total += it->second.count;
    }
    if (total <= 0)
        return 0;
    if (to_remove > total)
        to_remove = total;

    // Floor share first, then hand the remainder out one man at a time in
    // ascending unit id. Integer arithmetic throughout — a float share would
    // make the split depend on rounding mode.
    int assigned = 0;
    std::vector<std::pair<entity_id, int>> hits;
    hits.reserve(units.size());
    for (const entity_id uid : units)
    {
        const auto it = w.units.find(uid);
        const int  c  = (it != w.units.end()) ? std::max(0, it->second.count) : 0;
        const long long share = (static_cast<long long>(to_remove) * c) / total;
        hits.emplace_back(uid, static_cast<int>(share));
        assigned += static_cast<int>(share);
    }
    // The remainder, one man at a time in ascending unit id, sweeping until it is
    // spent. Written as a bounded outer loop rather than an index that wraps:
    // termination is guaranteed (to_remove is clamped to the standing total, so
    // while assigned < to_remove some unit still has room), but a wrapping index
    // makes that argument something a reader has to reconstruct, and a loop that
    // is only obviously-terminating after a proof is a hazard.
    for (int pass = 0; assigned < to_remove && pass <= to_remove; ++pass)
    {
        bool progressed = false;
        for (std::size_t i = 0; i < hits.size() && assigned < to_remove; ++i)
        {
            const auto it = w.units.find(hits[i].first);
            if (it == w.units.end())
                continue;
            if (hits[i].second < it->second.count)
            {
                ++hits[i].second;
                ++assigned;
                progressed = true;
            }
        }
        if (!progressed)
            break; // every unit is at its own count; nothing left to assign
    }

    int removed = 0;
    for (const auto& [uid, hit] : hits)
    {
        if (hit <= 0)
            continue;
        const auto it = w.units.find(uid);
        if (it == w.units.end())
            continue;
        const int take = std::min(hit, it->second.count);
        it->second.count -= take;
        removed += take;
    }
    return removed;
}

/// Men currently standing on a side.
int side_count(const world& w, const std::vector<entity_id>& units)
{
    int n = 0;
    for (const entity_id uid : units)
    {
        const auto it = w.units.find(uid);
        if (it != w.units.end())
            n += std::max(0, it->second.count);
    }
    return n;
}

/// Build the resolver's stack for a side, in ascending unit id.
std::vector<army_stack_entry> build_side(const world& w, const std::vector<entity_id>& units)
{
    std::vector<army_stack_entry> out;
    out.reserve(units.size());
    for (const entity_id uid : units)
    {
        const auto it = w.units.find(uid);
        if (it != w.units.end() && it->second.count > 0)
            out.push_back(unit_to_stack_entry(w, it->second));
    }
    return out;
}

/// True iff a stack can actually fight — at least one entry with a land class
/// and a positive count.
///
/// THE DEGENERATE CASE THIS SCREENS, and it is not theoretical. `resolve_battle`
/// scores naval entries at EXACTLY zero (combat.cpp: sum_stack skips them,
/// class_base_power returns 0) and rejects nothing. With BOTH sides at zero
/// power its tie-break is a strict `>`, so a 0-vs-0 fight resolves as a DEFENDER
/// VICTORY and still returns 400/200 per-mille losses — casualties inflicted on
/// forces that could not have fought. Discovery opens a battle on stance and
/// position alone and never inspects unit class, so nothing else would have
/// caught it. Unreachable with today's land-only production roster; guarded
/// rather than left to be discovered when the first naval row lands.
bool stack_can_fight(const std::vector<army_stack_entry>& stack)
{
    for (const army_stack_entry& e : stack)
        if (e.cls != unit_class::naval && e.count > 0)
            return true;
    return false;
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

battle_tick run_battles(world& w, const recipe_registry& reg, int tick)
{
    battle_tick out;
    (void)reg; // the resolver's tuning is campaign_battle_params, not the registry

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
        // corp -> province -> its unit ids, ascending. Built once per tick.
        std::map<entity_id, std::map<uint32_t, std::vector<entity_id>>> by_corp;
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
                by_corp[u.owner][prov].push_back(uid);
            }
        }

        std::vector<candidate> candidates;
        for (const auto& [from, to] : w.corp_hostile_pairs)
        {
            const auto fit = by_corp.find(from);
            const auto tit = by_corp.find(to);
            if (fit == by_corp.end() || tit == by_corp.end())
                continue;
            for (const auto& [prov, fu] : fit->second)
            {
                const auto pit = tit->second.find(prov);
                if (pit == tit->second.end())
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
        // with a third corp C where A < C < B the sorted order is
        // (P,A,B), (P,A,C), (P,B,A) — the mutual pair is not adjacent, so the
        // dedup silently did nothing in exactly the case it was written for. The
        // sort is what makes (P,A,B) reach `exists` before (P,B,A); the scan is
        // what refuses the second. One mechanism, and it works for any number of
        // corps.
        std::sort(candidates.begin(), candidates.end(), candidate_less);

        for (const candidate& c : candidates)
        {
            // Already fighting? A corp pair fights at most one battle per
            // province at a time.
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
            nb.attacker_units = by_corp[c.attacker][c.province];
            nb.defender_units = by_corp[c.defender][c.province];
            if (nb.attacker_units.empty() || nb.defender_units.empty())
                continue;

            const terrain_pick g = defender_ground(w, nb.defender_units);

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

            // Supply is already per-unit and already folded into the stack by
            // unit_to_stack_entry; the side-level figure the resolver takes is
            // the mean, so an army with one starving column is measurably
            // weaker without a second supply model.
            auto mean_supply = [&](const std::vector<entity_id>& us) {
                long long sum = 0; int n = 0;
                for (const entity_id uid : us)
                {
                    const auto it = w.units.find(uid);
                    if (it == w.units.end()) continue;
                    sum += it->second.supply_factor_permille;
                    ++n;
                }
                return n ? static_cast<int>(sum / n) : 1000;
            };

            const std::vector<army_stack_entry> atk_stack = build_side(w, nb.attacker_units);
            const std::vector<army_stack_entry> def_stack = build_side(w, nb.defender_units);
            if (!stack_can_fight(atk_stack) || !stack_can_fight(def_stack))
                continue; // see stack_can_fight: a 0-vs-0 resolve is a false victory

            nb.state = begin_campaign_battle(
                id,
                // Doctrine is an ALL-ZERO STUB on both sides, and deliberately
                // named as one: no per-corp doctrine field exists anywhere on
                // corporation_component, so resolve_battle's doctrine machinery
                // contributes nothing to any production battle. Giving it real
                // rows is its own item, not a line to invent here.
                atk_stack, doctrine_row{},
                def_stack, doctrine_row{},
                g.sub, g.cov, g.density, g.lf,
                // Summer is FORCED, not chosen: the world carries no season, so
                // the resolver's winter penalty is dead in production until one
                // exists. Named here so it reads as a gap rather than a default.
                season::summer,
                mean_supply(nb.attacker_units), mean_supply(nb.defender_units));

            w.battles.push_back(std::move(nb));
            ++out.opened;
        }
        std::sort(w.battles.begin(), w.battles.end(), battle_order_less);
    }

    if (w.battles.empty())
        return out;

    // -----------------------------------------------------------------------
    // 2. STEP — several rounds per tick (ruling 3)
    // -----------------------------------------------------------------------
    // `w.battles` is kept sorted, so this walk is order-independent even though
    // discovery joined against unordered containers.
    const campaign_battle_params params{};
    for (active_battle& b : w.battles)
    {
        if (b.state.end != campaign_battle_end::in_progress)
            continue;

        // STRENGTH IS CAPTURED BEFORE THE WITHDRAWAL, NOT AFTER — and the order
        // is the whole point rather than a detail. Captured after, the parting
        // cost was invisible: withdraw_campaign_battle reduces strength_permille,
        // so a `before` read afterwards saw the already-reduced value, `drop`
        // came out zero for the withdrawing side, and disengaging killed nobody
        // while the record that held the cost was erased at the end of the tick.
        // Withdrawal was free in men and expensive only in a number nothing
        // outlived. It bites now.
        const int before_a = b.state.attacker_strength_permille;
        const int before_d = b.state.defender_strength_permille;

        // WITHDRAWAL IS HONOURED AT THE TICK BOUNDARY, before the next round
        // batch — that is what makes the window a real window. The price is the
        // resolver's own three-term cost; this pass adds no second rule.
        if (b.withdraw_requested != withdrawing_side::none)
        {
            withdraw_campaign_battle(b.state, b.withdraw_requested, params);
            b.withdraw_requested = withdrawing_side::none;
            ++out.withdrew;
        }

        int fought = 0;
        const int budget = std::max(1, params.rounds_per_tick);
        while (fought < budget && b.state.end == campaign_battle_end::in_progress)
        {
            if (!step_campaign_battle(b.state, params))
                break;
            ++fought;
        }
        if (fought > 0)
        {
            ++out.stepped;
            out.rounds += fought;
        }

        // -------------------------------------------------------------------
        // 3. PER-UNIT HITS
        // -------------------------------------------------------------------
        // The strength LOST this tick, as a share of what marched in, applied to
        // the men actually standing. Losses land per round batch, so a battle
        // weakens units while it runs rather than only at the end — which is
        // what makes a mid-fight withdrawal a real decision.
        auto apply_side = [&](const std::vector<entity_id>& units, int before, int after) {
            const int drop = before - after;
            if (drop <= 0) return;
            const int standing = side_count(w, units);
            if (standing <= 0) return;
            const int want = static_cast<int>(
                (static_cast<long long>(standing) * drop) / std::max(1, before));
            out.losses += distribute_losses(w, units, want);
        };
        apply_side(b.attacker_units, before_a, b.state.attacker_strength_permille);
        apply_side(b.defender_units, before_d, b.state.defender_strength_permille);

        if (b.state.end != campaign_battle_end::in_progress)
            ++out.concluded;
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

    return out;
}
