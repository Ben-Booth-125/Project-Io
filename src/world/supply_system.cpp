#include "supply_system.hpp"

#include "logistics.hpp"
#include "orbital_system.hpp"

#include <algorithm>
#include <cmath>
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
        if (convoy.arrived)
            continue;
        convoy.progress += convoy.speed;
        if (convoy.progress >= 1.0f)
        {
            convoy.progress = 1.0f;
            convoy.arrived  = true;
        }
    }
}

void credit_arrived_convoys(world& w, int tick)
{
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

/// Propellant burned by one space-mode convoy launch (BL-308). PRODUCTION.md
/// § Launchpad specifies the cost *per launch*, not per tonne or per AU — the
/// pad is the thing being fuelled, so a launch costs the same whatever it
/// carries. One authored round unit, in the same magnitude family as the recipe
/// batches (scripts/recipes.lua): two batches of the atmosphere route, or three
/// of the airless one, buys a launch.
constexpr float propellant_per_launch = 1.0f;

/// Propellant the corp can actually burn launching `cargo` units of `ri` off
/// `body` — its on-body stockpile, minus the cargo itself when the cargo IS
/// propellant (a launch cannot burn the propellant it is exporting).
float launch_propellant_available(const world& w, entity_id corp, entity_id body,
                                  std::size_t ri, float cargo_qty)
{
    const auto pit = w.corp_body_pools.find({corp, body});
    if (pit == w.corp_body_pools.end())
        return 0.0f;
    float avail = pit->second.quantities[static_cast<std::size_t>(resource_type::propellant)];
    if (ri == static_cast<std::size_t>(resource_type::propellant))
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

/// BL-148/149 logistics-node lookups, built once per dispatch pass. `pop_tile_scale`
/// maps a population-centre's tile to its scale (tier 1–5, BL-148 — cities are free hubs);
/// `hub_tiles` holds every completed inland_logistics_hub's tile (BL-149 — the player
/// extends the land network). A convoy's intra-body haul is discounted for each such node
/// its A* path crosses.
struct logistics_nodes
{
    std::unordered_map<entity_id, int> pop_tile_scale;
    std::unordered_set<entity_id>      hub_tiles;
};

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

void dispatch_convoys(world& w, const recipe_registry& reg,
                      float logistics_cost_land, float logistics_cost_space)
{
    // One dispatch pass per (corp, dest_body, resource) shortfall.
    // Shortfall = market demand exceeded supply in the last clearing pass.
    // We fix quantities at a single batch = shortfall amount, capped by source surplus.

    // BL-148/149: build the logistics-node lookups once — cities (population centres) and the
    // player's inland logistics hubs discount any intra-body haul whose A* path crosses them.
    const logistics_nodes  nodes = collect_logistics_nodes(w);
    const logistics_node_params& node_params = reg.logistics_nodes();

    // BL-354: inter-body distances are evaluated at this day tick via the tick-pure
    // angle, so sourcing, pricing and convoy speed are a pure function of tick.
    // current_day_tick is set by every tick path (app + main) before dispatch runs.
    const int day_tick = w.current_day_tick;

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
        corporation_component& corp = w.corporations.at(corp_id);

        // Iterate markets (not corp pools) so we catch shortfalls even on bodies
        // where the corp has no existing pool entry.
        for (const entity_id dest_market_id : market_ids)
        {
            const market_component& dest_market = w.markets.at(dest_market_id);
            const entity_id dest_body = dest_market.body;

            for (std::size_t ri = 0; ri < resource_count; ++ri)
            {
                const float shortfall = dest_market.demand[ri] - dest_market.supply[ri];
                if (shortfall <= 0.0f)
                    continue;

                // Find cheapest reachable source.
                entity_id best_src_body    = null_entity;
                entity_id best_src_market  = null_entity;
                float     best_cost        = std::numeric_limits<float>::max();
                float     best_qty         = 0.0f;
                convoy_mode best_mode      = convoy_mode::land;
                /// Econ ticks the winning leg takes (Ben, 2026-08-12). Carried
                /// out of the source loop because the intra-body branch is the
                /// only place the terrain-weighted path is in scope, and the
                /// reference it returns must not outlive a cache invalidation.
                int       best_travel_ticks = 1;

                for (auto& [src_key, src_pool] : w.corp_body_pools)
                {
                    if (src_key.first != corp_id)
                        continue;
                    const entity_id src_body = src_key.second;

                    const float surplus = src_pool.quantities[ri];
                    if (surplus <= 0.0f)
                        continue;
                    const float qty = std::min(surplus, shortfall);

                    convoy_mode mode;
                    float       dist;
                    float       unit_cost;
                    int         travel_ticks = 1;
                    float       node_discount = 0.0f; // BL-148/149: intra-body city/hub discount.
                    if (src_body == dest_body)
                    {
                        // Intra-body (BL-077): haul the corp's on-body stockpile from its
                        // representative tile to the short market's centre, terrain-weighted
                        // over the tile grid (land, or sea when the path must cross water).
                        const entity_id origin      = corp_representative_tile(w, corp, src_body);
                        const entity_id dest_centre = dest_market.centre_tile;
                        if (origin == null_entity || dest_centre == null_entity)
                            continue; // no production anchor / unanchored market: cannot route
                        const logistics_path& path = intra_body_path(w, src_body, origin, dest_centre);
                        if (!path.reachable)
                            continue;
                        mode      = path.crosses_ocean ? convoy_mode::sea : convoy_mode::land;
                        dist      = path.cost;
                        unit_cost = reg.logistics_cost(mode);
                        // Distance now costs TIME as well as money (Ben,
                        // 2026-08-12). Computed here because `path` is a
                        // reference into the A* cache and must be read before
                        // anything can invalidate it.
                        travel_ticks = convoy_travel_ticks(w, src_body, path);
                        // BL-148/149: discount the haul for the cities + hubs its path crosses.
                        node_discount = node_discount_fraction(path, nodes, node_params);
                    }
                    else
                    {
                        // Inter-body: straight-line space lane, launchpad-gated.
                        if (!corp_has_launchpad_on(w, corp, src_body))
                            continue;
                        // BL-308: the pad also has to be FUELLED. A launch burns
                        // propellant_per_launch from the corp's stockpile on the
                        // source body; without it the lane is shut exactly as if
                        // no pad existed. Deterministic — a pure read of the pool.
                        if (launch_propellant_available(w, corp_id, src_body, ri, qty)
                            < propellant_per_launch)
                            continue;
                        mode      = convoy_mode::space;
                        dist      = body_distance_au(w, src_body, dest_body, day_tick);
                        unit_cost = logistics_cost_space;
                        // The space lane keeps its own calibration — roughly one
                        // econ tick per AU. Unchanged deliberately: it is the
                        // only leg the AU model was ever right for, and it is
                        // parked with the space arc anyway (era/space).
                        travel_ticks = (dist > 1.0f) ? static_cast<int>(dist + 0.999f) : 1;
                    }

                    const float cost = unit_cost * dist * qty * (1.0f - node_discount);
                    if (cost < best_cost)
                    {
                        best_src_body   = src_body;
                        best_src_market = market_for_body(w, src_body);
                        best_cost       = cost;
                        best_qty        = qty;
                        best_mode       = mode;
                        best_travel_ticks = travel_ticks;
                    }
                }

                if (best_src_body == null_entity)
                    continue;
                if (corp.balance < best_cost)
                    continue;

                // Debit cost and source pool; create the convoy.
                corp.balance -= best_cost;
                w.pool_for(corp_id, best_src_body).quantities[ri] -= best_qty;

                // BL-308: burn the launch's propellant. Charged once per launch
                // (not per unit, not per AU) and only on the space lane; the
                // availability gate above already ran against this same pool, so
                // this cannot drive it negative.
                if (best_mode == convoy_mode::space)
                    w.pool_for(corp_id, best_src_body).quantities[
                        static_cast<std::size_t>(resource_type::propellant)] -= propellant_per_launch;

                // Speed is progress-per-tick, so a leg taking N ticks advances
                // 1/N each tick (Ben, 2026-08-12).
                //
                // WAS: `1 / distance_in_AU`, an interplanetary calibration.
                // `body_distance_au` returns 0 for two markets on the same body,
                // so it clamped to 1.0 and EVERY intra-body convoy arrived in a
                // single econ tick regardless of how far it went — distance cost
                // money and never cost time. `best_travel_ticks` now carries the
                // terrain-weighted, physically-scaled figure for the winning leg.
                const float speed = 1.0f / static_cast<float>(best_travel_ticks);

                convoy_component c;
                c.source_market  = best_src_market;
                c.dest_market    = dest_market_id;
                c.mode           = best_mode;
                c.cargo_resource = static_cast<resource_type>(ri);
                c.cargo_qty      = best_qty;
                c.progress       = 0.0f;
                c.speed          = speed;
                c.corp           = corp_id;
                c.arrived        = false;
                w.convoys.push_back(c);
            }
        }
    }
    (void)logistics_cost_land; // intra-body reads reg.logistics_cost(land/sea) directly; this
                               // param is retained for caller/signature stability.
}
