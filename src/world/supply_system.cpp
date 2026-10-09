#include "supply_system.hpp"

#include "logistics.hpp"
#include "market_clearing.hpp" // processor_reservation (BL-995)
#include "orbital_system.hpp"
#include "stance.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

void advance_convoys(world& w)
{
    for (auto& convoy : w.convoys)
    {
        // BL-452: a HELD convoy stops advancing and waits on its lane. It is
        // skipped rather than slowed — hold is a stop, not a throttle — and it
        // costs nothing further, since the haul was paid once at dispatch.
        if (convoy.arrived || convoy.held)
            continue;
        convoy.progress += convoy.speed;
        if (convoy.progress >= 1.0f)
        {
            convoy.progress = 1.0f;
            convoy.arrived  = true;
        }
    }
}

std::vector<interception_record> intercept_convoys(world& w, int tick)
{
    std::vector<interception_record> cuts;
    if (w.convoys.empty() || w.units.empty() || w.corp_hostile_pairs.empty())
        return cuts; // nothing declared, nothing standing, or nothing in flight

    // Tile -> units standing on it, built from a SORTED unit-id walk so the
    // per-tile candidate order is a property of the ids and never of the
    // unordered_map's layout. The lowest-id hostile unit on a contested tile is
    // therefore always the interceptor, on every replay of the same seed.
    std::vector<entity_id> unit_ids;
    unit_ids.reserve(w.units.size());
    for (const auto& [uid, uc] : w.units)
        unit_ids.push_back(uid);
    std::sort(unit_ids.begin(), unit_ids.end());

    std::unordered_map<entity_id, std::vector<entity_id>> units_on_tile;
    for (const entity_id uid : unit_ids)
    {
        const unit_component& uc = w.units.at(uid);
        if (uc.position == null_entity || uc.owner == null_entity || uc.count <= 0)
            continue;
        units_on_tile[uc.position].push_back(uid);
    }
    if (units_on_tile.empty())
        return cuts;

    // Walk convoys in w.convoys order. dispatch_convoys builds that order from
    // sorted corp and market id walks, and the player's verb appends, so it is
    // already replay-stable; nothing here re-sorts it and risks disagreeing with
    // credit_arrived_convoys about which convoy is which.
    std::vector<std::uint32_t> cut_ids;
    for (const convoy_component& cv : w.convoys)
    {
        const entity_id tile = convoy_tile_at(w, cv);
        if (tile == null_entity)
            continue; // inter-body leg in transit, or an unresolvable lane

        const auto occ = units_on_tile.find(tile);
        if (occ == units_on_tile.end())
            continue;

        entity_id interceptor_unit = null_entity;
        entity_id interceptor_corp = null_entity;
        for (const entity_id uid : occ->second)
        {
            const unit_component& uc = w.units.at(uid);
            if (uc.owner == cv.corp)
                continue; // your own escort is not your ambusher
            // DIRECTED, and only this direction: the tile's holder must have
            // DECLARED hostility toward the cargo's owner. A corp that has been
            // declared against but has not answered is a victim, not a raider.
            //
            // BL-1071 CALL: a market's own export (owner null_entity) walks this
            // same check, and no declaration can name it — hostility is declared
            // corporation to corporation — so today it is never cut. When a
            // declaration can name a market or its nation, it applies here with
            // no other change.
            if (!is_hostile(w, uc.owner, cv.corp))
                continue;
            interceptor_unit = uid;
            interceptor_corp = uc.owner;
            break; // sorted order: first hit is the lowest-id hostile unit
        }
        if (interceptor_unit == null_entity)
            continue;

        interception_record rec;
        rec.convoy_id        = cv.id;
        rec.victim_corp      = cv.corp;
        rec.interceptor_corp = interceptor_corp;
        rec.interceptor_unit = interceptor_unit;
        rec.tile             = tile;
        rec.cargo_resource   = cv.cargo_resource;
        rec.cargo_qty        = cv.cargo_qty;
        rec.tick             = tick;

        const auto tit = w.tiles.find(tile);
        rec.body = (tit != w.tiles.end()) ? tit->second.body : null_entity;

        // CAPTURE, with destruction as the fallback. The cargo is credited whole
        // to the interceptor's pool at the interception body — the same pool a
        // delivery would have credited, so quantity in == quantity credited by
        // construction. It is destroyed only when there is nowhere to put it: no
        // body under the tile, or an interceptor that is not a corporation and
        // therefore holds no pools. An interceptor sitting on goods it has no
        // market for is a legitimate outcome and is NOT special-cased away.
        const bool creditable = rec.body != null_entity &&
                                w.corporations.find(interceptor_corp) != w.corporations.end() &&
                                std::isfinite(cv.cargo_qty);
        if (creditable)
        {
            // BL-1003: the pool of the market whose catchment holds the
            // interception tile — where the captured cargo physically is.
            w.pool_at(interceptor_corp, pool_key_for_tile(w, tile)).quantities[
                static_cast<std::size_t>(cv.cargo_resource)] += cv.cargo_qty;
            rec.outcome = interception_outcome::captured;
        }
        else
        {
            rec.outcome = interception_outcome::destroyed;
        }

        cuts.push_back(rec);
        cut_ids.push_back(cv.id);
    }

    if (!cut_ids.empty())
    {
        // Erase the cut convoys. They never arrive, so credit_arrived_convoys
        // below never sees them and the destination pool is never credited —
        // which is the whole mechanic: the lane was cut.
        w.convoys.erase(
            std::remove_if(w.convoys.begin(), w.convoys.end(),
                           [&cut_ids](const convoy_component& c) {
                               return std::find(cut_ids.begin(), cut_ids.end(), c.id)
                                      != cut_ids.end();
                           }),
            w.convoys.end());
    }

    return cuts;
}

void credit_arrived_convoys(world& w, int tick, std::vector<interception_record>* out_cuts)
{
    // BL-458: interdiction runs FIRST, before anything is credited. This is the
    // ordering the item specifies — after advance_convoys, before crediting —
    // and it is placed here rather than at the four call sites because this is
    // the one seam app.cpp, main.cpp and every harness already share. A convoy
    // cut here never reaches the loop below, so it never credits its destination.
    // BL-995: this tick's deliveries start empty (world::arrived_this_tick —
    // transient, the dispatch pass later this tick reads it).
    w.arrived_this_tick.clear();

    std::vector<interception_record> cuts = intercept_convoys(w, tick);
    if (out_cuts != nullptr)
        out_cuts->insert(out_cuts->end(), cuts.begin(), cuts.end());

    // Credit in insertion order, then erase in one sweep.
    for (const auto& convoy : w.convoys)
    {
        if (!convoy.arrived)
            continue;

        // The destination market's body, for the trade-route record below.
        const auto mit = w.markets.find(convoy.dest_market);
        if (mit == w.markets.end())
            continue;
        const entity_id dest_body = mit->second.body;

        // BL-1071: a MARKET'S OWN EXPORT (owner sentinel null_entity) lands on
        // the destination market's SHELF — real inventory, where that market's
        // consumers draw it — not in any pool, since no corporation owns it.
        // Remembered for this tick's dispatch under (null, market): a shelf
        // delivery meets its destination's economy step before it may move again
        // (market_shelf_surplus). It runs no trade route: a market export is
        // intra-body by construction, and routes are a corporation's record.
        if (convoy.corp == null_entity)
        {
            const std::size_t r = static_cast<std::size_t>(convoy.cargo_resource);
            mit->second.inventory[r] += convoy.cargo_qty;
            w.arrived_this_tick[{null_entity, convoy.dest_market}].quantities[r] += convoy.cargo_qty;
            continue;
        }

        // Credit the DESTINATION MARKET's pool (BL-1003) — so a same-body haul
        // sells at the destination, not back at home. The credit is the whole delivery —
        // deliberately NO direct write into market supply here. This runs after
        // clear_markets in the tick, and clear_markets zeroes the supply/demand
        // arrays at its top, so a supply += here would be erased before pricing
        // ever read it — yet it WOULD be read pre-clearing by run_economy_step's
        // consumers (the AI scorer's glut forecast among them): a private signal
        // nothing priced ever agreed with. The cargo reaches pricing through the
        // ordinary auto-surplus path off this pool at the next clear (BL-382).
        w.pool_at(convoy.corp, convoy.dest_market).quantities[
            static_cast<std::size_t>(convoy.cargo_resource)] += convoy.cargo_qty;
        // BL-995: remembered for this tick's dispatch — a delivery sells at its
        // destination's clear before it may move again.
        w.arrived_this_tick[{convoy.corp, convoy.dest_market}].quantities[
            static_cast<std::size_t>(convoy.cargo_resource)] += convoy.cargo_qty;

        // Record the persistent trade route this completed lane ran (BL-088). The
        // route is body-level, so collapse both market endpoints to their bodies;
        // skip intra-body lanes (they light nothing) and lanes with an unresolved
        // source. The route is upserted (never duplicated) and never erased — its
        // last-traffic stamp lets the fog (BL-089) age it to 'stale' at read time.
        const entity_id src_body = body_of_market(w, convoy.source_market);
        if (src_body != null_entity && dest_body != null_entity && src_body != dest_body)
        {
            const auto same_lane = [&](const trade_route& r) {
                return r.corp == convoy.corp &&
                       ((r.body_a == src_body && r.body_b == dest_body) ||
                        (r.body_a == dest_body && r.body_b == src_body));
            };
            const auto rit = std::find_if(w.trade_routes.begin(), w.trade_routes.end(), same_lane);
            if (rit == w.trade_routes.end())
            {
                trade_route route;
                route.body_a       = src_body;
                route.body_b       = dest_body;
                route.corp         = convoy.corp;
                route.last_tick    = tick;
                route.convoy_count = 1;
                w.trade_routes.push_back(route);

                // World history log (BL-208): only on FIRST establishment of this
                // body-pair lane, never on a repeat completion (which only bumps
                // the trade_route above — untouched, byte-for-byte, by this log
                // entry). Self-contained — history_topic/world::history_log are
                // already visible via world.hpp (this file already includes it
                // via supply_system.hpp), so no new translation-unit dependency
                // for the existing tools/verify/README.md hand-written recipes
                // that link supply_system.cpp.
                //
                // BL-282 (Ben, NR-030, 2026-08-03): a new route is a TWO-body
                // event but world_history_entry carries one body tag, so tagging
                // only the destination made a body-scoped "what happened at X"
                // filter miss the route from its source side. Push TWO entries
                // instead — same narration, one tagged per endpoint. This keeps
                // the struct's one-body invariant and every existing reader
                // correct, where widening it with a `body_b` would change a
                // struct four other call sites depend on and leave every other
                // topic carrying a field it never sets.
                //
                // Order is fixed src-then-dest, not iteration-dependent, so the
                // log stays byte-identical across replays.
                const auto src_it  = w.bodies.find(src_body);
                const auto dest_it = w.bodies.find(dest_body);
                const std::string narration =
                    std::string("New trade route established: ") +
                    (src_it != w.bodies.end() ? src_it->second.name : "?") +
                    " <-> " +
                    (dest_it != w.bodies.end() ? dest_it->second.name : "?");
                for (const entity_id tagged : { src_body, dest_body })
                {
                    world_history_entry e;
                    e.timestamp = tick;
                    e.topic     = history_topic::trade_route;
                    e.body      = tagged;
                    e.corp      = convoy.corp;
                    e.event     = narration;
                    w.history_log.push_back(std::move(e));
                }
            }
            else
            {
                rit->last_tick = tick;
                ++rit->convoy_count;
            }

            // Proximity-glimpse peek (BL-099): a player convoy completing this inter-body
            // lane faintly lights any frontier body it passed near — the "route past a
            // frontier to reveal it" mechanic. Player-only (a glimpse is the player's own
            // commercial reach expanding their sight); sampled here at the discrete
            // completion tick from live orbital positions, then stored (never reconstructed).
            if (convoy.corp == w.player_entity)
                record_proximity_glimpses(w, src_body, dest_body, tick);
        }
    }

    // Retire arrived convoys.
    w.convoys.erase(
        std::remove_if(w.convoys.begin(), w.convoys.end(),
                       [](const convoy_component& c) { return c.arrived; }),
        w.convoys.end());
}

