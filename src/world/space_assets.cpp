#include "space_assets.hpp"

#include "components.hpp"
#include "market_clearing.hpp" // market_for_tile (BL-1582: the home market)
#include "nation_tech.hpp"     // nation_has_mechanic (BL-1582: the launch gate)
#include "orbital_graph.hpp"   // orbital_path, orbital_slot_capacity (BL-1581)
#include "space_bombard.hpp"    // BL-1599: space_blockaded
#include "space_tech_keys.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

// BL-1327, BL-1581, BL-1582 — see space_assets.hpp for the design. This is the arithmetic.

bool space_launch_legal_node(space_asset_class cls, orbital_node_kind k, bool home_body)
{
    const bool orbital = orbital_node_is_orbital(k);
    switch (cls)
    {
        case space_asset_class::satellite:      return orbital;
        case space_asset_class::station_core:   return orbital;
        case space_asset_class::station_module: return orbital; // its station's node (stations.cpp)
        case space_asset_class::rover:          return !orbital && !home_body;
        case space_asset_class::crewed_mission: return orbital || !home_body;
    }
    return false;
}

bool space_slot_legal(space_asset_class cls, space_slot slot)
{
    switch (cls)
    {
        case space_asset_class::satellite:      return slot == space_slot::orbit;
        case space_asset_class::rover:          return slot == space_slot::surface;
        case space_asset_class::crewed_mission: return slot == space_slot::orbit || slot == space_slot::surface;
        case space_asset_class::station_module: return slot == space_slot::orbit;
        case space_asset_class::station_core:   return slot == space_slot::orbit;
    }
    return false;
}

entity_id space_home_body_of(const world& w, entity_id nation)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return null_entity;
    if (nit->second.capital_tile != null_entity)
    {
        const auto tit = w.tiles.find(nit->second.capital_tile);
        if (tit != w.tiles.end() && w.bodies.count(tit->second.body) != 0)
            return tit->second.body;
    }
    return w.bodies.count(w.home_body) != 0 ? w.home_body : null_entity;
}

int orbital_slots_held(const world& w, orbital_node n)
{
    int held = 0;
    for (const auto& [aid, a] : w.space_assets) // std::map: ascending
        if (space_class_holds_slot(a.cls) && a.state != space_asset_state::lost && a.node == n)
            ++held;
    return held;
}

bool space_has_staffed_pad(const world& w, entity_id nation, entity_id body)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end() || body == null_entity)
        return false;
    std::vector<entity_id> ids(nit->second.state_assets.begin(), nit->second.state_assets.end());
    std::sort(ids.begin(), ids.end());
    for (const entity_id bid : ids)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        if (b.type != building_type::launchpad || b.ticks_remaining > 0 || b.decommissioned
            || b.state_idle || !(b.workforce_assigned > 0.0f) || w.tile_occupied(b.tile))
            continue;
        const auto tit = w.tiles.find(b.tile);
        if (tit != w.tiles.end() && tit->second.body == body)
            return true;
    }
    return false;
}

entity_id space_home_market(const world& w, entity_id nation)
{
    const entity_id home = space_home_body_of(w, nation);
    if (home == null_entity)
        return null_entity;
    const auto nit = w.nations.find(nation);
    if (nit != w.nations.end() && nit->second.capital_tile != null_entity
        && w.tiles.count(nit->second.capital_tile) != 0)
    {
        const entity_id m = market_for_tile(w, nit->second.capital_tile);
        const auto mit = w.markets.find(m);
        if (mit != w.markets.end() && mit->second.body == home)
            return m;
    }
    entity_id best = null_entity;
    for (const auto& [mid, mc] : w.markets)
        if (mc.body == home && (best == null_entity || mid < best))
            best = mid;
    return best;
}

