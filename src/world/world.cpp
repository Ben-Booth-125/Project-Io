#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

entity_id world::create_entity()
{
    return m_next_id++;
}

// ---------------------------------------------------------------------------
// Tick-boundary state hash (BL-204) — FNV-1a over a canonicalised tick snapshot
// ---------------------------------------------------------------------------
namespace {

constexpr uint64_t fnv_offset_basis = 14695981039346656037ull;
constexpr uint64_t fnv_prime        = 1099511628211ull;

void fnv1a_bytes(uint64_t& h, const void* data, std::size_t n)
{
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (std::size_t i = 0; i < n; ++i)
    {
        h ^= p[i];
        h *= fnv_prime;
    }
}

void fnv1a_i32(uint64_t& h, int32_t v) { fnv1a_bytes(h, &v, sizeof v); }
void fnv1a_u32(uint64_t& h, uint32_t v) { fnv1a_bytes(h, &v, sizeof v); }
void fnv1a_f32(uint64_t& h, float v) { fnv1a_bytes(h, &v, sizeof v); }

} // namespace

uint64_t world::state_hash(int tick) const
{
    uint64_t h = fnv_offset_basis;
    fnv1a_i32(h, tick);

    // Corporations: balance is the tick-mutating field the money loop drives.
    {
        std::vector<entity_id> ids;
        ids.reserve(corporations.size());
        for (const auto& kv : corporations) ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end());
        for (const entity_id id : ids)
        {
            const corporation_component& cc = corporations.at(id);
            fnv1a_u32(h, id);
            fnv1a_f32(h, cc.balance);
        }
    }

    // Buildings: every field the AI / economy tick may mutate.
    {
        std::vector<entity_id> ids;
        ids.reserve(buildings.size());
        for (const auto& kv : buildings) ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end());
        for (const entity_id id : ids)
        {
            const building_component& b = buildings.at(id);
            fnv1a_u32(h, id);
            fnv1a_f32(h, b.workforce_assigned);
            fnv1a_i32(h, b.workforce_target);
            // workforce_auto is a dial the tick reads and the seam writes
            // (set_workforce clears it, set_workforce_auto sets it), so a
            // divergence in it is a real divergence — it was missing here
            // until BL-293 gave the flag a command verb.
            fnv1a_i32(h, b.workforce_auto ? 1 : 0);
            fnv1a_u32(h, b.recipe);
            fnv1a_i32(h, b.decommissioned ? 1 : 0);
            fnv1a_i32(h, b.ticks_remaining);
            fnv1a_f32(h, b.construction_progress);
            // BL-641's supply factor is real state the building-upkeep pass
            // writes, so a divergence in it is a real divergence and it has to
            // be folded. It is folded SPARSELY — only when the building is not
            // fully supplied — and that is a deliberate, complete choice rather
            // than a partial one:
            //
            //   * COMPLETE as a detector. Two worlds that differ in any
            //     building's factor differ here: at least one side is
            //     non-neutral, and the non-neutral side folds (id, value) while
            //     the other folds nothing. Neutral state contributes nothing,
            //     exactly as a pool that was never created contributes nothing
            //     to the `corp_body_pools` walk below.
            //   * WHY IT MATTERS. At the shipped rates every building is
            //     neutral, so a world in which this mechanism has never fired
            //     hashes exactly as it did before the field existed. That is
            //     BL-641's R4 — the shape lands inert and turning the sink on is
            //     a data change — and it is what lets the pinned world-hash
            //     goldens (spectator_determinism) stand unmoved.
            if (b.supply_factor_permille != 1000)
            {
                fnv1a_u32(h, id);
                fnv1a_i32(h, b.supply_factor_permille);
            }
        }
    }

    // Markets: the resolved price array is the clearing tick's output.
    {
        std::vector<entity_id> ids;
        ids.reserve(markets.size());
        for (const auto& kv : markets) ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end());
        for (const entity_id id : ids)
        {
            const market_component& m = markets.at(id);
            fnv1a_u32(h, id);
            for (const float p : m.price) fnv1a_f32(h, p);
        }
    }

    // Corp/body stockpile pools — std::map, already sorted by (corp,body).
    for (const auto& [key, sc] : corp_body_pools)
    {
        fnv1a_u32(h, key.first);
        fnv1a_u32(h, key.second);
        for (const float q : sc.quantities) fnv1a_f32(h, q);
    }

    // Tiles: resource_remaining is drawn down by extraction every tick.
    {
        std::vector<entity_id> ids;
        ids.reserve(tiles.size());
        for (const auto& kv : tiles) ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end());
        for (const entity_id id : ids)
        {
            const tile_component& t = tiles.at(id);
            fnv1a_u32(h, id);
            for (const float q : t.resource_remaining) fnv1a_f32(h, q);
        }
    }

    // Units: hiring and combat move every field here.
    {
        std::vector<entity_id> ids;
        ids.reserve(units.size());
        for (const auto& kv : units) ids.push_back(kv.first);
        std::sort(ids.begin(), ids.end());
        for (const entity_id id : ids)
        {
            const unit_component& u = units.at(id);
            fnv1a_u32(h, id);
            fnv1a_u32(h, u.position);
            fnv1a_u32(h, u.owner);
            fnv1a_i32(h, u.count);
            fnv1a_u32(h, u.type);
            // BL-459 dropped the stored `strength` (a duplicate of `count`, so
            // it fed this hash the same bytes twice). BL-454's supply factor and
            // muster base replace it — both are real state the upkeep pass
            // writes, so BOTH must be hashed. THIS MOVES EVERY WORLD-HASH
            // GOLDEN in a world that holds any unit, and it moves them
            // structurally (new state), not behaviourally.
            fnv1a_i32(h, u.supply_factor_permille);
            fnv1a_u32(h, u.muster_base);
            // BL-470: the march order. `position` above already moves every
            // tick a marching unit steps, but the ORDER ITSELF (dest, the
            // remaining path, how far along it, the banked fractional
            // progress) is real state a replay must reproduce exactly — the
            // whole point of the determinism guard below is to prove two
            // independent runs agree on this, not just on where the unit
            // ends up.
            fnv1a_u32(h, u.order.dest);
            fnv1a_u32(h, static_cast<uint32_t>(u.order.path.size()));
            for (const entity_id t : u.order.path) fnv1a_u32(h, t);
            fnv1a_u32(h, static_cast<uint32_t>(u.order.next_index));
            fnv1a_f32(h, u.order.progress);
        }
    }

    // The order book (BL-293). Hashed in STORED order, not sorted: the book is a
    // price-TIME priority queue, so its sequence is part of the state, and two
    // books holding the same orders in a different order really are different
    // worlds. That makes this the one section here whose determinism rests on the
    // container rather than on a re-sort — which is exactly why it is hashed:
    // insertion order is only stable because every write goes through the command
    // seam, and this is the assertion that catches it if that ever stops being
    // true. `next_order_id` is folded too, so a world that has issued and erased
    // an order does not hash equal to one that never issued it.
    fnv1a_u32(h, next_order_id);
    fnv1a_u32(h, static_cast<uint32_t>(sell_orders.size()));
    for (const sell_order& o : sell_orders)
    {
        fnv1a_u32(h, o.id);
        fnv1a_u32(h, o.corp);
        fnv1a_u32(h, o.body);
        fnv1a_u32(h, static_cast<uint32_t>(o.resource));
        fnv1a_f32(h, o.quantity);
        fnv1a_f32(h, o.floor_price);
    }
    fnv1a_u32(h, static_cast<uint32_t>(buy_orders.size()));
    for (const buy_order& o : buy_orders)
    {
        fnv1a_u32(h, o.id);
        fnv1a_u32(h, o.corp);
        fnv1a_u32(h, o.body);
        fnv1a_u32(h, static_cast<uint32_t>(o.resource));
        fnv1a_f32(h, o.quantity);
        fnv1a_f32(h, o.max_price);
        fnv1a_u32(h, o.preferred_seller);
    }

    // Battles in progress (BL-467). A battle is created, stepped and ended BY a
    // tick, so it is exactly the kind of state this hash exists to catch a
    // divergence in — unlike `provinces`, which is generation output and is
    // deliberately excluded for the mirror-image reason.
    //
    // NOTHING IS FOLDED WHEN THE LIST IS EMPTY, and that is on purpose rather
    // than a happy accident of the loop: a world that never fights must hash
    // exactly as it did before this item, so BL-467 does not silently move every
    // pinned hash in the project on worlds where it does nothing. The size is
    // folded first so an empty list and a list of one zero-valued battle cannot
    // collide.
    //
    // The per-round TRACE is deliberately not folded. It is retained only for the
    // span of the fight (the card and the dispatches read it) and discarded at
    // battle end, so folding it would make the hash depend on a display buffer.
    // What is folded is what a divergence would actually be: who is fighting
    // whom, where, how far in, and how much of each side is left.
    if (!battles.empty())
    {
        fnv1a_u32(h, static_cast<uint32_t>(battles.size()));
        for (const active_battle& b : battles)
        {
            fnv1a_u32(h, b.province);
            fnv1a_u32(h, b.attacker);
            fnv1a_u32(h, b.defender);
            fnv1a_i32(h, b.state.rounds_fought);
            fnv1a_i32(h, b.state.attacker_strength_permille);
            fnv1a_i32(h, b.state.defender_strength_permille);
            fnv1a_i32(h, static_cast<int>(b.state.end));
            // The stream position, not the stream's history: two battles that
            // have drawn a different number of times must not hash equal.
            fnv1a_u32(h, static_cast<uint32_t>(b.state.rng_state & 0xFFFFFFFFull));
            fnv1a_u32(h, static_cast<uint32_t>(b.state.rng_state >> 32));
            for (const entity_id u : b.attacker_units) fnv1a_u32(h, u);
            for (const entity_id u : b.defender_units) fnv1a_u32(h, u);
        }
    }

    // NATIONS AS ACTORS (Sprint N3, 2026-08-23). The treasury and the authored
    // weight map are folded ONLY when non-trivial, on the battles precedent
    // above: a world where no nation has ever been credited or scored hashes
    // exactly as it did before nations acted, so every empty-nation fixture's
    // pinned value survives. Sorted walks: `nations` is unordered, so its ids
    // are collected and sorted first; `nation_budgets` is a std::map.
    {
        bool any_treasury = false;
        for (const auto& [nid, nc] : nations)
            if (nc.treasury != 0.0f) { any_treasury = true; break; }
        if (any_treasury)
        {
            std::vector<entity_id> ids;
            ids.reserve(nations.size());
            for (const auto& [nid, nc] : nations) ids.push_back(nid);
            std::sort(ids.begin(), ids.end());
            fnv1a_u32(h, static_cast<uint32_t>(ids.size()));
            for (const entity_id nid : ids)
            {
                fnv1a_u32(h, nid);
                fnv1a_f32(h, nations.at(nid).treasury);
            }
        }
        if (!nation_budgets.empty())
        {
            fnv1a_u32(h, static_cast<uint32_t>(nation_budgets.size()));
            for (const auto& [nid, nb] : nation_budgets)
            {
                fnv1a_u32(h, nid);
                for (const float wgt : nb.weights) fnv1a_f32(h, wgt);
                fnv1a_f32(h, nb.reserve_fraction);
            }
        }
    }

    // Province holder (BL-569). UNLIKE `provinces` itself (generation output,
    // never folded), this is live state a decisive battle mutates, so it is
    // folded exactly like tiles/buildings/units above — unconditionally, in
    // its own stored order, which IS ascending province::id (the positional
    // alignment with `provinces.provinces` that `seed_province_holders` and
    // `run_battles` both keep). No sort needed, and no "skip when empty"
    // guard: unlike battles/nation_budgets there is no prior golden to
    // protect, since a generated world always seeds this vector.
    fnv1a_u32(h, static_cast<uint32_t>(province_holder.size()));
    for (const entity_id holder : province_holder)
        fnv1a_u32(h, holder);

    return h;
}