namespace {

/// Euclidean distance (AU) between two body entities at the given day tick.
/// Uses the tick-pure angle (orbital_angle_at_tick), never the frame-advanced
/// orbital_angle_rad — this feeds source selection, dispatch pricing and convoy
/// speed inside the econ tick, which must not depend on frame rate (BL-354).
/// Moons are approximated at their parent's position.
float body_distance_au(const world& w, entity_id a, entity_id b, int day_tick)
{
    if (a == b)
        return 0.0f;

    auto pos = [&](entity_id id) -> std::pair<float, float> {
        const auto it = w.bodies.find(id);
        if (it == w.bodies.end())
            return {0.0f, 0.0f};
        const body_component& bc = it->second;
        const float r = bc.orbital_radius_au;
        const float theta = orbital_angle_at_tick(bc, day_tick);
        return { r * std::cos(theta), r * std::sin(theta) };
    };

    const auto [ax, ay] = pos(a);
    const auto [bx, by] = pos(b);
    const float dx = ax - bx;
    const float dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

/// True if `corp` has a launchpad building whose tile is on `body`.
bool corp_has_launchpad_on(const world& w, const corporation_component& corp, entity_id body)
{
    for (entity_id asset : corp.assets)
    {
        const auto bit = w.buildings.find(asset);
        if (bit == w.buildings.end())
            continue;
        if (bit->second.type != building_type::launchpad)
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit != w.tiles.end() && tit->second.body == body)
            return true;
    }
    return false;
}

/// Stock of the drawn good `dr` the corp can actually burn launching `cargo_qty`
/// units of `ri` out of the source pool `src_key` (BL-1003: the pool the cargo
/// leaves), minus the cargo itself when the cargo IS the drawn good (a launch
/// cannot burn the propellant it is exporting).
float launch_draw_available(const world& w, entity_id corp, entity_id src_key,
                            std::size_t dr, std::size_t ri, float cargo_qty)
{
    const stockpile_component* p = w.find_pool(corp, src_key);
    if (p == nullptr)
        return 0.0f;
    float avail = p->quantities[dr];
    if (ri == dr)
        avail -= cargo_qty;
    return avail;
}

} // namespace

bool launch_burns_from_pool(const world& w, entity_id corp, entity_id pool_key)
{
    const auto cit = w.corporations.find(corp);
    if (cit == w.corporations.end())
        return false;
    const entity_id body = pool_key_body(w, pool_key);
    return body != null_entity && corp_has_launchpad_on(w, cit->second, body);
}

entity_id corp_representative_tile(const world& w, const corporation_component& corp, entity_id body)
{
    entity_id best_building = null_entity;
    entity_id best_tile     = null_entity;
    for (const entity_id bid : corp.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const auto tit = w.tiles.find(bit->second.tile);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;
        if (best_building == null_entity || bid < best_building)
        {
            best_building = bid;
            best_tile     = bit->second.tile;
        }
    }
    return best_tile;
}

entity_id convoy_origin_tile(const world& w, const corporation_component& corp, entity_id src_key)
{
    const auto mit = w.markets.find(src_key);
    if (mit == w.markets.end())
        return corp_representative_tile(w, corp, src_key); // body-level pool
    // A market pool: the corp's lowest-id building in THAT catchment, else the
    // market's own centre (stock that arrived by convoy sits at the market).
    entity_id best_building = null_entity;
    entity_id best_tile     = null_entity;
    for (const entity_id bid : corp.assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        if (pool_key_for_tile(w, bit->second.tile) != src_key)
            continue;
        if (best_building == null_entity || bid < best_building)
        {
            best_building = bid;
            best_tile     = bit->second.tile;
        }
    }
    return best_tile != null_entity ? best_tile : mit->second.centre_tile;
}

