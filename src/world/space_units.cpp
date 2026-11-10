#include "space_units.hpp"

#include "components.hpp"
#include "market_clearing.hpp" // market_for_tile
#include "nation_tech.hpp"     // nation_has_mechanic
#include "orbital_graph.hpp"
#include "space_assets.hpp"    // space_home_body_of, k_space_quarters_to_loss_ceiling
#include "space_convoy.hpp"     // BL-1596: run_space_convoy_day, prune_space_hauls
#include "space_tech_keys.hpp"
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

// BL-1594 / BL-1595 — see space_units.hpp for the design. This is the arithmetic.

namespace {

using rt = resource_type;
constexpr uint8_t k_all_hulls = 0x1F;
constexpr uint8_t k_warships  = (1u << 3) | (1u << 4); // frigate, cruiser
constexpr uint8_t k_carriers  = (1u << 0) | (1u << 1); // hauler, lander
constexpr uint8_t k_tug_only  = (1u << 2);

constexpr space_good g(rt r, float q) { return {r, q}; }
constexpr space_good none() { return {rt::spacecraft_components, 0.0f}; }

/// THE HULL TABLE (first cut, in-band; see the report). Mass in mass units, hull
/// points, crew drawn from the astronaut pool, credits of upkeep a quarter.
const std::array<space_hull_spec, space_hull_count> k_hulls{{
    {"Hauler",  10.0f,  20.0f, 1,  400.0f, true,  space_keys::hull_hauler,
     {g(rt::spacecraft_components, 10.0f), g(rt::alloys, 4.0f), none()}},
    {"Lander",   8.0f,  15.0f, 2,  500.0f, true,  space_keys::hull_lander,
     {g(rt::spacecraft_components, 8.0f), g(rt::alloys, 4.0f), none()}},
    {"Tug",      8.0f,  20.0f, 1,  400.0f, true,  space_keys::hull_tug,
     {g(rt::spacecraft_components, 8.0f), g(rt::machinery, 4.0f), none()}},
    {"Frigate", 14.0f,  40.0f, 3,  900.0f, true,  space_keys::hull_frigate,
     {g(rt::spacecraft_components, 14.0f), g(rt::alloys, 8.0f), none()}},
    {"Cruiser", 40.0f, 120.0f, 8, 3000.0f, false, space_keys::hull_cruiser,
     {g(rt::spacecraft_components, 40.0f), g(rt::alloys, 30.0f), g(rt::electronics, 10.0f)}},
}};

/// THE ITEM CATALOGUE (first cut). An empty key means the item opens with the hull
/// (the bays: no research node names a bay).
const std::vector<space_item> k_items{
    //  id                       name                          slot                      hulls        key                               mass  ve    tank  cargo pax fp     arm   hp
    {"drive.chemical",        "Restartable chemical drive", space_unit_slot::drive,      k_all_hulls, space_keys::drive_chemical,        4.0f, 10.0f, 0.0f,  0.0f, 0, 0.0f,  0.0f,  0.0f,
     {g(rt::spacecraft_components, 4.0f), g(rt::machinery, 2.0f), none()}},
    {"drive.nuclear_thermal", "Nuclear thermal drive",      space_unit_slot::drive,      k_all_hulls, space_keys::drive_nuclear_thermal, 8.0f, 20.0f, 0.0f,  0.0f, 0, 0.0f,  0.0f,  0.0f,
     {g(rt::spacecraft_components, 8.0f), g(rt::electronics, 3.0f), g(rt::machinery, 2.0f)}},
    {"tanks.standard",        "Storable-propellant tanks",  space_unit_slot::tanks,      k_all_hulls, space_keys::tanks_standard,        2.0f,  0.0f, 20.0f, 0.0f, 0, 0.0f,  0.0f,  0.0f,
     {g(rt::alloys, 3.0f), none(), none()}},
    {"tanks.extended",        "Extended tankage",           space_unit_slot::tanks,      k_all_hulls, space_keys::tanks_extended,        4.0f,  0.0f, 45.0f, 0.0f, 0, 0.0f,  0.0f,  0.0f,
     {g(rt::alloys, 6.0f), none(), none()}},
    {"armament.kinetic",      "Kinetic interceptors",       space_unit_slot::armament,   k_warships,  space_keys::armament_kinetic,      3.0f,  0.0f, 0.0f,  0.0f, 0, 10.0f, 0.0f,  0.0f,
     {g(rt::ordnance, 6.0f), g(rt::electronics, 2.0f), none()}},
    {"armament.missile",      "Orbital missiles",           space_unit_slot::armament,   k_warships,  space_keys::armament_missile,      4.0f,  0.0f, 0.0f,  0.0f, 0, 18.0f, 0.0f,  0.0f,
     {g(rt::ordnance, 8.0f), g(rt::electronics, 4.0f), none()}},
    {"armour.whipple",        "Whipple shielding",          space_unit_slot::armour,     k_warships,  space_keys::armour_whipple,        3.0f,  0.0f, 0.0f,  0.0f, 0, 0.0f,  8.0f,  10.0f,
     {g(rt::alloys, 4.0f), none(), none()}},
    {"armour.composite",      "Composite hull armour",      space_unit_slot::armour,     k_warships,  space_keys::armour_composite,      5.0f,  0.0f, 0.0f,  0.0f, 0, 0.0f,  15.0f, 20.0f,
     {g(rt::alloys, 6.0f), g(rt::machinery, 2.0f), none()}},
    {"bay.cargo",             "Cargo bay",                  space_unit_slot::bay,        k_carriers,  std::string_view{},                2.0f,  0.0f, 0.0f,  40.0f, 0, 0.0f, 0.0f,  0.0f,
     {g(rt::steel, 6.0f), none(), none()}},
    {"bay.passenger",         "Passenger bay",              space_unit_slot::bay,        k_carriers,  std::string_view{},                3.0f,  0.0f, 0.0f,  0.0f, 6, 0.0f,  0.0f,  0.0f,
     {g(rt::steel, 4.0f), g(rt::electronics, 1.0f), none()}},
    {"arm.capture",           "Capture arm",                space_unit_slot::debris_arm, k_tug_only,  space_keys::debris_arm,            3.0f,  0.0f, 0.0f,  0.0f, 0, 0.0f,  0.0f,  0.0f,
     {g(rt::machinery, 4.0f), g(rt::electronics, 1.0f), none()}},
};

bool hull_ok(space_hull h) { return static_cast<std::size_t>(h) < space_hull_count; }

bool node_ok(const world& w, orbital_node n)
{
    if (static_cast<std::size_t>(n.kind) >= static_cast<std::size_t>(orbital_node_kind_count))
        return false;
    const auto bit = w.bodies.find(n.body);
    return bit != w.bodies.end() && bit->second.type != body_type::star;
}

bool in_transit(const space_unit& u) { return u.state == space_unit_state::in_transit; }

/// Whether @p nation holds every key @p h and @p d need.
bool keys_held(const world& w, entity_id nation, space_hull h, const space_design& d)
{
    if (!nation_has_mechanic(w, nation, k_hulls[static_cast<std::size_t>(h)].key))
        return false;
    for (std::size_t s = 0; s < space_unit_slot_count; ++s)
    {
        if (d[s] == k_no_space_item)
            continue;
        const space_item& it = k_items[d[s]];
        if (!it.key.empty() && !nation_has_mechanic(w, nation, it.key))
            return false;
    }
    return true;
}

/// The nation's home shelf: its capital tile's market, else the lowest-id market on
/// its home body. Null when neither resolves.
entity_id home_market_of(const world& w, entity_id nation)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end())
        return null_entity;
    if (nit->second.capital_tile != null_entity)
    {
        const entity_id m = market_for_tile(w, nit->second.capital_tile);
        if (m != null_entity)
            return m;
    }
    const entity_id body = space_home_body_of(w, nation);
    for (const auto& [mid, m] : w.markets) // ascending
        if (m.body == body && body != null_entity)
            return mid;
    return null_entity;
}

