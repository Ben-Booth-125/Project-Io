#include "offworld_base.hpp"

#include "logistics.hpp"       // lay_road_edge, road_edge_neighbour, invalidate_logistics_caches
#include "market_clearing.hpp" // maybe_spawn_market, market_for_tile
#include "nation_tech.hpp"     // nation_has_mechanic
#include "province.hpp"        // partition_body_if_unpartitioned, province_holder_for
#include "space_assets.hpp"    // space_home_body_of
#include "space_convoy.hpp"    // space_haul
#include "space_tech_keys.hpp" // space_keys::base_founding
#include "space_units.hpp"     // space_quote_lot, space_take_lot, evaluate_space_design
#include "survey_system.hpp"   // survey_tile_mapped
#include "world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

// See offworld_base.hpp for the rules. Docs: NATIONS.md § Off-world bases.

namespace {

/// The passenger berths @p u's design holds.
int32_t berths_of(const space_unit& u)
{
    return evaluate_space_design(u.hull, u.design).passengers;
}

/// The nation's Landers landed on @p body's surface, ascending id.
std::vector<entity_id> landed_landers(const world& w, entity_id nation, entity_id body)
{
    std::vector<entity_id> out;
    for (const auto& [id, u] : w.space_units) // ascending
        if (u.owner == nation && u.hull == space_hull::lander && u.state == space_unit_state::landed
            && u.node.body == body && u.node.kind == orbital_node_kind::surface)
            out.push_back(id);
    return out;
}

/// The kit aboard @p unit: its held cargo when that cargo is the kit's good.
float kit_aboard(const world& w, const offworld_base_params& p, entity_id unit)
{
    const auto hit = w.space_hauls.find(unit);
    if (hit == w.space_hauls.end() || hit->second.phase != space_haul_phase::held
        || hit->second.cargo_res != p.kit_res)
        return 0.0f;
    return std::max(0.0f, w.space_units.at(unit).cargo);
}

/// Does a centre or a building stand on @p tile?
bool tile_occupied(const world& w, entity_id tile)
{
    for (const auto& [cid, t] : w.population_centre_tile)
        if (t == tile)
            return true;
    for (const auto& [bid, b] : w.buildings)
        if (b.tile == tile)
            return true;
    return false;
}

entity_id owner_of(const world& w, entity_id tile)
{
    const auto it = w.tile_to_nation.find(tile);
    return it != w.tile_to_nation.end() ? it->second : null_entity;
}

void set_province_holder(world& w, uint32_t pid, entity_id holder)
{
    const auto& v = w.provinces.provinces;
    const auto it = std::lower_bound(v.begin(), v.end(), pid,
                                     [](const province& p, uint32_t x) { return p.id < x; });
    if (it == v.end() || it->id != pid)
        return;
    const std::size_t i = static_cast<std::size_t>(it - v.begin());
    if (w.province_holder.size() < v.size())
        w.province_holder.resize(v.size(), null_entity);
    w.province_holder[i] = holder;
}

} // namespace

int32_t space_unit_colonists(const world& w, entity_id unit)
{
    const auto it = w.space_colonists.find(unit);
    return it != w.space_colonists.end() && w.space_units.count(unit) != 0 ? it->second : 0;
}