namespace {

/// Fraction in [0, cap] to discount one leg's haul cost by — summed over the
/// population-centre (scale-weighted) and hub (flat) tiles the leg crosses, capped.
/// Deterministic: a pure function of the tiles and the node sets.
float node_discount_fraction(const std::vector<entity_id>& tiles, const logistics_nodes& nodes,
                             const logistics_node_params& np)
{
    float disc = 0.0f;
    for (const entity_id t : tiles)
    {
        if (const auto it = nodes.pop_tile_scale.find(t); it != nodes.pop_tile_scale.end())
            disc += np.city_discount_per_scale * static_cast<float>(it->second);
        if (nodes.hub_tiles.count(t) != 0)
            disc += np.hub_discount;
    }
    // Enforce the invariant a route is never free (and never *credits* the corp) regardless of
    // how the cap tunable is authored: a misconfigured cap >= 1 would otherwise flip the sign of
    // the haul cost. Clamp the final discount to [0, 0.95] at this single choke point.
    return std::clamp(std::min(disc, np.discount_cap), 0.0f, 0.95f);
}

/// One leg of an intra-body route (BL-1186): its mode and what it is priced from.
struct route_leg
{
    convoy_mode mode      = convoy_mode::land;
    float       unit_cost = 0.0f; ///< reg.logistics_cost(mode)
    float       dist      = 0.0f; ///< the leg's own path cost
    float       discount  = 0.0f; ///< BL-148/149 node discount over the leg's own tiles
};

/// A routed intra-body haul (BL-1186, SUPPLY.md § Logistical cost): one land leg, or
/// land -> port -> sea -> port -> land. Priced by finish_route.
struct intra_route
{
    std::array<route_leg, 3> legs{};
    int         n_legs       = 0;
    int         ports        = 0; ///< ports the cargo passes through; handling is per port
    int         travel_ticks = 1;
    convoy_mode mode         = convoy_mode::land; ///< the convoy's mode: sea when any leg is
};

/// Credits per unit one leg costs: rate x distance x (1 - discount).
float route_leg_per_unit(const route_leg& l)
{
    return l.unit_cost * l.dist * (1.0f - l.discount);
}

/// THE INTRA-BODY ROUTER (BL-1186, goods cross markets; SUPPLY.md § Logistical cost and
/// § Infrastructure gates). Route `origin` -> `dest` on `body` as the cheapest of:
///
///   LAND   one land leg. The unconfined cheapest path when it stays on land (the BL-077
///          path, unchanged — every land route prices exactly as it always did), else the
///          cheapest LAND-ONLY path (B2: a pair whose cheapest path crosses water is no
///          longer refused when a dearer overland road exists).
///   SEA    land origin -> port A, sea A -> port B, land port B -> dest (B1). A and B range
///          over every built, active Port on the body; the pair minimising the WHOLE route,
///          both handling fees included, wins. A sea leg runs only port to port, so the
///          markets may lie inland behind their land legs — the Port is no longer demanded
///          on the market centre itself.
///
/// Cheapest by credits per unit (route_leg_per_unit, plus handling per port). Ties go to
/// LAND, then to the lower (A, B) by tile id: a total order, independent of every hash.
/// The choice does not depend on quantity (every term is linear in it), so a one-unit
/// probe and the committed cargo route the same way. False when no route exists: no
/// overland path and no port pair joining the two.
///
/// The route-wide `crosses_ocean` refusal (BL-608's "Port at both endpoints") is retired
/// with this: that bit billed or refused a whole route for the water on any part of it.
bool route_intra_body(world& w, const recipe_registry& reg, const logistics_nodes& nodes,
                      entity_id body, entity_id origin, entity_id dest, intra_route& out)
{
    if (origin == null_entity || dest == null_entity)
        return false; // no production anchor / unanchored market: cannot route
    const logistics_node_params& np = reg.logistics_nodes();
    const float land_rate = reg.logistics_cost(convoy_mode::land);
    const float sea_rate  = reg.logistics_cost(convoy_mode::sea);
    const float handling  = reg.port_handling();

    // The unconfined cheapest path. NOT an early exit when unreachable: a sea leg also
    // makes the port <-> water hop across the two hex diagonals the four-way flood never
    // steps (logistics.cpp, leg_flood_field_for), so a pair the unconfined flood cannot
    // join may still have a port route. No LAND route exists then, though: a land leg
    // walks the same four-way edges and never enters water, so it is a subset.
    const logistics_path& path = intra_body_path(w, body, origin, dest);

    bool        have = false;
    float       best = 0.0f;
    intra_route route;

    // LAND. Read before anything can touch the caches the reference points into
    // (std::map nodes stay put on insert; only a clear would move them).
    if (path.reachable && !path.crosses_ocean)
    {
        route.n_legs       = 1;
        route.legs[0]      = {convoy_mode::land, land_rate, path.cost,
                              node_discount_fraction(path.tiles, nodes, np)};
        route.travel_ticks = convoy_travel_ticks(w, body, path);
        route.mode         = convoy_mode::land;
        best = route_leg_per_unit(route.legs[0]);
        have = true;
    }
    else if (path.reachable)
    {
        const logistics_path& land = intra_body_leg_path(w, body, origin, dest, leg_domain::land);
        if (land.reachable)
        {
            route.n_legs       = 1;
            route.legs[0]      = {convoy_mode::land, land_rate, land.cost,
                                  node_discount_fraction(land.tiles, nodes, np)};
            route.travel_ticks = convoy_travel_ticks(w, body, land); // crosses_ocean false
            route.mode         = convoy_mode::land;
            best = route_leg_per_unit(route.legs[0]);
            have = true;
        }
    }

    // SEA. Every ordered pair of distinct active ports, ascending. The legs out of the
    // origin and into the destination are each priced once per port.
    //
    // THE CHEAP EXIT. Every sea route pays both handling fees on top of three
    // non-negative legs, so its per-unit cost is at least 2 x handling — in float too:
    // adding a non-negative term never rounds a sum below an addend. When a land route
    // already costs no more than that, no port pair can beat it, and the whole search
    // (its per-port land floods and node-discount passes) is skipped. The answer is the
    // one the full search gives; only the work differs.
    const float fees = 2.0f * handling; // loading at A, unloading at B
    const std::vector<entity_id> ports = body_active_port_tiles(w, body); // copy: a plain value
    if (ports.size() >= 2 && !(have && !(fees < best)))
    {
        struct end_leg
        {
            bool  ok = false;
            route_leg leg;
            float per_unit = 0.0f;
        };
        std::vector<end_leg> to_port(ports.size());
        for (std::size_t i = 0; i < ports.size(); ++i)
        {
            const logistics_path& a = intra_body_leg_path(w, body, origin, ports[i], leg_domain::land);
            if (a.reachable)
            {
                to_port[i].ok       = true;
                to_port[i].leg      = {convoy_mode::land, land_rate, a.cost,
                                       node_discount_fraction(a.tiles, nodes, np)};
                to_port[i].per_unit = route_leg_per_unit(to_port[i].leg);
            }
        }
        // The leg on from each port, priced on first need: a pair the bound below already
        // rules out never floods the destination's land field. The answer is the same
        // either way (a pure function of the tiles); only the work differs.
        std::vector<end_leg> from_port(ports.size());
        std::vector<char>    from_done(ports.size(), 0);
        const auto from = [&](std::size_t j) -> const end_leg& {
            if (!from_done[j])
            {
                from_done[j] = 1;
                const logistics_path& b =
                    intra_body_leg_path(w, body, ports[j], dest, leg_domain::land);
                if (b.reachable)
                {
                    from_port[j].ok       = true;
                    from_port[j].leg      = {convoy_mode::land, land_rate, b.cost,
                                             node_discount_fraction(b.tiles, nodes, np)};
                    from_port[j].per_unit = route_leg_per_unit(from_port[j].leg);
                }
            }
            return from_port[j];
        };
        for (std::size_t i = 0; i < ports.size(); ++i)
        {
            if (!to_port[i].ok)
                continue;
            // A lower bound on every route out through port i: its first leg and the fees.
            if (have && !(to_port[i].per_unit + fees < best))
                continue;
            for (std::size_t j = 0; j < ports.size(); ++j)
            {
                if (j == i)
                    continue;
                const end_leg& on = from(j);
                if (!on.ok)
                    continue;
                // A lower bound before the sea leg is routed: both land legs and the fees.
                if (have && !(to_port[i].per_unit + on.per_unit + fees < best))
                    continue;
                const logistics_path& s =
                    intra_body_leg_path(w, body, ports[i], ports[j], leg_domain::sea);
                if (!s.reachable)
                    continue;
                // The sea leg carries no node discount: the BL-148/149 nodes are cities and
                // hubs, land infrastructure, and a port city at either end already discounts
                // the land leg it closes.
                const route_leg sea{convoy_mode::sea, sea_rate, s.cost, 0.0f};
                const float per_unit = to_port[i].per_unit + route_leg_per_unit(sea)
                                     + on.per_unit + fees;
                if (have && !(per_unit < best))
                    continue;
                route.n_legs  = 3;
                route.legs[0] = to_port[i].leg;
                route.legs[1] = sea;
                route.legs[2] = on.leg;
                route.ports   = 2;
                route.mode    = convoy_mode::sea;
                // Each leg at its own speed (SUPPLY.md): caravan overland, coastal by sea;
                // summed, then quantised once.
                route.travel_ticks = travel_ticks_for_days(
                    leg_travel_days(w, body, to_port[i].leg.dist, convoy_mode::land)
                    + leg_travel_days(w, body, sea.dist, convoy_mode::sea)
                    + leg_travel_days(w, body, on.leg.dist, convoy_mode::land));
                best = per_unit;
                have = true;
            }
        }
    }

    if (have)
        out = route;
    return have;
}

/// The priced tail every intra-body route shares: each leg's rate x distance x qty x
/// (1 - its discount), plus handling x ports x qty. A single land leg is exactly the
/// pre-BL-1186 expression (0 + x is x), so a land route prices to the same bits.
convoy_leg finish_route(const intra_route& route, float qty, float handling)
{
    convoy_leg leg;
    float cost = 0.0f;
    for (int i = 0; i < route.n_legs; ++i)
    {
        const route_leg& l = route.legs[static_cast<std::size_t>(i)];
        cost += l.unit_cost * l.dist * qty * (1.0f - l.discount);
    }
    if (route.ports > 0)
        cost += handling * static_cast<float>(route.ports) * qty;
    // A cost that is not a finite, non-negative number is not a price. Reached
    // by an absurd (but finite) quantity overflowing the product; refused here
    // rather than debited, since `balance -= inf` is unrecoverable.
    if (!std::isfinite(cost) || cost < 0.0f)
        return leg;

    leg.viable       = true;
    leg.mode         = route.mode;
    leg.cost         = cost;
    leg.travel_ticks = route.travel_ticks < 1 ? 1 : route.travel_ticks;
    return leg;
}

/// The space lane's priced tail: cost = rate x distance x qty x (1 - discount), the
/// discount always 0 off the surface.
convoy_leg finish_leg(convoy_mode mode, float unit_cost, float dist, float qty,
                      float node_discount, int travel_ticks)
{
    convoy_leg leg;
    const float cost = unit_cost * dist * qty * (1.0f - node_discount);
    // A cost that is not a finite, non-negative number is not a price. Reached
    // by an absurd (but finite) quantity overflowing the product; refused here
    // rather than debited, since `balance -= inf` is unrecoverable.
    if (!std::isfinite(cost) || cost < 0.0f)
        return leg;

    leg.viable       = true;
    leg.mode         = mode;
    leg.cost         = cost;
    leg.travel_ticks = travel_ticks < 1 ? 1 : travel_ticks;
    return leg;
}

} // namespace

// ---------------------------------------------------------------------------
// The shared dispatch (BL-452) — see supply_system.hpp for why it is shared.
// ---------------------------------------------------------------------------

const std::array<float, resource_count>& launch_draw_per_convoy()
{
    // PRODUCTION.md § Launchpad specifies the cost *per launch*, not per tonne
    // or per AU. One authored round unit of propellant, in the same magnitude
    // family as the recipe batches (scripts/recipes.lua): two batches of the
    // atmosphere route, or three of the airless one, buys a launch.
    //
    // A vector rather than a scalar so the pass DECLARES what it draws — see
    // the header for why BL-648 needs that. Pure, immutable, computed once;
    // nothing here reads world state, so it cannot vary between replays.
    static const std::array<float, resource_count> draw = [] {
        std::array<float, resource_count> d{};
        d[static_cast<std::size_t>(resource_type::propellant)] = 1.0f;
        return d;
    }();
    return draw;
}

logistics_nodes collect_logistics_nodes(const world& w)
{
    logistics_nodes nodes;
    for (const auto& [centre_id, tile_id] : w.population_centre_tile)
    {
        const auto pit = w.population_centres.find(centre_id);
        const int  scale = (pit != w.population_centres.end()) ? pit->second.scale : 1;
        nodes.pop_tile_scale[tile_id] = scale;
    }
    for (const auto& [bid, bc] : w.buildings)
    {
        // A hub confers its discount only while it is built AND active — a decommissioned
        // hub is inert, matching how the production loop treats it (economy_system.cpp).
        if (bc.type == building_type::inland_logistics_hub && bc.ticks_remaining <= 0
            && !bc.decommissioned)
            nodes.hub_tiles.insert(bc.tile);
    }
    return nodes;
}