/// The market a building's tile clears against, else the lowest-id market on @p body.
entity_id market_on_body(const world& w, entity_id tile, entity_id body)
{
    if (tile != null_entity)
    {
        const entity_id m = market_for_tile(w, tile);
        if (m != null_entity)
            return m;
    }
    for (const auto& [mid, m] : w.markets)
        if (m.body == body)
            return mid;
    return null_entity;
}

/// Price a whole lot off @p market's shelf. `shelf_short` when any good cannot be
/// filled whole (or the ceiling refuses it); otherwise the credits.
struct lot_quote
{
    bool  shelf_ok = false;
    float credits  = 0.0f;
};

lot_quote quote_lot(const world& w, const space_unit_params& p, entity_id market,
                    const std::array<float, resource_count>& lot)
{
    lot_quote q;
    const auto mit = w.markets.find(market);
    if (mit == w.markets.end())
        return q;
    const market_component& m = mit->second;
    double credits = 0.0;
    for (std::size_t r = 0; r < resource_count; ++r) // ascending resource: a fixed order
    {
        const float need = lot[r];
        if (!(need > 0.0f))
            continue;
        const float price = posted_price(m, r);
        if (!shelf_admits(m, r, p.reservation_mult, true) || !(price > 0.0f) || !std::isfinite(price)
            || !(m.inventory[r] >= need))
            return q;
        credits += static_cast<double>(need) * static_cast<double>(price);
    }
    if (!std::isfinite(credits) || credits > static_cast<double>(std::numeric_limits<float>::max()))
        return q;
    q.shelf_ok = true;
    q.credits  = static_cast<float>(credits);
    return q;
}