entity_id owner_corp_of(const world& w, entity_id building)
{
    if (building == null_entity)
        return null_entity;
    for (const auto& [corp_id, cc] : w.corporations)
        for (const entity_id a : cc.assets)
            if (a == building)
                return corp_id;
    return null_entity;
}

bool is_player_owned(const world& w, entity_id building)
{
    const entity_id corp = owner_corp_of(w, building);
    if (corp == null_entity)
        return false;
    const auto it = w.corporations.find(corp);
    return it != w.corporations.end() && it->second.is_player;
}

entity_id body_of_market(const world& w, entity_id market)
{
    if (market == null_entity)
        return null_entity;
    const auto it = w.markets.find(market);
    return it != w.markets.end() ? it->second.body : null_entity;
}

namespace {

/// True if the player corporation owns a building on `body`.
bool player_present_on(const world& w, entity_id body)
{
    const auto cit = w.corporations.find(w.player_entity);
    if (cit == w.corporations.end())
        return false;
    for (const entity_id bld : cit->second.assets)
    {
        const auto bit = w.buildings.find(bld);
        if (bit == w.buildings.end())
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit != w.tiles.end() && tit->second.body == body)
            return true;
    }
    return false;
}

/// True if a live player convoy is in transit on a lane touching `body`.
bool player_lane_active_on(const world& w, entity_id body)
{
    for (const auto& cv : w.convoys)
    {
        if (cv.corp != w.player_entity)
            continue;
        if (body_of_market(w, cv.source_market) == body ||
            body_of_market(w, cv.dest_market) == body)
            return true;
    }
    return false;
}