convoy_leg price_convoy_leg(world& w, const recipe_registry& reg,
                            const logistics_nodes& nodes, entity_id corp_id,
                            entity_id src_key, entity_id dest_market_id,
                            std::size_t ri, float qty, float logistics_cost_space,
                            const entity_id* known_origin)
{
    convoy_leg leg;

    const auto cit = w.corporations.find(corp_id);
    if (cit == w.corporations.end())
        return leg;
    // BL-1003: the source is a POOL — a market, or a market-less body. Moving
    // goods into the market they already sit in is not a haul.
    const entity_id src_body = pool_key_body(w, src_key);
    if (src_body == null_entity || src_key == dest_market_id)
        return leg;
    const auto mit = w.markets.find(dest_market_id);
    if (mit == w.markets.end())
        return leg;
    if (ri >= resource_count)
        return leg;
    // BL-708 — A GRID GOOD IS NEVER CARGO (docs/economy/PRODUCTION.md § Power;
    // docs/economy/LOGISTICS.md § 3a). Power has a price and clears like any
    // other good, but transmission IS the road network, at a flat one-tick
    // latency: there is no convoy to load, no tonnage to haul and no leg to
    // price. It is the roster's first good whose MOVEMENT and MARKET are
    // separate questions, and only the movement is special.
    //
    // Refused HERE, at the one shared seam, rather than at each of the callers:
    // `dispatch_convoys`' net-price rule, the player's `dispatch_convoy` verb
    // and the rival scorer's directed dispatch (BL-600) all price through this
    // function, so one non-viable leg closes all three and no fourth path can
    // open later without meeting it. A non-viable leg is the same answer an
    // unroutable lane already gives, so every caller already handles it.
    if (reg.grid_goods().grid(ri))
        return leg;
    // A non-finite quantity would make every comparison below meaningless and
    // the cost NaN; refuse it here so both callers refuse it identically.
    if (!std::isfinite(qty) || !(qty > 0.0f))
        return leg;

    const corporation_component& corp        = cit->second;
    const market_component&      dest_market = mit->second;
    const entity_id              dest_body   = dest_market.body;

    if (src_body == dest_body)
    {
        // Intra-body (BL-077, BL-1186): haul the source pool's stock from its
        // origin tile (BL-1003: `convoy_origin_tile` — the corp's lowest-id
        // building in the source catchment, else the source market's centre) to
        // the short market's centre — overland, or land -> port -> sea -> port
        // -> land where that route is cheaper or the only one (route_intra_body).
        const entity_id origin      = known_origin != nullptr
                                          ? *known_origin // BL-1079: resolved once per pool
                                          : convoy_origin_tile(w, corp, src_key);
        const entity_id dest_centre = dest_market.centre_tile;
        intra_route route;
        if (!route_intra_body(w, reg, nodes, src_body, origin, dest_centre, route))
            return leg;
        return finish_route(route, qty, reg.port_handling());
    }

    convoy_mode mode;
    float       dist;
    float       unit_cost;
    int         travel_ticks = 1;
    {
        // Inter-body: straight-line space lane, launchpad-gated.
        if (!corp_has_launchpad_on(w, corp, src_body))
            return leg;
        // BL-308: the pad also has to be FUELLED. A launch burns
        // `launch_draw_per_convoy()` from the corp's stockpile on the source
        // body; without it the lane is shut exactly as if no pad existed.
        // Deterministic — a pure read of the pool, walked in ascending resource
        // index so a multi-good draw cannot depend on container layout.
        {
            const auto& launch_draw = launch_draw_per_convoy();
            for (std::size_t dr = 0; dr < resource_count; ++dr)
                if (launch_draw[dr] > 0.0f
                    && launch_draw_available(w, corp_id, src_key, dr, ri, qty) < launch_draw[dr])
                    return leg;
        }
        mode      = convoy_mode::space;
        // BL-354: evaluated at the tick-pure angle, so sourcing, pricing and
        // convoy speed are a pure function of tick, never of frame rate.
        dist      = body_distance_au(w, src_body, dest_body, w.current_day_tick);
        unit_cost = logistics_cost_space;
        // The space lane keeps its own calibration — roughly one econ tick per
        // AU. Unchanged deliberately: it is the only leg the AU model was ever
        // right for, and it is parked with the space arc anyway (era/space).
        travel_ticks = (dist > 1.0f) ? static_cast<int>(dist + 0.999f) : 1;
    }

    return finish_leg(mode, unit_cost, dist, qty, 0.0f, travel_ticks);
}

convoy_leg price_market_export_leg(world& w, const recipe_registry& reg,
                                   const logistics_nodes& nodes, entity_id src_market,
                                   entity_id dest_market, float qty)
{
    if (src_market == dest_market || !std::isfinite(qty) || !(qty > 0.0f))
        return convoy_leg{};
    const auto sit = w.markets.find(src_market);
    const auto dit = w.markets.find(dest_market);
    if (sit == w.markets.end() || dit == w.markets.end())
        return convoy_leg{};
    const entity_id body = sit->second.body;
    if (body == null_entity || dit->second.body != body)
        return convoy_leg{}; // BL-1071 CALL: a market has no pad and no propellant
    // BL-1071 CALL: the haul leaves from the market's own centre — where
    // convoyed and unsold stock sits (convoy_origin_tile's fallback for a pool
    // with no building in the catchment). Unanchored: no road to leave by.
    // BL-1186: the same router a corporation's leg takes (route_intra_body), so a pair
    // this answers viable for is one a convoy can actually run, and BL-1185's
    // placement, asking this one function, can never disagree with the dispatcher.
    intra_route route;
    if (!route_intra_body(w, reg, nodes, body, sit->second.centre_tile,
                          dit->second.centre_tile, route))
        return convoy_leg{};
    return finish_route(route, qty, reg.port_handling());
}

namespace {

/// BL-597's passive-LP gate, shared by a corporation's convoy (commit_convoy) and a
/// market's own export (BL-1071): the remaining passive LP this tick at the anchor
/// NEAREST `origin` on `src_body`, as a slot the caller draws from — null when no
/// anchor is reachable (no passive LP exists to draw against). Reading it builds the
/// body's pool on first touch and mutates nothing else. The pointer stays valid for
/// the pass: the per-body pools are node-based maps nothing below erases from.
float* passive_lp_slot(world& w, const recipe_registry& reg, entity_id src_body,
                       entity_id origin, lp_pool_map& pools_by_body)
{
    const military_capability_params& mil = reg.military();
    std::unordered_map<entity_id, float>& pools =
        lp_pool_for_body(pools_by_body, w, src_body, mil.active_lp_per_anchor_tick);
    const entity_id nearest_anchor =
        (origin != null_entity) ? nearest_lp_anchor(w, src_body, origin, pools) : null_entity;
    if (nearest_anchor == null_entity)
        return nullptr;
    return &pools.at(nearest_anchor);
}

/// How much of a `qty` cargo the anchor's remaining passive LP `pool` admits.
///
/// WHOLE (BL-597, `allow_partial` false): all of it when the pool holds it (the 1e-6
/// slack is the original test's), else nothing — the commanded quantity of a player's
/// verb or a rival's directed dispatch is sent whole or refused, mutating nothing.
///
/// PARTIAL (BL-1186 E1, the auto-dispatch passes): what the pool still holds, up to
/// the cargo. LP stays the CAP (LOGISTICS.md rule 1); it no longer refuses a large
/// surplus whole — before this no single convoy could carry more than one anchor's
/// tick, so any cargo above it never moved at all. Not below one unit unless the
/// cargo itself is smaller: an anchor's last crumbs do not make a convoy.
float passive_lp_grant(float pool, float qty, bool allow_partial)
{
    if (pool + 1e-6f >= qty)
        return qty;
    if (!allow_partial)
        return 0.0f;
    return (pool >= std::min(qty, 1.0f)) ? pool : 0.0f;
}

} // namespace