offworld_base_result embark_colonists(world& w, entity_id nation, entity_id unit, int32_t count)
{
    if (nation == null_entity || w.nations.count(nation) == 0)
        return offworld_base_result::rejected_unknown_nation;
    const auto uit = w.space_units.find(unit);
    if (unit == null_entity || uit == w.space_units.end())
        return offworld_base_result::rejected_unknown_unit;
    const space_unit& u = uit->second;
    if (u.owner != nation)
        return offworld_base_result::rejected_not_owner;
    const int32_t berths = berths_of(u);
    if (u.hull != space_hull::lander || berths <= 0)
        return offworld_base_result::rejected_not_lander;
    if (count < 1 || count > k_space_colonists_max)
        return offworld_base_result::rejected_invalid;
    const entity_id home = space_home_body_of(w, nation);
    const bool at_home =
        home != null_entity && u.node.body == home
        && ((u.state == space_unit_state::idle && u.node.kind == orbital_node_kind::low_orbit)
            || (u.state == space_unit_state::landed && u.node.kind == orbital_node_kind::surface));
    if (!at_home)
        return offworld_base_result::rejected_wrong_node;
    if (space_staffed_pad_on(w, nation, home) == null_entity)
        return offworld_base_result::rejected_no_pad;
    const int32_t aboard = space_unit_colonists(w, unit);
    if (count > berths - aboard)
        return offworld_base_result::rejected_no_berths;

    // The most populous of the nation's centres on its home body that can spare
    // them and keep a thousand; ties to the lower id.
    std::vector<entity_id> ids;
    for (const auto& [cid, tile] : w.population_centre_tile)
        ids.push_back(cid);
    std::sort(ids.begin(), ids.end());
    entity_id best = null_entity;
    int32_t   best_pop = 0;
    for (const entity_id cid : ids)
    {
        const auto pit = w.population_centres.find(cid);
        if (pit == w.population_centres.end() || pit->second.razed)
            continue;
        const entity_id tile = w.population_centre_tile.at(cid);
        const auto tit = w.tiles.find(tile);
        if (tit == w.tiles.end() || tit->second.body != home || owner_of(w, tile) != nation)
            continue;
        const int32_t pop = pit->second.population;
        if (pop <= count)
            continue;
        if (best == null_entity || pop > best_pop)
        {
            best = cid;
            best_pop = pop;
        }
    }
    if (best == null_entity)
        return offworld_base_result::rejected_no_people;

    w.population_centres.at(best).population -= count;
    w.space_colonists[unit] = aboard + count;
    return offworld_base_result::ok;
}