/// Flat orbital-plane position (AU) of a body — the same projection the supply system
/// uses for lane distance (r*cos(theta), r*sin(theta)); not recursed through parents.
std::pair<float, float> body_pos(const world& w, entity_id id)
{
    const auto it = w.bodies.find(id);
    if (it == w.bodies.end())
        return {0.0f, 0.0f};
    const body_component& bc = it->second;
    return { bc.orbital_radius_au * std::cos(bc.orbital_angle_rad),
             bc.orbital_radius_au * std::sin(bc.orbital_angle_rad) };
}

} // namespace

activity_vis body_activity_visibility(const world& w, entity_id body, int now_tick,
                                      int route_fresh_ticks, int glimpse_fresh_ticks)
{
    if (body == null_entity)
        return activity_vis::unknown;

    // Visible: the player's own presence or a live lane — the current, direct feed.
    if (body == w.home_body || player_present_on(w, body) || player_lane_active_on(w, body))
        return activity_vis::visible;

    // Otherwise the freshest player route touching this body decides known vs stale.
    bool  have_route = false;
    int   freshest   = 0;
    for (const auto& r : w.trade_routes)
    {
        if (r.corp != w.player_entity)
            continue;
        if (r.body_a != body && r.body_b != body)
            continue;
        if (!have_route || r.last_tick > freshest)
            freshest = r.last_tick;
        have_route = true;
    }

    if (!have_route)
    {
        // No route of its own — but a proximity glimpse (BL-099) may light it faintly. A
        // glimpse never rises above known_stale (a peek, not a data feed) and decays once
        // its freshness window passes.
        const auto git = w.body_last_glimpse_tick.find(body);
        if (git != w.body_last_glimpse_tick.end() && now_tick - git->second <= glimpse_fresh_ticks)
            return activity_vis::known_stale;
        return activity_vis::unknown;
    }

    return (now_tick - freshest <= route_fresh_ticks) ? activity_vis::known
                                                      : activity_vis::known_stale;
}

