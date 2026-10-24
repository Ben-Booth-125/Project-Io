#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_set>
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
    //
    // THE SEAT IS FOLDED HERE TOO (BL-1082, ruled by Ben 2026-09-24): each
    // corp's `is_player` inside the sorted walk, and `player_entity` once after
    // it. The seat is the one folded field a tick never moves — the generator's
    // draw or `corp_verb::take_seat` writes it, pre-play — and it is folded
    // because a pick IS a different campaign: without it two different picks on
    // one world hashed alike, and `seat_pick_check` had to read the player id
    // beside the hash to tell them apart (found by BL-1076). Both halves are
    // folded, not one: `player_entity` names the seat, the flags say exactly one
    // corp carries it, and a world where the two disagree is a broken invariant
    // the hash should see. Integer-only and cheap; every world carries a seat,
    // so the fold moves every hash once, structurally (new state, not new
    // behaviour).
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
            fnv1a_i32(h, cc.is_player ? 1 : 0);
            // BL-1206: a refund credited at the seat and not yet booked —
            // sparse, so a corporation with none hashes as before.
            if (cc.refund_unbooked != 0.0f)
            {
                fnv1a_u32(h, id);
                fnv1a_f32(h, cc.refund_unbooked);
                fnv1a_f32(h, cc.refund_opening);
            }
            // BL-1266: the trade reserve and the points made — sparse, so a
            // corporation that never traded hashes as before.
            if (cc.trade_reserve != 0.0f || cc.trade_points != 0.0f)
            {
                fnv1a_u32(h, 0x7AADEu);
                fnv1a_f32(h, cc.trade_reserve);
                fnv1a_f32(h, cc.trade_points);
            }
        }
        fnv1a_u32(h, player_entity);
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
            //     to the `corp_market_pools` walk below.
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
            // BL-1206: what the site has been charged — the seat's refund. Folded
            // sparsely on the supply factor's argument: a building never built
            // under construction holds 0 and contributes nothing.
            if (b.construction_paid != 0.0f)
            {
                fnv1a_u32(h, id);
                fnv1a_f32(h, b.construction_paid);
            }
            // BL-1296: a state building the treasury could not pay for —
            // sparse, so a world with no idled state building hashes as before.
            if (b.state_idle)
            {
                fnv1a_u32(h, 0x57A7E1u);
                fnv1a_u32(h, id);
            }
        }
    }

    // BL-1296: state industry ownership — which nation owns which buildings
    // outright, in stored order (the order the funding gate spends in). Folded
    // only for a nation that holds any, ascending nation id, so a world with no
    // state industry hashes exactly as it did before the field existed.
    {
        std::vector<entity_id> ids;
        for (const auto& [nid, nc] : nations)
            if (!nc.state_assets.empty())
                ids.push_back(nid);
        std::sort(ids.begin(), ids.end());
        for (const entity_id nid : ids)
        {
            const std::vector<entity_id>& sa = nations.at(nid).state_assets;
            fnv1a_u32(h, 0x57A7E2u);
            fnv1a_u32(h, nid);
            fnv1a_u32(h, static_cast<uint32_t>(sa.size()));
            for (const entity_id bid : sa)
                fnv1a_u32(h, bid);
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
            // BL-1196 household channel: the growth gate branches on fill / bid
            // (population_met_ratio) before the next clear rewrites them, and
            // both are saved, so a divergence in either is a real divergence.
            for (const float b : m.household_bid) fnv1a_f32(h, b);
            for (const float f : m.household_fill) fnv1a_f32(h, f);
            // BL-1203: dispatch branches on both (the hauler's room), and both
            // are saved.
            for (const float x : m.household_weight) fnv1a_f32(h, x);
            for (const float x : m.hauler_want) fnv1a_f32(h, x);
            // BL-1227: the unposted bid the scorer's veto reads, and its tick.
            for (const float x : m.unposted_bid) fnv1a_f32(h, x);
            for (const int32_t t : m.unposted_bid_tick) fnv1a_u32(h, static_cast<uint32_t>(t));
            // BL-1217 G1b R2: the background's draw at the last clear — the
            // supply clause branches on it (input_reach.cpp) and it is saved.
            // Folded SPARSELY (good index, value; non-zero slots only), behind
            // a tag so it cannot alias a neighbouring run.
            for (std::size_t r = 0; r < m.background_fill.size(); ++r)
                if (m.background_fill[r] != 0.0f)
                {
                    fnv1a_u32(h, 0xB6F11u);
                    fnv1a_u32(h, static_cast<uint32_t>(r));
                    fnv1a_f32(h, m.background_fill[r]);
                }
        }
    }

    // BL-1125: the fold map routes every tick (market_for_tile) — std::map, ascending.
    for (const auto& [fid, fm] : folded_markets)
    {
        fnv1a_u32(h, fid);
        fnv1a_u32(h, fm.centre_tile);
        fnv1a_u32(h, fm.into);
    }

    // BL-1255: the rail track field — std::map, ascending tile — and the stations
    // a convoy rides. Folded only when the world carries track, so a world with
    // no rail hashes exactly as it did before the field existed.
    if (!rail_track.empty())
    {
        fnv1a_u32(h, 0x7A11u);
        for (const auto& [tile, owner] : rail_track)
        {
            fnv1a_u32(h, tile);
            fnv1a_u32(h, owner);
        }
        for (const convoy_component& cv : convoys)
            if (cv.station_a != null_entity || cv.station_b != null_entity)
            {
                fnv1a_u32(h, cv.id);
                fnv1a_u32(h, cv.station_a);
                fnv1a_u32(h, cv.station_b);
            }
        for (const entity_id o : rail_idle)
            fnv1a_u32(h, o);
    }

    // BL-1309: the Logistic Point share rules a nation has SET — sparse and
    // sorted, so a world where every nation shares mechanically hashes as before.
    {
        std::vector<entity_id> ruled;
        for (const auto& [nid, nc] : nations)
            if (nc.lp_shares.set)
                ruled.push_back(nid);
        std::sort(ruled.begin(), ruled.end());
        for (const entity_id nid : ruled)
        {
            fnv1a_u32(h, 0x1F09u);
            fnv1a_u32(h, nid);
            for (const float f : nations.at(nid).lp_shares.share)
                fnv1a_f32(h, f);
        }
    }

    // BL-1335: taxes and Discontent — sparse and sorted, only the nations whose
    // (tier, Discontent) differs from the opening default, so a world where no
    // tier has moved and no Discontent has built hashes exactly as before.
    {
        std::vector<entity_id> moved;
        for (const auto& [nid, nc] : nations)
            if (nc.tax_tier != k_default_tax_tier || nc.discontent != 0.0f)
                moved.push_back(nid);
        std::sort(moved.begin(), moved.end());
        for (const entity_id nid : moved)
        {
            fnv1a_u32(h, 0x7A8Eu);
            fnv1a_u32(h, nid);
            fnv1a_u32(h, static_cast<uint32_t>(nations.at(nid).tax_tier));
            fnv1a_f32(h, nations.at(nid).discontent);
        }
    }

    // BL-1404: the majors, a saved fact — sparse and sorted, folded only when
    // any nation is one, so a world with no major hashes exactly as before.
    {
        std::vector<entity_id> majors;
        for (const auto& [nid, nc] : nations)
            if (nc.is_major)
                majors.push_back(nid);
        std::sort(majors.begin(), majors.end());
        if (!majors.empty())
        {
            fnv1a_u32(h, 0x3A70u);
            fnv1a_u32(h, static_cast<uint32_t>(majors.size()));
            for (const entity_id nid : majors)
                fnv1a_u32(h, nid);
        }
    }

    // BL-1265: corporations hold no pools — every good is on a market's shelf,
    // folded with the markets above.

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

    // The manual trades (BL-1266). Hashed in STORED order, not sorted: an
    // owner's reserve is spent in placement order, so the sequence is part of
    // the state. `next_trade_id` is folded too, so a world that has set and
    // cleared a trade does not hash equal to one that never set one.
    fnv1a_u32(h, next_trade_id);
    fnv1a_u32(h, static_cast<uint32_t>(trades.size()));
    for (const standing_trade& t : trades)
    {
        fnv1a_u32(h, t.id);
        fnv1a_u32(h, t.owner);
        fnv1a_u32(h, static_cast<uint32_t>(t.resource));
        fnv1a_u32(h, t.from_market);
        fnv1a_u32(h, t.to_market);
        fnv1a_f32(h, t.points);
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
        // BL-1273: each nation's tech state — held set, pool (the head's
        // progress) and queue. Live state the quarterly research pass writes.
        // Folded only when the map is non-empty, on the budgets' precedent. The
        // map IS empty where no nation has anything to hold: the research pass
        // creates an entry only for a nation with RP, a queue or a held node,
        // and the settle's handover clears it — so such a world hashes as it
        // did before this state existed. The pool folds as its int64. A std::map:
        // ascending with no sort; the held set folds as 32-bit words.
        if (!nation_tech.empty())
        {
            fnv1a_u32(h, 0x7EC4u); // tag: cannot alias the budget run above
            fnv1a_u32(h, static_cast<uint32_t>(nation_tech.size()));
            for (const auto& [nid, st] : nation_tech)
            {
                fnv1a_u32(h, nid);
                fnv1a_u32(h, static_cast<uint32_t>(static_cast<uint64_t>(st.pool_milli) & 0xFFFFFFFFull));
                fnv1a_u32(h, static_cast<uint32_t>(static_cast<uint64_t>(st.pool_milli) >> 32));
                for (int i = 0; i < io::dg::node_count; i += 32)
                {
                    uint32_t word = 0;
                    for (int b = 0; b < 32 && i + b < io::dg::node_count; ++b)
                        if (st.held.test(static_cast<std::size_t>(i + b)))
                            word |= (1u << b);
                    fnv1a_u32(h, word);
                }
                fnv1a_u32(h, static_cast<uint32_t>(st.queue.size()));
                for (const uint16_t n : st.queue)
                    fnv1a_u32(h, n);
            }
        }
        // BL-1327: the space assets, on the nation_tech precedent — folded only
        // when the store is non-empty, so a world that never launched hashes as
        // it did before. A std::map: ascending id, no sort.
        if (!space_assets.empty())
        {
            fnv1a_u32(h, 0x5ACEu); // tag: cannot alias the tech run above
            fnv1a_u32(h, static_cast<uint32_t>(space_assets.size()));
            for (const auto& [aid, a] : space_assets)
            {
                fnv1a_u32(h, aid);
                fnv1a_u32(h, a.owner);
                fnv1a_u32(h, static_cast<uint32_t>(a.cls));
                fnv1a_u32(h, a.dest.body);
                fnv1a_u32(h, static_cast<uint32_t>(a.dest.slot));
                fnv1a_u32(h, static_cast<uint32_t>(a.state));
                fnv1a_i32(h, a.launch_tick);
                fnv1a_i32(h, a.arrival_tick);
                fnv1a_f32(h, a.upkeep);
                fnv1a_i32(h, a.unpaid_quarters);
                fnv1a_f32(h, a.launch_cost);
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
    if (corp != null_entity)
    {
        const auto it = w.corporations.find(corp);
        return it != w.corporations.end() && it->second.is_player;
    }
    // BL-1402 (Ben, 2026-10-23): the player IS its nation, so a building its
    // nation owns is the player's.
    const entity_id pn = player_nation(w);
    return pn != null_entity && owner_nation_of(w, building) == pn;
}

building_owner building_owner_of(const world& w, entity_id building)
{
    building_owner out;
    if (building == null_entity)
        return out;
    if (const entity_id corp = owner_corp_of(w, building); corp != null_entity)
    {
        out.kind = owner_kind::corporation;
        out.id   = corp;
        return out;
    }
    if (const entity_id nation = owner_nation_of(w, building); nation != null_entity)
    {
        out.kind = owner_kind::nation;
        out.id   = nation;
    }
    return out;
}

entity_id owner_nation_of(const world& w, entity_id building)
{
    if (building == null_entity)
        return null_entity;
    for (const auto& [nid, nc] : w.nations)
        for (const entity_id a : nc.state_assets)
            if (a == building)
                return nid;
    return null_entity;
}

const std::vector<entity_id>* assets_of(const world& w, entity_id owner)
{
    if (const auto cit = w.corporations.find(owner); cit != w.corporations.end())
        return &cit->second.assets;
    if (const auto nit = w.nations.find(owner); nit != w.nations.end())
        return &nit->second.state_assets;
    return nullptr;
}

std::vector<entity_id>* assets_of(world& w, entity_id owner)
{
    if (const auto cit = w.corporations.find(owner); cit != w.corporations.end())
        return &cit->second.assets;
    if (const auto nit = w.nations.find(owner); nit != w.nations.end())
        return &nit->second.state_assets;
    return nullptr;
}

std::vector<entity_id> economic_owner_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.corporations.size());
    for (const auto& kv : w.corporations)
        ids.push_back(kv.first);
    for (const auto& [nid, nc] : w.nations)
        if (!nc.state_assets.empty())
            ids.push_back(nid);
    std::sort(ids.begin(), ids.end());
    return ids;
}