namespace {

/// A body's distance from the star in AU: its own orbital radius if it orbits
/// the star, else its top-level ancestor's (a moon reads its planet's). The
/// walk is bounded, so a malformed parent cycle cannot hang the tick.
float heliocentric_au(const world& w, entity_id body)
{
    entity_id cur = body;
    for (int hop = 0; hop < 8; ++hop)
    {
        const auto bit = w.bodies.find(cur);
        if (bit == w.bodies.end())
            return 0.0f;
        const body_component& b = bit->second;
        if (b.parent == null_entity || w.bodies.count(b.parent) == 0)
            return std::isfinite(b.orbital_radius_au) ? b.orbital_radius_au : 0.0f;
        cur = b.parent;
    }
    return 0.0f;
}

/// The key a satellite role needs; empty for `none`.
std::string_view role_key(space_satellite_role r)
{
    switch (r)
    {
        case space_satellite_role::comms:       return space_keys::role_comms;
        case space_satellite_role::navigation:  return space_keys::role_navigation;
        case space_satellite_role::observation: return space_keys::role_observation;
        case space_satellite_role::survey:      return space_keys::role_survey;
        case space_satellite_role::none:        break;
    }
    return {};
}

bool role_in_range(space_satellite_role r)
{
    return static_cast<std::size_t>(r) < space_satellite_role_count;
}

/// The goods half of a quote: price @p components + @p propellant off the home
/// shelf. False (and `q.result = rejected_shelf`) when the shelf cannot fill
/// the whole lot, the ceiling refuses it, or a price is not positive.
bool price_goods(const world& w, const space_asset_params& p, entity_id nation,
                 float components, float propellant, space_launch_quote& q)
{
    q.components = components;
    q.propellant = propellant;
    q.goods_value = 0.0f;
    q.market = space_home_market(w, nation);
    const auto mit = w.markets.find(q.market);
    if (mit == w.markets.end())
    {
        q.result = space_launch_result::rejected_shelf;
        return false;
    }
    const market_component& mc = mit->second;
    const std::pair<resource_type, float> lot[] = {
        { resource_type::spacecraft_components, components },
        { resource_type::propellant,            propellant },
    };
    for (const auto& [good, qty] : lot)
    {
        if (!(qty > 0.0f))
            continue;
        const std::size_t ri = static_cast<std::size_t>(good);
        const float unit = posted_price(mc, ri);
        if (!std::isfinite(qty) || !shelf_admits(mc, ri, p.reservation_mult, /*off_buys=*/true)
            || !(unit > 0.0f) || !std::isfinite(unit) || !(mc.inventory[ri] >= qty))
        {
            q.result = space_launch_result::rejected_shelf;
            return false;
        }
        q.goods_value += qty * unit;
    }
    return true;
}

/// Draw the quoted lot off its shelf (the quote proved it is there).
void draw_goods(world& w, const space_launch_quote& q)
{
    market_component& mc = w.markets.at(q.market);
    if (q.components > 0.0f)
        mc.inventory[static_cast<std::size_t>(resource_type::spacecraft_components)] -= q.components;
    if (q.propellant > 0.0f)
        mc.inventory[static_cast<std::size_t>(resource_type::propellant)] -= q.propellant;
}

/// The path's quarters, days over `days_per_quarter` rounded down, clamped in
/// float before the cast.
int32_t path_quarters(const space_asset_params& p, double days, int32_t cap)
{
    if (!(p.days_per_quarter > 0.0f) || !std::isfinite(days) || !(days > 0.0))
        return 0;
    const double q = std::floor(days / static_cast<double>(p.days_per_quarter));
    if (!std::isfinite(q) || q <= 0.0)
        return 0;
    return static_cast<int32_t>(std::min(q, static_cast<double>(cap)));
}

} // namespace

float space_heliocentric_au(const world& w, entity_id body)
{
    return heliocentric_au(w, body);
}