bool commit_convoy(world& w, const recipe_registry& reg, entity_id corp_id, entity_id src_body,
                   entity_id src_market, entity_id dest_market_id,
                   std::size_t ri, float qty, const convoy_leg& leg,
                   lp_pool_map* shared_lp_pools, bool* out_refused_no_lp,
                   bool allow_partial_lp, float* out_sent)
{
    if (!leg.viable || ri >= resource_count)
        return false;
    const auto cit = w.corporations.find(corp_id);
    if (cit == w.corporations.end())
        return false;
    corporation_component& corp = cit->second;
    // The solvency gate, in ONE place for every caller. A whole cargo is weighed at its
    // full cost before the LP gate (the BL-597 order); a cargo the LP cap may trim
    // (BL-1186 E1) is weighed below at the cost of what will actually go.
    if (!allow_partial_lp && corp.balance < leg.cost)
        return false;

    // BL-1003: the pool the cargo leaves is the SOURCE MARKET's, or — when the
    // source body has no market — the body-level pool.
    const entity_id src_key =
        (w.markets.find(src_market) != w.markets.end()) ? src_market : src_body;

    // BL-597: the passive-LP admissibility gate, before any mutation —
    // same "refused outright, mutates nothing" contract as BL-596's active
    // gate (run_unit_march). Space legs have no intra-body path at all and
    // are out of scope (LOGISTICS.md's Logistic Points design is
    // tile-grounded — "cities are the locus"), matching BL-596's own march
    // gate, which likewise only fires for a unit walking a tile path.
    //
    // THE DRAW IS CARGO QUANTITY, NOT DISTANCE (Ben, 2026-08-25, ruling on
    // NR-620). The first cut drew `leg.dist` and LOGISTICS.md forbids that
    // twice over: constraint 3, "if cost is proportional to distance, LP
    // *is* haulage cost again", and rule 1, "the convoy already charges
    // distance in credits... would double-charge distance". Measured, the
    // distance draw collapsed real convoy traffic from 1055 dispatches to
    // 284 (haulage_measure on the generated world) — most hauls need many
    // times an anchor's whole tick and were refused on an idle tick, for
    // ever. Quantity is what "how much can move through HERE" actually
    // says: the anchor passes so many units of goods per tick, and credits
    // remain the sole price of distance.
    float send = qty;
    float cost = leg.cost;
    if (leg.mode != convoy_mode::space)
    {
        // Same locus as price_convoy_leg's own origin — `convoy_origin_tile`
        // of the source pool, the convoy's actual dispatch point.
        const entity_id origin = convoy_origin_tile(w, corp, src_key);
        lp_pool_map  local_pools;
        float* const slot = passive_lp_slot(w, reg, src_body, origin,
                                            shared_lp_pools ? *shared_lp_pools : local_pools);
        send = slot ? passive_lp_grant(*slot, qty, allow_partial_lp) : 0.0f;
        if (!(send > 0.0f))
        {
            if (out_refused_no_lp)
                *out_refused_no_lp = true;
            return false;
        }
        // Cost is linear in quantity (rate x distance x qty, handling x qty), so a
        // trimmed cargo pays its share of the priced leg and no more.
        if (send < qty)
            cost = leg.cost * (send / qty);
        if (allow_partial_lp && corp.balance < cost)
            return false;
        // Granted: consume the anchor's pool. LP is the CAP, not a second PRICE
        // (LOGISTICS.md rule 1) — `cost` is the only credit charge a passive draw
        // pays; unlike BL-596's active draw, there is no LP-specific credit line.
        *slot -= send;
    }
    else if (allow_partial_lp && corp.balance < cost)
    {
        return false;
    }

    // Debit cost and source pool; create the convoy.
    corp.balance -= cost;
    w.pool_at(corp_id, src_key).quantities[ri] -= send;

    // BL-308: burn the launch's draw. Charged once per launch (not per unit,
    // not per AU) and only on the space lane; price_convoy_leg's availability
    // gate already ran against this same pool and the same vector, so this
    // cannot drive it negative. Ascending resource index — the same
    // determinism discipline the gate above uses.
    if (leg.mode == convoy_mode::space)
    {
        auto&       quantities  = w.pool_at(corp_id, src_key).quantities;
        const auto& launch_draw = launch_draw_per_convoy();
        // BL-1227 (AI_OPPONENT.md § 2B, Ben 2026-10-08): launch fuel taken from
        // the corporation's pool is a buyer that posts no bid.
        const auto fuel_mit = w.markets.find(src_key);
        for (std::size_t dr = 0; dr < resource_count; ++dr)
            if (launch_draw[dr] > 0.0f)
            {
                quantities[dr] -= launch_draw[dr];
                if (fuel_mit != w.markets.end())
                    note_unposted_bid(fuel_mit->second, dr, launch_draw[dr], w.current_econ_tick);
            }
    }

    convoy_component c;
    c.id             = w.allocate_convoy_id();
    c.source_market  = src_market;
    c.dest_market    = dest_market_id;
    c.mode           = leg.mode;
    c.cargo_resource = static_cast<resource_type>(ri);
    c.cargo_qty      = send;
    c.progress       = 0.0f;
    // Speed is progress-per-tick, so a leg taking N ticks advances 1/N each
    // tick (Ben, 2026-08-12).
    //
    // WAS: `1 / distance_in_AU`, an interplanetary calibration.
    // `body_distance_au` returns 0 for two markets on the same body, so it
    // clamped to 1.0 and EVERY intra-body convoy arrived in a single econ tick
    // regardless of how far it went — distance cost money and never cost time.
    // `leg.travel_ticks` carries the terrain-weighted, physically-scaled figure.
    c.speed          = 1.0f / static_cast<float>(leg.travel_ticks);
    c.corp           = corp_id;
    c.arrived        = false;
    c.held           = false;
    c.cost_paid      = cost;
    w.convoys.push_back(c);
    if (out_sent)
        *out_sent = send;
    return true;
}

// ---------------------------------------------------------------------------
// BL-995 — the net-price rule's shared sizing (supply_system.hpp)
// ---------------------------------------------------------------------------

float dispatch_market_price(const market_component& mc, std::size_t r)
{
    if (r >= resource_count || mc.base_price[r] <= 0.0f)
        return 0.0f;
    return (mc.price[r] > 0.0f) ? mc.price[r] : mc.base_price[r];
}

float dispatch_home_price(const world& w, entity_id src_key, std::size_t r)
{
    const auto it = w.markets.find(src_key);
    return (it != w.markets.end()) ? dispatch_market_price(it->second, r) : 0.0f;
}

order_floor_map collect_order_floors(const world& w)
{
    // Several orders on one triple: the HIGHEST floor binds — a haul never
    // sells below a price any of the corp's orders on the good named.
    order_floor_map floors;
    for (const sell_order& o : w.sell_orders)
    {
        float& f = floors[{o.corp, o.body, static_cast<std::size_t>(o.resource)}];
        f = std::max(f, o.floor_price);
    }
    return floors;
}

float dispatch_source_price(const world& w, const order_floor_map& floors, entity_id corp,
                            entity_id src_key, std::size_t r)
{
    const float home = dispatch_home_price(w, src_key, r);
    if (floors.empty())
        return home;
    const auto it = floors.find({corp, pool_key_body(w, src_key), r});
    return (it != floors.end()) ? std::max(home, it->second) : home;
}

float dispatch_absorbable(const world& w, const recipe_registry& reg, entity_id dest,
                          std::size_t r, float landed_cost)
{
    // THE DERIVATION. Clearing aims each market's price at the UNSMOOTHED
    // target (price_target, market_clearing.hpp):
    //     p_target(S') = clamp(base x sqrt(D / S'), floor x base, ceil x base)
    // with D this market's demand and S' the supply listed at its next clear.
    // A haul pays while the price it lands into is above its landed cost, so
    // the market can absorb supply up to the S* at which p_target(S*) ==
    // landed. Inside the band, base x sqrt(D / S*) = landed gives
    //     S* = D x (base / landed)^2,
    // and what is left to absorb is S* - S, S being LAST clear's listings plus
    // the shelf's share standing now (`pricing_supply` — what the market
    // already has to sell). At the band's edges:
    //   * landed below floor x base: the target never falls to the landed
    //     cost however much is listed, so there is no finite S* — unbounded
    //     (the caller's surplus caps the send);
    //   * landed at or above ceil x base, or no demand at all (the target sits
    //     on the floor): no supply level prices above landed — nothing.
    // The ZERO-SUPPLY case sizes by the same S* (BL-1203 cold review, fix 1,
    // below); its old rule — absorb the whole unmet demand — aimed the target
    // at base, under the landed cost of any haul that costs anything.
    //
    // It is sized off the TARGET, never the eased market price: the eased
    // price lags its target, so a size read off it asks for more every tick
    // and the price saws below the landed cost (cold review, 2026-09-23). The
    // DESTINATION is still chosen on the last resolved price (the doc's rule).
    if (r >= resource_count)
        return 0.0f;
    const auto it = w.markets.find(dest);
    if (it == w.markets.end())
        return 0.0f;
    const market_component& dm = it->second;
    const float base = dm.base_price[r];
    if (base <= 0.0f)
        return 0.0f;
    // BL-1172: the SAME supply the price law resolves on — listings plus the
    // shelf's share, k ticks of demand at most (`pricing_supply`,
    // market_clearing.hpp; here the last clear's demand, as D below is). The
    // shipped k is 0 — listings only — until shelf spoilage, BL-1179.
    const float S = pricing_supply(dm, r, reg.price_band().shelf_supply_ticks);
    if (!(landed_cost > 0.0f))
        return (S <= 0.0f) ? std::max(0.0f, dm.demand[r]) : std::numeric_limits<float>::infinity();
    // BL-1203 COLD REVIEW, FIX 1 — THE SHIPPER'S MARGIN. The send is sized to
    // leave the destination's target at the landed cost PLUS the margin the
    // destination was chosen on (dispatch_margin, SUPPLY.md § Dispatch
    // trigger), not at the landed cost itself: sized at L the send aimed every
    // destination at zero margin and pushed its local sellers down to the
    // importer's landed cost. L' = L x (1 + margin) is the price aimed at.
    const float aim = landed_cost * (1.0f + reg.dispatch_margin());
    // BL-1203 (SUPPLY.md § Dispatch trigger, "What a hauler sees as unmet
    // demand", Ben 2026-10-05): D is the demand a hauler landing at L' meets —
    // the households' bid RE-READ at that price (their elastic factor
    // clamp((base / L')^e) in place of the one their posted bid carries;
    // inject_population_demand's formula, from the clear's pre-elastic weight).
    //
    // COLD REVIEW, FIX 2 — THE SUPPRESSED WANT IS GATED ON THE PROJECTED SALE.
    // See below, after the W-silent size.
    float D = dm.demand[r];
    if (reg.hauler_room() && std::isfinite(aim) && dm.household_weight[r] > 0.0f)
    {
        const population_demand_params& pd = reg.population_demand();
        const float at_aim = dm.household_weight[r]
                           * std::clamp(std::pow(base / aim, pd.demand_elasticity),
                                        pd.elasticity_min, pd.elasticity_max);
        D += at_aim - dm.household_bid[r];
    }
    const float lo = base * reg.price_band().floor_mult;
    const float hi = base * reg.price_band().ceil_mult;
    if (aim < lo)
        return std::numeric_limits<float>::infinity();
    if (aim >= hi || D <= 0.0f)
        return 0.0f;
    // FIX 1 applies to the ZERO-SUPPLY case too: sending the whole unmet
    // demand into an empty market resolves its target to base x sqrt(D / D) =
    // base, below the aim whenever the landed cost is above base. S* is the
    // honest size there as well — S is simply 0.
    const float ratio  = base / aim;
    float       s_star = D * ratio * ratio; // the W-silent size: target lands at the aim
    // FIX 2 (cold review) — THE SUPPRESSED WANT (`hauler_want`, W) IS GATED ON
    // THE PROJECTED POST-LANDING PRICE, NOT ON THE LANDED COST ALONE. Its buyers
    // bid only once the POSTED price is under their ceiling, and a cargo sells
    // at the clear of its arrival tick — while the posted price still stands
    // where it stood and W is still silent. So W counts only (a) when the aim
    // is under the buyers' ceiling (they will bid once a delivery has brought
    // the price there) and (b) only up to the size whose FIRST clear, with W
    // silent and the households bidding at the posted price (`demand` as it
    // stands), still sells at or above the aim. That clear eases the posted
    // price P toward its target by k_price_smoothing (resolve_price), so it
    // sells at or above the aim while
    //     target >= T* = P - (P - aim) / k_price_smoothing,
    // i.e. supply <= demand x (base / T*)^2; a T* at or under the floor bounds
    // nothing. This guards the cargo's own sale only: the main size above stays
    // the TARGET-based one (the 2026-09-23 rule: never size off the eased price).
    const float W        = dm.hauler_want[r];
    const float res_mult = reg.price_band().reservation_mult;
    if (reg.hauler_room() && W > 0.0f && res_mult > 0.0f && aim <= base * res_mult)
    {
        const float s_with = (D + W) * ratio * ratio;
        const float P      = (dm.price[r] > 0.0f) ? dm.price[r] : base;
        const float t_star = P - (P - aim) / k_price_smoothing;
        float       cap    = std::numeric_limits<float>::infinity();
        if (t_star > lo)
        {
            const float d_post = std::max(0.0f, dm.demand[r]);
            cap = d_post * (base / t_star) * (base / t_star);
        }
        s_star = std::max(s_star, std::min(s_with, cap));
    }
    return std::max(0.0f, s_star - std::max(0.0f, S));
}