entity_id owner_home_nation(const world& w, entity_id owner)
{
    if (const auto cit = w.corporations.find(owner); cit != w.corporations.end())
        return cit->second.home_nation;
    if (w.nations.find(owner) != w.nations.end())
        return owner;
    return null_entity;
}

float* purse_of(world& w, entity_id owner)
{
    if (const auto cit = w.corporations.find(owner); cit != w.corporations.end())
        return &cit->second.balance;
    if (const auto nit = w.nations.find(owner); nit != w.nations.end())
        return &nit->second.treasury;
    return nullptr;
}

const float* purse_of(const world& w, entity_id owner)
{
    if (const auto cit = w.corporations.find(owner); cit != w.corporations.end())
        return &cit->second.balance;
    if (const auto nit = w.nations.find(owner); nit != w.nations.end())
        return &nit->second.treasury;
    return nullptr;
}

float purse_room_of(const world& w, entity_id owner)
{
    if (const auto cit = w.corporations.find(owner); cit != w.corporations.end())
        return cit->second.balance;
    // A nation's uncommitted treasury - state_uncommitted_treasury's rule
    // (state_industry.cpp), restated so this file needs no economy header.
    if (const auto nit = w.nations.find(owner); nit != w.nations.end())
    {
        float committed = 0.0f;
        if (const auto lit = w.state_ledger.find(owner); lit != w.state_ledger.end())
            committed = lit->second.committed;
        return std::max(0.0f, nit->second.treasury - committed);
    }
    return 0.0f;
}