space_launch_quote quote_space_launch(const world& w, const space_asset_params& p,
                                      entity_id nation, space_asset_class cls,
                                      orbital_node node, space_satellite_role role)
{
    space_launch_quote q;
    // Untrusted-input discipline: the enum values are range-checked as values,
    // so a caller passing a cast integer is refused, not indexed out of bounds.
    if (static_cast<std::size_t>(cls) >= space_asset_class_count
        || static_cast<int>(node.kind) >= orbital_node_kind_count || !role_in_range(role)
        || (cls == space_asset_class::satellite) == (role == space_satellite_role::none))
    {
        q.result = space_launch_result::rejected_invalid;
        return q;
    }
    const auto nit = w.nations.find(nation);
    if (nation == null_entity || nit == w.nations.end())
    {
        q.result = space_launch_result::rejected_unknown_nation;
        return q;
    }
    const entity_id home = space_home_body_of(w, nation);
    const auto bit = w.bodies.find(node.body);
    if (home == null_entity || node.body == null_entity || bit == w.bodies.end())
    {
        q.result = space_launch_result::rejected_unknown_body;
        return q;
    }
    const bool at_home = node.body == home;
    if (bit->second.type == body_type::star || !space_launch_legal_node(cls, node.kind, at_home))
    {
        q.result = space_launch_result::rejected_illegal_destination;
        return q;
    }

    // The reach tier: only the travel-time floor reads it now.
    const bool surface = node.kind == orbital_node_kind::surface;
    float au = 0.0f;
    if (at_home)
        q.reach = space_reach::home_orbit;
    else if (bit->second.parent == home)
        q.reach = surface ? space_reach::moon_surface : space_reach::moon_orbit;
    else
    {
        q.reach = surface ? space_reach::far_surface : space_reach::far_orbit;
        au = std::fabs(heliocentric_au(w, node.body) - heliocentric_au(w, home));
    }

    // BL-1582: the gate. Rocketry for every class; the role's key for a
    // satellite; the core's key for a station core. (Module and upgrade keys:
    // stations.cpp, before it calls the launch.)
    if (!nation_has_mechanic(w, nation, space_keys::dispatch)
        || (cls == space_asset_class::satellite && !nation_has_mechanic(w, nation, role_key(role)))
        || (cls == space_asset_class::station_core && !nation_has_mechanic(w, nation, space_keys::station_core)))
    {
        q.result = space_launch_result::rejected_tech_locked;
        return q;
    }
    if (!space_has_staffed_pad(w, nation, home))
    {
        q.result = space_launch_result::rejected_no_launchpad;
        return q;
    }
    // BL-1599 (SPACE_FLEETS.md § Blockade): no launch from a blockaded body reaches orbit.
    if (space_blockaded(w, nation, home))
    {
        q.result = space_launch_result::rejected_blockaded;
        return q;
    }
    // BL-1581: a slot-holding class needs a free slot at the node.
    if (space_class_holds_slot(cls) && orbital_slots_held(w, node) >= orbital_slot_capacity(w, node))
    {
        q.result = space_launch_result::rejected_no_slot;
        return q;
    }

    // The path from the home surface prices the thrust and times the trip.
    const orbital_path_result path =
        orbital_path(w, orbital_graph_params{}, orbital_node{home, orbital_node_kind::surface}, node);
    if (!path.ok || !std::isfinite(path.dv) || path.dv < 0.0 || !std::isfinite(path.days) || path.days < 0.0)
    {
        q.result = space_launch_result::rejected_no_route;
        return q;
    }
    q.dv   = path.dv;
    q.days = path.days;

    const std::size_t ci = static_cast<std::size_t>(cls);
    const std::size_t ri = static_cast<std::size_t>(q.reach);
    // Clamp in float before the cast: a huge separation (or a huge dial) must
    // never reach an out-of-range int conversion.
    const int32_t cap     = p.max_extra_quarters > 0 ? p.max_extra_quarters : 0;
    const float   extra_q = std::floor(p.far_quarters_per_au * au);
    const float   extra_c = (std::isfinite(extra_q) && extra_q > 0.0f) ? std::min(extra_q, static_cast<float>(cap)) : 0.0f;
    const int32_t floor_q = p.travel_quarters[ri] + static_cast<int32_t>(extra_c);
    q.travel_quarters = std::max(floor_q, path_quarters(p, path.days, cap + p.travel_quarters[ri]));

    q.credits = p.launch_cost[ci] + p.credits_per_dv * static_cast<float>(path.dv);
    const float components = p.components_lot[ci];
    const float propellant = p.propellant_per_dv[ci] * static_cast<float>(path.dv);
    if (!std::isfinite(q.credits) || q.credits < 0.0f || q.travel_quarters < 0
        || !std::isfinite(p.upkeep[ci]) || p.upkeep[ci] < 0.0f
        || !std::isfinite(components) || components < 0.0f
        || !std::isfinite(propellant) || propellant < 0.0f)
    {
        q.result = space_launch_result::rejected_invalid;
        return q;
    }
    if (!price_goods(w, p, nation, components, propellant, q))
        return q;
    q.cost = q.credits + q.goods_value;
    if (!std::isfinite(q.cost))
    {
        q.result = space_launch_result::rejected_invalid;
        return q;
    }
    if (!(nit->second.treasury >= q.cost))
    {
        q.result = space_launch_result::rejected_insufficient_treasury;
        return q;
    }
    q.result = space_launch_result::launched;
    return q;
}

