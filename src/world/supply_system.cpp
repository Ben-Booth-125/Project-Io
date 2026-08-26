#include "supply_system.hpp"

#include "logistics.hpp"
#include "orbital_system.hpp"
#include "stance.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <string>
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
            w.pool_for(interceptor_corp, rec.body).quantities[
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
    std::vector<interception_record> cuts = intercept_convoys(w, tick);
    if (out_cuts != nullptr)
        out_cuts->insert(out_cuts->end(), cuts.begin(), cuts.end());

    // Credit in insertion order, then erase in one sweep.
    for (const auto& convoy : w.convoys)
    {
        if (!convoy.arrived)
            continue;

        // Find the destination market's body so we can credit the (corp, body) pool.
        const auto mit = w.markets.find(convoy.dest_market);
        if (mit == w.markets.end())
            continue;
        const entity_id dest_body = mit->second.body;

        // Credit the destination pool. The credit is the whole delivery —
        // deliberately NO direct write into market supply here. This runs after
        // clear_markets in the tick, and clear_markets zeroes the supply/demand
        // arrays at its top, so a supply += here would be erased before pricing
        // ever read it — yet it WOULD be read pre-clearing by run_economy_step's
        // consumers (the AI scorer's glut forecast among them): a private signal
        // nothing priced ever agreed with. The cargo reaches pricing through the
        // ordinary auto-surplus path off this pool at the next clear (BL-382).
        w.pool_for(convoy.corp, dest_body).quantities[
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
/// units of `ri` off `body` — its on-body stockpile, minus the cargo itself when
/// the cargo IS the drawn good (a launch cannot burn the propellant it is
/// exporting).
float launch_draw_available(const world& w, entity_id corp, entity_id body,
                            std::size_t dr, std::size_t ri, float cargo_qty)
{
    const auto pit = w.corp_body_pools.find({corp, body});
    if (pit == w.corp_body_pools.end())
        return 0.0f;
    float avail = pit->second.quantities[dr];
    if (ri == dr)
        avail -= cargo_qty;
    return avail;
}

/// Tile of the corp's lowest-id building on `body` — its production anchor, used as the
/// intra-body haul origin (BL-077). Mirrors market_clearing's representative_tile, kept local
/// here to avoid a supply_system -> market_clearing link dependency. null_entity if the corp
/// holds nothing on the body.
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

/// True when an active (built AND non-decommissioned) Port sits on `tile` —
/// the sea-mode endpoint gate (BL-608, SUPPLY.md § Infrastructure gates:
/// "Port building at both endpoints"). Ownership-agnostic like
/// `is_supply_anchor` (logistics.cpp): a Port is body infrastructure, not a
/// corp asset check — unlike `corp_has_launchpad_on` above, which is
/// deliberately per-corp because a launchpad IS the corp's own pad. Same
/// built+active test `is_supply_anchor` and `collect_logistics_nodes` already
/// use for port/hub anchors, narrowed to Port only (a hub does not gate sea).
bool tile_has_active_port(const world& w, entity_id tile)
{
    if (tile == null_entity)
        return false;
    for (const auto& [bid, bc] : w.buildings)
    {
        (void)bid;
        if (bc.tile == tile && bc.type == building_type::port
            && bc.ticks_remaining <= 0 && !bc.decommissioned)
            return true;
    }
    return false;
}

/// Find the market entity for a given body — the lowest-id one, so the pick is
/// stable when a body hosts several markets (w.markets is an unordered_map; the
/// first hit would inherit hash layout). Returns null_entity if none exists.
entity_id market_for_body(const world& w, entity_id body)
{
    entity_id best = null_entity;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == body && (best == null_entity || mid < best))
            best = mid;
    return best;
}

/// Fraction in [0, cap] to discount an intra-body haul cost by — summed over the
/// population-centre (scale-weighted) and hub (flat) tiles the path crosses, capped.
/// Deterministic: a pure function of the path tiles and the node sets.
float node_discount_fraction(const logistics_path& path, const logistics_nodes& nodes,
                             const logistics_node_params& np)
{
    float disc = 0.0f;
    for (const entity_id t : path.tiles)
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
                            entity_id src_body, entity_id dest_market_id,
                            std::size_t ri, float qty, float logistics_cost_space)
{
    convoy_leg leg;

    const auto cit = w.corporations.find(corp_id);
    if (cit == w.corporations.end())
        return leg;
    const auto mit = w.markets.find(dest_market_id);
    if (mit == w.markets.end())
        return leg;
    if (ri >= resource_count)
        return leg;
    // A non-finite quantity would make every comparison below meaningless and
    // the cost NaN; refuse it here so both callers refuse it identically.
    if (!std::isfinite(qty) || !(qty > 0.0f))
        return leg;

    const corporation_component& corp        = cit->second;
    const market_component&      dest_market = mit->second;
    const entity_id              dest_body   = dest_market.body;
    const logistics_node_params& node_params = reg.logistics_nodes();

    convoy_mode mode;
    float       dist;
    float       unit_cost;
    int         travel_ticks  = 1;
    float       node_discount = 0.0f; // BL-148/149: intra-body city/hub discount.
    if (src_body == dest_body)
    {
        // Intra-body (BL-077): haul the corp's on-body stockpile from its
        // representative tile to the short market's centre, terrain-weighted
        // over the tile grid (land, or sea when the path must cross water).
        const entity_id origin      = corp_representative_tile(w, corp, src_body);
        const entity_id dest_centre = dest_market.centre_tile;
        if (origin == null_entity || dest_centre == null_entity)
            return leg; // no production anchor / unanchored market: cannot route
        const logistics_path& path = intra_body_path(w, src_body, origin, dest_centre);
        if (!path.reachable)
            return leg;
        if (path.crosses_ocean)
        {
            // BL-608: sea mode is gated on an active Port at BOTH endpoints
            // (SUPPLY.md § Infrastructure gates). The cheapest A* path already
            // crosses water — there is no cheaper land-only route BL-522's
            // per-leg pricing would recover — so an ungated pair is refused
            // outright rather than silently re-priced at the land rate: a
            // cart cannot be billed for a lane it cannot physically cross.
            // `leg` is still default-constructed (viable == false) here, so
            // this mutates nothing; the caller's existing `!leg.viable` path
            // (corp_command.cpp: dispatch_convoy -> rejected_placement) surfaces it.
            if (!tile_has_active_port(w, origin) || !tile_has_active_port(w, dest_centre))
                return leg;
            mode = convoy_mode::sea;
        }
        else
        {
            mode = convoy_mode::land;
        }
        dist      = path.cost;
        unit_cost = reg.logistics_cost(mode);
        // Distance now costs TIME as well as money (Ben, 2026-08-12). Computed
        // here because `path` is a reference into the A* cache and must be read
        // before anything can invalidate it.
        travel_ticks = convoy_travel_ticks(w, src_body, path);
        // BL-148/149: discount the haul for the cities + hubs its path crosses.
        node_discount = node_discount_fraction(path, nodes, node_params);
    }
    else
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
                    && launch_draw_available(w, corp_id, src_body, dr, ri, qty) < launch_draw[dr])
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

bool commit_convoy(world& w, const recipe_registry& reg, entity_id corp_id, entity_id src_body,
                   entity_id src_market, entity_id dest_market_id,
                   std::size_t ri, float qty, const convoy_leg& leg,
                   lp_pool_map* shared_lp_pools, bool* out_refused_no_lp)
{
    if (!leg.viable || ri >= resource_count)
        return false;
    const auto cit = w.corporations.find(corp_id);
    if (cit == w.corporations.end())
        return false;
    corporation_component& corp = cit->second;
    if (corp.balance < leg.cost)
        return false; // the solvency gate, in ONE place for both callers

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
    if (leg.mode != convoy_mode::space)
    {
        lp_pool_map local_pools;
        lp_pool_map& pools_by_body = shared_lp_pools ? *shared_lp_pools : local_pools;
        const military_capability_params& mil = reg.military();
        std::unordered_map<entity_id, float>& pools =
            lp_pool_for_body(pools_by_body, w, src_body, mil.active_lp_per_anchor_tick);

        // Same locus as price_convoy_leg's own origin — the corp's
        // representative (lowest-id building) tile on the source body, the
        // convoy's actual dispatch point.
        const entity_id origin = corp_representative_tile(w, corp, src_body);
        const entity_id nearest_anchor =
            (origin != null_entity) ? nearest_lp_anchor(w, src_body, origin, pools) : null_entity;

        if (nearest_anchor == null_entity)
        {
            // No reachable anchor on this body at all — no passive LP exists
            // to draw against.
            if (out_refused_no_lp)
                *out_refused_no_lp = true;
            return false;
        }

        float& pool = pools.at(nearest_anchor);
        if (pool + 1e-6f < qty)
        {
            // The nearest anchor's pool is already exhausted (by a
            // higher-priority draw earlier this tick — possibly THIS SAME
            // TICK'S active march pass, if `shared_lp_pools` ties the two
            // together — or simply too small).
            if (out_refused_no_lp)
                *out_refused_no_lp = true;
            return false;
        }

        // Granted: consume the anchor's pool. LP is the CAP, not a second
        // PRICE (LOGISTICS.md rule 1) — `leg.cost` (already debited below)
        // is the only credit charge a passive draw pays; unlike BL-596's
        // active draw, there is no separate LP-specific credit line here.
        pool -= qty;
    }

    // Debit cost and source pool; create the convoy.
    corp.balance -= leg.cost;
    w.pool_for(corp_id, src_body).quantities[ri] -= qty;

    // BL-308: burn the launch's draw. Charged once per launch (not per unit,
    // not per AU) and only on the space lane; price_convoy_leg's availability
    // gate already ran against this same pool and the same vector, so this
    // cannot drive it negative. Ascending resource index — the same
    // determinism discipline the gate above uses.
    if (leg.mode == convoy_mode::space)
    {
        auto&       quantities  = w.pool_for(corp_id, src_body).quantities;
        const auto& launch_draw = launch_draw_per_convoy();
        for (std::size_t dr = 0; dr < resource_count; ++dr)
            if (launch_draw[dr] > 0.0f)
                quantities[dr] -= launch_draw[dr];
    }

    convoy_component c;
    c.id             = w.allocate_convoy_id();
    c.source_market  = src_market;
    c.dest_market    = dest_market_id;
    c.mode           = leg.mode;
    c.cargo_resource = static_cast<resource_type>(ri);
    c.cargo_qty      = qty;
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
    c.cost_paid      = leg.cost;
    w.convoys.push_back(c);
    return true;
}

convoy_dispatch_tick dispatch_convoys(world& w, const recipe_registry& reg,
                      float logistics_cost_land, float logistics_cost_space,
                      lp_pool_map* shared_lp_pools)
{
    convoy_dispatch_tick out;

    // BL-597: this pass's passive-LP pools. Local (and so shared across
    // every convoy THIS call commits) when the caller did not hand us a
    // shared instance — see this function's own doc comment.
    lp_pool_map local_pools;
    lp_pool_map& pools_by_body = shared_lp_pools ? *shared_lp_pools : local_pools;

    // One dispatch pass per (corp, dest_body, resource) shortfall.
    // Shortfall = market demand exceeded supply in the last clearing pass.
    // We fix quantities at a single batch = shortfall amount, capped by source surplus.

    // BL-148/149: build the logistics-node lookups once — cities (population centres) and the
    // player's inland logistics hubs discount any intra-body haul whose A* path crosses them.
    const logistics_nodes nodes = collect_logistics_nodes(w);

    // BL-354 note: inter-body distances are evaluated inside price_convoy_leg at
    // w.current_day_tick via the tick-pure angle, so sourcing, pricing and convoy
    // speed are a pure function of tick. current_day_tick is set by every tick
    // path (app + main) before dispatch runs.

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

    for (const entity_id corp_id : corp_ids)
    {
        // Iterate markets (not corp pools) so we catch shortfalls even on bodies
        // where the corp has no existing pool entry.
        for (const entity_id dest_market_id : market_ids)
        {
            const market_component& dest_market = w.markets.at(dest_market_id);

            for (std::size_t ri = 0; ri < resource_count; ++ri)
            {
                const float shortfall = dest_market.demand[ri] - dest_market.supply[ri];
                if (shortfall <= 0.0f)
                    continue;

                // Find the cheapest reachable source. The SHORTFALL SCAN is
                // this function's own contribution (BL-452): everything below
                // the winner — pricing the leg and committing the cargo — is
                // the shared dispatch the player's `dispatch_convoy` verb calls
                // with this scan removed, so the two cannot drift apart.
                entity_id  best_src_body = null_entity;
                float      best_qty      = 0.0f;
                convoy_leg best_leg;
                best_leg.cost = std::numeric_limits<float>::max();

                for (auto& [src_key, src_pool] : w.corp_body_pools)
                {
                    if (src_key.first != corp_id)
                        continue;
                    const entity_id src_body = src_key.second;

                    const float surplus = src_pool.quantities[ri];
                    if (surplus <= 0.0f)
                        continue;
                    const float qty = std::min(surplus, shortfall);

                    const convoy_leg leg = price_convoy_leg(
                        w, reg, nodes, corp_id, src_body, dest_market_id, ri, qty,
                        logistics_cost_space);
                    if (!leg.viable)
                        continue; // unroutable / unpadded / unfuelled lane
                    if (leg.cost < best_leg.cost)
                    {
                        best_src_body = src_body;
                        best_qty      = qty;
                        best_leg      = leg;
                    }
                }

                if (best_src_body == null_entity)
                    continue;

                // Commit through the shared path: the solvency gate, the
                // passive-LP gate (BL-597), the pool debit, the propellant
                // burn and the convoy itself all live there, so a rival's
                // convoy and the player's are the same object built by the
                // same code.
                bool refused_no_lp = false;
                if (commit_convoy(w, reg, corp_id, best_src_body, market_for_body(w, best_src_body),
                              dest_market_id, ri, best_qty, best_leg, &pools_by_body, &refused_no_lp))
                    ++out.dispatched;
                else if (refused_no_lp)
                    ++out.refused_no_lp;
            }
        }
    }
    (void)logistics_cost_land; // intra-body reads reg.logistics_cost(land/sea) directly; this
                               // param is retained for caller/signature stability.
    return out;
}