/// Draw a quoted lot: the goods leave the shelf (consumed — the market-direct
/// purchase, as the space programme's BL-742 fallback) and the treasury pays.
void take_lot(world& w, entity_id nation, entity_id market, const std::array<float, resource_count>& lot,
              float credits)
{
    market_component& m = w.markets.at(market);
    for (std::size_t r = 0; r < resource_count; ++r)
        if (lot[r] > 0.0f)
            m.inventory[r] = std::max(0.0f, m.inventory[r] - lot[r]);
    w.nations.at(nation).treasury -= credits;
}

/// The working modules of @p kind at @p station. 0 for an unknown station.
int working_modules(const world& w, const station_params& stp, entity_id station, station_module_kind kind)
{
    if (w.stations.find(station) == w.stations.end())
        return 0;
    const station_budget b = evaluate_station_budget(w, stp, station, w.current_econ_tick);
    int n = 0;
    for (const entity_id e : b.working)
    {
        const auto ait = w.space_assets.find(e);
        if (ait != w.space_assets.end() && ait->second.module == static_cast<uint8_t>(kind))
            ++n;
    }
    return n;
}

/// The raise both verbs share once the site is resolved: quote the lot, check the
/// crew, then (on @p commit) write. @p extra_prop is the climb's propellant.
space_unit_op raise_at(world& w, const space_unit_params& p, entity_id nation, space_hull h,
                       entity_id market, float extra_prop, orbital_node node,
                       space_unit_state state, entity_id station)
{
    space_unit_op op;
    const space_design* d = space_unit_design_of(w, nation, h);
    const space_design_stats st = evaluate_space_design(h, *d);
    if (market == null_entity)
    {
        op.result = space_unit_result::rejected_no_market;
        return op;
    }
    std::array<float, resource_count> lot = st.goods;
    const float prop = st.tank + extra_prop;
    lot[static_cast<std::size_t>(rt::propellant)] += prop;
    const lot_quote q = quote_lot(w, p, market, lot);
    if (!q.shelf_ok)
    {
        op.result = space_unit_result::rejected_shelf_short;
        return op;
    }
    nation_component& nc = w.nations.at(nation);
    if (!(nc.treasury >= q.credits))
    {
        op.result = space_unit_result::rejected_insufficient_treasury;
        return op;
    }
    if (nc.astronauts < st.crew)
    {
        op.result = space_unit_result::rejected_no_crew;
        return op;
    }

    // Every check passed: write.
    take_lot(w, nation, market, lot, q.credits);
    nc.astronauts -= st.crew;
    space_unit u;
    u.id          = w.create_entity();
    u.owner       = nation;
    u.hull        = h;
    u.design      = *d;
    u.state       = state;
    u.node        = node;
    u.dest        = node;
    u.station     = station;
    u.propellant  = st.tank;
    u.hull_points = st.hull_points;
    u.crew        = st.crew;
    u.upkeep      = st.upkeep;
    w.space_units.emplace(u.id, u);

    op.result           = space_unit_result::ok;
    op.unit             = u.id;
    op.credits          = q.credits;
    op.propellant       = prop;
    op.climb_propellant = extra_prop;
    op.market           = market;
    return op;
}

/// The common gate of a raise: nation, hull, design, keys.
space_unit_result raise_gate(const world& w, entity_id nation, space_hull h)
{
    if (!hull_ok(h))
        return space_unit_result::rejected_invalid;
    if (w.nations.find(nation) == w.nations.end())
        return space_unit_result::rejected_unknown_nation;
    const space_design* d = space_unit_design_of(w, nation, h);
    if (d == nullptr || !evaluate_space_design(h, *d).valid)
        return space_unit_result::rejected_no_design;
    if (!keys_held(w, nation, h, *d))
        return space_unit_result::rejected_tech_locked;
    return space_unit_result::ok;
}

