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
        // BL-1195: where this tick's travel starts — interdiction sweeps from here
        // to the new head. A held convoy does not move, so its sweep is its head.
        convoy.progress_before = convoy.progress;
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
    // BL-1195: this tick's sweep starts are spent on EVERY return, early or not —
    // a later read in the same tick (or a tick that does not advance) checks the
    // head alone (the components.hpp invariant on progress_before). A scope guard,
    // so no early return below can skip it; it runs after the cut convoys are
    // erased, which is harmless (the reset touches only survivors).
    struct sweep_reset
    {
        world& w;
        ~sweep_reset()
        {
            for (convoy_component& cv : w.convoys)
                cv.progress_before = -1.0f;
        }
    } const reset_on_exit{w};

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
        // BL-1195: THE SWEEP. A tick is 90 days and most hauls take one to three,
        // so the cargo crosses most of its lane between two reads. Every tile it
        // crossed this tick — from where it stood before advance_convoys to where
        // it stands now, in lane order, both ends included — is checked, and the
        // first holding a hostile unit is where it is cut. Position is read off the
        // lane's clock (convoy_lane_index), each leg at its own speed.
        const convoy_route lane = convoy_route_tiles(w, cv);
        const int head = convoy_lane_index(lane.at, cv.progress);
        if (head < 0)
            continue; // inter-body leg in transit, or an unresolvable lane
        const int from = (cv.progress_before >= 0.0f)
                             ? std::min(convoy_lane_index(lane.at, cv.progress_before), head)
                             : head;

        entity_id tile             = null_entity;
        entity_id interceptor_unit = null_entity;
        entity_id interceptor_corp = null_entity;
        for (int li = from; li <= head && interceptor_unit == null_entity; ++li)
        {
        tile = lane.tiles[static_cast<std::size_t>(li)];
        const auto occ = units_on_tile.find(tile);
        if (occ == units_on_tile.end())
            continue;
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
        // BL-1265: the captured cargo LANDS on the shelf of the market whose
        // catchment holds the interception tile — where it physically is — and,
        // landing being selling, the interceptor is paid it at that tick's
        // clearing price. No market under the tile: nowhere to land, destroyed.
        const entity_id capture_market = (rec.body != null_entity) ? market_for_tile(w, tile) : null_entity;
        const bool creditable = capture_market != null_entity &&
                                w.corporations.find(interceptor_corp) != w.corporations.end() &&
                                std::isfinite(cv.cargo_qty);
        if (creditable)
        {
            w.land_goods(interceptor_corp, capture_market,
                         static_cast<std::size_t>(cv.cargo_resource), cv.cargo_qty);
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

        // BL-1265 (TRADE.md § A trade, step 3): the cargo LANDS on the
        // destination market and is sold there at this tick's clear — landing
        // is selling; the trader is paid the quantity at the clearing price.
        // Deliberately NO direct write into market supply here: the clear lists
        // every landing as this tick's supply itself (`world::landed_this_tick`).
        // A cargo with no corporation behind it lands on the shelf unpaid.
        w.land_goods(convoy.corp, convoy.dest_market,
                     static_cast<std::size_t>(convoy.cargo_resource), convoy.cargo_qty);
        if (convoy.corp == null_entity)
            continue; // no trade route: routes are a corporation's record

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

} // namespace

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

namespace {

/// BL-1265 (TRADE.md § A trade, "Between bodies"): the stock of the launch-drawn
/// good `dr` a launch of `cargo_qty` units of `ri` can BUY off the source
/// market's shelf — what the shelf holds, where the fair-price ceiling admits
/// it — less the cargo itself when the cargo IS the drawn good (a launch cannot
/// burn the propellant it is exporting). 0 where the ceiling refuses the shelf.
float launch_shelf_available(const world& w, const recipe_registry& reg, entity_id src_market,
                             std::size_t dr, std::size_t ri, float cargo_qty)
{
    const auto mit = w.markets.find(src_market);
    if (mit == w.markets.end())
        return 0.0f;
    if (!shelf_admits(mit->second, dr, reg.price_band().reservation_mult, /*off_buys=*/true))
        return 0.0f;
    float avail = std::max(0.0f, mit->second.inventory[dr]);
    if (ri == dr)
        avail -= cargo_qty;
    return avail;
}

} // namespace

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
    entity_id   port_a       = null_entity; ///< BL-1195: the loading Port of a sea route
    entity_id   port_b       = null_entity; ///< BL-1195: the unloading Port of a sea route
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
                route.port_a  = ports[i];
                route.port_b  = ports[j];
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
    leg.port_a       = route.n_legs == 3 ? route.port_a : null_entity;
    leg.port_b       = route.n_legs == 3 ? route.port_b : null_entity;
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

convoy_leg price_trade_leg(world& w, const recipe_registry& reg,
                           const logistics_nodes& nodes, entity_id corp_id,
                           entity_id src_market, entity_id dest_market_id,
                           std::size_t ri, float qty)
{
    convoy_leg leg;

    const auto cit = w.corporations.find(corp_id);
    if (cit == w.corporations.end())
        return leg;
    // BL-1265: a trade moves goods from one market's shelf to another's.
    // Moving goods into the market they already sit on is not a haul.
    const auto sit = w.markets.find(src_market);
    const auto mit = w.markets.find(dest_market_id);
    if (sit == w.markets.end() || mit == w.markets.end() || src_market == dest_market_id)
        return leg;
    const entity_id src_body = sit->second.body;
    if (src_body == null_entity)
        return leg;
    if (ri >= resource_count)
        return leg;
    // BL-708 — A GRID GOOD IS NEVER CARGO (docs/economy/PRODUCTION.md § Power;
    // docs/economy/LOGISTICS.md § 3a). Power has a price and clears like any
    // other good, but transmission IS the road network: there is no convoy to
    // load and no leg to price. Refused HERE, at the one shared seam every
    // trade prices through.
    if (reg.grid_goods().grid(ri))
        return leg;
    // A non-finite quantity would make every comparison below meaningless and
    // the cost NaN; refuse it here so every caller refuses it identically.
    if (!std::isfinite(qty) || !(qty > 0.0f))
        return leg;

    const corporation_component& corp        = cit->second;
    const market_component&      dest_market = mit->second;
    const entity_id              dest_body   = dest_market.body;

    if (src_body == dest_body)
    {
        // Intra-body (BL-077, BL-1186): the source shelf's goods leave from its
        // market's centre for the destination market's centre — overland, or
        // land -> port -> sea -> port -> land where that route is cheaper or the
        // only one (route_intra_body). Corporation-independent: the same leg
        // `price_market_leg` prices.
        leg = price_market_leg(w, reg, nodes, src_market, dest_market_id, qty);
        return leg;
    }

    // Inter-body (TRADE.md § A trade, "Between bodies"): the space lane,
    // launchpad-gated — the trader must hold a pad on the source body.
    if (!corp_has_launchpad_on(w, corp, src_body))
        return leg;
    // BL-308 / BL-1265: the pad also has to be FUELLED. A launch burns
    // `launch_draw_per_convoy()`, which the trader BUYS off the source shelf;
    // without it on the shelf (under the ceiling) the lane is shut exactly as if
    // no pad existed. Ascending resource index: a fixed order.
    {
        const auto& launch_draw = launch_draw_per_convoy();
        for (std::size_t dr = 0; dr < resource_count; ++dr)
            if (launch_draw[dr] > 0.0f
                && launch_shelf_available(w, reg, src_market, dr, ri, qty) < launch_draw[dr])
                return leg;
    }
    // BL-354: evaluated at the tick-pure angle, so sourcing, pricing and
    // convoy speed are a pure function of tick, never of frame rate.
    const float dist      = body_distance_au(w, src_body, dest_body, w.current_day_tick);
    const float unit_cost = reg.logistics_cost(convoy_mode::space);
    // The space lane keeps its own calibration — roughly one econ tick per AU.
    const int travel_ticks = (dist > 1.0f) ? static_cast<int>(dist + 0.999f) : 1;
    return finish_leg(convoy_mode::space, unit_cost, dist, qty, 0.0f, travel_ticks);
}

convoy_leg price_market_leg(world& w, const recipe_registry& reg,
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
        return convoy_leg{}; // same body only: a space lane is the trader's (pad, propellant)
    // The haul leaves from the market's own centre — where the shelf stands.
    // Unanchored: no road to leave by. BL-1186: the same router every leg takes
    // (route_intra_body), so a pair this answers viable for is one a convoy can
    // actually run.
    intra_route route;
    if (!route_intra_body(w, reg, nodes, body, sit->second.centre_tile,
                          dit->second.centre_tile, route))
        return convoy_leg{};
    convoy_leg leg = finish_route(route, qty, reg.port_handling());
    leg.origin_tile = sit->second.centre_tile; // BL-1195: the lane starts at the centre
    return leg;
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

bool commit_trade_shipment(world& w, const recipe_registry& reg, economy_report& report,
                           entity_id corp_id, entity_id src_market, entity_id dest_market_id,
                           std::size_t ri, float qty, const convoy_leg& leg,
                           lp_pool_map* shared_lp_pools, bool* out_refused_no_lp,
                           float* out_sent)
{
    if (!leg.viable || ri >= resource_count || !std::isfinite(qty) || !(qty > 0.0f))
        return false;
    const auto cit = w.corporations.find(corp_id);
    if (cit == w.corporations.end())
        return false;
    const auto sit = w.markets.find(src_market);
    if (sit == w.markets.end() || w.markets.find(dest_market_id) == w.markets.end())
        return false;
    corporation_component& corp = cit->second;
    market_component&      src  = sit->second;
    const entity_id src_body    = src.body;
    const float res_mult        = reg.price_band().reservation_mult;

    // TRADE.md § A trade, step 1: IT BUYS at the source shelf, at its posted
    // price, as any buyer does — under the fair-price ceiling (Ben, 2026-10-10).
    // A shelf the ceiling refuses sells nothing; a short shelf sells what it
    // holds. A space launch's own propellant is bought from the same shelf, so
    // when the cargo IS that good the launch's draw is left behind.
    if (!shelf_admits(src, ri, res_mult, /*off_buys=*/true))
        return false;
    const auto& launch_draw = launch_draw_per_convoy();
    float on_shelf = std::max(0.0f, src.inventory[ri]);
    if (leg.mode == convoy_mode::space && launch_draw[ri] > 0.0f)
        on_shelf -= launch_draw[ri];
    float send = std::min(qty, on_shelf);
    if (!(send > 0.0f))
        return false;

    // BL-597: the passive-LP admissibility gate, before any mutation. THE DRAW IS
    // CARGO QUANTITY, NOT DISTANCE (Ben, 2026-08-25, ruling on NR-620): the
    // anchor passes so many units of goods per tick, and credits remain the sole
    // price of distance. Trade points are the OWNER's capacity; Logistic Points
    // are the PLACE's, and a shipment passes both (TRADE.md § The Planetary
    // Marketplace). The cap TRIMS a shipment to what the anchor still admits
    // (BL-1186 E1); space legs have no tile path and are out of its scope.
    float* slot = nullptr;
    if (leg.mode != convoy_mode::space)
    {
        lp_pool_map local_pools;
        slot = passive_lp_slot(w, reg, src_body, src.centre_tile,
                               shared_lp_pools ? *shared_lp_pools : local_pools);
        send = slot ? passive_lp_grant(*slot, send, /*allow_partial=*/true) : 0.0f;
        if (!(send > 0.0f))
        {
            if (out_refused_no_lp)
                *out_refused_no_lp = true;
            return false;
        }
        if (shared_lp_pools == nullptr)
            slot = nullptr; // a private pool: nothing outlives this call to draw down
    }

    // Cost is linear in quantity (rate x distance x qty, handling x qty), so a
    // trimmed cargo pays its share of the priced leg and no more.
    const float haul     = (send < qty) ? leg.cost * (send / qty) : leg.cost;
    const float price_a  = posted_price(src, ri);
    float       purchase = send * price_a;
    if (leg.mode == convoy_mode::space)
        for (std::size_t dr = 0; dr < resource_count; ++dr)
            if (launch_draw[dr] > 0.0f)
                purchase += launch_draw[dr] * posted_price(src, dr);
    // The solvency gate, in ONE place for every trade: the trader pays the haul
    // now and the purchase at this tick's clear, and must be able to cover both.
    if (!(corp.balance >= haul + purchase))
        return false;

    // Granted: consume the anchor's capacity (LP is the CAP, not a second PRICE).
    if (slot != nullptr)
        *slot -= send;

    // The purchase: off the shelf now, billed at the posted price by the clear
    // (`report.purchases`), and posted as this tick's want on the source market
    // (`report.wants`) — a trade's buying reads to the price as any bid does.
    src.inventory[ri] = std::max(0.0f, src.inventory[ri] - send);
    report.purchases[std::make_pair(corp_id, src_market)][ri] += send;
    report.wants[std::make_pair(corp_id, src_market)][ri]     += send;
    // BL-308: the launch's propellant, once per launch, bought the same way.
    if (leg.mode == convoy_mode::space)
        for (std::size_t dr = 0; dr < resource_count; ++dr)
            if (launch_draw[dr] > 0.0f)
            {
                src.inventory[dr] = std::max(0.0f, src.inventory[dr] - launch_draw[dr]);
                report.purchases[std::make_pair(corp_id, src_market)][dr] += launch_draw[dr];
                report.wants[std::make_pair(corp_id, src_market)][dr]     += launch_draw[dr];
            }

    // TRADE.md § A trade, step 2: IT PAYS THE HAUL, and the goods travel as a
    // convoy for the leg's travel time (SUPPLY.md).
    corp.balance -= haul;

    convoy_component c;
    c.id             = w.allocate_convoy_id();
    c.source_market  = src_market;
    c.dest_market    = dest_market_id;
    c.mode           = leg.mode;
    c.cargo_resource = static_cast<resource_type>(ri);
    c.cargo_qty      = send;
    c.progress       = 0.0f;
    // Speed is progress-per-tick, so a leg taking N ticks advances 1/N each tick.
    c.speed          = 1.0f / static_cast<float>(leg.travel_ticks);
    c.corp           = corp_id;
    c.arrived        = false;
    c.held           = false;
    c.cost_paid      = haul;
    c.origin_tile    = leg.origin_tile; // BL-1195: the lane follows the priced legs
    c.port_a         = leg.port_a;
    c.port_b         = leg.port_b;
    w.convoys.push_back(c);
    if (out_sent)
        *out_sent = send;
    return true;
}

// ---------------------------------------------------------------------------
// Trade sizing (TRADE.md § Auto and reserved trade)
// ---------------------------------------------------------------------------

float dispatch_market_price(const market_component& mc, std::size_t r)
{
    if (r >= resource_count || mc.base_price[r] <= 0.0f)
        return 0.0f;
    return (mc.price[r] > 0.0f) ? mc.price[r] : mc.base_price[r];
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

float trade_pending(const world& w, entity_id dest, std::size_t r)
{
    if (r >= resource_count)
        return 0.0f;
    // Every owner's cargo bound for `dest` — held convoys included (their cargo
    // is committed; a hold pauses it, it does not un-send it), and shipments
    // committed earlier in THIS pass included (commit_trade_shipment appends to
    // w.convoys before the next decision is sized). Vector order: fixed.
    float pending = 0.0f;
    for (const convoy_component& c : w.convoys)
        if (c.dest_market == dest && static_cast<std::size_t>(c.cargo_resource) == r)
            pending += c.cargo_qty;
    // Plus this tick's landings on `dest` — production and arrivals, which
    // list at the next clear. A std::map: ascending owner, a fixed order.
    for (const auto& [key, landed] : w.landed_this_tick)
        if (key.second == dest)
            pending += landed.quantities[r];
    return pending;
}

float trade_room(const world& w, const recipe_registry& reg, entity_id dest, std::size_t r,
                 float landed_cost)
{
    const float absorb = dispatch_absorbable(w, reg, dest, r, landed_cost);
    if (!(absorb > 0.0f))
        return 0.0f;
    return std::max(0.0f, absorb - trade_pending(w, dest, r));
}