bool owner_holds(const world& w, entity_id owner, entity_id building)
{
    const std::vector<entity_id>* a = assets_of(w, owner);
    return a != nullptr && std::find(a->begin(), a->end(), building) != a->end();
}

bool world_is_folded(const world& w)
{
    for (const auto& [cid, cc] : w.corporations)
        if (!cc.assets.empty())
            return false;
    for (const auto& [nid, nc] : w.nations)
        if (!nc.state_assets.empty())
            return true;
    return false;
}

entity_id player_actor(const world& w)
{
    const entity_id pn = player_nation(w);
    return (pn != null_entity && world_is_folded(w)) ? pn : w.player_entity;
}

entity_id champion_of(const world& w, entity_id nation)
{
    if (nation == null_entity)
        return null_entity;
    // The seat IS its nation's champion (the player plays a major and operates
    // its champion — CONCEPT.md § Player identity), so a seated specialist is
    // named first; otherwise the lowest-id specialist at home.
    if (const auto pit = w.corporations.find(w.player_entity);
        pit != w.corporations.end() && !pit->second.is_background
        && pit->second.home_nation == nation)
        return w.player_entity;
    entity_id best = null_entity;
    for (const auto& [cid, cc] : w.corporations)
        if (!cc.is_background && cc.home_nation == nation && (best == null_entity || cid < best))
            best = cid;
    return best;
}

