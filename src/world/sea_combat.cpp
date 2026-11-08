#include "sea_combat.hpp"

#include "battle_system.hpp"   // BL-1531: a landing opens the ordinary battle
#include "fronts.hpp"          // BL-1531: a landing occupies what it enters
#include "air_combat.hpp"      // air_wing_flies, wing_stats, wing_strength_permille, air_tile_distance
#include "economy_system.hpp"  // draw_goods_at_tile, step_unit_supply_toward, economy_report
#include "embargo.hpp"         // embargo_refuses
#include "hex_neighbors.hpp"
#include "logistics.hpp"       // convoy_route_tiles, econ_tick_days_world, invalidate_logistics_caches
#include "market_clearing.hpp" // market_for_tile
#include "nation_step.hpp"     // BL-1550: nation_wages_short
#include "province.hpp"
#include "recipe_registry.hpp"
#include "stance.hpp"          // is_hostile
#include "supply_system.hpp"   // interception_record
#include "unit_designs.hpp"    // BL-1527: the ship kit
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <tuple>

// ---------------------------------------------------------------------------
// The sea roster. One row per class; numbers are the class's (Lua). The gate is the
// modern land rows' binary shelf test: steel-or-ore and fuel access.
// ---------------------------------------------------------------------------

const std::vector<ship_roster_row>& ship_roster_table()
{
    static const std::vector<ship_roster_row> rows = {
        {"Attack Submarine", ship_class::submarine, {500, 0, 0, 500}},
        {"Fleet Destroyer",  ship_class::destroyer, {500, 0, 0, 500}},
        {"Capital Ship",     ship_class::capital,   {700, 0, 0, 700}},
        {"Fleet Transport",  ship_class::transport, {400, 0, 0, 400}},
    };
    return rows;
}

std::vector<uint16_t> available_ship_rows(const world& w, entity_id nation)
{
    std::vector<uint16_t> out;
    if (w.nations.find(nation) == w.nations.end())
        return out;
    const campaign_roster_gate_input g = campaign_gate_input(w, nation);
    const std::vector<ship_roster_row>& t = ship_roster_table();
    for (std::size_t i = 0; i < t.size(); ++i)
    {
        const roster_gate& r = t[i].gate;
        if (g.ore_q >= r.ore_q && g.farm_q >= r.farm_q && g.port_q >= r.port_q
            && g.energy_q >= r.energy_q)
            out.push_back(static_cast<uint16_t>(i));
    }
    return out;
}

namespace {
/// BL-1533: a ship that leaves the world leaves every side's sightings with it, so a sighting
/// always names a standing ship (the reader refuses one that does not).
void forget_sightings_of(world& w, entity_id ship)
{
    for (auto it = w.sea_sightings.begin(); it != w.sea_sightings.end();)
        it = it->second == ship ? w.sea_sightings.erase(it) : std::next(it);
}
} // namespace

ship_class ship_class_of(const sea_ship& s)
{
    const auto& t = ship_roster_table();
    return s.type < t.size() ? t[s.type].cls : ship_class::transport;
}

ship_live_stats ship_stats(const recipe_registry& reg, const sea_ship& s)
{
    const ship_class c = ship_class_of(s);
    const ship_class_stats& b = reg.military().sea.cls[static_cast<std::size_t>(c)];
    const ship_combat_stats k = ship_kit_delta(c, s.kit);
    ship_live_stats o;
    o.hp        = std::max(1, b.hp + k.hp);
    o.attack    = std::max(0, b.attack + k.attack);
    o.detection = std::max(0, b.detection + k.detection);
    o.stealth   = std::max(0, b.stealth + k.stealth);
    o.days_per_tile_permille = std::max(1, b.days_per_tile_permille + k.days_per_tile_permille);
    o.capacity  = std::max(0, b.capacity + k.capacity);
    o.targets   = static_cast<uint8_t>(b.targets | k.targets);
    return o;
}

corp_command_result ship_design_check(const world& w, entity_id nation, uint32_t cls,
                                      uint16_t slot, uint16_t item)
{
    // BL-1533 (AI_OPPONENT.md § 11, the fleet grant read 2026-11-08): any nation, for its
    // own designs. The seam has already pinned the actor to the nation it acts as.
    if (w.nations.find(nation) == w.nations.end())
        return corp_command_result::rejected_not_owner;
    if (cls >= static_cast<uint32_t>(ship_class_count) || slot >= k_ship_kit_slot_count)
        return corp_command_result::rejected_invalid;
    const ship_class c = static_cast<ship_class>(cls);
    const std::vector<ship_kit_item>& cat = ship_kit_catalogue();
    if (item >= cat.size() || cat[item].cls != c || cat[item].slot != slot)
        return corp_command_result::rejected_invalid;
    if (!ship_kit_item_researched(w, nation, item))
        return corp_command_result::rejected_tech_locked;
    if (ship_design_fitted(w, nation, c, slot) == item)
        return corp_command_result::rejected_state;
    return corp_command_result::applied;
}

corp_command_result refit_ship_check(const world& w, entity_id nation, entity_id ship)
{
    const auto it = w.ships.find(ship);
    if (it == w.ships.end())
        return corp_command_result::rejected_invalid;
    const sea_ship& s = it->second;
    if (s.owner != nation)
        return corp_command_result::rejected_not_owner;
    if (s.kit.fit == ship_design_of(w, nation, ship_class_of(s)).fit)
        return corp_command_result::rejected_state; // already the live design
    if (s.muster_days > 0 || s.mission != sea_mission::none)
        return corp_command_result::rejected_state; // raising, or still holding a mission
    if (!s.route.empty() || s.tile == null_entity || s.tile != port_mouth_tile(w, s.home_port))
        return corp_command_result::rejected_placement;
    // Its home must still be its nation's standing port, the rule a hire meets.
    const auto port = w.buildings.find(s.home_port);
    if (port == w.buildings.end() || own_port_at(w, nation, port->second.tile) != s.home_port)
        return corp_command_result::rejected_placement;
    return corp_command_result::applied;
}

int ship_crew(const recipe_registry& reg, uint16_t row)
{
    const auto& t = ship_roster_table();
    if (row >= t.size())
        return 0;
    return std::max(0, reg.military().sea.cls[static_cast<std::size_t>(t[row].cls)].crew);
}

bool ship_class_works(const recipe_registry& reg, ship_class c, sea_mission m)
{
    if (m == sea_mission::none)
        return true;
    return (reg.military().sea.cls[static_cast<std::size_t>(c)].missions & sea_mission_bit(m)) != 0;
}

int ship_strength_permille(const sea_ship& s)
{
    const int64_t hp = std::max<int64_t>(0, s.hp_ppm) * 1000 / k_ship_hp_full;
    return static_cast<int>(hp * std::max(0, s.morale_permille) / 1000
                            * std::max(0, s.supply_factor_permille) / 1000);
}

// ---------------------------------------------------------------------------
// Water
// ---------------------------------------------------------------------------

namespace {

bool hostile_either(const world& w, entity_id a, entity_id b)
{
    return a != b && (is_hostile(w, a, b) || is_hostile(w, b, a));
}

/// One body's raster, read-only: the derived cache where it is built, else built here.
/// Both are the same row-major table, so the check's answer does not depend on which.
const std::vector<entity_id>& raster_of(const world& w, entity_id body, std::vector<entity_id>& local)
{
    const auto it = w.body_tile_index.find(body);
    if (it != w.body_tile_index.end())
        return it->second;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return local;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    local.assign(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh), null_entity);
    for (const auto& [tid, tc] : w.tiles)
        if (tc.body == body && tc.grid_x >= 0 && tc.grid_x < gw && tc.grid_y >= 0 && tc.grid_y < gh)
            local[static_cast<std::size_t>(tc.grid_y) * gw + tc.grid_x] = tid;
    return local;
}

/// The six neighbours of grid cell (@p gx, @p gy), in side order, cylinder wrap east-west.
/// A cell off the top or bottom reads as -1.
void neighbour_cells(int gx, int gy, int gw, int gh, int (&out)[6])
{
    for (int side = 0; side < 6; ++side)
    {
        const hex_neighbors::coord c = hex_neighbors::neighbour(gx, gy, side);
        if (c.gy < 0 || c.gy >= gh || gw <= 0)
        {
            out[side] = -1;
            continue;
        }
        const int x = ((c.gx % gw) + gw) % gw;
        out[side] = c.gy * gw + x;
    }
}

/// BL-1531: who holds port @p bid now: its occupier, else its owner.
entity_id port_holder(const world& w, entity_id bid)
{
    const auto taken = w.tile_occupier.find(w.buildings.at(bid).tile);
    return taken != w.tile_occupier.end() ? taken->second : owner_nation_of(w, bid);
}

bool is_sea_tile_id(const world& w, entity_id t)
{
    const auto it = w.tiles.find(t);
    return it != w.tiles.end() && is_sea(it->second.substrate);
}