/// The move every order makes, over @p ids (ascending). Refused whole.
space_unit_op order_units(world& w, const std::vector<entity_id>& ids, orbital_node from, orbital_node to)
{
    space_unit_op op;
    if (!node_ok(w, to) || !node_ok(w, from))
    {
        op.result = space_unit_result::rejected_invalid;
        return op;
    }
    if (from == to)
    {
        op.result = space_unit_result::rejected_no_route;
        return op;
    }
    if (to.kind == orbital_node_kind::surface)
        for (const entity_id id : ids)
            if (w.space_units.at(id).hull != space_hull::lander)
            {
                op.result = space_unit_result::rejected_surface;
                return op;
            }
    const orbital_path_result path = orbital_path(w, orbital_graph_params{}, from, to);
    if (!path.ok || !std::isfinite(path.dv) || !std::isfinite(path.days))
    {
        op.result = space_unit_result::rejected_no_route;
        return op;
    }
    const float dv = static_cast<float>(path.dv);
    std::vector<float> burns;
    burns.reserve(ids.size());
    for (const entity_id id : ids)
    {
        const float burn = space_unit_burn_for(w.space_units.at(id), dv);
        if (!std::isfinite(burn) || burn > w.space_units.at(id).propellant)
        {
            op.result = space_unit_result::rejected_insufficient_dv;
            op.unit = id;
            op.dv = dv;
            return op;
        }
        burns.push_back(burn);
    }
    const int32_t days = std::max<int32_t>(
        1, static_cast<int32_t>(std::min(std::ceil(path.days), 1.0e6)));
    for (std::size_t i = 0; i < ids.size(); ++i)
    {
        space_unit& u = w.space_units.at(ids[i]);
        u.propellant     = std::max(0.0f, u.propellant - burns[i]);
        u.state          = space_unit_state::in_transit;
        u.station        = null_entity;
        u.node           = from;
        u.dest           = to;
        u.days_remaining = days;
        op.propellant   += burns[i];
    }
    op.result = space_unit_result::ok;
    op.unit   = ids.empty() ? null_entity : ids.front();
    op.dv     = dv;
    op.days   = days;
    return op;
}

} // namespace

bool space_hull_has_slot(space_hull h, space_unit_slot s)
{
    switch (s)
    {
    case space_unit_slot::drive:
    case space_unit_slot::tanks:      return hull_ok(h);
    case space_unit_slot::armament:
    case space_unit_slot::armour:     return h == space_hull::frigate || h == space_hull::cruiser;
    case space_unit_slot::bay:        return h == space_hull::hauler || h == space_hull::lander;
    case space_unit_slot::debris_arm: return h == space_hull::tug;
    }
    return false;
}

const std::array<space_hull_spec, space_hull_count>& space_hull_table() { return k_hulls; }

const std::vector<space_item>& space_item_catalogue() { return k_items; }

space_design_stats evaluate_space_design(space_hull h, const space_design& d)
{
    space_design_stats st;
    if (!hull_ok(h))
        return st;
    const space_hull_spec& hs = k_hulls[static_cast<std::size_t>(h)];
    st.dry_mass    = hs.mass;
    st.hull_points = hs.hull_points;
    st.crew        = hs.crew;
    st.upkeep      = hs.upkeep;
    for (const space_good& gd : hs.goods)
        if (gd.qty > 0.0f)
            st.goods[static_cast<std::size_t>(gd.res)] += gd.qty;
    for (std::size_t s = 0; s < space_unit_slot_count; ++s)
    {
        const space_unit_slot slot = static_cast<space_unit_slot>(s);
        const bool has = space_hull_has_slot(h, slot);
        if (d[s] == k_no_space_item)
        {
            if (has)
                return st; // an empty slot the hull has: not a design
            continue;
        }
        if (!has || d[s] >= k_items.size())
            return st;
        const space_item& it = k_items[d[s]];
        if (it.slot != slot || (it.hull_mask & (1u << static_cast<unsigned>(h))) == 0)
            return st;
        st.dry_mass    += it.mass;
        st.exhaust     += it.exhaust;
        st.tank        += it.tank;
        st.cargo       += it.cargo;
        st.passengers  += it.passengers;
        st.firepower   += it.firepower;
        st.armour      += it.armour;
        st.hull_points += it.hull_points;
        for (const space_good& gd : it.goods)
            if (gd.qty > 0.0f)
                st.goods[static_cast<std::size_t>(gd.res)] += gd.qty;
    }
    st.valid = st.dry_mass > 0.0f && st.exhaust > 0.0f;
    if (st.valid)
        st.dv_capacity = st.exhaust * std::log((st.dry_mass + st.tank) / st.dry_mass);
    return st;
}