offworld_base_op found_offworld_base(world& w, const recipe_registry& reg, const offworld_base_params& p,
                                     entity_id nation, entity_id tile)
{
    offworld_base_op op;
    const auto nit = w.nations.find(nation);
    if (nation == null_entity || nit == w.nations.end())
    {
        op.result = offworld_base_result::rejected_unknown_nation;
        return op;
    }
    const auto tit = w.tiles.find(tile);
    if (tile == null_entity || tit == w.tiles.end())
        return op; // rejected_invalid
    const tile_component& tc = tit->second;
    const entity_id body = tc.body;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end() || bit->second.type == body_type::star || bit->second.grid_width <= 0
        || bit->second.grid_height <= 0 || body == space_home_body_of(w, nation) || body == w.home_body
        || is_water(tc.substrate))
        return op;
    if (!nation_has_mechanic(w, nation, space_keys::base_founding))
    {
        op.result = offworld_base_result::rejected_tech_locked;
        return op;
    }
    if (!survey_tile_mapped(w, nation, body, tc.grid_x, tc.grid_y))
    {
        op.result = offworld_base_result::rejected_unmapped;
        return op;
    }
    const uint32_t pid_before = w.provinces.province_of(tile);
    if (owner_of(w, tile) != null_entity || tile_occupied(w, tile)
        || (pid_before != 0 && province_holder_for(w, pid_before) != null_entity))
    {
        op.result = offworld_base_result::rejected_claimed;
        return op;
    }

    const std::vector<entity_id> landers = landed_landers(w, nation, body);
    int64_t colonists = 0;
    float   kit = 0.0f;
    for (const entity_id id : landers)
    {
        colonists += space_unit_colonists(w, id);
        kit += kit_aboard(w, p, id);
    }
    if (colonists < std::max<int32_t>(1, p.min_colonists))
    {
        op.result = offworld_base_result::rejected_no_colonists;
        return op;
    }
    if (!(kit >= p.kit_qty))
    {
        op.result = offworld_base_result::rejected_no_kit;
        return op;
    }

    // ---- Every check passed: write. --------------------------------------
    // 1. The partition, on a body with none.
    op.partitioned = partition_body_if_unpartitioned(w, body);
    const uint32_t pid = w.provinces.province_of(tile);

    // 2. The province's unowned tiles become the nation's, ascending.
    std::vector<entity_id> claim;
    if (const province* pr = w.provinces.find(pid))
    {
        for (const entity_id t : pr->tiles)
            if (owner_of(w, t) == null_entity)
                claim.push_back(t);
    }
    else
        claim.push_back(tile); // a tile no partition reaches: the base holds its own ground
    nation_component& nc = w.nations.at(nation);
    for (const entity_id t : claim)
    {
        w.set_tile_nation(t, nation);
        nc.tiles.push_back(t);
        if (const auto ct = w.tiles.find(t); ct != w.tiles.end())
            for (std::size_t r = 0; r < resource_count; ++r)
                nc.resource_abundance[r] += ct->second.resource_deposit[r];
    }
    if (pid != 0)
        set_province_holder(w, pid, nation);
    op.province = pid;
    op.tiles_claimed = static_cast<int32_t>(claim.size());

    // 3. The colonists and the kit leave the Landers, lowest id first.
    float kit_left = p.kit_qty;
    for (const entity_id id : landers)
    {
        w.space_colonists.erase(id);
        const float k = kit_aboard(w, p, id);
        if (!(k > 0.0f) || !(kit_left > 0.0f))
            continue;
        const float take = std::min(k, kit_left);
        space_unit& u = w.space_units.at(id);
        u.cargo = std::max(0.0f, u.cargo - take);
        kit_left -= take;
        if (!(u.cargo > 0.0f))
        {
            u.cargo = 0.0f;
            w.space_hauls.erase(id);
        }
    }

    // 4. The population centre, with every colonist landed.
    const entity_id centre = w.create_entity();
    population_centre_component pcc;
    pcc.scale        = 1;
    pcc.population   = static_cast<int>(colonists);
    pcc.habitability = std::clamp(tc.habitability, 0.0f, 1.0f);
    w.population_centres[centre] = pcc;
    w.population_centre_tile[centre] = tile;
    int bases_here = 1;
    for (const auto& [cid, b] : w.offworld_bases)
    {
        const auto ctile = w.population_centre_tile.find(cid);
        if (ctile != w.population_centre_tile.end())
            if (const auto ct = w.tiles.find(ctile->second); ct != w.tiles.end() && ct->second.body == body)
                ++bases_here;
    }
    w.population_centre_name[centre] =
        bit->second.name + " Base" + (bases_here > 1 ? " " + std::to_string(bases_here) : std::string());
    op.centre = centre;
    op.colonists = static_cast<int32_t>(colonists);

    // 5. The market: one founded on the tile when the body has none.
    const entity_id spawned = maybe_spawn_market(w, reg, body, tile);
    op.new_market = spawned != null_entity;
    op.market = market_for_tile(w, tile);

    // 6. A Track on the base's tile. A building needs a road and a road is laid
    // from a road (PRODUCTION.md § A building needs a road; LOGISTICS.md § 4), and
    // generation gives every market centre's tile at least a Track so no ground
    // begins with nowhere to build from; a base is held to the same rule. One
    // crossing to the cheapest neighbour of its own dry ground, ties to the lower
    // side; none when the base stands alone on its ground.
    {
        int   best_side = -1;
        float best_cost = 0.0f;
        for (int side = 0; side < 6; ++side)
        {
            if (!road_side_is_crossable(side, tc.grid_y))
                continue; // a side no walk crosses takes no road (lay_road_edge refuses it)
            const entity_id nb = road_edge_neighbour(w, tile, side);
            const auto nt = w.tiles.find(nb);
            if (nb == null_entity || nt == w.tiles.end() || is_water(nt->second.substrate)
                || owner_of(w, nb) != nation)
                continue;
            const float c = tile_traversal_cost(nt->second);
            if (best_side < 0 || c < best_cost)
            {
                best_side = side;
                best_cost = c;
            }
        }
        if (best_side >= 0 && lay_road_edge(w, tile, best_side, 1))
            op.road_laid = true;
    }
    invalidate_logistics_caches(w);

    offworld_base rec;
    rec.founded_tick = w.current_econ_tick;
    w.offworld_bases[centre] = rec;
    op.result = offworld_base_result::ok;
    return op;
}

entity_id offworld_base_tile(const world& w, entity_id centre)
{
    if (w.offworld_bases.count(centre) == 0)
        return null_entity;
    const auto it = w.population_centre_tile.find(centre);
    return it != w.population_centre_tile.end() ? it->second : null_entity;
}

bool offworld_base_fed(const world& w, entity_id centre)
{
    if (w.offworld_bases.empty())
        return false;
    const auto bit = w.offworld_bases.find(centre);
    if (bit == w.offworld_bases.end() || bit->second.unfed_quarters != 0)
        return false;
    const auto pit = w.population_centres.find(centre);
    return pit != w.population_centres.end() && !pit->second.razed && pit->second.population > 0;
}