bool is_major_nation(const world& w, entity_id nation)
{
    // BL-1404: majors are a saved fact.
    const auto it = w.nations.find(nation);
    return it != w.nations.end() && it->second.is_major;
}

void mark_majors_from_champions(world& w)
{
    // BL-1404: a pure function of the roster; the per-nation write is keyed, so
    // the unordered walks cannot reach the result.
    std::unordered_set<entity_id> homes;
    for (const auto& [cid, cc] : w.corporations)
        if (!cc.is_background && cc.home_nation != null_entity)
            homes.insert(cc.home_nation);
    for (auto& [nid, nc] : w.nations)
        nc.is_major = homes.count(nid) != 0;
}

entity_id player_nation(const world& w)
{
    const auto cit = w.corporations.find(w.player_entity);
    return cit != w.corporations.end() ? cit->second.home_nation : null_entity;
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
    // BL-1402: the seat's own holdings and its nation's (the player IS its nation).
    const auto on_body = [&](const std::vector<entity_id>& assets) {
        for (const entity_id bld : assets)
        {
            const auto bit = w.buildings.find(bld);
            if (bit == w.buildings.end())
                continue;
            const auto tit = w.tiles.find(bit->second.tile);
            if (tit != w.tiles.end() && tit->second.body == body)
                return true;
        }
        return false;
    };
    if (const auto cit = w.corporations.find(w.player_entity);
        cit != w.corporations.end() && on_body(cit->second.assets))
        return true;
    if (const auto nit = w.nations.find(player_nation(w)); nit != w.nations.end())
        return on_body(nit->second.state_assets);
    return false;
}

/// True if a live player convoy is in transit on a lane touching `body`.
bool player_lane_active_on(const world& w, entity_id body)
{
    for (const auto& cv : w.convoys)
    {
        if (cv.corp != w.player_entity && cv.corp != player_nation(w)) // BL-1402: or its nation's
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