space_unit_result set_space_unit_design(world& w, entity_id nation, space_hull h, const space_design& d)
{
    if (!hull_ok(h))
        return space_unit_result::rejected_invalid;
    if (w.nations.find(nation) == w.nations.end())
        return space_unit_result::rejected_unknown_nation;
    if (!evaluate_space_design(h, d).valid)
        return space_unit_result::rejected_invalid;
    if (!keys_held(w, nation, h, d))
        return space_unit_result::rejected_tech_locked;
    w.space_unit_designs[nation][static_cast<uint8_t>(h)] = d;
    return space_unit_result::ok;
}

const space_design* space_unit_design_of(const world& w, entity_id nation, space_hull h)
{
    const auto nit = w.space_unit_designs.find(nation);
    if (nit == w.space_unit_designs.end())
        return nullptr;
    const auto dit = nit->second.find(static_cast<uint8_t>(h));
    return dit == nit->second.end() ? nullptr : &dit->second;
}

entity_id space_staffed_pad_on(const world& w, entity_id nation, entity_id body)
{
    const std::vector<entity_id>* assets = assets_of(w, nation);
    if (assets == nullptr)
        return null_entity;
    entity_id best = null_entity;
    for (const entity_id bid : *assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit == w.buildings.end())
            continue;
        const building_component& b = bit->second;
        if (b.type != building_type::launchpad || b.ticks_remaining > 0 || b.decommissioned
            || !(b.workforce_assigned > 0.0f))
            continue;
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;
        if (best == null_entity || bid < best)
            best = bid;
    }
    return best;
}

space_unit_op raise_space_unit_at_pad(world& w, const space_unit_params& p, entity_id nation,
                                      space_hull h, entity_id body)
{
    space_unit_op op;
    op.result = raise_gate(w, nation, h);
    if (op.result != space_unit_result::ok)
        return op;
    if (!k_hulls[static_cast<std::size_t>(h)].surface_launch)
    {
        op.result = space_unit_result::rejected_not_launchable;
        return op;
    }
    const orbital_node low{body, orbital_node_kind::low_orbit};
    if (!node_ok(w, low))
    {
        op.result = space_unit_result::rejected_invalid;
        return op;
    }
    const entity_id pad = space_staffed_pad_on(w, nation, body);
    if (pad == null_entity)
    {
        op.result = space_unit_result::rejected_no_pad;
        return op;
    }
    const space_design_stats st = evaluate_space_design(h, *space_unit_design_of(w, nation, h));
    const double climb = orbital_climb_dv(w, orbital_graph_params{}, body);
    const float rate = std::isfinite(p.launch_propellant_per_dv) ? std::max(0.0f, p.launch_propellant_per_dv) : 0.0f;
    const float climb_prop = static_cast<float>(climb * rate * (st.dry_mass + st.tank));
    if (!std::isfinite(climb_prop))
    {
        op.result = space_unit_result::rejected_invalid;
        return op;
    }
    const entity_id market = market_on_body(w, w.buildings.at(pad).tile, body);
    return raise_at(w, p, nation, h, market, climb_prop, low, space_unit_state::idle, null_entity);
}

space_unit_op raise_space_unit_at_shipyard(world& w, const space_unit_params& p,
                                           const station_params& stp, entity_id nation,
                                           space_hull h, entity_id station)
{
    space_unit_op op;
    op.result = raise_gate(w, nation, h);
    if (op.result != space_unit_result::ok)
        return op;
    const auto sit = w.stations.find(station);
    if (sit == w.stations.end())
    {
        op.result = space_unit_result::rejected_unknown_station;
        return op;
    }
    if (sit->second.owner != nation)
    {
        op.result = space_unit_result::rejected_not_owner;
        return op;
    }
    if (working_modules(w, stp, station, station_module_kind::shipyard) == 0)
    {
        op.result = space_unit_result::rejected_no_shipyard;
        return op;
    }
    return raise_at(w, p, nation, h, home_market_of(w, nation), 0.0f, sit->second.node,
                    space_unit_state::docked, station);
}