entity_id offworld_base_capital(const world& w, entity_id nation, entity_id body)
{
    if (w.offworld_bases.empty() || nation == null_entity)
        return null_entity;
    for (const auto& [cid, b] : w.offworld_bases) // ascending centre id
    {
        const entity_id tile = offworld_base_tile(w, cid);
        const auto tit = w.tiles.find(tile);
        if (tit != w.tiles.end() && tit->second.body == body && owner_of(w, tile) == nation)
            return tile;
    }
    return null_entity;
}

float offworld_base_habitability(const world& w, entity_id centre, float own)
{
    return offworld_base_fed(w, centre) ? 1.0f : own;
}

void run_offworld_base_quarter(world& w, const offworld_base_params& p, offworld_base_quarter_report* out)
{
    // Colonists aboard a unit that is gone are gone with it.
    for (auto it = w.space_colonists.begin(); it != w.space_colonists.end();)
        it = w.space_units.count(it->first) == 0 ? w.space_colonists.erase(it) : std::next(it);
    if (w.offworld_bases.empty())
        return;

    offworld_base_quarter_report r;
    space_unit_params sp;
    sp.reservation_mult = p.reservation_mult;
    for (auto& [cid, base] : w.offworld_bases) // ascending centre id
    {
        const auto pit = w.population_centres.find(cid);
        const entity_id tile = offworld_base_tile(w, cid);
        if (pit == w.population_centres.end() || tile == null_entity)
            continue;
        population_centre_component& pc = pit->second;
        if (pc.razed || pc.population <= 0)
            continue;
        const entity_id owner = owner_of(w, tile);
        const auto tit = w.tiles.find(tile);
        const float hab = tit != w.tiles.end() ? std::clamp(tit->second.habitability, 0.0f, 1.0f) : 0.0f;
        const float short_by = 1.0f - hab;

        std::array<float, resource_count> lot{};
        lot[static_cast<std::size_t>(resource_type::water)] =
            std::max(0.0f, p.water_per_k) * static_cast<float>(pc.population) * short_by;
        lot[static_cast<std::size_t>(resource_type::food_rations)] =
            std::max(0.0f, p.food_per_k) * static_cast<float>(pc.population) * short_by;
        const bool needs = lot[static_cast<std::size_t>(resource_type::water)] > 0.0f
                        || lot[static_cast<std::size_t>(resource_type::food_rations)] > 0.0f;

        bool fed = !needs;
        if (needs && owner != null_entity && w.nations.count(owner) != 0)
        {
            const entity_id market = market_for_tile(w, tile);
            float credits = 0.0f;
            if (market != null_entity && space_quote_lot(w, sp, market, lot, &credits)
                && w.nations.at(owner).treasury >= credits)
            {
                space_take_lot(w, owner, market, lot, credits);
                fed = true;
                r.water_bought += lot[static_cast<std::size_t>(resource_type::water)];
                r.food_bought  += lot[static_cast<std::size_t>(resource_type::food_rations)];
                r.spent        += credits;
            }
        }
        if (fed)
        {
            base.unfed_quarters = 0;
            ++r.fed;
            continue;
        }
        ++r.unfed;
        base.unfed_quarters = std::min(base.unfed_quarters + 1, k_offworld_unfed_max);
        // BL-1642 (the tick is a month): the loss is taken a quarter at a time - on
        // every third consecutive unfed month, at the quarter's share 1-(1-s)^3 with
        // its minimum of one head - so a small base shrinks at the old calendar rate
        // rather than losing its floor head every month.
        if (base.unfed_quarters % k_econ_ticks_per_quarter != 0)
            continue;
        const double s_month   = std::clamp(static_cast<double>(p.shrink_share), 0.0, 1.0);
        const double keep      = 1.0 - s_month;
        const double s_quarter = 1.0 - keep * keep * keep; // three months; no pow on the tick
        const int lost = std::min(pc.population,
                                  std::max(1, static_cast<int>(std::ceil(static_cast<double>(pc.population)
                                                                         * s_quarter - 1e-9))));
        pc.population -= lost;
        r.heads_lost += lost;
        if (pc.population <= 0)
        {
            // The razed settlement tier (POPULATION.md § Growth, decline and razing):
            // the entity, its tile and its ground stay; no labour, no demand.
            pc.population = 0;
            pc.scale = 1;
            pc.razed = true;
            pc.growth_accumulator = 0;
            ++r.razed;
        }
    }
    if (out != nullptr)
        *out = r;
}