float dispatch_pending(const world& w, const recipe_registry& reg, entity_id dest,
                       std::size_t r, const std::vector<entity_id>& sorted_corp_ids,
                       reservation_memo& memo)
{
    if (r >= resource_count)
        return 0.0f;
    // Every corp's cargo bound for `dest` — held convoys included (their cargo
    // is committed; a hold pauses it, it does not un-send it), and convoys
    // committed earlier in THIS pass included (commit_convoy appends to
    // w.convoys before the next decision is sized). Vector order: fixed.
    float pending = 0.0f;
    for (const convoy_component& c : w.convoys)
        if (c.dest_market == dest && static_cast<std::size_t>(c.cargo_resource) == r)
            pending += c.cargo_qty;
    // Plus every corp's stock already IN `dest` above its processor
    // reservation — this tick's production and arrivals, which list at the
    // next clear. Ascending corp id: a fixed accumulation order.
    for (const entity_id corp : sorted_corp_ids)
    {
        const stockpile_component* p = w.find_pool(corp, dest);
        if (p == nullptr || !(p->quantities[r] > 0.0f))
            continue;
        auto mit = memo.find({corp, dest});
        if (mit == memo.end())
            mit = memo.emplace(std::make_pair(corp, dest),
                               auto_surplus_reservation(w, reg, corp, dest)).first;
        const float excess = p->quantities[r] - mit->second[r];
        if (excess > 0.0f)
            pending += excess;
    }
    return pending;
}

float dispatch_room(const world& w, const recipe_registry& reg, entity_id dest, std::size_t r,
                    float landed_cost, const std::vector<entity_id>& sorted_corp_ids,
                    reservation_memo& memo)
{
    const float absorb = dispatch_absorbable(w, reg, dest, r, landed_cost);
    if (!(absorb > 0.0f))
        return 0.0f;
    return std::max(0.0f, absorb - dispatch_pending(w, reg, dest, r, sorted_corp_ids, memo));
}

float dispatch_arrived(const world& w, entity_id corp, entity_id key, std::size_t r)
{
    const auto it = w.arrived_this_tick.find({corp, key});
    return (it != w.arrived_this_tick.end() && r < resource_count) ? it->second.quantities[r]
                                                                   : 0.0f;
}

float market_shelf_surplus(const world& w, entity_id market, std::size_t r)
{
    if (r >= resource_count)
        return 0.0f;
    const auto it = w.markets.find(market);
    if (it == w.markets.end())
        return 0.0f;
    const market_component& m = it->second;
    // BL-1071 CALL: WHAT THE MARKET'S OWN CONSUMERS NEED is last clear's DEMAND
    // for the good at this market (`demand` still holds it at dispatch — clearing
    // zeroes it only at its own top). Demand is the per-tick want of everything
    // local that draws on the shelf (processors' wants, construction sites'
    // wants, the population), so one tick of it stays on the shelf for the next
    // tick's economy step, which draws before the next clear refills anything.
    // It is a WANT, not a measured draw, so it errs on the side of keeping stock
    // home: a processor that covers its want from its own pool still counts.
    // The alternative — this tick's measured shelf draw — needs a persistent
    // per-market record the save would carry, and reads zero at exactly the
    // stranded shelves this exists for (construction paused elsewhere, nothing
    // drawing here).
    //
    // Minus THIS TICK'S shelf deliveries: a market export delivered this tick
    // reaches its destination's economy step before it may move again — the
    // same arrived-this-tick rule a corporation's delivery obeys (the transient
    // record is keyed (null corporation, market) for shelf deliveries).
    const float surplus = m.inventory[r] - std::max(0.0f, m.demand[r])
                        - dispatch_arrived(w, null_entity, market, r);
    return surplus > 0.0f ? surplus : 0.0f;
}

namespace {

/// BL-1071 — a market exports its own shelf. SUPPLY.md § Dispatch trigger ("A
/// market exports its own shelf"); the net-price rule of dispatch_convoys,
/// applied to stock no corporation holds any more. Runs after every
/// corporation's dispatch in the same pass, so it takes only the room left.
void export_market_shelves(world& w, const recipe_registry& reg, const logistics_nodes& nodes,
                           const std::vector<entity_id>& corp_ids,
                           const std::vector<entity_id>& market_ids, reservation_memo& memo,
                           lp_pool_map* shared_lp_pools, convoy_dispatch_tick& out)
{
    const float              margin     = reg.dispatch_margin();
    const grid_goods_params& grid_rules = reg.grid_goods();

    struct candidate
    {
        float     net;
        entity_id dest;
        float     haul;
    };
    std::vector<candidate> cands;

    for (const entity_id src : market_ids) // ascending: a fixed commit order
    {
        const auto sit = w.markets.find(src);
        if (sit == w.markets.end())
            continue;
        const entity_id src_body = sit->second.body;
        // The haul leaves from the market's centre (price_market_export_leg);
        // an unanchored market has no road to leave by, so its shelf stays.
        const entity_id origin = sit->second.centre_tile;
        if (src_body == null_entity || origin == null_entity)
            continue;

        for (std::size_t ri = 0; ri < resource_count; ++ri)
        {
            if (grid_rules.grid(ri)) // BL-708: a grid good is never cargo
                continue;
            const float surplus = market_shelf_surplus(w, src, ri);
            if (!(surplus > 0.0f))
                continue;

            const float price_src = dispatch_market_price(w.markets.at(src), ri);
            const float gate      = price_src + margin * price_src;
            const float probe_qty = std::min(surplus, 1.0f);

            cands.clear();
            for (const entity_id dest_id : market_ids)
            {
                if (dest_id == src)
                    continue;
                const market_component& dm = w.markets.at(dest_id);
                // BL-1071 CALL: SAME BODY ONLY. A space lane needs a launchpad on
                // the source body and burns propellant from the SHIPPER's pool;
                // a market owns neither, so it has no lane off its body.
                if (dm.body != src_body)
                    continue;
                const float price_d = dispatch_market_price(dm, ri);
                if (!(price_d > gate))
                    continue;
                const convoy_leg leg =
                    price_market_export_leg(w, reg, nodes, src, dest_id, probe_qty);
                if (!leg.viable)
                    continue;
                const float haul = leg.cost / probe_qty;
                const float net  = price_d - haul;
                if (!(net - price_src > margin * price_src))
                    continue;
                cands.push_back({net, dest_id, haul});
            }
            if (cands.empty())
                continue;
            std::sort(cands.begin(), cands.end(), [](const candidate& a, const candidate& b) {
                return (a.net != b.net) ? (a.net > b.net) : (a.dest < b.dest);
            });

            for (const candidate& c : cands)
            {
                // The room corporations' dispatch uses (absorbable less every
                // corporation's cargo in flight to d and stock sitting in d above
                // its reservation) — AND, BL-1071 CALL, less the stock already on
                // d's SHELF. Shelf stock is not listed at a clear, so it never
                // lowers d's price the way a seller's delivery does; without this
                // term a destination whose price cannot respond would be sent the
                // same room every tick for ever. What is on d's shelf reaches d's
                // consumers first, so it is pending in every sense that matters.
                const float landed = price_src + c.haul;
                const float absorb = dispatch_absorbable(w, reg, c.dest, ri, landed);
                if (!(absorb > 0.0f))
                    continue;
                const float pending = dispatch_pending(w, reg, c.dest, ri, corp_ids, memo)
                                    + std::max(0.0f, w.markets.at(c.dest).inventory[ri]);
                const float room = absorb - pending;
                const float qty  = std::min(surplus, room);
                if (!(qty > 0.0f) || !std::isfinite(qty))
                    continue;

                const convoy_leg leg = price_market_export_leg(w, reg, nodes, src, c.dest, qty);
                if (!leg.viable)
                    continue;

                // The passive-LP cap binds a market's export exactly as it binds
                // a corporation's (BL-597): the anchor nearest the market centre.
                // BL-1186 E1: it sends what the anchor still admits rather than
                // refusing a cargo above it whole.
                lp_pool_map  local_pools;
                float* const slot = passive_lp_slot(
                    w, reg, src_body, origin, shared_lp_pools ? *shared_lp_pools : local_pools);
                const float send = slot ? passive_lp_grant(*slot, qty, /*allow_partial=*/true)
                                        : 0.0f;
                if (!(send > 0.0f))
                {
                    ++out.refused_no_lp;
                    break;
                }
                *slot -= send;
                if (send < qty)
                    ++out.trimmed_by_lp;

                // BL-1071 CALL (FINANCE.md): NO BALANCE MOVES. The market has no
                // treasury — it is already the counterparty that pays auto-surplus
                // sellers and takes buyers' money with no ledger of its own — so
                // the haul is "paid out of the export" only in the rule's sense:
                // an export whose destination net does not beat home by the
                // margin is never sent, and the haul's cost is the margin that
                // vanishes. Nobody is debited and nobody is credited; the cost is
                // recorded on the convoy (cost_paid) so a reader can see it.
                // Goods are conserved exactly: shelf debit = cargo = the
                // destination shelf's credit on arrival.
                w.markets.at(src).inventory[ri] -= send;

                convoy_component cv;
                cv.id             = w.allocate_convoy_id();
                cv.source_market  = src;
                cv.dest_market    = c.dest;
                cv.mode           = leg.mode;
                cv.cargo_resource = static_cast<resource_type>(ri);
                cv.cargo_qty      = send;
                cv.progress       = 0.0f;
                cv.speed          = 1.0f / static_cast<float>(leg.travel_ticks);
                // BL-1071 CALL: owned by NO corporation — the sentinel
                // `corp == null_entity` IS "the source market's export", read
                // with `source_market`. On arrival it lands on the destination
                // SHELF (credit_arrived_convoys), not in any pool.
                cv.corp           = null_entity;
                cv.arrived        = false;
                cv.held           = false;
                cv.cost_paid      = (send < qty) ? leg.cost * (send / qty) : leg.cost;
                w.convoys.push_back(cv);
                ++out.market_exports;
                break; // one destination per (market, good) per pass
            }
        }
    }
}

} // namespace