float space_unit_dv_remaining(const space_unit& u)
{
    const space_design_stats st = evaluate_space_design(u.hull, u.design);
    if (!st.valid)
        return 0.0f;
    const float dry = st.dry_mass + std::max(0.0f, u.cargo);
    return st.exhaust * std::log((dry + std::max(0.0f, u.propellant)) / dry);
}

float space_unit_burn_for(const space_unit& u, float dv)
{
    const space_design_stats st = evaluate_space_design(u.hull, u.design);
    if (!st.valid || !std::isfinite(dv) || dv < 0.0f)
        return std::numeric_limits<float>::infinity();
    const double wet = static_cast<double>(st.dry_mass) + std::max(0.0f, u.cargo) + std::max(0.0f, u.propellant);
    return static_cast<float>(wet * (1.0 - std::exp(-static_cast<double>(dv) / st.exhaust)));
}

space_unit_op order_space_unit(world& w, const space_unit_params& p, entity_id nation,
                               entity_id unit, orbital_node to)
{
    (void)p;
    space_unit_op op;
    const auto uit = w.space_units.find(unit);
    if (uit == w.space_units.end())
    {
        op.result = space_unit_result::rejected_unknown_unit;
        return op;
    }
    if (uit->second.owner != nation)
    {
        op.result = space_unit_result::rejected_not_owner;
        return op;
    }
    if (in_transit(uit->second))
    {
        op.result = space_unit_result::rejected_busy;
        return op;
    }
    return order_units(w, {unit}, uit->second.node, to);
}

space_unit_op order_space_fleet(world& w, const space_unit_params& p, entity_id nation,
                                orbital_node from, orbital_node to)
{
    (void)p;
    space_unit_op op;
    if (w.nations.find(nation) == w.nations.end())
    {
        op.result = space_unit_result::rejected_unknown_nation;
        return op;
    }
    std::vector<entity_id> ids;
    for (const auto& [id, u] : w.space_units) // ascending
        if (u.owner == nation && u.node == from && !in_transit(u))
            ids.push_back(id);
    if (ids.empty())
    {
        op.result = space_unit_result::rejected_unknown_unit;
        return op;
    }
    return order_units(w, ids, from, to);
}

int space_station_berths(const world& w, const space_unit_params& p, const station_params& stp,
                         entity_id station)
{
    return working_modules(w, stp, station, station_module_kind::dock) * std::max(0, p.berths_per_dock);
}

int space_station_docked(const world& w, entity_id station)
{
    int n = 0;
    for (const auto& [id, u] : w.space_units)
        if (u.state == space_unit_state::docked && u.station == station)
            ++n;
    return n;
}

space_unit_result dock_space_unit(world& w, const space_unit_params& p, const station_params& stp,
                                  entity_id nation, entity_id unit, entity_id station)
{
    const auto uit = w.space_units.find(unit);
    if (uit == w.space_units.end())
        return space_unit_result::rejected_unknown_unit;
    space_unit& u = uit->second;
    if (u.owner != nation)
        return space_unit_result::rejected_not_owner;
    if (u.state != space_unit_state::idle)
        return space_unit_result::rejected_busy;
    const auto sit = w.stations.find(station);
    if (sit == w.stations.end())
        return space_unit_result::rejected_unknown_station;
    // Own stations only: relations expose no cheap "ally" (see the header).
    if (sit->second.owner != nation)
        return space_unit_result::rejected_not_owner;
    if (sit->second.node != u.node)
        return space_unit_result::rejected_wrong_node;
    if (space_station_docked(w, station) >= space_station_berths(w, p, stp, station))
        return space_unit_result::rejected_no_dock;
    u.state   = space_unit_state::docked;
    u.station = station;
    return space_unit_result::ok;
}

space_unit_result undock_space_unit(world& w, entity_id nation, entity_id unit)
{
    const auto uit = w.space_units.find(unit);
    if (uit == w.space_units.end())
        return space_unit_result::rejected_unknown_unit;
    space_unit& u = uit->second;
    if (u.owner != nation)
        return space_unit_result::rejected_not_owner;
    if (u.state != space_unit_state::docked)
        return space_unit_result::rejected_busy;
    u.state   = space_unit_state::idle;
    u.station = null_entity;
    return space_unit_result::ok;
}