space_launch_quote launch_space_asset(world& w, const space_asset_params& p,
                                      entity_id nation, space_asset_class cls,
                                      orbital_node node, space_satellite_role role)
{
    const int econ_tick = w.current_econ_tick; // the tick about to run (header)
    space_launch_quote q = quote_space_launch(w, p, nation, cls, node, role);
    if (q.result != space_launch_result::launched)
        return q; // rejected whole: nothing above wrote to `w`

    // Every check has passed; only now does the world move.
    space_asset a;
    a.id    = w.create_entity();
    a.owner = nation;
    a.cls   = cls;
    a.node  = node;
    a.dest  = {node.body, space_slot_of(node.kind)};
    a.role  = role;
    a.state = space_asset_state::in_transit;
    a.launch_tick  = econ_tick;
    a.arrival_tick = econ_tick + q.travel_quarters;
    a.upkeep       = p.upkeep[static_cast<std::size_t>(cls)];
    a.unpaid_quarters = 0;
    a.launch_cost     = q.cost;

    draw_goods(w, q);
    w.nations.at(nation).treasury -= q.cost;
    w.space_assets.emplace(a.id, a);
    q.asset = a.id;
    return q;
}

satellite_order retask_satellite(world& w, entity_id nation, entity_id asset, space_satellite_role role)
{
    satellite_order r;
    if (!role_in_range(role) || role == space_satellite_role::none)
        return r; // rejected_invalid
    if (w.nations.count(nation) == 0)
    {
        r.result = satellite_order_result::rejected_unknown_nation;
        return r;
    }
    const auto ait = w.space_assets.find(asset);
    if (ait == w.space_assets.end())
    {
        r.result = satellite_order_result::rejected_unknown_asset;
        return r;
    }
    space_asset& a = ait->second;
    if (a.owner != nation)
    {
        r.result = satellite_order_result::rejected_not_owner;
        return r;
    }
    if (a.cls != space_asset_class::satellite)
        return r; // rejected_invalid
    if (a.state == space_asset_state::lost || a.role == role)
    {
        r.result = satellite_order_result::rejected_state;
        return r;
    }
    if (!nation_has_mechanic(w, nation, role_key(role)))
    {
        r.result = satellite_order_result::rejected_tech_locked;
        return r;
    }
    a.role = role;
    r.result = satellite_order_result::ok;
    return r;
}

satellite_order quote_move_satellite(const world& w, const space_asset_params& p, entity_id nation,
                                     entity_id asset, orbital_node to, space_launch_quote* detail)
{
    satellite_order r;
    space_launch_quote q;
    const auto finish = [&]() {
        if (detail != nullptr)
            *detail = q;
        return r;
    };
    if (static_cast<int>(to.kind) >= orbital_node_kind_count)
        return finish(); // rejected_invalid
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
    {
        r.result = satellite_order_result::rejected_unknown_nation;
        return finish();
    }
    const auto ait = w.space_assets.find(asset);
    if (ait == w.space_assets.end())
    {
        r.result = satellite_order_result::rejected_unknown_asset;
        return finish();
    }
    const space_asset& a = ait->second;
    if (a.owner != nation)
    {
        r.result = satellite_order_result::rejected_not_owner;
        return finish();
    }
    if (a.cls != space_asset_class::satellite)
        return finish(); // rejected_invalid
    if (a.state != space_asset_state::active || a.node == to) // in place and paid up: a decaying one waits
    {
        r.result = satellite_order_result::rejected_state;
        return finish();
    }
    r.result = satellite_order_result::rejected_launch;
    const auto bit = w.bodies.find(to.body);
    if (bit == w.bodies.end())
    {
        q.result = r.launch = space_launch_result::rejected_unknown_body;
        return finish();
    }
    if (bit->second.type == body_type::star || !orbital_node_is_orbital(to.kind))
    {
        q.result = r.launch = space_launch_result::rejected_illegal_destination;
        return finish();
    }
    if (!nation_has_mechanic(w, nation, space_keys::dispatch))
    {
        q.result = r.launch = space_launch_result::rejected_tech_locked;
        return finish();
    }
    if (orbital_slots_held(w, to) >= orbital_slot_capacity(w, to))
    {
        q.result = r.launch = space_launch_result::rejected_no_slot;
        return finish();
    }
    const orbital_path_result path = orbital_path(w, orbital_graph_params{}, a.node, to);
    if (!path.ok || !std::isfinite(path.dv) || path.dv < 0.0 || !std::isfinite(path.days) || path.days < 0.0)
    {
        q.result = r.launch = space_launch_result::rejected_no_route;
        return finish();
    }
    q.dv = path.dv;
    q.days = path.days;
    const std::size_t ci = static_cast<std::size_t>(space_asset_class::satellite);
    q.travel_quarters = path_quarters(p, path.days, p.max_extra_quarters > 0 ? p.max_extra_quarters : 0);
    q.credits = p.credits_per_dv * static_cast<float>(path.dv);
    const float propellant = p.propellant_per_dv[ci] * static_cast<float>(path.dv);
    if (!std::isfinite(q.credits) || q.credits < 0.0f || !std::isfinite(propellant) || propellant < 0.0f)
    {
        q.result = r.launch = space_launch_result::rejected_invalid;
        return finish();
    }
    if (!price_goods(w, p, nation, 0.0f, propellant, q))
    {
        r.launch = q.result;
        return finish();
    }
    q.cost = q.credits + q.goods_value;
    if (!std::isfinite(q.cost) || !(nit->second.treasury >= q.cost))
    {
        q.result = r.launch = space_launch_result::rejected_insufficient_treasury;
        return finish();
    }
    q.result = r.launch = space_launch_result::launched;
    r.result = satellite_order_result::ok;
    r.cost   = q.cost;
    return finish();
}