convoy_dispatch_tick dispatch_convoys(world& w, const recipe_registry& reg,
                      float logistics_cost_land, float logistics_cost_space,
                      lp_pool_map* shared_lp_pools)
{
    // BL-995 (trade reaches for price) — SUPPLY.md § Dispatch trigger. The
    // SELLER chases a NET PRICE: for every (corp, market) pool holding a good
    // above its processor reservation, haul it to the market where it fetches
    // the most once the haul is paid, if that beats selling at home by more than
    // the authored margin. It replaced a buyer-side shortfall scan that never
    // read a price; a shortfall needs no trigger of its own, because a short
    // market prices the good high and this rule already reaches it.
    //
    //   net(d) = price_d - haul_per_unit(src -> d)
    //   send to argmax_d net(d)  if  net(d) - price_src > margin x price_src
    //
    // HANDLING AND DUTY. The design charges both: arrival duty at a border
    // (MARKETS.md § Tariffs — rate x price_d x qty, paid by the convoy on
    // arrival) and handling at every port a cargo passes (SUPPLY.md §
    // Logistical cost). Handling is IN the leg (BL-1186: finish_route adds
    // port_handling x ports x qty to the priced cost), so haul_per_unit below
    // already carries it into net(d) and the landed cost. Duty is not built —
    // the tariff is still the matched-trade charge clearing puts on the buyer —
    // so its term is zero here; when built it is a per-unit cost of the haul and
    // MUST enter net(d) beside haul_per_unit.
    //
    // WHEN IT RUNS. After run_economy_step and BEFORE clear_markets (app.cpp
    // step_economy). Every market-side read below is therefore LAST tick's
    // clearing: `price` is last tick's resolved price, and `supply` / `demand`
    // still hold what the last clear listed — clear_markets zeroes both at its
    // own top, and nothing between two clears writes either (the demand
    // injectors all run inside clear_markets). Pools, by contrast, are live: the
    // economy step has produced into them and arrivals have been credited.
    convoy_dispatch_tick out;

    // BL-597: this pass's passive-LP pools. Local (and so shared across
    // every convoy THIS call commits) when the caller did not hand us a
    // shared instance — see this function's own doc comment.
    lp_pool_map local_pools;
    lp_pool_map& pools_by_body = shared_lp_pools ? *shared_lp_pools : local_pools;

    // BL-148/149: build the logistics-node lookups once — cities (population centres) and the
    // player's inland logistics hubs discount any intra-body haul whose A* path crosses them.
    const logistics_nodes nodes = collect_logistics_nodes(w);

    const float              margin     = reg.dispatch_margin();
    const grid_goods_params& grid_rules = reg.grid_goods();
    const auto&              launch_draw = launch_draw_per_convoy();

    // Sorted id walks (the standing.hpp convention): corporations and markets are
    // unordered_maps, and convoy insertion order — hence trade-route creation order
    // and the serialised history_log trade_route entries — would otherwise inherit
    // hash layout.
    std::vector<entity_id> corp_ids;
    corp_ids.reserve(w.corporations.size());
    for (const auto& [id, corp] : w.corporations)
        corp_ids.push_back(id);
    std::sort(corp_ids.begin(), corp_ids.end());

    std::vector<entity_id> market_ids;
    market_ids.reserve(w.markets.size());
    for (const auto& [id, market] : w.markets)
        market_ids.push_back(id);
    std::sort(market_ids.begin(), market_ids.end());

    // Processor reservations do not change within the pass (nothing below
    // touches a building), so each (corp, market) is computed once.
    reservation_memo memo;

    // BL-1229 (steel stays home) — AN ORDER IS A FLOOR, NOT A HOLD (Ben,
    // 2026-10-07; MARKETS.md step 4). A good under a standing SELL ORDER still
    // travels: the dispatcher hauls an ordered (corp, body, good) pool by the
    // same net-price rule and the same room as unordered surplus, with the
    // order's floor standing in for the home price wherever the floor is the
    // higher — the seller's alternative to the haul is a sale the order would
    // accept, and the order refuses any below its floor. So a haul must net the
    // seller more than the floor (by the margin, as it must beat home), and is
    // sized so the destination's price lands no lower than floor + haul. Before
    // this rule an order held its pool out of every convoy (the BL-995 CALL) and
    // stranded ~30% of all steel surplus while processors elsewhere starved.
    //
    // Several orders on one triple: the HIGHEST floor binds (collect_order_floors;
    // dispatch_source_price is the one rule this pass and the rival scorer's
    // directed dispatch share). A capped order's `quantity` caps what is LISTED
    // at home per tick, not what may be hauled: the haul draws on the same
    // surplus above the processor reservation as an unordered pool. One rule for
    // the player's orders and a rival's.
    //
    // A HAULED POOL KEEPS ITS ORDER (BL-1229 review). Dispatch runs before the
    // clear, so a pool hauled empty would read as "nothing to sell" to the
    // order's auto-close, and once closed the haul would price at the home price
    // and could ship below the floor. Every ordered triple this pass hauls from
    // is recorded in `w.hauled_ordered_this_tick`, which the clear reads as
    // "not empty" and then clears (world.hpp).
    const order_floor_map order_floor = collect_order_floors(w);
    w.hauled_ordered_this_tick.clear();

    struct candidate
    {
        float     net;
        entity_id dest;
        float     haul;
        bool      space;
        float     price_d; // BL-1222: recorded for the lens only; no rule reads it
    };
    std::vector<candidate> cands;

    // BL-1222 (trade-flow lens) — the PLAYER corporation's record of this pass.
    // WRITE-ONLY: every line that touches `tf` below records a value the pass has
    // already computed for its own decision (or, for the classes of candidates the
    // one-destination rule never reached, one extra `dispatch_room` read — a pure
    // query); no branch the dispatcher takes reads it. Rolled into the world's
    // trailing window at the end of the pass.
    trade_flow_pass tf;
    tf.corp = w.player_entity;
    const auto tf_note = [&tf](entity_id dest, std::size_t ri, trade_refusal cls) {
        const auto key = std::make_pair(dest, static_cast<std::uint16_t>(ri));
        const auto it  = tf.best.find(key);
        if (it == tf.best.end())
            tf.best.emplace(key, cls);
        else if (static_cast<int>(cls) > static_cast<int>(it->second))
            it->second = cls;
    };
    // A market is SHORT of a good when last clear's demand outran its supply —
    // the Scarcity lens's public signal (LENSES.md § Scarcity lens).
    const auto tf_short = [&w](entity_id dest, std::size_t ri) {
        const market_component& m = w.markets.at(dest);
        return m.demand[ri] > m.supply[ri];
    };

    for (const entity_id corp_id : corp_ids)
    {
        const bool tf_player = (corp_id == w.player_entity);
        // This corp's pool keys, ascending (a std::map slice). Collected first
        // so nothing committed below can disturb the walk.
        std::vector<entity_id> src_keys;
        for (auto it = w.corp_market_pools.lower_bound({corp_id, entity_id{0}});
             it != w.corp_market_pools.end() && it->first.first == corp_id; ++it)
            src_keys.push_back(it->first.second);

        for (const entity_id src_key : src_keys)
        {
            const entity_id src_body = pool_key_body(w, src_key);
            if (src_body == null_entity)
                continue;
            const bool src_is_market = (w.markets.find(src_key) != w.markets.end());

            // BL-1079 (live tick speedups): this pool's intra-body origin tile,
            // resolved on the first intra-body leg and reused for every other
            // destination and good of the pool. It reads only the corp's
            // buildings and the market set, and nothing in this pass moves either
            // (commit_convoy writes balances, pools and the convoy list), so the
            // value is the one each leg used to recompute for itself.
            bool      origin_known = false;
            entity_id origin_tile  = null_entity;
            const auto pool_origin = [&]() -> const entity_id* {
                if (!origin_known)
                {
                    origin_tile  = convoy_origin_tile(w, w.corporations.at(corp_id), src_key);
                    origin_known = true;
                }
                return &origin_tile;
            };

            // The same processor reservation auto-surplus holds back. NOT the
            // held opening stock (BL-1217 D5, `world::opening_stock_held`):
            // auto-surplus keeps that off the HOME shelf until the home market
            // bids, but a haul goes only where someone wants the good, so held
            // stock may be hauled. What a seller may haul is therefore what it
            // would list at home PLUS any opening stock held there.
            auto rit = memo.find({corp_id, src_key});
            if (rit == memo.end())
                rit = memo.emplace(std::make_pair(corp_id, src_key),
                                   auto_surplus_reservation(w, reg, corp_id, src_key)).first;
            const std::array<float, resource_count> reserve = rit->second;

            for (std::size_t ri = 0; ri < resource_count; ++ri)
            {
                // BL-708: a grid good is never cargo (price_convoy_leg refuses
                // it too; skipped here before any leg is priced).
                if (grid_rules.grid(ri))
                    continue;

                // Live pool read: an earlier commit in this pass (a launch's
                // propellant burn) may have drawn this pool down. THIS TICK'S
                // DELIVERIES into the pool are not shippable: SUPPLY.md — a
                // delivery reaches its destination's clear before it can move
                // again (world::arrived_this_tick).
                const float pool_qty = w.pool_at(corp_id, src_key).quantities[ri];
                const float surplus =
                    pool_qty - reserve[ri] - dispatch_arrived(w, corp_id, src_key, ri);
                if (!(surplus > 0.0f))
                    continue;

                // BL-995 CALL: the HOME price is 0 where the goods cannot be
                // sold at home at all — a market-less body's body-level pool,
                // or a home market that does not price the good (auto-surplus
                // lists neither). Any positive net price then beats home.
                //
                // BL-1229: under a standing sell order the home price the rule
                // weighs is max(home, floor) — the order accepts no sale below
                // its floor, so a haul must beat the floor as it beats home, and
                // `landed` below (the room's aim) is floor + haul.
                const float price_src = dispatch_source_price(w, order_floor, corp_id, src_key, ri);
                const float gate      = price_src + margin * price_src;

                // Every destination that clears the margin, with its net price.
                // The per-unit haul is priced on a ONE-UNIT leg (cost is linear
                // in quantity, so this is exact); pricing the choice at the full
                // surplus would let a space leg carrying propellant be refused
                // for burning the fuel it is exporting (cold review #10).
                const float probe_qty = std::min(surplus, 1.0f);
                cands.clear();
                for (const entity_id dest_id : market_ids)
                {
                    if (dest_id == src_key)
                        continue;
                    const market_component& dm = w.markets.at(dest_id);
                    const float price_d = dispatch_market_price(dm, ri);
                    const bool  tf_rec  = tf_player && tf_short(dest_id, ri);
                    // net(d) <= price_d (a haul is never negative), so a
                    // destination whose GROSS price cannot clear the gate cannot
                    // clear it net: skip it before routing a leg.
                    if (!(price_d > gate))
                    {
                        if (tf_rec)
                            tf_note(dest_id, ri, trade_refusal::gate);
                        continue;
                    }
                    const convoy_leg leg = price_convoy_leg(
                        w, reg, nodes, corp_id, src_key, dest_id, ri, probe_qty,
                        logistics_cost_space,
                        dm.body == src_body ? pool_origin() : nullptr);
                    if (!leg.viable)
                    {
                        if (tf_rec)
                            tf_note(dest_id, ri, dm.body == src_body ? trade_refusal::no_route
                                                                     : trade_refusal::no_lane);
                        continue; // unroutable / unpadded / unfuelled / same market
                    }
                    const float haul = leg.cost / probe_qty;
                    const float net  = price_d - haul;
                    // The margin gate: net(d) - price_src > margin x price_src.
                    // With no home price this reduces to net(d) > 0.
                    if (!(net - price_src > margin * price_src))
                    {
                        if (tf_rec)
                            tf_note(dest_id, ri, trade_refusal::costly);
                        continue;
                    }
                    cands.push_back({net, dest_id, haul, leg.mode == convoy_mode::space, price_d});
                }
                if (cands.empty())
                    continue;
                // argmax net(d), ties to the lower market id — a total order.
                std::sort(cands.begin(), cands.end(), [](const candidate& a, const candidate& b) {
                    return (a.net != b.net) ? (a.net > b.net) : (a.dest < b.dest);
                });

                // BL-995 CALL: the send goes to the best-net destination that
                // still has ROOM (absorbable less what is already pending there).
                // A destination the rest of the market has already filled cannot
                // take the cargo at a price above its landed cost, so the next-
                // best destination that still beats home by the margin takes it,
                // rather than the cargo defaulting to a home sale.
                std::size_t tf_tried = 0; // BL-1222: candidates the loop below reached
                for (const candidate& c : cands)
                {
                    ++tf_tried;
                    const bool  tf_rec = tf_player && tf_short(c.dest, ri);
                    const float landed = price_src + c.haul;
                    const float room =
                        dispatch_room(w, reg, c.dest, ri, landed, corp_ids, memo);
                    float qty = std::min(surplus, room);
                    // A space launch burns its propellant from this same pool:
                    // when the cargo IS the propellant, leave the launch's draw.
                    if (c.space && launch_draw[ri] > 0.0f)
                        qty = std::min(qty, pool_qty - launch_draw[ri]);
                    if (!(qty > 0.0f) || !std::isfinite(qty))
                    {
                        // BL-1222: the propellant clamp is the source's own limit
                        // and ranks below the destination's room, so it is named
                        // whenever it zeroes the send, room or none.
                        if (tf_rec)
                            tf_note(c.dest, ri,
                                    (c.space && launch_draw[ri] > 0.0f
                                     && !(pool_qty - launch_draw[ri] > 0.0f))
                                        ? trade_refusal::no_propellant
                                        : trade_refusal::no_room);
                        continue;
                    }

                    // Re-price the committed leg at its real quantity.
                    const convoy_leg leg = price_convoy_leg(
                        w, reg, nodes, corp_id, src_key, c.dest, ri, qty,
                        logistics_cost_space, c.space ? nullptr : pool_origin());
                    if (!leg.viable)
                    {
                        if (tf_rec)
                            tf_note(c.dest, ri, w.markets.at(c.dest).body == src_body
                                                    ? trade_refusal::no_route
                                                    : trade_refusal::no_lane);
                        continue;
                    }

                    // Commit through the shared path: the solvency gate, the
                    // passive-LP gate (BL-597), the pool debit, the propellant
                    // burn and the convoy itself all live there, so a rival's
                    // convoy and the player's are the same object built by the
                    // same code.
                    // BL-1186 E1: the auto-dispatch sends what the passive-LP
                    // cap admits, rather than refusing a cargo above it whole.
                    bool  refused_no_lp = false;
                    float sent          = 0.0f;
                    if (commit_convoy(w, reg, corp_id, src_body,
                                      src_is_market ? src_key : null_entity,
                                      c.dest, ri, qty, leg, &pools_by_body, &refused_no_lp,
                                      /*allow_partial_lp=*/true, &sent))
                    {
                        ++out.dispatched;
                        if (sent < qty)
                            ++out.trimmed_by_lp;
                        // BL-1229 review: a hauled pool keeps its order.
                        if (order_floor.count({corp_id, src_body, ri}) != 0)
                            w.hauled_ordered_this_tick.insert({corp_id, src_body, ri});
                        if (tf_player)
                        {
                            tf.shipments.push_back({src_key, c.dest,
                                                    static_cast<std::uint16_t>(ri), sent,
                                                    c.price_d});
                            if (tf_rec)
                                tf_note(c.dest, ri, trade_refusal::sent);
                        }
                    }
                    else
                    {
                        if (refused_no_lp)
                            ++out.refused_no_lp;
                        // Refused at commit: the LP cap (`room`, held back), or
                        // otherwise solvency — with a viable leg and a known
                        // corp, commit_convoy's only other refusal.
                        if (tf_rec)
                            tf_note(c.dest, ri, refused_no_lp ? trade_refusal::room
                                                              : trade_refusal::no_funds);
                    }
                    break; // one destination per (pool, good) per pass
                }

                // BL-1222: the candidates the one-destination rule never reached.
                // They cleared the margin; whether they had room is one extra
                // `dispatch_room` read each — a pure query (its memo is a cache
                // of values this pass never changes), paid for the player only,
                // and only for a short destination. The propellant clamp is read
                // as the loop above reads it; solvency is not knowable without
                // committing, so an unreached candidate is never `no funds`.
                if (tf_player)
                    for (std::size_t k = tf_tried; k < cands.size(); ++k)
                    {
                        const candidate& c = cands[k];
                        if (!tf_short(c.dest, ri))
                            continue;
                        if (c.space && launch_draw[ri] > 0.0f
                            && !(pool_qty - launch_draw[ri] > 0.0f))
                        {
                            tf_note(c.dest, ri, trade_refusal::no_propellant);
                            continue;
                        }
                        const float room = dispatch_room(w, reg, c.dest, ri, price_src + c.haul,
                                                         corp_ids, memo);
                        tf_note(c.dest, ri,
                                room > 0.0f ? trade_refusal::room : trade_refusal::no_room);
                    }
            }
        }
    }

    // BL-1071 — A MARKET EXPORTS ITS OWN SHELF, after every corporation has
    // claimed the room it wanted (SUPPLY.md § Dispatch trigger).
    export_market_shelves(w, reg, nodes, corp_ids, market_ids, memo, &pools_by_body, out);

    // BL-1222: roll the player's record into the lens's trailing window.
    w.player_trade_flow.push_back(std::move(tf));
    while (w.player_trade_flow.size() > world::trade_flow_window)
        w.player_trade_flow.erase(w.player_trade_flow.begin());

    (void)logistics_cost_land; // intra-body reads reg.logistics_cost(land/sea) directly; this
                               // param is retained for caller/signature stability.
    return out;
}