float body_closest_approach_au(const world& w, entity_id body, entity_id lane_a, entity_id lane_b)
{
    constexpr float far_away = std::numeric_limits<float>::max();
    if (body == null_entity || body == lane_a || body == lane_b)
        return far_away;
    if (w.bodies.find(body)   == w.bodies.end() ||
        w.bodies.find(lane_a) == w.bodies.end() ||
        w.bodies.find(lane_b) == w.bodies.end())
        return far_away;

    const auto [px, py] = body_pos(w, body);
    const auto [ax, ay] = body_pos(w, lane_a);
    const auto [bx, by] = body_pos(w, lane_b);

    const float abx  = bx - ax;
    const float aby  = by - ay;
    const float len2 = abx * abx + aby * aby;

    // Project the body onto the segment, clamped to its endpoints; a degenerate segment
    // (coincident endpoints) collapses to the point distance.
    float t = 0.0f;
    if (len2 > 0.0f)
    {
        t = ((px - ax) * abx + (py - ay) * aby) / len2;
        t = std::clamp(t, 0.0f, 1.0f);
    }
    const float cx = ax + t * abx;
    const float cy = ay + t * aby;
    const float dx = px - cx;
    const float dy = py - cy;
    return std::sqrt(dx * dx + dy * dy);
}

void record_proximity_glimpses(world& w, entity_id lane_a, entity_id lane_b, int tick, float radius_au)
{
    if (lane_a == null_entity || lane_b == null_entity || lane_a == lane_b)
        return;

    for (const auto& entry : w.bodies)
    {
        const entity_id id = entry.first;
        if (id == lane_a || id == lane_b || id == w.star_body)
            continue;
        if (body_closest_approach_au(w, id, lane_a, lane_b) <= radius_au)
            w.body_last_glimpse_tick[id] = tick; // latest glimpse wins; tick is monotonic
    }
}