space_unit_op refuel_space_unit(world& w, const space_unit_params& p, const station_params& stp,
                                entity_id nation, entity_id unit)
{
    space_unit_op op;
    const auto uit = w.space_units.find(unit);
    if (uit == w.space_units.end())
    {
        op.result = space_unit_result::rejected_unknown_unit;
        return op;
    }
    space_unit& u = uit->second;
    op.unit = unit;
    if (u.owner != nation)
    {
        op.result = space_unit_result::rejected_not_owner;
        return op;
    }
    const space_design_stats st = evaluate_space_design(u.hull, u.design);
    const float need = std::max(0.0f, st.tank - u.propellant);

    if (u.state == space_unit_state::docked)
    {
        // From the depot's stock: as much as it holds, free (the stock is paid for).
        const auto dit = w.station_depot_stock.find(u.station);
        const float stock = dit == w.station_depot_stock.end() ? 0.0f : dit->second;
        if (working_modules(w, stp, u.station, station_module_kind::fuel_depot) == 0 || !(stock > 0.0f)
            || !(need > 0.0f))
        {
            op.result = space_unit_result::rejected_no_fuel;
            return op;
        }
        const float take = std::min(need, stock);
        u.propellant = std::min(st.tank, u.propellant + take);
        dit->second = stock - take;
        if (!(dit->second > 0.0f))
            w.station_depot_stock.erase(dit);
        op.result     = space_unit_result::ok;
        op.propellant = take;
        return op;
    }

    if (u.state != space_unit_state::idle)
    {
        op.result = space_unit_result::rejected_busy;
        return op;
    }
    if (u.node.kind != orbital_node_kind::low_orbit)
    {
        op.result = space_unit_result::rejected_wrong_node;
        return op;
    }
    const entity_id pad = space_staffed_pad_on(w, nation, u.node.body);
    if (pad == null_entity)
    {
        op.result = space_unit_result::rejected_no_pad;
        return op;
    }
    if (!(need > 0.0f))
    {
        op.result = space_unit_result::rejected_no_fuel;
        return op;
    }
    const entity_id market = market_on_body(w, w.buildings.at(pad).tile, u.node.body);
    if (market == null_entity)
    {
        op.result = space_unit_result::rejected_no_market;
        return op;
    }
    std::array<float, resource_count> lot{};
    lot[static_cast<std::size_t>(rt::propellant)] = need;
    const lot_quote q = quote_lot(w, p, market, lot);
    if (!q.shelf_ok)
    {
        op.result = space_unit_result::rejected_shelf_short;
        return op;
    }
    if (!(w.nations.at(nation).treasury >= q.credits))
    {
        op.result = space_unit_result::rejected_insufficient_treasury;
        return op;
    }
    take_lot(w, nation, market, lot, q.credits);
    u.propellant = st.tank;
    op.result     = space_unit_result::ok;
    op.credits    = q.credits;
    op.propellant = need;
    op.market     = market;
    return op;
}

std::vector<entity_id> space_armed_units_at(const world& w, entity_id nation, orbital_node node)
{
    std::vector<entity_id> out;
    for (const auto& [id, u] : w.space_units)
        if (u.owner == nation && u.node == node && !in_transit(u) && space_unit_firepower(u) > 0.0f)
            out.push_back(id);
    return out;
}

bool space_quote_lot(const world& w, const space_unit_params& p, entity_id market,
                     const std::array<float, resource_count>& lot, float* credits)
{
    const lot_quote q = quote_lot(w, p, market, lot);
    if (credits != nullptr)
        *credits = q.credits;
    return q.shelf_ok;
}

void space_take_lot(world& w, entity_id nation, entity_id market,
                    const std::array<float, resource_count>& lot, float credits)
{
    take_lot(w, nation, market, lot, credits);
}

entity_id space_market_on_body(const world& w, entity_id tile, entity_id body)
{
    return market_on_body(w, tile, body);
}

space_unit_result space_raise_ready(const world& w, entity_id nation, space_hull h)
{
    return raise_gate(w, nation, h);
}

float space_unit_firepower(const space_unit& u) { return evaluate_space_design(u.hull, u.design).firepower; }
float space_unit_armour(const space_unit& u) { return evaluate_space_design(u.hull, u.design).armour; }
float space_unit_max_hull_points(const space_unit& u) { return evaluate_space_design(u.hull, u.design).hull_points; }