/// @p nation's own completed, standing ports, ascending id.
std::vector<entity_id> own_ports(const world& w, entity_id nation)
{
    std::vector<entity_id> out;
    const std::vector<entity_id>* assets = assets_of(w, nation);
    if (assets == nullptr)
        return out;
    for (const entity_id bid : *assets)
    {
        const auto bit = w.buildings.find(bid);
        if (bit != w.buildings.end() && bit->second.type == building_type::port
            && bit->second.ticks_remaining <= 0 && !bit->second.decommissioned)
            out.push_back(bid);
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

bool province_holds(const std::vector<uint32_t>& set, uint32_t p)
{
    return std::binary_search(set.begin(), set.end(), p);
}

/// Every tile of every province in @p set, the course a ship may end on.
std::vector<entity_id> zone_tiles(const world& w, const std::vector<uint32_t>& set)
{
    std::vector<entity_id> out;
    for (const uint32_t id : set)
        if (const ::province* p = w.provinces.find(id))
            out.insert(out.end(), p->tiles.begin(), p->tiles.end());
    return out;
}

} // namespace

entity_id port_mouth_tile(const world& w, entity_id port)
{
    const auto bit = w.buildings.find(port);
    if (bit == w.buildings.end())
        return null_entity;
    const entity_id start = bit->second.tile;
    const auto sit = w.tiles.find(start);
    if (sit == w.tiles.end())
        return null_entity;
    if (is_sea(sit->second.substrate))
        return start;
    const auto bodyit = w.bodies.find(sit->second.body);
    if (bodyit == w.bodies.end())
        return null_entity;
    const int gw = bodyit->second.grid_width, gh = bodyit->second.grid_height;
    std::vector<entity_id> local;
    const std::vector<entity_id>& grid = raster_of(w, sit->second.body, local);
    std::vector<int> frontier{sit->second.grid_y * gw + sit->second.grid_x};
    std::set<int> seen(frontier.begin(), frontier.end());
    for (int step = 0; step < 2; ++step)
    {
        std::vector<int> next;
        for (const int cell : frontier)
        {
            int nb[6];
            neighbour_cells(cell % gw, cell / gw, gw, gh, nb);
            for (const int n : nb)
            {
                if (n < 0 || !seen.insert(n).second)
                    continue;
                const entity_id t = grid[static_cast<std::size_t>(n)];
                if (t != null_entity && is_sea_tile_id(w, t))
                    return t;
                next.push_back(n);
            }
        }
        frontier = std::move(next);
    }
    return null_entity;
}

std::vector<entity_id> sea_route(const world& w, entity_id from, const std::vector<entity_id>& goals,
                                 bool* reachable)
{
    std::vector<entity_id> route;
    if (reachable != nullptr)
        *reachable = false;
    const auto sit = w.tiles.find(from);
    if (sit == w.tiles.end() || !is_sea(sit->second.substrate))
        return route;
    const std::set<entity_id> goal(goals.begin(), goals.end());
    if (goal.count(from) != 0)
    {
        if (reachable != nullptr)
            *reachable = true;
        return route;
    }
    const auto bodyit = w.bodies.find(sit->second.body);
    if (bodyit == w.bodies.end())
        return route;
    const int gw = bodyit->second.grid_width, gh = bodyit->second.grid_height;
    std::vector<entity_id> local;
    const std::vector<entity_id>& grid = raster_of(w, sit->second.body, local);
    if (grid.empty())
        return route;
    std::vector<int> parent(grid.size(), -2);
    const int start = sit->second.grid_y * gw + sit->second.grid_x;
    parent[static_cast<std::size_t>(start)] = -1;
    std::deque<int> q{start};
    int found = -1;
    while (!q.empty() && found < 0)
    {
        const int cell = q.front();
        q.pop_front();
        int nb[6];
        neighbour_cells(cell % gw, cell / gw, gw, gh, nb);
        for (const int n : nb) // side order: the tie-break, so a route is one route
        {
            if (n < 0 || parent[static_cast<std::size_t>(n)] != -2)
                continue;
            const entity_id t = grid[static_cast<std::size_t>(n)];
            if (t == null_entity || !is_sea_tile_id(w, t))
                continue;
            parent[static_cast<std::size_t>(n)] = cell;
            if (goal.count(t) != 0)
            {
                found = n;
                break;
            }
            q.push_back(n);
        }
    }
    if (found < 0)
        return route;
    for (int c = found; c != start; c = parent[static_cast<std::size_t>(c)])
        route.push_back(grid[static_cast<std::size_t>(c)]);
    std::reverse(route.begin(), route.end());
    if (reachable != nullptr)
        *reachable = true;
    return route;
}

bool sea_province_in_range(const world& w, entity_id nation, uint32_t province_id, int range)
{
    const ::province* pr = w.provinces.find(province_id);
    if (pr == nullptr || pr->tiles.empty())
        return false;
    const auto bodyit = w.bodies.find(pr->body);
    if (bodyit == w.bodies.end())
        return false;
    const int gw = bodyit->second.grid_width, gh = bodyit->second.grid_height;
    std::vector<entity_id> local;
    const std::vector<entity_id>& grid = raster_of(w, pr->body, local);
    if (grid.empty())
        return false;
    const std::set<entity_id> goal(pr->tiles.begin(), pr->tiles.end());

    std::vector<int> frontier;
    std::set<int> seen;
    for (const entity_id port : own_ports(w, nation))
    {
        const entity_id mouth = port_mouth_tile(w, port);
        const auto mit = w.tiles.find(mouth);
        if (mit == w.tiles.end() || mit->second.body != pr->body)
            continue;
        if (goal.count(mouth) != 0)
            return true;
        const int cell = mit->second.grid_y * gw + mit->second.grid_x;
        if (seen.insert(cell).second)
            frontier.push_back(cell);
    }
    for (int step = 0; step < range && !frontier.empty(); ++step)
    {
        std::vector<int> next;
        for (const int cell : frontier)
        {
            int nb[6];
            neighbour_cells(cell % gw, cell / gw, gw, gh, nb);
            for (const int n : nb)
            {
                if (n < 0 || seen.count(n) != 0)
                    continue;
                const entity_id t = grid[static_cast<std::size_t>(n)];
                if (t == null_entity || !is_sea_tile_id(w, t))
                    continue;
                if (goal.count(t) != 0)
                    return true;
                seen.insert(n);
                next.push_back(n);
            }
        }
        frontier = std::move(next);
    }
    return false;
}

std::vector<entity_id> own_standing_ports(const world& w, entity_id nation)
{
    return own_ports(w, nation);
}

std::set<uint32_t> sea_provinces_in_range(const world& w, entity_id nation, int range)
{
    // The same walk as `sea_province_in_range`, from every port at once and per body: a
    // province is in reach when the walk touches one of its tiles within @p range steps.
    std::set<uint32_t> out;
    std::map<entity_id, std::vector<entity_id>> mouths_by_body; // ordered
    for (const entity_id port : own_ports(w, nation))
    {
        const entity_id mouth = port_mouth_tile(w, port);
        const auto mit = w.tiles.find(mouth);
        if (mit != w.tiles.end())
            mouths_by_body[mit->second.body].push_back(mouth);
    }
    for (const auto& [body, mouths] : mouths_by_body)
    {
        const auto bodyit = w.bodies.find(body);
        if (bodyit == w.bodies.end())
            continue;
        const int gw = bodyit->second.grid_width, gh = bodyit->second.grid_height;
        std::vector<entity_id> local;
        const std::vector<entity_id>& grid = raster_of(w, body, local);
        if (grid.empty())
            continue;
        std::vector<int> frontier;
        std::set<int> seen;
        for (const entity_id mouth : mouths)
        {
            const tile_component& t = w.tiles.at(mouth);
            const int cell = t.grid_y * gw + t.grid_x;
            if (seen.insert(cell).second)
                frontier.push_back(cell);
            if (const uint32_t p = w.provinces.province_of(mouth); p != 0)
                out.insert(p);
        }
        for (int step = 0; step < range && !frontier.empty(); ++step)
        {
            std::vector<int> next;
            for (const int cell : frontier)
            {
                int nb[6];
                neighbour_cells(cell % gw, cell / gw, gw, gh, nb);
                for (const int n : nb)
                {
                    if (n < 0 || seen.count(n) != 0)
                        continue;
                    const entity_id t = grid[static_cast<std::size_t>(n)];
                    if (t == null_entity || !is_sea_tile_id(w, t))
                        continue;
                    seen.insert(n);
                    next.push_back(n);
                    if (const uint32_t p = w.provinces.province_of(t); p != 0)
                        out.insert(p);
                }
            }
            frontier = std::move(next);
        }
    }
    return out;
}

std::vector<entity_id> held_water_hostile_coast(const world& w, const recipe_registry& reg,
                                                entity_id nation, int range)
{
    std::set<entity_id> out;
    for (const uint32_t pid : sea_provinces_in_range(w, nation, range))
    {
        if (province_kind_of(w, pid) == province_kind::land || !sea_holds(w, reg, pid, nation))
            continue;
        const ::province* pr = w.provinces.find(pid);
        if (pr == nullptr)
            continue;
        const auto bodyit = w.bodies.find(pr->body);
        if (bodyit == w.bodies.end())
            continue;
        const int gw = bodyit->second.grid_width, gh = bodyit->second.grid_height;
        std::vector<entity_id> local;
        const std::vector<entity_id>& grid = raster_of(w, pr->body, local);
        if (grid.empty())
            continue;
        for (const entity_id st : pr->tiles)
        {
            const auto sit = w.tiles.find(st);
            if (sit == w.tiles.end() || !is_sea_tile_id(w, st))
                continue;
            int nb[6];
            neighbour_cells(sit->second.grid_x, sit->second.grid_y, gw, gh, nb);
            for (const int n : nb)
            {
                if (n < 0)
                    continue;
                const entity_id t = grid[static_cast<std::size_t>(n)];
                if (t == null_entity || is_sea_tile_id(w, t))
                    continue;
                const auto own = w.tile_to_nation.find(t);
                if (own != w.tile_to_nation.end() && own->second != nation
                    && is_hostile(w, nation, own->second))
                    out.insert(t);
            }
        }
    }
    return std::vector<entity_id>(out.begin(), out.end());
}

// ---------------------------------------------------------------------------
// The doors
// ---------------------------------------------------------------------------

entity_id own_port_at(const world& w, entity_id nation, entity_id tile)
{
    for (const entity_id bid : own_ports(w, nation)) // ascending: the lowest id
        if (w.buildings.at(bid).tile == tile)
            return bid;
    return null_entity;
}

corp_command_result hire_ship_check(const world& w, const recipe_registry& reg, entity_id nation,
                                    entity_id tile, uint16_t row, entity_id* port_out)
{
    if (row >= ship_roster_table().size())
        return corp_command_result::rejected_invalid;
    const std::vector<uint16_t> ok = available_ship_rows(w, nation);
    if (std::find(ok.begin(), ok.end(), row) == ok.end())
        return corp_command_result::rejected_invalid;
    if (w.tiles.find(tile) == w.tiles.end())
        return corp_command_result::rejected_invalid;
    const entity_id port = own_port_at(w, nation, tile);
    if (port == null_entity || port_mouth_tile(w, port) == null_entity)
        return corp_command_result::rejected_placement;
    const auto nit = w.nations.find(nation);
    if (nit->second.sea_recruits < ship_crew(reg, row))
        return corp_command_result::rejected_no_recruits;
    if (port_out != nullptr)
        *port_out = port;
    return corp_command_result::applied;
}

corp_command_result sea_mission_check(const world& w, const recipe_registry& reg,
                                      entity_id nation, entity_id ship, uint32_t mission,
                                      const uint32_t* provinces, std::size_t n,
                                      std::vector<uint32_t>* out_set)
{
    const auto sit = w.ships.find(ship);
    if (sit == w.ships.end())
        return corp_command_result::rejected_invalid;
    const sea_ship& s = sit->second;
    if (s.owner != nation)
        return corp_command_result::rejected_not_owner;
    if (mission >= static_cast<uint32_t>(sea_mission_count))
        return corp_command_result::rejected_invalid;
    const sea_mission m = static_cast<sea_mission>(mission);
    const ship_class cls = ship_class_of(s);
    if (!ship_class_works(reg, cls, m))
        return corp_command_result::rejected_invalid;

    std::vector<uint32_t> set;
    for (std::size_t i = 0; i < n; ++i)
        if (provinces[i] != no_province)
            set.push_back(provinces[i]);
    std::sort(set.begin(), set.end());
    if (std::adjacent_find(set.begin(), set.end()) != set.end()
        || set.size() > k_sea_mission_provinces_max)
        return corp_command_result::rejected_invalid;
    if ((m == sea_mission::none) != set.empty())
        return corp_command_result::rejected_invalid;
    for (const uint32_t id : set)
        if (w.provinces.find(id) == nullptr)
            return corp_command_result::rejected_invalid;

    const int range = reg.military().sea.cls[static_cast<std::size_t>(cls)].range;
    for (const uint32_t id : set)
    {
        if (province_kind_of(w, id) == province_kind::land)
            return corp_command_result::rejected_placement;
        if (!sea_province_in_range(w, nation, id, range))
            return corp_command_result::rejected_placement;
    }
    if (!set.empty())
    {
        bool reachable = false;
        (void)sea_route(w, s.tile, zone_tiles(w, set), &reachable);
        if (!reachable)
            return corp_command_result::rejected_placement;
    }
    // BL-1530 (MILITARY.md § Sea, Blockade): a blockade lies at the mouth of a port of a nation
    // its nation has declared hostile, every province of it. Anywhere else it is a raid.
    if (m == sea_mission::blockade)
        for (const uint32_t id : set)
        {
            bool mouth = false;
            for (const auto& [bid, b] : w.buildings) // a pure any-of
                if (b.type == building_type::port && b.ticks_remaining <= 0 && !b.decommissioned
                    && port_holder(w, bid) != nation && is_hostile(w, nation, port_holder(w, bid))
                    && w.provinces.province_of(port_mouth_tile(w, bid)) == id)
                {
                    mouth = true;
                    break;
                }
            if (!mouth)
                return corp_command_result::rejected_placement;
        }
    if (m == s.mission && set == s.provinces)
        return corp_command_result::rejected_state;
    if (out_set != nullptr)
        *out_set = std::move(set);
    return corp_command_result::applied;
}

namespace {

/// Set @p s's course from where it stands: home when it has broken off or has no mission,
/// else to the zone unless it already stands in it.
/// BL-1531: the sea tiles touching @p coast, ascending. Empty when @p coast is water or no tile.
std::vector<entity_id> landing_mouths(const world& w, entity_id coast)
{
    std::vector<entity_id> out;
    const auto cit = w.tiles.find(coast);
    if (cit == w.tiles.end() || is_water(cit->second.substrate))
        return out;
    const auto bit = w.bodies.find(cit->second.body);
    if (bit == w.bodies.end())
        return out;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    std::vector<entity_id> local;
    const std::vector<entity_id>& raster = raster_of(w, cit->second.body, local);
    int cells[6];
    neighbour_cells(cit->second.grid_x, cit->second.grid_y, gw, gh, cells);
    for (const int cell : cells)
        if (cell >= 0 && static_cast<std::size_t>(cell) < raster.size()
            && is_sea_tile_id(w, raster[static_cast<std::size_t>(cell)]))
            out.push_back(raster[static_cast<std::size_t>(cell)]);
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    return out;
}

/// BL-1531: what a landing on @p tile needs from @p nation's side.
enum class landing_ground : uint8_t { refused, open, needs_hold };
landing_ground landing_ground_of(const world& w, entity_id nation, entity_id tile)
{
    const entity_id holder = w.tile_controller(tile);
    if (holder == null_entity || holder == nation)
        return landing_ground::open;
    if (hostile_either(w, nation, holder))
        return landing_ground::needs_hold;
    return are_friends(w, nation, holder) ? landing_ground::open : landing_ground::refused;
}

/// BL-1531: the port a unit on @p tile embarks at: on it, else beside it, lowest id.
entity_id embark_port(const world& w, entity_id nation, entity_id tile)
{
    entity_id best = own_port_at(w, nation, tile);
    if (best != null_entity)
        return best;
    const auto cit = w.tiles.find(tile);
    if (cit == w.tiles.end())
        return null_entity;
    const auto bit = w.bodies.find(cit->second.body);
    if (bit == w.bodies.end())
        return null_entity;
    std::vector<entity_id> local;
    const std::vector<entity_id>& raster = raster_of(w, cit->second.body, local);
    int cells[6];
    neighbour_cells(cit->second.grid_x, cit->second.grid_y, bit->second.grid_width, bit->second.grid_height, cells);
    for (const int cell : cells)
    {
        if (cell < 0 || static_cast<std::size_t>(cell) >= raster.size())
            continue;
        const entity_id p = own_port_at(w, nation, raster[static_cast<std::size_t>(cell)]);
        if (p != null_entity && (best == null_entity || p < best))
            best = p;
    }
    return best;
}

/// BL-1531: units in the surf off transport @p ship, ascending.
std::vector<entity_id> surf_of(const world& w, entity_id ship)
{
    std::vector<entity_id> out;
    for (const auto& [uid, u] : w.units) // sorted below
        if (u.landing_ship == ship && u.count > 0)
            out.push_back(uid);
    std::sort(out.begin(), out.end());
    return out;
}

/// BL-1531: everyone aboard @p ship and in the surf off it goes down with it. @return units lost.
int lose_passengers(world& w, entity_id ship, sea_ship& s)
{
    int lost = static_cast<int>(s.aboard.size());
    s.aboard.clear();
    for (const entity_id uid : surf_of(w, ship))
    {
        // Left listed for the upkeep pass's cleanup, as a battle's dead are.
        w.units.at(uid).count = 0;
        ++lost;
    }
    return lost;
}

void plot_course(world& w, sea_ship& s)
{
    s.route.clear();
    if (s.landing_tile != null_entity && !s.returning)
    {
        const std::vector<entity_id> mouths = landing_mouths(w, s.landing_tile);
        if (!std::binary_search(mouths.begin(), mouths.end(), s.tile))
            s.route = sea_route(w, s.tile, mouths);
    }
    else if (s.returning || s.mission == sea_mission::none)
    {
        const entity_id mouth = port_mouth_tile(w, s.home_port);
        if (mouth != null_entity)
            s.route = sea_route(w, s.tile, {mouth});
    }
    else if (!province_holds(s.provinces, w.provinces.province_of(s.tile)))
    {
        s.route = sea_route(w, s.tile, zone_tiles(w, s.provinces));
    }
    if (s.route.empty())
        s.sail_bank_permille = 0;
}

/// One present ship in one province, weighted by the share of its day spent there.
struct presence
{
    entity_id ship   = null_entity;
    int       weight = 1000;
};

uint64_t mix(uint64_t h, uint64_t v)
{
    h ^= v + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
    h ^= h >> 30;
    h *= 0xBF58476D1CE4E5B9ull;
    h ^= h >> 27;
    h *= 0x94D049BB133111EBull;
    return h ^ (h >> 31);
}

/// Is @p s raised and at work in its zone: not broken off, no course left, in a province of it?
bool on_station(const world& w, const sea_ship& s)
{
    return s.muster_days <= 0 && !s.returning && s.mission != sea_mission::none && s.route.empty()
           && province_holds(s.provinces, w.provinces.province_of(s.tile));
}

/// Who is where today. A ship on station is spread over its zone at 1/N each; any other
/// raised ship is in the province under it. Ascending ship id inside each province.
std::map<uint32_t, std::vector<presence>> sea_presence(const world& w)
{
    std::map<uint32_t, std::vector<presence>> present;
    for (const auto& [sid, s] : w.ships)
    {
        if (s.muster_days > 0)
            continue;
        if (on_station(w, s))
        {
            const int weight = 1000 / static_cast<int>(s.provinces.size());
            for (const uint32_t p : s.provinces)
                present[p].push_back({sid, weight});
        }
        else if (const uint32_t here = w.provinces.province_of(s.tile); here != 0)
        {
            present[here].push_back({sid, 1000});
        }
    }
    return present;
}

/// The day's roll: does @p viewer, at detection @p d, find @p ship of stealth @p st in @p province_id?
/// A pure hash, so the map's read and the daily pass agree.
bool detection_roll(uint64_t seed, int day_tick, uint32_t province_id, entity_id viewer, entity_id ship,
                    int64_t d, int64_t st)
{
    const int64_t chance = d * 1000 / (d + st);
    uint64_t h = mix(seed, static_cast<uint64_t>(static_cast<int64_t>(day_tick)));
    h = mix(h, province_id);
    h = mix(h, viewer);
    h = mix(h, ship);
    return static_cast<int64_t>(h % 1000) < chance;
}

/// Each side's detection in @p province_id: its ships', a patrol's raised, plus its recon wings'.
std::map<entity_id, int64_t> side_detection(const world& w, const recipe_registry& reg, uint32_t province_id,
                                            const std::vector<presence>& ships,
                                            std::map<entity_id, int64_t>* recon_points)
{
    const sea_params& sp = reg.military().sea;
    std::map<entity_id, int64_t> detection;
    for (const presence& pc : ships)
    {
        const sea_ship& s = w.ships.at(pc.ship);
        int64_t d = static_cast<int64_t>(ship_stats(reg, s).detection) * ship_strength_permille(s) / 1000
                    * pc.weight / 1000;
        // Raised only on station: a patrol in transit reads as any other ship.
        if (s.mission == sea_mission::patrol && on_station(w, s))
            d = d * sp.patrol_detection_permille / 1000;
        detection[s.owner] += d;
    }
    for (const auto& [wid, wg] : w.wings)
    {
        if (wg.mission != air_mission::recon || !air_wing_flies(w, wg)
            || !std::binary_search(wg.provinces.begin(), wg.provinces.end(), province_id))
            continue;
        const int64_t rp = static_cast<int64_t>(wing_stats(reg, wg).recon)
                           * wing_strength_permille(reg, wg) / 1000
                           / static_cast<int64_t>(wg.provinces.size());
        if (rp > 0)
        {
            detection[wg.owner] += rp;
            if (recon_points != nullptr)
                (*recon_points)[wg.owner] += rp;
        }
    }
    return detection;
}

/// The surface force @p s brings to a province it works, at @p weight: nothing for a class
/// or a mission that does not count (MILITARY.md § Sea).
int64_t surface_force(const world& w, const recipe_registry& reg, const sea_ship& s, int weight)
{
    const ship_class c = ship_class_of(s);
    if ((c != ship_class::destroyer && c != ship_class::capital) || !on_station(w, s)
        || (s.mission != sea_mission::patrol && s.mission != sea_mission::escort
            && s.mission != sea_mission::blockade))
        return 0;
    return static_cast<int64_t>(ship_stats(reg, s).attack) * ship_strength_permille(s) / 1000 * weight / 1000;
}

/// The market a prize of @p raider's lands on: its nation's nearest own port within its class's
/// range over water of @p from whose market will take its goods (the embargo holds at this
/// border as at any). Null with none.
entity_id prize_market_for(const world& w, const recipe_registry& reg, const sea_ship& raider, entity_id from)
{
    std::vector<entity_id> mouths;
    std::map<entity_id, entity_id> port_at_mouth; // the lowest port id at each mouth
    for (const entity_id port : own_ports(w, raider.owner))
    {
        const entity_id market = market_for_tile(w, w.buildings.at(port).tile);
        if (market == null_entity || embargo_refuses(w, raider.owner, market)
            || w.closed_ports.count(w.buildings.at(port).tile) != 0)
            continue;
        const entity_id mouth = port_mouth_tile(w, port);
        if (mouth != null_entity && port_at_mouth.emplace(mouth, port).second)
            mouths.push_back(mouth);
    }
    if (mouths.empty())
        return null_entity;
    bool reachable = false;
    const std::vector<entity_id> path = sea_route(w, from, mouths, &reachable);
    if (!reachable)
        return null_entity;
    const int range = reg.military().sea.cls[static_cast<std::size_t>(ship_class_of(raider))].range;
    if (static_cast<int>(path.size()) > range)
        return null_entity;
    const entity_id at = path.empty() ? from : path.back();
    return market_for_tile(w, w.buildings.at(port_at_mouth.at(at)).tile);
}

} // namespace

corp_command_result embark_check(const world& w, const recipe_registry& reg, entity_id nation,
                                 entity_id unit, entity_id ship)
{
    const auto uit = w.units.find(unit);
    if (uit == w.units.end())
        return corp_command_result::rejected_invalid;
    const unit_component& u = uit->second;
    if (u.owner != nation)
        return corp_command_result::rejected_not_owner;
    const auto& rows = unit_roster_table();
    if (u.count <= 0 || u.type >= rows.size() || (rows[u.type].domains & domain_bit(traversal_domain::land)) == 0)
        return corp_command_result::rejected_invalid;
    const auto sit = w.ships.find(ship);
    if (sit == w.ships.end())
        return corp_command_result::rejected_invalid;
    const sea_ship& s = sit->second;
    if (s.owner != nation)
        return corp_command_result::rejected_not_owner;
    if (ship_class_of(s) != ship_class::transport)
        return corp_command_result::rejected_invalid;
    if (unit_in_battle(w, unit) || unit_refitting(u) || u.landing_ship != null_entity
        || s.muster_days > 0 || s.returning || !s.route.empty() || s.landing_tile != null_entity
        || static_cast<int>(s.aboard.size()) >= ship_stats(reg, s).capacity)
        return corp_command_result::rejected_state;
    const entity_id port = embark_port(w, nation, u.position);
    if (port == null_entity)
        return corp_command_result::rejected_placement;
    const building_component& b = w.buildings.at(port);
    if (w.closed_ports.count(b.tile) != 0 || port_mouth_tile(w, port) != s.tile
        || w.tile_occupier.count(b.tile) != 0) // an enemy holds it
        return corp_command_result::rejected_placement;
    return corp_command_result::applied;
}

void embark_unit(world& w, entity_id unit, entity_id ship)
{
    unit_component u = w.units.at(unit);
    u.position = null_entity;
    u.order = movement_order{};
    u.front = null_entity;
    u.defend_province = 0xFFFFFFFFu;
    u.defend_post = null_entity;
    u.tile_since_day = -1;
    u.battle_ready_carry = 0;
    u.battle_hp_carry = 0;
    u.landing_ship = null_entity;
    u.muster_base = null_entity; // a unit that has sailed answers to no base it left behind
    w.forget_air_unit(unit);
    w.units.erase(unit);
    std::vector<std::pair<entity_id, unit_component>>& a = w.ships.at(ship).aboard;
    const auto at = std::lower_bound(a.begin(), a.end(), unit,
                                     [](const auto& e, entity_id id) { return e.first < id; });
    a.insert(at, {unit, std::move(u)});
}

namespace {
/// The landing door's legs. @p allow_empty lets an empty ship pass the "units aboard" leg;
/// @p extra_type, when >= 0, is a land row taken as aboard too: what a landing WOULD be.
corp_command_result landing_check_core(const world& w, const recipe_registry& reg, entity_id nation,
                                       entity_id ship, entity_id tile, bool allow_empty, int extra_type)
{
    const auto sit = w.ships.find(ship);
    if (sit == w.ships.end())
        return corp_command_result::rejected_invalid;
    const sea_ship& s = sit->second;
    if (s.owner != nation)
        return corp_command_result::rejected_not_owner;
    if (ship_class_of(s) != ship_class::transport)
        return corp_command_result::rejected_invalid;
    if (!surf_of(w, ship).empty())
        return corp_command_result::rejected_state;
    if (tile == null_entity)
        return s.landing_tile != null_entity ? corp_command_result::applied : corp_command_result::rejected_state;
    if (s.muster_days > 0 || s.returning || (s.aboard.empty() && !allow_empty))
        return corp_command_result::rejected_state;
    const std::vector<entity_id> mouths = landing_mouths(w, tile);
    if (mouths.empty() || landing_ground_of(w, nation, tile) == landing_ground::refused)
        return corp_command_result::rejected_placement;
    const int range = reg.military().sea.cls[static_cast<std::size_t>(ship_class_of(s))].range;
    bool in_range = false;
    for (const entity_id m : mouths)
    {
        bool reachable = false;
        (void)sea_route(w, s.tile, {m}, &reachable);
        in_range = in_range || (reachable && sea_province_in_range(w, nation, w.provinces.province_of(m), range));
    }
    if (!in_range)
        return corp_command_result::rejected_placement;
    const tile_component& ground = w.tiles.at(tile);
    for (const auto& [uid, u] : s.aboard)
        if (terrain_class_effects_for_type(reg.military(), u.type, ground).walled)
            return corp_command_result::rejected_terrain;
    if (extra_type >= 0
        && terrain_class_effects_for_type(reg.military(), static_cast<uint16_t>(extra_type), ground).walled)
        return corp_command_result::rejected_terrain;
    return corp_command_result::applied;
}
} // namespace

corp_command_result land_units_check(const world& w, const recipe_registry& reg, entity_id nation,
                                     entity_id ship, entity_id tile)
{
    return landing_check_core(w, reg, nation, ship, tile, false, -1);
}

corp_command_result landing_would_admit(const world& w, const recipe_registry& reg, entity_id nation,
                                        entity_id ship, entity_id tile, int unit_type)
{
    if (tile == null_entity)
        return corp_command_result::rejected_invalid;
    return landing_check_core(w, reg, nation, ship, tile, true, unit_type);
}

void set_ship_landing(world& w, const recipe_registry& reg, sea_ship& s, entity_id tile)
{
    s.landing_tile = tile;
    s.landing_days = tile != null_entity ? reg.military().sea.landing_days : 0;
    plot_course(w, s);
}

void set_ship_mission(world& w, sea_ship& s, sea_mission m, std::vector<uint32_t> provinces)
{
    s.mission   = m;
    s.provinces = std::move(provinces);
    plot_course(w, s);
}

// ---------------------------------------------------------------------------
// Sea control
// ---------------------------------------------------------------------------

namespace {

/// Each side's surface force in one province today.
std::map<entity_id, int64_t> surface_forces(const world& w, const recipe_registry& reg, uint32_t province)
{
    std::map<entity_id, int64_t> out;
    for (const auto& [sid, s] : w.ships)
        if (province_holds(s.provinces, province) && !s.provinces.empty())
            if (const int64_t f = surface_force(w, reg, s, 1000 / static_cast<int>(s.provinces.size())); f > 0)
                out[s.owner] += f;
    return out;
}

int share_of(const world& w, const std::map<entity_id, int64_t>& sides, entity_id nation)
{
    const auto own = sides.find(nation);
    if (own == sides.end())
        return 0;
    int64_t hostile = 0;
    for (const auto& [side, f] : sides)
        if (side != nation && hostile_either(w, nation, side))
            hostile += f;
    return static_cast<int>(own->second * 1000 / (own->second + hostile));
}

} // namespace

int sea_control_share_permille(const world& w, const recipe_registry& reg, uint32_t province, entity_id nation)
{
    return share_of(w, surface_forces(w, reg, province), nation);
}

bool sea_holds(const world& w, const recipe_registry& reg, uint32_t province, entity_id nation)
{
    const std::map<entity_id, int64_t> sides = surface_forces(w, reg, province);
    return sides.count(nation) != 0 && share_of(w, sides, nation) >= reg.military().sea.sea_hold_share_permille;
}

void refresh_closed_ports(world& w, const recipe_registry& reg)
{
    std::set<entity_id> closed;
    std::vector<const sea_ship*> blockaders;
    for (const auto& [sid, s] : w.ships)
        if (s.mission == sea_mission::blockade && on_station(w, s))
            blockaders.push_back(&s);
    if (!blockaders.empty())
        for (const auto& [bid, b] : w.buildings)
        {
            if (b.type != building_type::port || b.ticks_remaining > 0 || b.decommissioned)
                continue;
            const entity_id mouth = port_mouth_tile(w, bid);
            const uint32_t province = mouth != null_entity ? w.provinces.province_of(mouth) : 0;
            if (province == 0)
                continue;
            const entity_id holder = port_holder(w, bid); // BL-1531: its occupier, else its owner
            for (const sea_ship* s : blockaders)
                if (s->owner != holder && province_holds(s->provinces, province) && is_hostile(w, s->owner, holder)
                    && sea_holds(w, reg, province, s->owner))
                {
                    closed.insert(b.tile);
                    break;
                }
        }
    if (closed != w.closed_ports)
    {
        w.closed_ports = std::move(closed);
        invalidate_logistics_caches(w);
    }
}

// ---------------------------------------------------------------------------
// The raid
// ---------------------------------------------------------------------------

std::vector<interception_record> raid_convoys(world& w, const recipe_registry& reg, int tick)
{
    std::vector<interception_record> out;
    if (w.convoys.empty() || w.ships.empty())
        return out;
    bool any_raider = false;
    for (const auto& [sid, s] : w.ships) // a pure any-of
        if ((s.mission == sea_mission::raid || s.mission == sea_mission::blockade) && on_station(w, s))
        {
            any_raider = true;
            break;
        }
    if (!any_raider)
        return out;

    const std::map<uint32_t, std::vector<presence>> present = sea_presence(w);
    std::map<uint32_t, std::map<entity_id, int64_t>> detection_at;
    const sea_params& sp = reg.military().sea;
    const uint64_t seed = static_cast<uint64_t>(w.provinces.seed);
    const auto force = [&](const sea_ship& s, const presence& pc) {
        return static_cast<int64_t>(ship_stats(reg, s).attack) * ship_strength_permille(s) / 1000
               * pc.weight / 1000;
    };

    // One day's rolls: the taker, or null. Lowest ship id first, by the ordered walk.
    const auto roll_day = [&](const convoy_component& cv, uint32_t province_id,
                              const std::vector<presence>& ships, int day, bool* unseen) -> entity_id {
        for (const presence& pr : ships)
        {
            const sea_ship& r = w.ships.at(pr.ship);
            // A blockading ship may raid its hostile cargo as a raider does (BL-1530).
            if ((r.mission != sea_mission::raid && r.mission != sea_mission::blockade)
                || !on_station(w, r) || r.owner == cv.corp
                || !is_hostile(w, r.owner, cv.corp) || are_friends(w, r.owner, cv.corp))
                continue;
            const int64_t attack = force(r, pr);
            if (attack <= 0)
                continue;
            uint64_t day_hash = mix(seed, static_cast<uint64_t>(static_cast<int64_t>(tick)));
            day_hash = mix(day_hash, static_cast<uint64_t>(day));
            day_hash = mix(day_hash, province_id);
            day_hash = mix(day_hash, pr.ship);
            // Has the owner's side found this raider today? One answer for all its cargo.
            if (detection_at.find(province_id) == detection_at.end())
                detection_at[province_id] = side_detection(w, reg, province_id, ships, nullptr);
            const auto& seen = detection_at.at(province_id);
            const auto dit = seen.find(cv.corp);
            const int64_t d = dit == seen.end() ? 0 : dit->second;
            const int64_t st = std::max(0, ship_stats(reg, r).stealth);
            // BL-1530: water the owner holds reveals a surface raider, as in the daily pass.
            const bool found = (ship_class_of(r) != ship_class::submarine && sea_holds(w, reg, province_id, cv.corp))
                               || (d > 0 && static_cast<int64_t>(mix(mix(day_hash, cv.corp), 1) % 1000) < d * 1000 / (d + st));
            // Only an escort whose side found it, and whose mask reaches its class, slows it.
            int64_t guard = 0;
            if (found)
                for (const presence& pe : ships)
                {
                    const sea_ship& e = w.ships.at(pe.ship);
                    if (e.owner == cv.corp && e.mission == sea_mission::escort && on_station(w, e)
                        && (ship_stats(reg, e).targets & ship_class_bit(ship_class_of(r))))
                        guard += force(e, pe);
                }
            const int64_t ppm = int64_t{sp.raid_take_permille_per_day} * pr.weight * attack / (attack + guard);
            if (static_cast<int64_t>(mix(mix(day_hash, cv.id), 2) % 1000000) < ppm)
            {
                *unseen = !found;
                return pr.ship;
            }
        }
        return null_entity;
    };

    // Lane order decides between a land cut and a raid: `intercept_convoys` runs after this,
    // so a cargo cut on the land leg before the water must not reach the water here. A sorted
    // walk, though only an any-of is ever asked of it.
    std::map<entity_id, std::vector<entity_id>> owners_on_tile;
    for (const auto& [uid, uc] : w.units)
        if (uc.position != null_entity && uc.owner != null_entity && uc.count > 0)
            owners_on_tile[uc.position].push_back(uc.owner);
    const auto land_cut_at = [&](const convoy_component& cv, entity_id t) {
        const auto it = owners_on_tile.find(t);
        if (it == owners_on_tile.end())
            return false;
        for (const entity_id o : it->second)
            if (o != cv.corp && is_hostile(w, o, cv.corp))
                return true;
        return false;
    };

    std::vector<uint32_t> taken;
    for (const convoy_component& cv : w.convoys)
    {
        if (cv.port_a == null_entity || cv.port_b == null_entity || cv.corp == null_entity)
            continue; // no sea leg; a market's own export can be named by no declaration
        const convoy_route lane = convoy_route_tiles(w, cv);
        const int head = convoy_lane_index(lane.at, cv.progress);
        if (head < 0)
            continue;
        const int before = cv.progress_before >= 0.0f ? convoy_lane_index(lane.at, cv.progress_before) : head;
        const int first = before < head ? before + 1 : head;

        // The sweep, as runs of one province. A held cargo sits on its tile the whole tick.
        struct crossing { uint32_t province; entity_id tile; float days; };
        std::vector<crossing> runs;
        for (int li = first; li <= head; ++li)
        {
            const entity_id t = lane.tiles[static_cast<std::size_t>(li)];
            const bool at_sea = lane.on_sea_leg(li) && is_sea_tile_id(w, t);
            if (!at_sea)
            {
                if (land_cut_at(cv, t))
                    break; // cut here first, by the land pass
                continue;
            }
            const uint32_t p = w.provinces.province_of(t);
            if (p == 0)
                continue;
            if (runs.empty() || runs.back().province != p)
                runs.push_back({p, t, 0.0f});
            if (li > 0)
                runs.back().days += (lane.at[static_cast<std::size_t>(li)] - lane.at[static_cast<std::size_t>(li - 1)])
                                    * lane.days;
        }
        if (cv.held && !runs.empty())
            runs.back().days = static_cast<float>(econ_tick_days_world);

        entity_id taker = null_entity;
        entity_id where = null_entity;
        bool unseen = false;
        for (const crossing& run : runs)
        {
            const auto pit = present.find(run.province);
            if (pit == present.end())
                continue;
            const int days = std::clamp(static_cast<int>(std::lround(run.days)), 1, econ_tick_days_world);
            for (int day = 0; day < days && taker == null_entity; ++day)
                taker = roll_day(cv, run.province, pit->second, day, &unseen);
            if (taker != null_entity)
            {
                where = run.tile;
                break;
            }
        }
        if (taker == null_entity)
            continue;

        const sea_ship& r = w.ships.at(taker);
        interception_record rec;
        rec.convoy_id        = cv.id;
        rec.victim_corp      = cv.corp;
        rec.interceptor_corp = r.owner;
        rec.interceptor_ship = taker;
        rec.tile             = where;
        rec.body             = w.tiles.at(where).body;
        rec.cargo_resource   = cv.cargo_resource;
        rec.cargo_qty        = cv.cargo_qty;
        rec.tick             = tick;
        rec.unseen           = unseen;
        rec.outcome          = interception_outcome::raid_sunk;
        // A submarine sinks what it takes. A surface raider lands a prize, or sinks it with no
        // own port in range: the goods left their source at dispatch, so neither path mints.
        if (ship_class_of(r) != ship_class::submarine && assets_of(w, r.owner) != nullptr
            && std::isfinite(cv.cargo_qty))
        {
            const entity_id market = prize_market_for(w, reg, r, where);
            if (market != null_entity)
            {
                w.land_goods(r.owner, market, static_cast<std::size_t>(cv.cargo_resource), cv.cargo_qty);
                rec.outcome      = interception_outcome::raid_prize;
                rec.prize_market = market;
            }
        }
        out.push_back(rec);
        taken.push_back(cv.id);
    }
    if (!taken.empty())
        w.convoys.erase(std::remove_if(w.convoys.begin(), w.convoys.end(),
                                       [&taken](const convoy_component& c) {
                                           return std::find(taken.begin(), taken.end(), c.id) != taken.end();
                                       }),
                        w.convoys.end());
    return out;
}

// ---------------------------------------------------------------------------
// The day
// ---------------------------------------------------------------------------

namespace {

/// BL-1531: units in the surf whose battle is over go back aboard, and the transport sails home;
/// with the transport gone, or no longer off that shore, they are lost.
void settle_surf(world& w, sea_day_tick& out)
{
    std::vector<entity_id> ids;
    for (const auto& [uid, u] : w.units) // sorted below
        if (u.landing_ship != null_entity && u.count > 0)
            ids.push_back(uid);
    if (ids.empty())
        return;
    std::sort(ids.begin(), ids.end());
    const std::set<entity_id> busy = units_in_live_battles(w);
    std::set<entity_id> repulsed;
    for (const entity_id uid : ids)
    {
        if (busy.count(uid) != 0)
            continue;
        unit_component& u = w.units.at(uid);
        const entity_id sid = u.landing_ship;
        const auto sit = w.ships.find(sid);
        if (sit == w.ships.end() || sit->second.tile != u.position)
        {
            u.count = 0; // the upkeep pass's cleanup takes it
            u.landing_ship = null_entity;
            ++out.troops_lost;
            continue;
        }
        embark_unit(w, uid, sid);
        repulsed.insert(sid);
    }
    for (const entity_id sid : repulsed)
    {
        // Not while any of its units still fights: a spent unit is back aboard, and the ship
        // stays for the rest.
        if (!surf_of(w, sid).empty())
            continue;
        sea_ship& s = w.ships.at(sid);
        s.landing_tile = null_entity;
        s.landing_days = 0;
        plot_course(w, s);
    }
}

/// BL-1531 (MILITARY.md § Sea, landings): each transport off its coast runs its landing's days,
/// which on hostile ground stop and start over whenever its side does not hold the water. Then
/// its units come ashore, or, with a hostile stack on the tile, fight for it from the surf.
void run_landings(world& w, const recipe_registry& reg, int day, sea_day_tick& out)
{
    const sea_params& sp = reg.military().sea;
    bool occupation_batch = false;
    for (auto& [sid_, s] : w.ships)
    {
        const entity_id sid = sid_;
        if (s.landing_tile == null_entity || s.muster_days > 0 || s.returning || !s.route.empty()
            || !surf_of(w, sid).empty())
            continue;
        const auto stand_down = [&]() {
            s.landing_tile = null_entity;
            s.landing_days = 0;
            plot_course(w, s);
        };
        const std::vector<entity_id> mouths = landing_mouths(w, s.landing_tile);
        if (!std::binary_search(mouths.begin(), mouths.end(), s.tile))
        {
            plot_course(w, s); // the water changed, or it was re-homed off its course
            if (s.route.empty())
                stand_down();
            continue;
        }
        if (s.aboard.empty())
        {
            stand_down();
            continue;
        }
        const landing_ground ground = landing_ground_of(w, s.owner, s.landing_tile);
        if (ground == landing_ground::refused) // peace, or a stance that moved while it sailed
        {
            stand_down();
            continue;
        }
        if (ground == landing_ground::needs_hold
            && !sea_holds(w, reg, w.provinces.province_of(s.tile), s.owner))
        {
            s.landing_days = sp.landing_days;
            continue;
        }
        if (s.landing_days > 0 && --s.landing_days > 0)
            continue;

        // Ashore, or at the door. A foreign stack that is not hostile bars the tile; it waits.
        entity_id holder = null_entity, holder_unit = null_entity;
        for (const auto& [uid, u] : w.units) // a minimum: walk order cannot reach it
            if (u.count > 0 && u.position == s.landing_tile && u.owner != s.owner
                && (holder_unit == null_entity || uid < holder_unit))
            {
                holder_unit = uid;
                holder = u.owner;
            }
        if (holder != null_entity && !hostile_either(w, s.owner, holder))
            continue;

        const tile_component& shore = w.tiles.at(s.landing_tile);
        std::vector<std::pair<entity_id, unit_component>> stays;
        std::vector<entity_id> landed;
        for (auto& e : s.aboard)
        {
            if (terrain_class_effects_for_type(reg.military(), e.second.type, shore).walled)
            {
                stays.push_back(std::move(e));
                continue;
            }
            unit_component& u = e.second;
            u.order = movement_order{};
            u.tile_since_day = day; // ONLY HASTE ATTACKS BLIND: a landing opens with no recon
            if (holder == null_entity)
            {
                u.position = s.landing_tile;
                (void)occupy_on_entry(w, u.owner, u.type, s.landing_tile, occupation_batch);
            }
            else
            {
                u.position = s.tile;
                u.landing_ship = sid;
            }
            landed.push_back(e.first);
            w.units[e.first] = std::move(u);
        }
        s.aboard = std::move(stays);
        if (holder == null_entity)
        {
            if (!landed.empty())
                ++out.landed;
            stand_down();
            continue;
        }
        if (landed.empty())
        {
            stand_down();
            continue;
        }
        // Into the fight already over this shore, or a new one. Refused (a spent force, or the
        // defender attacking from here), they stand in the surf with no battle, and settle_surf
        // takes them back aboard today.
        if (join_tile_battle(w, s.landing_tile, s.owner, holder, landed)
            || open_tile_battle(w, day, s.landing_tile, s.owner, holder, landed, reg.military().recon))
            ++out.contested;
    }
    if (occupation_batch)
        reanchor_fronts(w);
    settle_surf(w, out);
}

} // namespace

sea_day_tick run_sea_day(world& w, const recipe_registry& reg, int day_tick)
{
    sea_day_tick out;
    w.ships_found_by_player.clear(); // BL-1534: the map reads today's, written below
    w.sea_sightings.clear();         // BL-1533: so do the sides' own sightings
    const entity_id map_viewer = player_nation(w);
    if (w.ships.empty())
    {
        settle_surf(w, out); // a save dropped the ship: nothing stranded survives it
        if (!w.closed_ports.empty()) // the last ship went: the ports open
            refresh_closed_ports(w, reg);
        return out;
    }
    const sea_params& sp = reg.military().sea;
    const auto stats = [&](const sea_ship& s) { return ship_stats(reg, s); }; // BL-1527: class + kit
    const auto rules = [&](const sea_ship& s) -> const ship_class_stats& {
        return sp.cls[static_cast<std::size_t>(ship_class_of(s))];
    };

    // 1. Musters tick; ships sail. A hire stands at its port's mouth until it is raised.
    for (auto& [sid, s] : w.ships)
    {
        if (s.muster_days > 0)
        {
            --s.muster_days;
            if (s.muster_days == 0)
                s.refitting = false;
            continue;
        }
        if (s.route.empty())
            continue;
        const int pace = std::max(1, stats(s).days_per_tile_permille);
        s.sail_bank_permille += 1000;
        while (!s.route.empty() && s.sail_bank_permille >= pace)
        {
            s.sail_bank_permille -= pace;
            s.tile = s.route.front();
            s.route.erase(s.route.begin());
            ++out.sailed;
        }
        if (s.route.empty())
            s.sail_bank_permille = 0;
    }

    // 2. Presence, per province.
    const std::map<uint32_t, std::vector<presence>> present = sea_presence(w);

    // 3. Detect and hit, from the day's starting strength. Damage in ppm of full HP.
    std::map<entity_id, int64_t> damage;
    const uint64_t seed = static_cast<uint64_t>(w.provinces.seed);
    for (const auto& [province_id, ships] : present)
    {
        std::map<entity_id, int64_t> recon_points; // the air pass's own arithmetic
        const std::map<entity_id, int64_t> detection =
            side_detection(w, reg, province_id, ships, &recon_points);

        // Which ships each hostile side finds here today.
        const std::map<entity_id, int64_t> sides = surface_forces(w, reg, province_id);
        std::map<entity_id, std::vector<const presence*>> found;
        for (const auto& [viewer, d] : detection)
        {
            if (d <= 0)
                continue;
            // BL-1530: holding the water reveals every hostile surface ship on it.
            const bool holds = sides.count(viewer) != 0 && share_of(w, sides, viewer) >= sp.sea_hold_share_permille;
            for (const presence& pc : ships)
            {
                const sea_ship& t = w.ships.at(pc.ship);
                const bool hostile = hostile_either(w, viewer, t.owner);
                const bool for_map = viewer == map_viewer && t.owner != viewer; // BL-1534
                if (!hostile && !for_map)
                    continue;
                const int64_t st = std::max(0, stats(t).stealth);
                // The roll is a pure hash, so rolling a neutral's ship for the map moves nothing.
                const bool rolled = detection_roll(seed, day_tick, province_id, viewer, pc.ship, d, st);
                const bool seen   = rolled || (hostile && holds && ship_class_of(t) != ship_class::submarine);
                if (seen && for_map)
                    w.ships_found_by_player.insert(pc.ship);
                if (seen && hostile)
                {
                    found[viewer].push_back(&pc);
                    w.sea_sightings.insert({viewer, pc.ship}); // BL-1533
                    ++out.detections;
                }
            }
        }

        // Every ship fires what its day here is worth, evenly over what its side found
        // and its mask allows. A broken ship is running, and does not stop to fire.
        for (const presence& pa : ships)
        {
            const sea_ship& a = w.ships.at(pa.ship);
            if (a.returning)
                continue;
            const ship_live_stats as = stats(a);
            const auto fit = found.find(a.owner);
            if (as.attack <= 0 || fit == found.end())
                continue;
            std::vector<const presence*> targets;
            for (const presence* pt : fit->second)
                if (as.targets & ship_class_bit(ship_class_of(w.ships.at(pt->ship))))
                    targets.push_back(pt);
            if (targets.empty())
                continue;
            const int64_t budget = static_cast<int64_t>(as.attack) * ship_strength_permille(a) / 1000
                                   * pa.weight / 1000 / static_cast<int64_t>(targets.size());
            for (const presence* pt : targets)
            {
                const sea_ship& t = w.ships.at(pt->ship);
                const int hp = std::max(1, stats(t).hp);
                damage[pt->ship] += budget * pt->weight / 1000 * 1000 / hp; // milli-HP to ppm
            }
        }

        // A tactical wing over the province bombs what its side found, as it bombs a land
        // unit: its own directed hostility, scaled by air share and recon. A ship cannot
        // shoot back.
        for (const auto& [wid, wg] : w.wings)
        {
            if (wg.mission != air_mission::tactical || !air_wing_flies(w, wg)
                || !std::binary_search(wg.provinces.begin(), wg.provinces.end(), province_id))
                continue;
            const auto fit = found.find(wg.owner);
            if (fit == found.end())
                continue;
            std::vector<const presence*> targets;
            for (const presence* pt : fit->second)
                if (is_hostile(w, wg.owner, w.ships.at(pt->ship).owner))
                    targets.push_back(pt);
            if (targets.empty())
                continue;
            const air_params& ap = reg.military().air;
            const int64_t share = air_share_permille(w, province_id, wg.owner);
            const int64_t scale = 1000 - int64_t{ap.bombing_penalty_permille} * (1000 - share) / 1000;
            int64_t bonus = 0;
            if (const auto rit = recon_points.find(wg.owner); rit != recon_points.end())
                bonus = std::min<int64_t>(rit->second, ap.recon_cap_permille);
            const air_wing_stats ws = wing_stats(reg, wg);
            const int64_t budget = static_cast<int64_t>(ws.soft + ws.hard)
                                   * sp.bomb_hp_per_hit_permille / 1000
                                   * wing_strength_permille(reg, wg) / 1000
                                   * scale / 1000 * (1000 + bonus) / 1000
                                   / static_cast<int64_t>(wg.provinces.size())
                                   / static_cast<int64_t>(targets.size());
            for (const presence* pt : targets)
            {
                const int hp = std::max(1, stats(w.ships.at(pt->ship)).hp);
                damage[pt->ship] += budget * pt->weight / 1000 * 1000 / hp;
            }
        }
    }

    // 4. Apply, in ascending ship id. A sunk ship is gone; one past its floors breaks off.
    std::vector<entity_id> sunk;
    for (const auto& [sid, dmg] : damage)
    {
        if (dmg <= 0)
            continue;
        sea_ship& s = w.ships.at(sid);
        ++out.hits;
        s.hp_ppm = static_cast<int32_t>(std::max<int64_t>(0, s.hp_ppm - dmg));
        const int64_t morale_loss = dmg * sp.morale_per_hp_permille / k_ship_hp_full;
        s.morale_permille = static_cast<int32_t>(std::max<int64_t>(0, s.morale_permille - morale_loss));
        if (s.hp_ppm <= 0)
        {
            sunk.push_back(sid);
            continue;
        }
        const ship_class_stats& cs = rules(s);
        if (!s.returning
            && (s.hp_ppm / (k_ship_hp_full / 1000) < cs.break_hp_permille
                || s.morale_permille < cs.break_morale_permille))
        {
            s.returning = true;
            plot_course(w, s);
            ++out.broke_off;
        }
    }
    for (const entity_id sid : sunk)
    {
        out.troops_lost += lose_passengers(w, sid, w.ships.at(sid)); // BL-1531
        forget_sightings_of(w, sid); // BL-1533
        w.ships.erase(sid);
        ++out.sunk;
    }

    // 5. In port: morale mends, and a mended ship sails again. A ship working a zone that
    // holds its own mouth is on station, not in port: it goes home to mend.
    for (auto& [sid, s] : w.ships)
    {
        if (s.muster_days > 0 || !s.route.empty() || (!s.returning && s.mission != sea_mission::none)
            || s.tile != port_mouth_tile(w, s.home_port))
            continue;
        s.morale_permille = std::min(1000, s.morale_permille + sp.dock_morale_per_day);
        if (s.returning && s.hp_ppm / (k_ship_hp_full / 1000) >= sp.resume_hp_permille
            && s.morale_permille >= sp.resume_morale_permille)
        {
            s.returning = false;
            plot_course(w, s);
        }
    }
    run_landings(w, reg, day_tick, out); // BL-1531
    // BL-1530: last, so a province lost today opens its port today.
    refresh_closed_ports(w, reg);
    return out;
}

// ---------------------------------------------------------------------------
// Upkeep
// ---------------------------------------------------------------------------

ship_upkeep_tick run_ship_upkeep(world& w, const recipe_registry& reg, economy_report& report)
{
    ship_upkeep_tick out;
    if (w.ships.empty())
        return out;
    const sea_params& sp = reg.military().sea;
    const unit_upkeep_params& up = reg.military().upkeep;
    std::vector<entity_id> drop;
    for (auto& [sid, s] : w.ships) // ascending id: shared shelves, so order is load-bearing
    {
        const auto nit = w.nations.find(s.owner);
        if (nit == w.nations.end())
        {
            drop.push_back(sid);
            continue;
        }
        const std::vector<entity_id> ports = own_ports(w, s.owner);
        const bool home_ok = std::find(ports.begin(), ports.end(), s.home_port) != ports.end();
        if (!home_ok)
        {
            if (s.muster_days > 0 && !s.refitting)
            {
                // A dropped hire refunds its crew, not its credits (§ Recruits). Not held
                // to the pool's cap, so nothing is discarded.
                const int crew = ship_crew(reg, s.type);
                nit->second.sea_recruits += crew;
                out.refunded += crew;
                drop.push_back(sid);
                continue;
            }
            // The nearest own port BY WATER: one it cannot sail to is no home to it.
            std::vector<entity_id> mouths;
            for (const entity_id bid : ports)
                if (const entity_id m = port_mouth_tile(w, bid); m != null_entity)
                    mouths.push_back(m);
            bool reachable = false;
            const std::vector<entity_id> way = sea_route(w, s.tile, mouths, &reachable);
            entity_id best = null_entity;
            if (reachable)
            {
                const entity_id goal = way.empty() ? s.tile : way.back();
                for (const entity_id bid : ports) // ascending: a shared mouth keeps the lower id
                    if (port_mouth_tile(w, bid) == goal)
                    {
                        best = bid;
                        break;
                    }
            }
            if (best == null_entity)
            {
                drop.push_back(sid);
                continue;
            }
            s.home_port = best;
            ++out.rehomed;
            if (s.returning || s.mission == sea_mission::none)
                plot_course(w, s);
        }
        ++out.ships;
        if (s.muster_days > 0 && !s.refitting)
            continue;
        const entity_id port_tile = w.buildings.at(s.home_port).tile;
        const int share = draw_goods_at_tile(w, reg, report, s.owner, port_tile, sp.ship_upkeep_goods);
        if (share < 1000)
            ++out.unmet;
        // BL-1550: an unpaid crew weakens by the same rule as an unsupplied one.
        s.supply_factor_permille = step_unit_supply_toward(
            s.supply_factor_permille, nation_wages_short(w, reg, s.owner) ? 0 : share, up);

        // Lying at home damaged, it refits by drawing goods.
        if (s.muster_days <= 0 && s.hp_ppm < k_ship_hp_full && s.route.empty()
            && (s.returning || s.mission == sea_mission::none)
            && s.tile == port_mouth_tile(w, s.home_port))
        {
            const int64_t gain = std::min<int64_t>(sp.refit_ppm_per_quarter, k_ship_hp_full - s.hp_ppm);
            std::array<float, resource_count> goods = sp.refit_goods;
            const float scale = sp.refit_ppm_per_quarter > 0
                                    ? static_cast<float>(gain) / static_cast<float>(sp.refit_ppm_per_quarter)
                                    : 0.0f;
            for (float& q : goods)
                q *= scale;
            const int met = draw_goods_at_tile(w, reg, report, s.owner, port_tile, goods);
            s.hp_ppm = static_cast<int32_t>(s.hp_ppm + gain * met / 1000);
        }
    }
    for (const entity_id sid : drop)
    {
        out.troops_lost += lose_passengers(w, sid, w.ships.at(sid)); // BL-1531
        forget_sightings_of(w, sid); // BL-1533
        w.ships.erase(sid);
        ++out.disbanded;
    }
    return out;
}

// ---- The opening fleet (BL-1532; MILITARY.md § Sea) ------------------------------

opening_fleet_report seed_major_opening_fleets(world& w, const std::vector<std::uint8_t>& fleet,
                                               int per_port, int sea_recruits)
{
    opening_fleet_report out;
    per_port = std::max(1, per_port);
    std::vector<entity_id> majors;
    for (const auto& [nid, nc] : w.nations)
        if (nc.is_major)
            majors.push_back(nid);
    std::sort(majors.begin(), majors.end());
    for (const entity_id nid : majors)
    {
        nation_component& nc = w.nations.at(nid);
        nc.sea_recruits = sea_recruits;
        ++out.majors;
        const uint32_t capital_province = w.provinces.province_of(nc.capital_tile);
        // (not in the capital's province, distance, tile, id): the capital port first.
        std::vector<std::tuple<int, int, entity_id, entity_id, entity_id>> berths;
        for (const entity_id bid : own_ports(w, nid))
        {
            const entity_id mouth = port_mouth_tile(w, bid);
            if (mouth == null_entity)
                continue; // a lake's shore: no ship lies there
            const entity_id tile = w.buildings.at(bid).tile;
            const bool at_capital = capital_province != 0 && w.provinces.province_of(tile) == capital_province;
            const int d = air_tile_distance(w, nc.capital_tile, tile);
            berths.emplace_back(at_capital ? 0 : 1, d < 0 ? std::numeric_limits<int>::max() : d, tile, bid, mouth);
        }
        if (berths.empty())
        {
            ++out.no_sea_port;
            continue;
        }
        std::sort(berths.begin(), berths.end());
        if (std::get<0>(berths.front()) != 0)
            ++out.off_capital;
        for (std::size_t i = 0; i < fleet.size(); ++i)
        {
            const std::size_t at = (i / static_cast<std::size_t>(per_port)) % berths.size();
            const ship_class cls = static_cast<ship_class>(fleet[i]);
            sea_ship s;
            s.owner     = nid;
            s.home_port = std::get<3>(berths[at]);
            s.tile      = std::get<4>(berths[at]);
            s.type      = static_cast<uint16_t>(fleet[i]); // one row per class, in class order
            s.kit       = ship_design_of(w, nid, cls);
            w.ships.emplace(w.create_entity(), std::move(s));
            ++out.ships;
            if (at != 0)
                ++out.spilled;
        }
    }
    return out;
}

void print_opening_fleets(const opening_fleet_report& r)
{
    std::printf("[opening_kit] fleet: %d major(s), %d ship(s) laid (%d past the capital port); %d major(s) "
                "with no sea port; %d with none in the capital's province\n",
                r.majors, r.ships, r.spilled, r.no_sea_port, r.off_capital);
    std::fflush(stdout);
}