satellite_order move_satellite(world& w, const space_asset_params& p, entity_id nation,
                               entity_id asset, orbital_node to)
{
    space_launch_quote q;
    const satellite_order r = quote_move_satellite(w, p, nation, asset, to, &q);
    if (r.result != satellite_order_result::ok)
        return r;
    draw_goods(w, q);
    w.nations.at(nation).treasury -= q.cost;
    space_asset& a = w.space_assets.at(asset);
    a.node  = to;
    a.dest  = {to.body, space_slot_of(to.kind)};
    a.state = space_asset_state::in_transit; // its old slot frees now; the new one is held
    a.arrival_tick = w.current_econ_tick + q.travel_quarters;
    // launch_tick stays the original launch's: the step's launch report reads it,
    // and a move is not a launch (its debit is reported by the command seam).
    if (a.arrival_tick < a.launch_tick)
        a.arrival_tick = a.launch_tick;
    return r;
}

void run_space_asset_step(world& w, const space_asset_params& p, int econ_tick,
                          space_asset_step_report* out)
{
    space_asset_step_report r;
    const int32_t loss_at =
        std::clamp(p.quarters_to_loss, int32_t{1}, k_space_quarters_to_loss_ceiling);
    for (auto& [id, a] : w.space_assets) // std::map: ascending id
    {
        if (a.launch_tick == econ_tick)
        {
            ++r.launched;
            r.launch_spent += a.launch_cost;
        }
        if (a.state == space_asset_state::lost)
            continue;
        if (a.state == space_asset_state::in_transit)
        {
            if (econ_tick < a.arrival_tick)
                continue; // still travelling; no upkeep in transit
            a.state = space_asset_state::active;
            ++r.arrived;
        }

        // In place: pay the quarter whole, or pay nothing.
        const auto nit = w.nations.find(a.owner);
        const bool can_pay = nit != w.nations.end() && std::isfinite(a.upkeep) && a.upkeep >= 0.0f
                          && nit->second.treasury >= a.upkeep;
        if (can_pay)
        {
            // BL-1539's leak, the same shape as garrison pay: a standing cost paid after
            // the return is written, booked on the next return as its `upkeep`, inside its
            // net (Ben, 2026-11-07). The exact difference, so the nets still telescope.
            const float before = nit->second.treasury;
            nit->second.treasury -= a.upkeep;
            nit->second.upkeep_unbooked += static_cast<double>(before) - static_cast<double>(nit->second.treasury);
            r.upkeep_paid += a.upkeep;
            a.unpaid_quarters = 0;
            a.state = space_asset_state::active;
            ++r.paid;
        }
        else
        {
            ++r.unpaid;
            // The bound is tested BEFORE the count moves, so the count never
            // passes loss_at (<= the ceiling) and can never overflow.
            if (a.unpaid_quarters >= loss_at - 1)
            {
                a.unpaid_quarters = loss_at;
                a.state = space_asset_state::lost;
                ++r.lost;
            }
            else
            {
                ++a.unpaid_quarters;
                a.state = space_asset_state::decaying;
            }
        }
    }
    if (out != nullptr)
        *out = r;
}