void run_space_unit_day(world& w, const space_unit_params& p, const station_params& stp,
                        space_unit_day_report* out)
{
    if (w.space_units.empty())
        return;
    space_unit_day_report r;
    // A station's docks are evaluated once a day, ascending station id.
    std::map<entity_id, bool> dock_works;
    for (auto& [id, u] : w.space_units) // ascending
    {
        if (u.state == space_unit_state::in_transit)
        {
            if (--u.days_remaining > 0)
                continue;
            u.days_remaining = 0;
            u.node  = u.dest;
            u.state = (u.node.kind == orbital_node_kind::surface) ? space_unit_state::landed
                                                                   : space_unit_state::idle;
            ++r.arrived;
            continue;
        }
        if (u.state != space_unit_state::docked)
            continue;
        auto [it, fresh] = dock_works.emplace(u.station, false);
        if (fresh)
            it->second = working_modules(w, stp, u.station, station_module_kind::dock) > 0;
        if (!it->second)
        {
            u.state   = space_unit_state::idle;
            u.station = null_entity;
            continue;
        }
        const float max_hp = space_unit_max_hull_points(u);
        const float add = std::min(std::max(0.0f, p.repair_per_day), std::max(0.0f, max_hp - u.hull_points));
        u.hull_points = std::min(max_hp, u.hull_points + add);
        r.repaired += add;
    }
    // BL-1596 (SPACE_FLEETS.md § Space convoys): the convoys fly on the same day, after
    // the arrivals above; an empty store does nothing.
    run_space_convoy_day(w, p, stp, &r.convoy);
    if (out != nullptr)
        *out = r;
}

void run_space_unit_quarter(world& w, const space_unit_params& p, const station_params& stp,
                            int econ_tick, space_unit_quarter_report* out)
{
    (void)econ_tick;
    space_unit_quarter_report r;
    if (w.space_units.empty() && w.station_depot_stock.empty() && w.stations.empty())
    {
        if (out != nullptr)
            *out = r;
        return;
    }

    // 1. Upkeep, ascending unit id; a unit lost here is erased, its crew with it.
    const int32_t to_loss = std::clamp(p.quarters_to_loss, 1, k_space_quarters_to_loss_ceiling);
    for (auto it = w.space_units.begin(); it != w.space_units.end();)
    {
        space_unit& u = it->second;
        const auto nit = w.nations.find(u.owner);
        if (nit != w.nations.end() && nit->second.treasury >= u.upkeep)
        {
            nit->second.treasury -= u.upkeep;
            u.unpaid_quarters = 0;
            r.upkeep_paid += u.upkeep;
            ++r.paid;
            ++it;
            continue;
        }
        ++r.unpaid;
        if (++u.unpaid_quarters >= to_loss)
        {
            ++r.lost;
            r.crew_lost += u.crew;
            it = w.space_units.erase(it);
            continue;
        }
        ++it;
    }

    // BL-1596: a hauler lost above takes its cargo (and any trade riding it) with it.
    prune_space_hauls(w);

    // 2. Depot stock: a station gone takes its stock with it.
    for (auto it = w.station_depot_stock.begin(); it != w.station_depot_stock.end();)
        it = (w.stations.count(it->first) == 0) ? w.station_depot_stock.erase(it) : std::next(it);

    // 3. The fill, ascending station id: up to the cap, off the owner's home shelf,
    // as far as the shelf (under the ceiling) and the treasury allow.
    const std::size_t pr = static_cast<std::size_t>(rt::propellant);
    for (const auto& [sid, st] : w.stations)
    {
        const int depots = working_modules(w, stp, sid, station_module_kind::fuel_depot);
        if (depots == 0)
            continue;
        const float cap = std::min(k_space_depot_stock_max,
                                   static_cast<float>(depots) * std::max(0.0f, p.depot_stock_per_depot));
        const auto dit = w.station_depot_stock.find(sid);
        const float have = dit == w.station_depot_stock.end() ? 0.0f : dit->second;
        float want = cap - have;
        if (!(want > 0.0f))
            continue;
        const entity_id market = home_market_of(w, st.owner);
        const auto mit = w.markets.find(market);
        const auto nit = w.nations.find(st.owner);
        if (mit == w.markets.end() || nit == w.nations.end())
            continue;
        market_component& m = mit->second;
        const float price = posted_price(m, pr);
        if (!shelf_admits(m, pr, p.reservation_mult, true) || !(price > 0.0f) || !std::isfinite(price))
            continue;
        want = std::min(want, std::max(0.0f, m.inventory[pr]));
        want = std::min(want, std::max(0.0f, nit->second.treasury) / price);
        if (!(want > 0.0f))
            continue;
        const float credits = want * price;
        m.inventory[pr] = std::max(0.0f, m.inventory[pr] - want);
        nit->second.treasury -= credits;
        w.station_depot_stock[sid] = have + want;
        r.depot_bought += want;
        r.depot_spent  += credits;
    }
    if (out != nullptr)
        *out = r;
}
