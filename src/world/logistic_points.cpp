#include "logistic_points.hpp"

#include "logistics.hpp"       // body_tile_grid, invalidate_logistics_caches
#include "market_clearing.hpp" // market_for_tile
#include "road_generation.hpp"  // lay_own_ground_join: the port link's road (Ben, 2026-10-25)
#include "river_generation.hpp" // BL-1340: hex_side_for_offset — a step prices the edge it crosses
#include "hex_neighbors.hpp"    // BL-1396: the sea walk crosses all six hex sides
#include "state_industry.hpp"  // state_gate_would_idle (the reading at the gate)

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <cstdio>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <utility>

bool lp_source_network(building_type t, lp_network& out)
{
    switch (t)
    {
        case building_type::inland_logistics_hub: out = lp_network::road; return true;
        case building_type::railway_station:      out = lp_network::rail; return true;
        case building_type::port:                 out = lp_network::sea;  return true;
        default: return false;
    }
}

bool lp_source_running(const building_component& b)
{
    return b.ticks_remaining <= 0 && !b.decommissioned && !b.state_idle;
}

bool lp_consumer(const building_component& b)
{
    return (b.type == building_type::extraction_site || b.type == building_type::processing_facility)
        && b.ticks_remaining <= 0 && !b.decommissioned && !b.state_idle;
}

float lp_need(const building_component& b)
{
    if (!lp_consumer(b))
        return 0.0f;
    const float wt  = std::clamp(b.workforce_target / 100.0f, 0.0f, 2.0f);
    const float run = std::max(0.0f, b.workforce_assigned) * wt;
    // Ben, 2026-10-24: a mid-quarter completion needs points for its share of the
    // quarter only (PRODUCTION.md § Construction is measured in days).
    const float n   = lp_need_per_run * run * building_output_share(b);
    return std::isfinite(n) ? n : 0.0f;
}

float lp_reach(lp_network n, float d)
{
    const std::size_t i = static_cast<std::size_t>(n);
    if (!std::isfinite(d) || d < 0.0f)
        return 0.0f;
    if (lp_full_strength(n, d)) // BL-1360: with the float tolerance (lp_full_eps)
        return 1.0f;
    const float over = d - lp_full_reach[i];
    if (over >= lp_fade_length[i])
        return 0.0f;
    return 1.0f - over / lp_fade_length[i];
}

namespace {

/// Cost of stepping INTO @p tc on network @p n, having left @p from across its hex side
/// @p side; negative = impassable. BL-1340: the road rung is the EDGE's tier — the
/// crossing the step actually makes — not a property of either tile, so a step off the
/// network pays `lp_offnet_step` even between two tiles that each carry roads elsewhere.
float step_cost(const world& w, entity_id tile, const tile_component& tc, lp_network n,
                const tile_component& from, int side)
{
    const bool water = is_water(tc.substrate);
    switch (n)
    {
        case lp_network::road:
        {
            // Ben, 2026-10-24: THE NETWORK IS THE ROADS AND THE RAIL. A hub's points
            // travel only along a roaded edge; a step off the road is impassable, so
            // no point reaches a tile no road leads to. (Off-network supply is the
            // units' alone, NR-1050: a march draws its nation's pool with no reach.)
            // A BRIDGE IS A ROAD (Ben, 2026-10-17: a road may span water): a roaded
            // edge onto a water tile carries points like any other, so a strait the
            // road crosses is no gap in the network; water no road crosses still is.
            (void)water;
            const std::uint8_t lvl = road_tier_on_side(from, side);
            return (lvl >= 1 && lvl <= 3) ? lp_road_step[lvl] : -1.0f;
        }
        case lp_network::rail:
            // BL-1360: never reached from lp_distance_field, which reads rail point to
            // point off the rail network; kept total so the switch stays exhaustive.
            if (water)
                return -1.0f;
            return w.rail_track.count(tile) != 0 ? 1.0f : lp_offnet_step;
        case lp_network::sea:
            // BL-1396: never reached from lp_distance_field, which walks sea over water
            // only (a step 1, a landing 1, no step inland); kept total for the switch.
            return water ? 1.0f : lp_offnet_step;
    }
    return -1.0f;
}

} // namespace

std::unordered_map<entity_id, float> lp_sea_walk(world& w, entity_id body,
                                                 const std::vector<entity_id>& seeds)
{
    const std::size_t si = static_cast<std::size_t>(lp_network::sea);
    return lp_sea_walk(w, body, seeds, lp_full_reach[si] + lp_fade_length[si]);
}

std::unordered_map<entity_id, float> lp_sea_walk(world& w, entity_id body,
                                                 const std::vector<entity_id>& seeds, float bound)
{
    // BL-1396 (Ben, 2026-10-21): A PORT'S POINTS CROSS WATER AND LAND ON THE SHORE. From
    // the seed tiles (a port's own tile, or a whole shore), the walk steps onto water,
    // crosses water a tile at a time and LANDS on the tiles beside it, every step costing
    // 1; a landing ends the walk, so the points reach the shores the water joins and no
    // further inland (a hub's ground), and land to land is never a sea step. ALL SIX HEX
    // SIDES (cold review): a port is placed against sea on any of its six sides
    // (placement_rules::is_coastal), so a four-way walk left a legal port with sea only
    // on a diagonal side reaching nothing. LAKES TOO (Ben, 2026-10-21, NR-1061: "Ports
    // should be able to cover lakes too"): the walk crosses any water (is_water), though
    // a port still STANDS only on a sea coast, as placement rules. Bounded at @p bound.
    //
    // Ben, 2026-10-25: NO LONGER A PORT'S FIELD - points ride the lanes (lp_lane_walk). This
    // walk survives as the MEASURE: the water distance within which two ports are laned
    // (road_generation.cpp, `lay_port_lanes`), and the reach generation's port link reads.
    std::unordered_map<entity_id, float> dist;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end() || seeds.empty())
        return dist;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    using item = std::pair<float, entity_id>;
    std::priority_queue<item, std::vector<item>, std::greater<item>> pq;
    std::vector<entity_id> sorted = seeds;
    std::sort(sorted.begin(), sorted.end());
    for (const entity_id s : sorted)
    {
        const auto tit = w.tiles.find(s);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;
        if (dist.emplace(s, 0.0f).second)
            pq.push({0.0f, s});
    }
    while (!pq.empty())
    {
        const auto [d, t] = pq.top();
        pq.pop();
        if (dist[t] < d)
            continue;
        const tile_component& tc = w.tiles.at(t);
        const bool here_sea = is_water(tc.substrate); // sea or lake (NR-1061)
        if (!here_sea && d > 0.0f)
            continue; // a landing: the walk ends on the coast
        for (int side = 0; side < 6; ++side)
        {
            const hex_neighbors::coord nb = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, side);
            if (nb.gy < 0 || nb.gy >= gh)
                continue;
            const int nx = ((nb.gx % gw) + gw) % gw; // columns wrap
            const std::size_t ci = static_cast<std::size_t>(nb.gy) * static_cast<std::size_t>(gw)
                                 + static_cast<std::size_t>(nx);
            if (ci >= grid.size())
                continue;
            const entity_id nt = grid[ci];
            if (nt == null_entity)
                continue;
            if (!here_sea && !is_water(w.tiles.at(nt).substrate))
                continue; // land to land is no sea route (only a seed is land)
            const float nd = d + 1.0f;
            if (nd > bound)
                continue;
            const auto it = dist.find(nt);
            if (it == dist.end() || nd < it->second)
            {
                dist[nt] = nd;
                pq.push({nd, nt});
            }
        }
    }
    return dist;
}

std::unordered_map<entity_id, float> lp_lane_walk(world& w, entity_id body,
                                                  const std::vector<entity_id>& seeds)
{
    // POINTS RIDE THE LANES (Ben, 2026-10-25; LOGISTICS.md § One pool, "Points ride the
    // lanes", and § 4b, "Every port lays lanes"). The network is the roads and the rail
    // (Ben, 2026-10-24), and on the water the lane is the network: from the seed tiles (a
    // port's own tile) the walk steps onto a LANED sea tile beside it, along laned water a
    // tile at a time, and LANDS only on a tile holding a standing port beside the lane - the
    // coastal road that port stands on. Every step costs 1; a landing ends the walk. All six
    // hex sides, as a port is placed against sea on any of them and as the any-water walk
    // read them (BL-1396), so a lane's cardinal staircase is ridden at about its hex length.
    // A lane over a LAKE is ridden as a lane over the sea (Ben, 2026-10-25, NR-1061: ports
    // on a lake lay lanes over it). Open water no lane crosses, and a shore with no port,
    // carry nothing. Bounded at
    // the sea's full reach plus its fade.
    std::unordered_map<entity_id, float> dist;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end() || seeds.empty())
        return dist;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    const std::size_t si = static_cast<std::size_t>(lp_network::sea);
    const float bound = lp_full_reach[si] + lp_fade_length[si];
    // Where a lane may land: every standing port's tile on this body.
    std::set<entity_id> landings;
    for (const auto& [bid, b] : w.buildings)
    {
        (void)bid;
        if (b.type != building_type::port || b.ticks_remaining > 0 || b.decommissioned)
            continue;
        const auto tit = w.tiles.find(b.tile);
        if (tit != w.tiles.end() && tit->second.body == body)
            landings.insert(b.tile);
    }
    using item = std::pair<float, entity_id>;
    std::priority_queue<item, std::vector<item>, std::greater<item>> pq;
    std::vector<entity_id> sorted = seeds;
    std::sort(sorted.begin(), sorted.end());
    for (const entity_id s : sorted)
    {
        const auto tit = w.tiles.find(s);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;
        if (dist.emplace(s, 0.0f).second)
            pq.push({0.0f, s});
    }
    while (!pq.empty())
    {
        const auto [d, t] = pq.top();
        pq.pop();
        if (dist[t] < d)
            continue;
        const tile_component& tc = w.tiles.at(t);
        const bool on_lane = is_water(tc.substrate);
        if (!on_lane && d > 0.0f)
            continue; // a landing: the walk ends on the port's coast
        for (int side = 0; side < 6; ++side)
        {
            const hex_neighbors::coord nb = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, side);
            if (nb.gy < 0 || nb.gy >= gh)
                continue;
            const int nx = ((nb.gx % gw) + gw) % gw; // columns wrap
            const std::size_t ci = static_cast<std::size_t>(nb.gy) * static_cast<std::size_t>(gw)
                                 + static_cast<std::size_t>(nx);
            if (ci >= grid.size())
                continue;
            const entity_id nt = grid[ci];
            if (nt == null_entity)
                continue;
            const tile_component& ntc = w.tiles.at(nt);
            if (is_water(ntc.substrate))
            {
                if (ntc.lane_level == 0)
                    continue; // open water: no lane, no points
            }
            else if (!on_lane || landings.count(nt) == 0)
                continue; // land only from a lane, and only where a port stands
            const float nd = d + 1.0f;
            if (nd > bound)
                continue;
            const auto it = dist.find(nt);
            if (it == dist.end() || nd < it->second)
            {
                dist[nt] = nd;
                pq.push({nd, nt});
            }
        }
    }
    return dist;
}

std::unordered_map<entity_id, float> lp_distance_field(world& w, entity_id body, lp_network n,
                                                       const std::vector<entity_id>& seeds)
{
    std::unordered_map<entity_id, float> dist;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end() || seeds.empty())
        return dist;

    // BL-1360: RAIL IS POINT TO POINT (Ben, 2026-10-16: "Railways can reach 20 tiles,
    // but they only supply the exact tile they connect to"; LOGISTICS.md § One pool).
    // A station's points land on its own tile and on every OTHER STATION's tile the
    // line joins it to within reach - never on the country the track passes through,
    // which is the rule goods already obey (they board and leave only at stations,
    // § 4c). The distance is the line's own track steps, read off the one rail
    // network the router uses, so cut and idle track and unfunded stations carry
    // nothing here exactly as they carry no cargo.
    if (n == lp_network::rail)
    {
        const float rail_bound = lp_full_reach[static_cast<std::size_t>(n)]
                               + lp_fade_length[static_cast<std::size_t>(n)];
        const world::rail_network& net = body_rail_network(w, body);
        std::vector<entity_id> sorted = seeds;
        std::sort(sorted.begin(), sorted.end());
        for (const entity_id s : sorted)
        {
            const auto tit = w.tiles.find(s);
            if (tit == w.tiles.end() || tit->second.body != body)
                continue;
            dist[s] = 0.0f;
            // `stations` is sorted ascending (body_rail_network), so a binary search.
            const auto sit = std::lower_bound(net.stations.begin(), net.stations.end(), s);
            if (sit == net.stations.end() || *sit != s)
                continue; // a station joined to no line delivers to its own tile only
            const std::size_t i = static_cast<std::size_t>(sit - net.stations.begin());
            for (std::size_t j = 0; j < net.stations.size(); ++j)
            {
                const int steps = (i < net.steps.size() && j < net.steps[i].size())
                                  ? net.steps[i][j] : -1;
                if (j == i || steps < 0 || static_cast<float>(steps) > rail_bound)
                    continue;
                const auto [it, fresh] = dist.emplace(net.stations[j], static_cast<float>(steps));
                if (!fresh && static_cast<float>(steps) < it->second)
                    it->second = static_cast<float>(steps);
            }
        }
        return dist;
    }

    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    const float bound = lp_full_reach[static_cast<std::size_t>(n)]
                      + lp_fade_length[static_cast<std::size_t>(n)];
    // Sea has its own walk, taken before this loop: along the lanes (Ben, 2026-10-25;
    // lp_lane_walk), which replaced BL-1396's any-water walk as a port's field.
    if (n == lp_network::sea)
        return lp_lane_walk(w, body, seeds);

    using item = std::pair<float, entity_id>;
    std::priority_queue<item, std::vector<item>, std::greater<item>> pq;
    std::vector<entity_id> sorted_seeds = seeds;
    std::sort(sorted_seeds.begin(), sorted_seeds.end());
    for (const entity_id s : sorted_seeds)
    {
        const auto tit = w.tiles.find(s);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;
        if (dist.emplace(s, 0.0f).second)
            pq.push({0.0f, s});
    }
    while (!pq.empty())
    {
        const auto [d, t] = pq.top();
        pq.pop();
        const auto dit = dist.find(t);
        if (dit != dist.end() && dit->second < d)
            continue;
        const tile_component& tc = w.tiles.at(t);
        const int x = tc.grid_x, y = tc.grid_y;
        const int cand[4] = {y > 0 ? (y - 1) * gw + x : -1, y * gw + (x + 1) % gw,
                             y + 1 < gh ? (y + 1) * gw + x : -1, y * gw + (x + gw - 1) % gw};
        // BL-1340: the hex side of each cardinal step, in the same order as `cand`
        // (N, E, S, W), read off the tile being LEFT — so the step prices the edge it
        // crosses. The two diagonal hex sides a 4-cardinal walk skips carry no road a
        // step can use, which is the geometry the river discount already lives with.
        const int step_dc[4] = {  0,  1,  0, -1 };
        const int step_dr[4] = { -1,  0,  1,  0 };
        for (int ci = 0; ci < 4; ++ci)
        {
            const int c = cand[ci];
            if (c < 0 || static_cast<std::size_t>(c) >= grid.size())
                continue;
            const entity_id nt = grid[static_cast<std::size_t>(c)];
            if (nt == null_entity)
                continue;
            const tile_component& ntc = w.tiles.at(nt);
            const int side = hex_side_for_offset(step_dc[ci], step_dr[ci], (y & 1) != 0);
            const float sc = step_cost(w, nt, ntc, n, tc, side);
            if (sc < 0.0f)
                continue;
            const float nd = d + sc;
            if (nd > bound)
                continue;
            const auto nit = dist.find(nt);
            if (nit == dist.end() || nd < nit->second)
            {
                dist[nt] = nd;
                pq.push({nd, nt});
            }
        }
    }
    return dist;
}

bool lp_share_rule_valid(const std::array<float, lp_class_count>& share)
{
    double sum = 0.0;
    for (const float s : share)
    {
        if (!std::isfinite(s) || s < 0.0f || s > 1.0f)
            return false;
        sum += s;
    }
    return std::fabs(sum - 1.0) <= 1e-4;
}

namespace {

struct resolver_input
{
    std::vector<entity_id>               ids;      ///< building ids ascending
    std::unordered_map<entity_id, entity_id> owner_of;
    std::map<entity_id, entity_id>       champion; ///< nation -> champion (memo)
};

entity_id host_nation(const world& w, entity_id tile)
{
    const auto it = w.tile_to_nation.find(tile);
    return it != w.tile_to_nation.end() ? it->second : null_entity;
}

void read_input(const world& w, resolver_input& in)
{
    in.ids.reserve(w.buildings.size());
    for (const auto& kv : w.buildings)
        in.ids.push_back(kv.first);
    std::sort(in.ids.begin(), in.ids.end());
    for (const entity_id owner : economic_owner_ids(w))
        if (const std::vector<entity_id>* a = assets_of(w, owner))
            for (const entity_id bid : *a)
                in.owner_of.emplace(bid, owner);
}

entity_id champion_memo(const world& w, resolver_input& in, entity_id nation)
{
    const auto it = in.champion.find(nation);
    if (it != in.champion.end())
        return it->second;
    // BL-1407: a major is the saved fact (no champion corporation exists).
    const entity_id c = is_major_nation(w, nation) ? nation : null_entity;
    in.champion.emplace(nation, c);
    return c;
}

using field_key = std::tuple<entity_id, int, entity_id>; // nation, network, body

} // namespace

// The routing helpers (defined with lp_route_to below), declared here because step 1
// of the resolver needs the chain to know which sources add to the pool.
namespace {
entity_id chain_anchor(const world& w, entity_id nation, entity_id body);
const std::set<entity_id>& memo_chain(world& w, lp_reach_memo& memo, entity_id nation,
                                      entity_id body, entity_id centre);
} // namespace

lp_tick resolve_logistic_points(world& w, const std::vector<lp_march_claim>& marches)
{
    lp_tick out;
    resolver_input in;
    read_input(w, in);

    // 1. Sources, ascending building id. Routing (step 2) reads each one on its own
    //    reach through the memo, so no per-network seed set is built here (BL-1356).
    //    ONLY A CONNECTED SOURCE ADDS TO THE POOL (Ben, 2026-10-21, NR-1057): a hub with
    //    no chain back to the capital makes points nobody can draw.
    lp_reach_memo memo;
    std::map<entity_id, lp_nation_row> rows;
    for (const entity_id bid : in.ids)
    {
        const building_component& b = w.buildings.at(bid);
        lp_network net;
        if (!lp_source_network(b.type, net) || !lp_source_running(b))
            continue;
        const auto tit = w.tiles.find(b.tile);
        if (tit == w.tiles.end())
            continue;
        const entity_id nation = host_nation(w, b.tile);
        const std::size_t ni = static_cast<std::size_t>(net);
        // Ben 2026-10-14: a source makes points IN PROPORTION to the upkeep
        // goods it receives (half its stone, half its points), never zeroed
        // outright — the building's upkeep supply factor (BL-641), linear.
        const float made = lp_source_points[ni] * building_supply_scalar(b);
        bool linked = true;
        if (nation != null_entity)
        {
            const entity_id anchor = chain_anchor(w, nation, tit->second.body);
            linked = anchor == null_entity
                  || memo_chain(w, memo, nation, tit->second.body, anchor).count(bid) != 0;
        }
        out.sources.push_back({bid, b.tile, nation, net, made, linked});
        if (nation == null_entity)
            continue; // nobody's ground: adds to no pool
        if (!linked)
            continue; // off the chain: adds to no pool either
        lp_nation_row& r = rows[nation];
        r.nation = nation;
        r.made[ni] += made;
        ++r.sources[ni];
    }
    for (auto& [nid, r] : rows)
        r.pool = r.made[0] + r.made[1] + r.made[2];

    // 1b. THE MARCH CLAIMS FIRST (BL-1352; LOGISTICS.md § How a tile's points get to
    // it). The claims arrive in the march pass's own order, and each is served whole
    // or refused — a half-fed march is not a thing the movement rule can spend — so
    // what production claims against below is `pool - march_granted`, not `pool`.
    // A claim on nobody's ground, or on a nation with no pool at all, is refused;
    // nothing about that unit moves, exactly as a building there produces nothing.
    for (const lp_march_claim& m : marches)
    {
        lp_march_row mr;
        mr.unit   = m.unit;
        mr.nation = m.nation;
        mr.need   = m.need;
        if (m.nation != null_entity && m.need > 0.0f)
        {
            // The row is created if this nation has none yet: a nation with no sources
            // still ASKED, and `march_claim` is the ledger of what was asked. Its pool
            // is 0, so the claim is granted nothing and production - which creates its
            // own row at step 2 - sees exactly what it saw before (cold review).
            //
            // `march_granted` IS the running total, so what is left is the pool minus
            // it — no second accumulator to keep in step with it.
            lp_nation_row& r = rows[m.nation];
            r.nation = m.nation;
            r.march_claim += m.need;
            if (m.need <= (r.pool - r.march_granted) + 1e-6f)
            {
                r.march_granted += m.need;
                mr.granted = m.need;
            }
        }
        out.marches.push_back(mr);
        out.march_granted[mr.unit] = mr.granted;
    }

    // 2. Consumers: need, coverage (ROUTED, BL-1356), class. The memo is step 1's.
    // Each row's place in the queue: inside the falloff first, then by its nearest
    // qualifying source's distance, then by building id (LOGISTICS.md: "distance
    // orders the queue; it never raises the bill").
    struct queue_key { bool full; float norm; };
    std::vector<queue_key> qkeys;
    for (const entity_id bid : in.ids)
    {
        const building_component& b = w.buildings.at(bid);
        if (!lp_consumer(b))
            continue;
        // An unowned building never reaches the production pass (it walks the
        // owners' asset lists), so it claims nothing.
        const auto oit = in.owner_of.find(bid);
        if (oit == in.owner_of.end())
            continue;
        lp_building_row row;
        row.building = bid;
        row.owner  = oit->second;
        row.nation = host_nation(w, b.tile);
        row.need   = lp_need(b);
        const auto tit = w.tiles.find(b.tile);
        if (row.nation != null_entity && tit != w.tiles.end())
        {
            // BL-1405 (LOGISTICS.md § Logistic Points, "One pool"; Ben, 2026-10-23):
            // ONE CLASS. After the fold every building on a nation's ground is the
            // nation's, so its pool serves its own buildings as one queue - the
            // state / champion / other split has no subject. Every claim is `state`.
            row.cls = lp_class::state;
            const lp_route route = lp_route_to(w, memo, row.nation, tit->second.body, b.tile);
            row.coverage = route.cover;
            qkeys.push_back({route.full, route.norm});
            lp_nation_row& r = rows[row.nation];
            r.nation = row.nation;
            r.claim[static_cast<std::size_t>(row.cls)] += row.need * row.coverage;
        }
        else
            qkeys.push_back({false, 0.0f});
        out.buildings.push_back(row);
    }

    // 3. Grants per nation. `plenty` marks a pool that covers every claim on it, so
    //    step 4 hands those requests their claim EXACTLY rather than what is left of
    //    a float sum drained in another order (cold review). BL-1405: the share rule
    //    retired, so `nation_component::lp_shares` is NOT read and `rule_set` is
    //    always false - one class, one queue.
    std::map<entity_id, std::array<bool, lp_class_count>> plenty;
    for (auto& [nid, r] : rows)
    {
        std::array<bool, lp_class_count> g{};
        r.rule_set = false;
        const float total = r.claim[0] + r.claim[1] + r.claim[2];
        // BL-1352: what the march left. A nation row created by a CONSUMER below
        // (no sources, so no row at 1b) has pool 0 and granted 0, so this is 0 for
        // it exactly as `pool` was before the march joined.
        const float avail = std::max(0.0f, r.pool - r.march_granted);
        const float f = total > 0.0f ? std::min(1.0f, avail / total) : 0.0f;
        g.fill(avail >= total);
        for (std::size_t k = 0; k < lp_class_count; ++k)
            r.granted[k] = r.claim[k] * f;
        plenty[nid] = g;
    }
    // 4. SERVE IN QUEUE ORDER (BL-1356). Each nation's grant per class is what step 3
    // settled; it is handed out request by request in queue order, each request
    // taking its routed claim (need x coverage) whole while the class has points left.
    // Where points are plenty this is exactly the pro-rata answer (every claim met);
    // where they are short, the near requests are met and the far ones wait, rather
    // than every building getting the same thin share. A far building's claim is never
    // larger than a near one's: distance orders the queue and never raises the bill.
    std::vector<std::size_t> order(out.buildings.size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::sort(order.begin(), order.end(), [&](std::size_t x, std::size_t y) {
        const lp_building_row& a = out.buildings[x];
        const lp_building_row& b = out.buildings[y];
        if (a.nation != b.nation) return a.nation < b.nation;
        if (qkeys[x].full != qkeys[y].full) return qkeys[x].full; // inside the falloff first
        if (qkeys[x].norm != qkeys[y].norm) return qkeys[x].norm < qkeys[y].norm;
        return a.building < b.building;
    });
    // ONE QUEUE (BL-1405): the mechanical share is "in proportion to need" only while
    // points suffice, and a queue when they do not.
    std::map<entity_id, float> left;
    for (const auto& [nid, r] : rows)
        left[nid] = r.granted[0] + r.granted[1] + r.granted[2];
    for (const std::size_t i : order)
    {
        lp_building_row& row = out.buildings[i];
        if (row.nation != null_entity && row.need > 0.0f)
        {
            float& pot  = left[row.nation];
            const float claim = row.need * row.coverage;
            const float give  = plenty[row.nation][0] ? claim : std::max(0.0f, std::min(claim, pot));
            pot -= give;
            row.granted  = give;
            row.fraction = std::clamp(give / row.need, 0.0f, 1.0f);
        }
    }
    // The nation rows report what the queue actually handed each class - with no rule
    // that is no longer the pro-rata split step 3 computed, and a ledger that showed
    // the split while the buildings got the queue would be two answers to one question.
    for (auto& [nid, r] : rows)
        r.granted.fill(0.0f);
    for (const lp_building_row& row : out.buildings)
    {
        out.fraction[row.building] = row.fraction;
        if (row.nation != null_entity)
            rows[row.nation].granted[static_cast<std::size_t>(row.cls)] += row.granted;
    }
    // BL-1361: what a shipment may draw is the pool less the march's grant and
    // production's (LOGISTICS.md: march, production, trade) - one pot (BL-1405).
    for (auto& [nid, r] : rows)
    {
        const float avail = std::max(0.0f, r.pool - r.march_granted);
        const float total = std::max(0.0f, avail - (r.granted[0] + r.granted[1] + r.granted[2]));
        r.left.fill(0.0f);
        r.left[0] = total;
    }
    // BL-1361: every market's anchor - its centre tile, routed as any request is, so a
    // shipment's share is capped by the cover there. Read here, with this tick's memo,
    // so the trade pass never re-floods a source field per shipment.
    for (const auto& [mid, mc] : w.markets)
    {
        lp_trade_anchor a;
        const auto tit = w.tiles.find(mc.centre_tile);
        if (tit != w.tiles.end())
        {
            a.nation = host_nation(w, mc.centre_tile);
            if (a.nation != null_entity)
                a.cover = lp_route_to(w, memo, a.nation, tit->second.body, mc.centre_tile).cover;
        }
        out.trade_anchors[mid] = a;
    }
    out.resolved = true;
    out.nations.reserve(rows.size());
    for (auto& [nid, r] : rows)
        out.nations.push_back(r);
    return out;
}

// BL-1361: a shipment draws points again (LOGISTICS.md § How a tile's points get to it,
// Ben 2026-10-16) - by cargo quantity, at the anchor it leaves, from what production left.
namespace {

/// The index in `t.nations` of the row an anchor draws on, or -1 when the anchor has
/// no nation or the nation made no pool this tick.
std::ptrdiff_t anchor_row(const lp_tick& t, const lp_trade_anchor& a)
{
    if (a.nation == null_entity)
        return -1;
    // `nations` is ascending nation id (the resolver's own std::map order).
    const auto it = std::lower_bound(t.nations.begin(), t.nations.end(), a.nation,
                                     [](const lp_nation_row& r, entity_id n) { return r.nation < n; });
    return (it != t.nations.end() && it->nation == a.nation) ? (it - t.nations.begin()) : -1;
}

/// The pot a trader draws from at a nation: BL-1405, the one pot, whoever trades -
/// the same single class the production queue serves its buildings in.
std::size_t trader_pot(const world&, const lp_nation_row&, entity_id)
{
    return static_cast<std::size_t>(lp_class::state);
}

} // namespace

float lp_trade_admit(const world& w, const lp_tick& t, entity_id trader, entity_id market,
                     float units)
{
    if (!std::isfinite(units) || !(units > 0.0f))
        return 0.0f;
    const auto ait = t.trade_anchors.find(market);
    if (ait == t.trade_anchors.end() || !(ait->second.cover > 0.0f))
        return 0.0f;
    const std::ptrdiff_t i = anchor_row(t, ait->second);
    if (i < 0)
        return 0.0f;
    const lp_nation_row& r = t.nations[static_cast<std::size_t>(i)];
    const float pot   = r.left[trader_pot(w, r, trader)];
    const float share = units * std::min(1.0f, ait->second.cover);
    const float admit = std::min(share, pot);
    // The retired cap's crumb rule (BL-1186 E1): an anchor's last crumbs do not make a
    // convoy - nothing below one unit unless the cargo's covered share is itself
    // smaller (cold review: the uncovered cargo refused every sub-unit cargo at a
    // cover below 1, however full the pot).
    return (admit + 1e-6f >= std::min(share, 1.0f)) ? std::max(0.0f, admit) : 0.0f;
}

void lp_trade_draw(const world& w, lp_tick& t, entity_id trader, entity_id market, float units)
{
    if (!std::isfinite(units) || !(units > 0.0f))
        return;
    const auto ait = t.trade_anchors.find(market);
    if (ait == t.trade_anchors.end())
        return;
    const std::ptrdiff_t i = anchor_row(t, ait->second);
    if (i < 0)
        return;
    lp_nation_row& r = t.nations[static_cast<std::size_t>(i)];
    float& pot = r.left[trader_pot(w, r, trader)];
    pot = std::max(0.0f, pot - units);
    r.trade_drawn += units;
}

float lp_trade_left(const lp_tick& t, entity_id market)
{
    const auto ait = t.trade_anchors.find(market);
    if (ait == t.trade_anchors.end() || !(ait->second.cover > 0.0f))
        return 0.0f;
    const std::ptrdiff_t i = anchor_row(t, ait->second);
    if (i < 0)
        return 0.0f;
    const lp_nation_row& r = t.nations[static_cast<std::size_t>(i)];
    return r.left[0] + r.left[1] + r.left[2];
}

// BL-1356 (LOGISTICS.md § How a tile's points get to it; the four mechanism rules,
// Ben 2026-10-20; the capital as anchor, 2026-10-21). Routing reads INDIVIDUAL
// sources: each is tested on its own reach, so a hub with no chain back to its
// nation's capital supplies nothing, and the coverage a tile gets is the qualifying reaches taken nearest first
// and summed - the fade caps each source's share and the next one tops it up.
namespace {

/// What a nation's chain is anchored to on @p body (Ben, 2026-10-21: "Logistics
/// should centre on the capital for all"): its capital tile, when it stands on this
/// body. Null when the nation has none here - nothing to be connected back to, so
/// every source passes the test on that ground.
entity_id chain_anchor(const world& w, entity_id nation, entity_id body)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end() || nit->second.capital_tile == null_entity)
        return null_entity;
    const auto tit = w.tiles.find(nit->second.capital_tile);
    if (tit == w.tiles.end() || tit->second.body != body)
        return null_entity;
    return nit->second.capital_tile;
}

const std::vector<lp_source_ref>& memo_sources(world& w, lp_reach_memo& memo, entity_id nation,
                                               entity_id body)
{
    const auto key = std::make_pair(nation, body);
    auto it = memo.sources.find(key);
    if (it != memo.sources.end())
        return it->second;
    std::vector<lp_source_ref> list;
    for (const auto& [bid, b] : w.buildings)
    {
        lp_network net;
        if (!lp_source_network(b.type, net) || !lp_source_running(b))
            continue;
        const auto st = w.tiles.find(b.tile);
        const auto sn = w.tile_to_nation.find(b.tile);
        if (st != w.tiles.end() && st->second.body == body && sn != w.tile_to_nation.end()
            && sn->second == nation)
            list.push_back({bid, b.tile, net});
    }
    // Ascending building id, so nothing downstream depends on the map's walk order.
    std::sort(list.begin(), list.end(),
              [](const lp_source_ref& x, const lp_source_ref& y) { return x.building < y.building; });
    return memo.sources.emplace(key, std::move(list)).first->second;
}

const std::unordered_map<entity_id, float>& memo_field(world& w, lp_reach_memo& memo,
                                                       const lp_source_ref& s, entity_id body)
{
    // BL-1396: a port's field is the world's cached sea field from its tile - returned by
    // reference, read at once by every caller and held across no world mutation. Along
    // the lanes (Ben, 2026-10-25: lp_lane_walk).
    if (s.network == lp_network::sea)
    {
        auto ct = w.lp_sea_fields.find(s.tile);
        if (ct == w.lp_sea_fields.end())
            ct = w.lp_sea_fields.emplace(s.tile, lp_lane_walk(w, body, {s.tile})).first;
        return ct->second;
    }
    auto it = memo.fields.find(s.building);
    if (it == memo.fields.end())
        it = memo.fields.emplace(s.building, lp_distance_field(w, body, s.network, {s.tile})).first;
    return it->second;
}

/// THE CHAIN (Ben, 2026-10-20, after the direct rule measured G1 96.0 -> 71.2): the
/// sources connected to @p centre are those whose own reach touches the centre, and
/// then, repeatedly, those whose reach touches the tile of one already connected,
/// until nothing more joins. A monotone closure, so its result is the same whatever
/// order the sources are visited in.
const std::set<entity_id>& memo_chain(world& w, lp_reach_memo& memo, entity_id nation,
                                      entity_id body, entity_id centre)
{
    const auto key = std::make_tuple(nation, body, centre);
    auto it = memo.connected.find(key);
    if (it != memo.connected.end())
        return it->second;
    const std::vector<lp_source_ref>& list = memo_sources(w, memo, nation, body);
    std::set<entity_id> in;
    std::vector<entity_id> anchors{centre};
    std::vector<const lp_source_ref*> joined; // the connected sources, for the reverse test
    // A LINK COUNTS EITHER WAY (Ben, 2026-10-21, NR-1058): @p s joins when its own reach
    // touches an anchor (the capital or a connected source's tile), OR a connected
    // source's reach touches s's tile - so a station a connected road hub covers is on
    // the chain, though a station's own field holds only other stations.
    const auto touches = [&](const lp_source_ref& s) {
        const auto& f = memo_field(w, memo, s, body);
        for (const entity_id a : anchors)
        {
            const auto dit = f.find(a);
            if (dit != f.end() && lp_reach(s.network, dit->second) > 0.0f)
                return true;
        }
        for (const lp_source_ref* j : joined)
        {
            const auto& fj = memo_field(w, memo, *j, body);
            const auto dit = fj.find(s.tile);
            if (dit != fj.end() && lp_reach(j->network, dit->second) > 0.0f)
                return true;
        }
        return false;
    };
    for (bool grew = true; grew;)
    {
        grew = false;
        for (const lp_source_ref& s : list)
        {
            if (in.count(s.building) != 0 || !touches(s))
                continue;
            in.insert(s.building);
            anchors.push_back(s.tile);
            joined.push_back(&s);
            grew = true;
        }
    }
    return memo.connected.emplace(key, std::move(in)).first->second;
}

/// Whether @p s is connected to @p centre. A market with no centre tile (the
/// single-market body's unanchored case) has nothing to be connected back to, so
/// every source passes there.
bool memo_connected(world& w, lp_reach_memo& memo, entity_id nation, const lp_source_ref& s,
                    entity_id body, entity_id centre)
{
    if (centre == null_entity)
        return true;
    return memo_chain(w, memo, nation, body, centre).count(s.building) != 0;
}

/// One qualifying source for a tile's request, as routing ranks it.
struct route_cand { float norm; entity_id building; float reach; lp_network net; float dist; };

/// THE QUALIFYING SOURCES for @p tile, nearest first (a tie to the lower building id):
/// every running source of @p nation on @p body whose own reach touches the tile and
/// that is connected to the capital. The one ranking `lp_route_to` routes on and
/// `lp_route_flow` reads back, so the two cannot disagree about which sources carry a
/// request. @p blocked is set when a source reached the tile but has no chain back.
std::vector<route_cand> route_cands(world& w, lp_reach_memo& memo, entity_id nation,
                                    entity_id body, entity_id tile, bool& blocked)
{
    // The chain's anchor is the nation's CAPITAL, whatever market the tile clears in.
    const entity_id centre = chain_anchor(w, nation, body);
    std::vector<route_cand> cands;
    for (const lp_source_ref& s : memo_sources(w, memo, nation, body))
    {
        const auto& f = memo_field(w, memo, s, body);
        const auto dit = f.find(tile);
        if (dit == f.end())
            continue;
        const float r = lp_reach(s.network, dit->second);
        if (!(r > 0.0f))
            continue;
        if (!memo_connected(w, memo, nation, s, body, centre))
        {
            blocked = true;
            continue;
        }
        cands.push_back({dit->second / lp_full_reach[static_cast<std::size_t>(s.network)],
                         s.building, r, s.network, dit->second});
    }
    // Nearest first; a tie in distance goes to the lower building id, never to order.
    std::sort(cands.begin(), cands.end(), [](const route_cand& x, const route_cand& y) {
        return x.norm != y.norm ? x.norm < y.norm : x.building < y.building;
    });
    return cands;
}

} // namespace

lp_route lp_route_to(world& w, lp_reach_memo& memo, entity_id nation, entity_id body,
                     entity_id tile)
{
    lp_route out;
    if (nation == null_entity || tile == null_entity)
        return out;
    const std::vector<route_cand> cands = route_cands(w, memo, nation, body, tile, out.blocked);
    if (cands.empty())
        return out; // `blocked` survives: the card names why a near hub gives nothing
    float sum = 0.0f;
    for (const route_cand& k : cands)
    {
        sum += k.reach;
        ++out.used;
        if (sum >= 1.0f)
            break; // covered: the further sources are not drawn on
    }
    out.cover    = std::min(1.0f, sum);
    out.reached  = true;
    out.full     = cands.front().reach >= 1.0f;
    out.norm     = cands.front().norm;
    out.network  = cands.front().net;
    out.distance = cands.front().dist;
    return out;
}

namespace {

/// Steps from @p target over @p nation's own dry ground (4-neighbour, columns wrap) -
/// what a chain is steered by. A grid distance steered it across water or round the
/// wrap toward ground it could never reach; a walk over the ground a hub may stand on
/// cannot. A tile it does not reach is not a step closer.
std::unordered_map<entity_id, int> own_ground_steps(world& w, entity_id nation, entity_id body,
                                                    entity_id target)
{
    std::unordered_map<entity_id, int> dist;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return dist;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    std::vector<entity_id> q{target};
    dist[target] = 0;
    for (std::size_t head = 0; head < q.size(); ++head)
    {
        const entity_id cur = q[head];
        const tile_component& tc = w.tiles.at(cur);
        const int x = tc.grid_x, y = tc.grid_y;
        const int cand[4] = {y > 0 ? (y - 1) * gw + x : -1, y * gw + (x + 1) % gw,
                             y + 1 < gh ? (y + 1) * gw + x : -1, y * gw + (x + gw - 1) % gw};
        for (const int cc : cand)
        {
            if (cc < 0 || static_cast<std::size_t>(cc) >= grid.size())
                continue;
            const entity_id nt = grid[static_cast<std::size_t>(cc)];
            const auto nn = w.tile_to_nation.find(nt);
            if (nt == null_entity || dist.count(nt) != 0 || nn == w.tile_to_nation.end()
                || nn->second != nation || is_water(w.tiles.at(nt).substrate))
                continue;
            dist[nt] = dist[cur] + 1;
            q.push_back(nt);
        }
    }
    return dist;
}

} // namespace

lp_chain_plan lp_plan_chain(world& w, lp_reach_memo& memo, entity_id nation, entity_id body,
                            entity_id target, const std::function<bool(entity_id)>& may_place,
                            int max_hubs)
{
    lp_chain_plan out;
    if (nation == null_entity || target == null_entity)
        return out;
    const lp_route now = lp_route_to(w, memo, nation, body, target);
    if (now.cover >= 1.0f)
    {
        out.reachable = true; // covered as it stands
        return out;
    }
    const entity_id anchor = chain_anchor(w, nation, body);
    if (anchor == null_entity)
    {
        // Nothing to be connected back to: any hub passes, so one on the target covers it.
        if (may_place(target))
        {
            out.reachable = true;
            out.tiles.push_back(target);
        }
        return out;
    }

    std::vector<entity_id> anchors{anchor};
    const std::set<entity_id>& chain = memo_chain(w, memo, nation, body, anchor);
    for (const lp_source_ref& sr : memo_sources(w, memo, nation, body))
        if (chain.count(sr.building) != 0)
            anchors.push_back(sr.tile);
    const auto fc    = lp_distance_field(w, body, lp_network::road, {target});
    const auto steps = own_ground_steps(w, nation, body, target);
    std::set<entity_id> planned;
    float extra = 0.0f; // the planned hubs' reach at the target, on top of what routes now
    int last_gap = std::numeric_limits<int>::max();
    for (int n = 0; n < max_hubs; ++n)
    {
        if (now.cover + extra >= 1.0f)
        {
            out.reachable = true;
            return out;
        }
        const auto fz = lp_distance_field(w, body, lp_network::road, anchors);
        const auto free_tile = [&](entity_id t) { return planned.count(t) == 0 && may_place(t); };

        // (a) one hub that carries the target at full strength AND touches the chain
        entity_id best = null_entity;
        float best_z = 0.0f;
        for (const auto& [t2, dc] : fc)
        {
            if (!lp_full_strength(lp_network::road, dc))
                continue;
            const auto zt = fz.find(t2);
            if (zt == fz.end() || !(lp_reach(lp_network::road, zt->second) > 0.0f) || !free_tile(t2))
                continue;
            if (best == null_entity || zt->second < best_z || (zt->second == best_z && t2 < best))
            {
                best   = t2;
                best_z = zt->second;
            }
        }
        if (best != null_entity)
        {
            out.tiles.push_back(best);
            out.reachable = true;
            return out;
        }

        // (b) extend the chain: the tile it touches nearest the target by the walk
        //     (further out first on a tie, then the lower tile id)
        entity_id ext = null_entity;
        int ext_gap = 0;
        float ext_z = 0.0f;
        for (const auto& [t2, dz] : fz)
        {
            if (!(lp_reach(lp_network::road, dz) > 0.0f) || !free_tile(t2))
                continue;
            const auto st = steps.find(t2);
            if (st == steps.end())
                continue;
            const int gap = st->second;
            if (ext == null_entity || gap < ext_gap || (gap == ext_gap && dz > ext_z)
                || (gap == ext_gap && dz == ext_z && t2 < ext))
            {
                ext     = t2;
                ext_gap = gap;
                ext_z   = dz;
            }
        }
        if (ext == null_entity || ext_gap >= last_gap)
            return out; // the chain cannot get closer: unreachable, and nothing is kept
        last_gap = ext_gap;
        out.tiles.push_back(ext);
        planned.insert(ext);
        anchors.push_back(ext);
        if (const auto it = fc.find(ext); it != fc.end())
            extra += lp_reach(lp_network::road, it->second);
    }
    out.tiles.clear(); // ran out of hubs before covering it: not reachable within the bound
    return out;
}

float lp_coverage_at(world& w, lp_reach_memo& memo, entity_id tile, lp_coverage_why* why)
{
    if (why != nullptr)
        *why = {};
    const auto tit = w.tiles.find(tile);
    const auto nit = w.tile_to_nation.find(tile);
    if (tit == w.tiles.end() || nit == w.tile_to_nation.end())
        return 0.0f;
    const lp_route r = lp_route_to(w, memo, nit->second, tit->second.body, tile);
    if (why != nullptr)
        *why = { r.reached, r.network, r.distance, r.used, r.blocked };
    return r.cover;
}

float lp_points_reaching(world& w, lp_reach_memo& memo, const lp_tick& t, entity_id tile,
                         int* sources_out)
{
    if (sources_out != nullptr)
        *sources_out = 0;
    const auto tit = w.tiles.find(tile);
    const auto nit = w.tile_to_nation.find(tile);
    if (tit == w.tiles.end() || nit == w.tile_to_nation.end())
        return 0.0f;
    const entity_id nation = nit->second;
    const entity_id body   = tit->second.body;
    const entity_id centre = chain_anchor(w, nation, body);
    float sum = 0.0f;
    int   n   = 0;
    // Ascending building id (memo_sources' order), so the float sum is replay-exact.
    for (const lp_source_ref& s : memo_sources(w, memo, nation, body))
    {
        const auto& f   = memo_field(w, memo, s, body);
        const auto  dit = f.find(tile);
        if (dit == f.end())
            continue;
        const float r = lp_reach(s.network, dit->second);
        if (!(r > 0.0f) || !memo_connected(w, memo, nation, s, body, centre))
            continue;
        // `t.sources` is ascending building id: a binary search, not a walk.
        const auto sit = std::lower_bound(
            t.sources.begin(), t.sources.end(), s.building,
            [](const lp_source_row& row, entity_id id) { return row.building < id; });
        if (sit == t.sources.end() || sit->building != s.building)
            continue; // not in this tick's resolve: it made nothing this tick
        sum += r * sit->made;
        ++n;
    }
    if (sources_out != nullptr)
        *sources_out = n;
    return sum;
}

namespace {

/// The tiles of the cheapest walk from @p from back to the seed of @p s's own field,
/// @p from first and the seed last - RECONSTRUCTED, not recorded: the resolver keeps
/// only each tile's distance, so the walk is read back off that field by stepping, at
/// every tile, to the 4-cardinal neighbour the Dijkstra in `lp_distance_field` relaxed
/// it from - the neighbour P with dist(P) + step_cost(P -> here) equal to dist(here),
/// the same float sum the walk made (a 1e-4 tolerance as a fallback), the lowest tile
/// id where two qualify. A rail field is point to point, so for it the ends alone. A SEA
/// field rides the lanes (Ben, 2026-10-25), so its walk is read back the same way over the
/// six hex sides `lp_lane_walk` steps on - every step 1, the predecessor a lane tile or the
/// port itself - and the routed flow lies on the LANE TILES between the two ports.
std::vector<entity_id> field_walk_back(world& w, entity_id body, const lp_source_ref& s,
                                       const std::unordered_map<entity_id, float>& f,
                                       entity_id from)
{
    std::vector<entity_id> out;
    out.push_back(from);
    if (from == s.tile)
        return out;
    if (s.network == lp_network::rail)
    {
        out.push_back(s.tile);
        return out;
    }
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return out;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    if (s.network == lp_network::sea)
    {
        entity_id cur = from;
        for (int guard = 0; guard < 4096; ++guard)
        {
            const auto cit = f.find(cur);
            if (cit == f.end() || !(cit->second > 0.0f))
                break; // the port (distance 0), or a tile the field never held
            const tile_component& tc = w.tiles.at(cur);
            entity_id best = null_entity;
            for (int side = 0; side < 6; ++side)
            {
                const hex_neighbors::coord nb = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, side);
                if (nb.gy < 0 || nb.gy >= gh)
                    continue;
                const std::size_t pi = static_cast<std::size_t>(nb.gy) * static_cast<std::size_t>(gw)
                                     + static_cast<std::size_t>(((nb.gx % gw) + gw) % gw);
                if (pi >= grid.size() || grid[pi] == null_entity)
                    continue;
                const entity_id p = grid[pi];
                const auto pit = f.find(p);
                if (pit == f.end() || pit->second + 1.0f != cit->second)
                    continue;
                // A predecessor is laned water or the port itself: a landing never steps on.
                if (!is_water(w.tiles.at(p).substrate) && pit->second > 0.0f)
                    continue;
                if (best == null_entity || p < best)
                    best = p;
            }
            if (best == null_entity)
                break;
            out.push_back(best);
            cur = best;
        }
        return out;
    }
    const int step_dc[4] = {  0,  1,  0, -1 };
    const int step_dr[4] = { -1,  0,  1,  0 };
    entity_id cur = from;
    for (int guard = 0; guard < 4096; ++guard)
    {
        const auto cit = f.find(cur);
        if (cit == f.end() || !(cit->second > 0.0f))
            break; // the seed (distance 0), or a tile the field never held
        const tile_component& tc = w.tiles.at(cur);
        entity_id best = null_entity, near = null_entity;
        for (int k = 0; k < 4; ++k)
        {
            // P stepped INTO `cur` by (dc, dr), so P sits at cur - (dc, dr).
            const int py = tc.grid_y - step_dr[k];
            if (py < 0 || py >= gh)
                continue;
            const int px = ((tc.grid_x - step_dc[k]) % gw + gw) % gw; // columns wrap
            const std::size_t pi = static_cast<std::size_t>(py) * static_cast<std::size_t>(gw)
                                 + static_cast<std::size_t>(px);
            if (pi >= grid.size() || grid[pi] == null_entity)
                continue;
            const entity_id p = grid[pi];
            const auto pit = f.find(p);
            if (pit == f.end() || !(pit->second < cit->second))
                continue;
            const tile_component& ptc = w.tiles.at(p);
            const int side = hex_side_for_offset(step_dc[k], step_dr[k], (py & 1) != 0);
            const float sc = step_cost(w, cur, tc, s.network, ptc, side);
            if (sc < 0.0f)
                continue;
            const float d = pit->second + sc;
            if (d == cit->second && (best == null_entity || p < best))
                best = p;
            else if (std::fabs(d - cit->second) <= 1e-4f && (near == null_entity || p < near))
                near = p;
        }
        const entity_id next = best != null_entity ? best : near;
        if (next == null_entity)
            break;
        out.push_back(next);
        cur = next;
    }
    return out;
}

} // namespace

std::unordered_map<entity_id, lp_flow> lp_route_flow(world& w, lp_reach_memo& memo,
                                                     const lp_tick& t, entity_id nation,
                                                     entity_id body)
{
    std::unordered_map<entity_id, lp_flow> flow;
    if (nation == null_entity || body == null_entity)
        return flow;
    const entity_id centre = chain_anchor(w, nation, body);
    const std::vector<lp_source_ref>& list = memo_sources(w, memo, nation, body);

    // 1. THE CHAIN AS A TREE. `memo_chain` keeps only WHICH sources are connected; the
    // link each joined by is recovered here by repeating its closure exactly - the same
    // rounds, the same ascending order, the same two tests in the same order - and
    // recording the first link that admitted each source: its own reach touching the
    // capital or a joined source's tile (the walk back along ITS field), else a joined
    // source's reach touching its tile (the walk back along THAT source's field). A
    // source's path to the capital is that link's tiles plus its parent's path.
    std::map<entity_id, std::vector<std::pair<entity_id, entity_id>>> to_root; // source -> (tile, walk source), sorted unique
    if (centre != null_entity)
    {
        std::vector<entity_id> anchor_tiles{centre};
        std::vector<entity_id> anchor_owner{null_entity}; // null = the capital itself
        std::vector<const lp_source_ref*> joined;
        std::set<entity_id> in;
        for (bool grew = true; grew;)
        {
            grew = false;
            for (const lp_source_ref& s : list)
            {
                if (in.count(s.building) != 0)
                    continue;
                std::vector<entity_id> walk;
                entity_id parent = null_entity, owner = null_entity;
                bool linked = false;
                const auto& f = memo_field(w, memo, s, body);
                for (std::size_t a = 0; a < anchor_tiles.size() && !linked; ++a)
                {
                    const auto dit = f.find(anchor_tiles[a]);
                    if (dit != f.end() && lp_reach(s.network, dit->second) > 0.0f)
                    {
                        walk   = field_walk_back(w, body, s, f, anchor_tiles[a]);
                        parent = anchor_owner[a];
                        owner  = s.building;
                        linked = true;
                    }
                }
                for (std::size_t j = 0; j < joined.size() && !linked; ++j)
                {
                    const auto& fj = memo_field(w, memo, *joined[j], body);
                    const auto dit = fj.find(s.tile);
                    if (dit != fj.end() && lp_reach(joined[j]->network, dit->second) > 0.0f)
                    {
                        walk   = field_walk_back(w, body, *joined[j], fj, s.tile);
                        parent = joined[j]->building;
                        owner  = joined[j]->building;
                        linked = true;
                    }
                }
                if (!linked)
                    continue;
                // Each tile of the link is tagged with the source whose FIELD the walk
                // ran down - the source whose points are carried along that link.
                std::vector<std::pair<entity_id, entity_id>> hop;
                hop.reserve(walk.size());
                for (const entity_id tile : walk)
                    hop.push_back({tile, owner});
                if (parent != null_entity)
                {
                    const auto pit = to_root.find(parent);
                    if (pit != to_root.end())
                        hop.insert(hop.end(), pit->second.begin(), pit->second.end());
                }
                std::sort(hop.begin(), hop.end());
                hop.erase(std::unique(hop.begin(), hop.end()), hop.end());
                to_root[s.building] = std::move(hop);
                in.insert(s.building);
                anchor_tiles.push_back(s.tile);
                anchor_owner.push_back(s.building);
                joined.push_back(&s);
                grew = true;
            }
        }
    }
    const auto source_ref = [&](entity_id building) -> const lp_source_ref* {
        const auto it = std::lower_bound(list.begin(), list.end(), building,
            [](const lp_source_ref& r, entity_id id) { return r.building < id; });
        return (it != list.end() && it->building == building) ? &*it : nullptr;
    };

    // 2. EVERY REQUEST, ALONG ITS ROUTE. Each producing building on this nation's ground
    // here is routed exactly as the resolver routed it (`route_cands`, nearest first,
    // topped up until covered); each source it drew on carries its share of the request
    // - its reach there, the last one cut to what covers it - and the same share of
    // what the building was GRANTED. That share is laid on every tile from the building
    // back to the source, and on along the source's chain to the capital, each tile
    // once per share. A building no source qualifies for lays its request on its own
    // tile, with nothing granted. Every walk a share used also records, per tile, the
    // source whose field it ran down - the sources whose points pass through there.
    std::unordered_map<entity_id, std::vector<entity_id>> owners; // tile -> walk sources
    std::vector<std::pair<entity_id, entity_id>> walk;             // (tile, source)
    std::vector<entity_id> tiles;
    for (const lp_building_row& br : t.buildings) // ascending building id
    {
        if (br.nation != nation || !(br.need > 0.0f))
            continue;
        const auto bit = w.buildings.find(br.building);
        if (bit == w.buildings.end())
            continue;
        const entity_id home = bit->second.tile;
        const auto tit = w.tiles.find(home);
        if (tit == w.tiles.end() || tit->second.body != body)
            continue;
        bool blocked = false;
        const std::vector<route_cand> cands = route_cands(w, memo, nation, body, home, blocked);
        float cover = 0.0f;
        std::vector<std::pair<const route_cand*, float>> carried; // source, its share of cover
        for (const route_cand& k : cands)
        {
            const float take = std::min(k.reach, 1.0f - cover);
            carried.push_back({&k, take});
            cover += take;
            if (cover >= 1.0f)
                break;
        }
        if (carried.empty() || !(cover > 0.0f))
        {
            flow[home].requested += br.need;
            continue;
        }
        for (const auto& [k, take] : carried)
        {
            const lp_source_ref* s = source_ref(k->building);
            if (s == nullptr)
                continue;
            const float share = take / cover;
            walk.clear();
            for (const entity_id tile :
                 field_walk_back(w, body, *s, memo_field(w, memo, *s, body), home))
                walk.push_back({tile, s->building});
            const auto rit = to_root.find(s->building);
            if (rit != to_root.end())
                walk.insert(walk.end(), rit->second.begin(), rit->second.end());
            tiles.clear();
            for (const auto& [tile, src] : walk)
            {
                tiles.push_back(tile);
                owners[tile].push_back(src);
            }
            std::sort(tiles.begin(), tiles.end());
            tiles.erase(std::unique(tiles.begin(), tiles.end()), tiles.end());
            for (const entity_id tile : tiles)
            {
                lp_flow& fl = flow[tile];
                fl.requested += br.need * share;
                fl.granted   += br.granted * share;
            }
        }
    }

    // 3. WHAT THE CHAIN COULD DELIVER THERE. Each source whose walk crosses a tile is
    // counted ONCE at that tile, however many requests' walks brought it: the points it
    // made this tick x its reach at the tile (its own field there). Summed in ascending
    // source id, so the float sum is replay-exact.
    for (auto& [tile, srcs] : owners)
    {
        std::sort(srcs.begin(), srcs.end());
        srcs.erase(std::unique(srcs.begin(), srcs.end()), srcs.end());
        float avail = 0.0f;
        for (const entity_id b : srcs)
        {
            const lp_source_ref* s = source_ref(b);
            if (s == nullptr)
                continue;
            const auto sit = std::lower_bound(
                t.sources.begin(), t.sources.end(), b,
                [](const lp_source_row& row, entity_id id) { return row.building < id; });
            if (sit == t.sources.end() || sit->building != b)
                continue;
            const auto& f = memo_field(w, memo, *s, body);
            const auto dit = f.find(tile);
            if (dit == f.end())
                continue;
            avail += sit->made * lp_reach(s->network, dit->second);
        }
        flow[tile].available = avail;
    }
    return flow;
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

lp_seed_report seed_logistics_sources(world& w)
{
    lp_seed_report rep;
    // EVERY PORT LAYS LANES (Ben, 2026-10-25; LOGISTICS.md § 4b): every port standing at
    // the fold is joined to every other within fifty water tiles before any reach is read,
    // since a port's points ride the lanes. Ascending id, pairs (lo, hi).
    {
        const port_lane_stats ls = lay_all_port_lanes(w);
        rep.port_lanes_laid  += ls.laid;
        rep.port_lane_tiles  += ls.tiles_added;
        rep.port_lanes_unreachable += ls.unreachable;
    }
    constexpr float k_pool_margin = 2.0f; // the settle grows need ~1.9x (measured)
    constexpr int   k_max_hubs    = 4000; // a bound, never reached on a shipped world

    resolver_input in;
    read_input(w, in);

    // The tiles that already hold a running hub, so none is stacked on one.
    std::set<entity_id> hub_tiles;
    for (const entity_id bid : in.ids)
    {
        const building_component& b = w.buildings.at(bid);
        lp_network net;
        if (!lp_source_network(b.type, net) || !lp_source_running(b))
            continue;
        const auto tit = w.tiles.find(b.tile);
        const entity_id nation = host_nation(w, b.tile);
        if (tit == w.tiles.end() || nation == null_entity)
            continue;
        if (net == lp_network::road)
            hub_tiles.insert(b.tile);
    }
    // BL-1356: coverage is ROUTED - the same rule the resolver serves by, so a hub
    // generation places counts only where the economy will let it deliver.
    lp_reach_memo memo;
    const auto coverage_of = [&](entity_id nation, entity_id body, entity_id tile) {
        return lp_route_to(w, memo, nation, body, tile).cover;
    };
    const auto place_source = [&](entity_id nation, entity_id tile, entity_id consumer_owner,
                                  building_type type) {
        entity_id owner = null_entity;
        if (champion_memo(w, in, nation) != null_entity)
            owner = nation; // a major: state industry
        else
            owner = consumer_owner;
        if (owner == null_entity)
            return false;
        const entity_id bid = w.create_entity();
        building_component bc;
        bc.tile               = tile;
        bc.type               = type;
        bc.workforce_assigned = 0.0f; // zero staff, as every network node
        bc.ticks_remaining    = 0;    // stands complete, as every generated holding
        w.buildings[bid]  = bc;
        w.stockpiles[bid] = stockpile_component{};
        if (const auto nit = w.nations.find(owner); nit != w.nations.end())
            nit->second.state_assets.push_back(bid);
        else
            return false;
        const entity_id body = w.tiles.at(tile).body;
        memo.sources.erase({nation, body}); // the new source joins the list; old fields stand
        memo.connected.clear();             // and may extend a chain
        hub_tiles.insert(tile);             // no second network node on this tile
        if (type == building_type::port)
        {
            // A port the seeding lands lays its lanes as it stands (Ben, 2026-10-25): a
            // bridge's two ports are joined by one. The lanes moved traversal cost, so the
            // world's caches went with them (lay_port_lanes); the memo's road fields stand.
            const port_lane_stats ls = lay_port_lanes(w, bid);
            rep.port_lanes_laid  += ls.laid;
            rep.port_lane_tiles  += ls.tiles_added;
            rep.port_lanes_unreachable += ls.unreachable;
            ++rep.ports_placed;
        }
        else
            ++rep.hubs_placed;
        return true;
    };
    const auto place_hub = [&](entity_id nation, entity_id tile, entity_id consumer_owner) {
        return place_source(nation, tile, consumer_owner, building_type::inland_logistics_hub);
    };

    // 1. COVERAGE: every consumer on a nation's ground reached at full strength,
    //    BY A HUB CONNECTED TO ITS NATION'S CAPITAL (BL-1356; the chain, Ben 2026-10-20;
    //    the capital as its anchor, 2026-10-21).
    //    Hubs are laid OUTWARD along the chain: where a tile exists that both carries
    //    the consumer at full strength and touches the chain (the capital, or a
    //    connected source's tile), one hub there covers it; otherwise a hub stands on
    //    the chain's edge nearest the consumer, joining the chain and carrying it
    //    closer, and the search repeats. A road step costs the edge it crosses, the same
    //    both ways, so a field FROM the chain reads whether a hub at X would touch it.
    //    Every choice takes the least key with a tile-id tie-break: order-independent.
    const auto placeable = [&](entity_id t, entity_id nation) {
        return host_nation(w, t) == nation && hub_tiles.count(t) == 0
            && !is_water(w.tiles.at(t).substrate);
    };
    // Steps from @p target over the nation's own dry ground (4-neighbour, columns wrap):
    // what the chain is steered by. A grid distance steered it across water or round the
    // wrap toward ground it could never reach (the bridge fixture caught it); a walk over
    // the ground a hub may stand on cannot. A tile it does not reach is not a step closer.
    const auto own_steps = [&](entity_id nation, entity_id body, entity_id target) {
        std::unordered_map<entity_id, int> dist;
        const auto bit = w.bodies.find(body);
        if (bit == w.bodies.end())
            return dist;
        const int gw = bit->second.grid_width, gh = bit->second.grid_height;
        const std::vector<entity_id>& grid = body_tile_grid(w, body);
        std::vector<entity_id> q{target};
        dist[target] = 0;
        for (std::size_t head = 0; head < q.size(); ++head)
        {
            const entity_id cur = q[head];
            const tile_component& tc = w.tiles.at(cur);
            const int x = tc.grid_x, y = tc.grid_y;
            const int cand[4] = {y > 0 ? (y - 1) * gw + x : -1, y * gw + (x + 1) % gw,
                                 y + 1 < gh ? (y + 1) * gw + x : -1, y * gw + (x + gw - 1) % gw};
            for (const int cc : cand)
            {
                if (cc < 0 || static_cast<std::size_t>(cc) >= grid.size())
                    continue;
                const entity_id nt = grid[static_cast<std::size_t>(cc)];
                if (nt == null_entity || dist.count(nt) != 0 || host_nation(w, nt) != nation
                    || is_water(w.tiles.at(nt).substrate))
                    continue;
                dist[nt] = dist[cur] + 1;
                q.push_back(nt);
            }
        }
        return dist;
    };
    constexpr int k_chain_steps = 64; // a bound on hubs laid for one consumer
    // The capital's own-ground region per (nation, body): every dry tile of the nation
    // a 4-neighbour walk reaches from the capital without leaving its ground. Read only
    // to say WHY a consumer is left uncovered (`unconnectable_cut_off`).
    std::map<std::pair<entity_id, entity_id>, std::set<entity_id>> home_region;
    const auto region_of = [&](entity_id nation, entity_id body, entity_id capital)
        -> const std::set<entity_id>& {
        const auto key = std::make_pair(nation, body);
        auto it = home_region.find(key);
        if (it != home_region.end())
            return it->second;
        std::set<entity_id> seen;
        const auto bit = w.bodies.find(body);
        if (bit != w.bodies.end())
        {
            const int gw = bit->second.grid_width, gh = bit->second.grid_height;
            const std::vector<entity_id>& grid = body_tile_grid(w, body);
            std::vector<entity_id> q{capital};
            seen.insert(capital);
            for (std::size_t head = 0; head < q.size(); ++head)
            {
                const tile_component& tc = w.tiles.at(q[head]);
                const int x = tc.grid_x, y = tc.grid_y;
                const int cand[4] = {y > 0 ? (y - 1) * gw + x : -1, y * gw + (x + 1) % gw,
                                     y + 1 < gh ? (y + 1) * gw + x : -1, y * gw + (x + gw - 1) % gw};
                for (const int cc : cand)
                {
                    if (cc < 0 || static_cast<std::size_t>(cc) >= grid.size())
                        continue;
                    const entity_id nt = grid[static_cast<std::size_t>(cc)];
                    if (nt == null_entity || seen.count(nt) != 0 || host_nation(w, nt) != nation
                        || is_water(w.tiles.at(nt).substrate))
                        continue;
                    seen.insert(nt);
                    q.push_back(nt);
                }
            }
        }
        return home_region.emplace(key, std::move(seen)).first->second;
    };
    // The road chain laid outward to one consumer (step 1 below; re-run after a bridge).
    // @p avoid: a tile the chain must not take - the shore tile a bridge's port needs.
    const auto lay_chain = [&](entity_id nation, entity_id body, entity_id ct, entity_id tile,
                               entity_id owner, entity_id avoid = null_entity) -> bool {
        const auto fc = lp_distance_field(w, body, lp_network::road, {tile});
        const auto steps = own_steps(nation, body, tile);
        int last_gap = std::numeric_limits<int>::max();
        bool covered = false;
        for (int step = 0; step < k_chain_steps && rep.hubs_placed < k_max_hubs; ++step)
        {
            if (coverage_of(nation, body, tile) >= 1.0f)
            {
                covered = true;
                break;
            }
            std::vector<entity_id> anchors{ct};
            const std::set<entity_id>& chain = memo_chain(w, memo, nation, body, ct);
            for (const lp_source_ref& sr : memo_sources(w, memo, nation, body))
                if (chain.count(sr.building) != 0)
                    anchors.push_back(sr.tile);
            const auto fz = lp_distance_field(w, body, lp_network::road, anchors);

            // (a) One hub that carries the consumer at full strength AND touches the chain.
            entity_id best = null_entity;
            float best_z = 0.0f;
            for (const auto& [t2, dc] : fc)
            {
                if (!lp_full_strength(lp_network::road, dc))
                    continue;
                const auto zt = fz.find(t2);
                if (zt == fz.end() || !(lp_reach(lp_network::road, zt->second) > 0.0f)
                    || !placeable(t2, nation) || t2 == avoid)
                    continue;
                if (best == null_entity || zt->second < best_z || (zt->second == best_z && t2 < best))
                {
                    best   = t2;
                    best_z = zt->second;
                }
            }
            if (best != null_entity)
            {
                place_hub(nation, best, owner);
                continue; // re-checks coverage at the top
            }

            // (b) Extend the chain: the tile the chain touches nearest the consumer on the
            //     grid (further out first on a tie, then the lower tile id).
            entity_id ext = null_entity;
            int ext_gap = 0;
            float ext_z = 0.0f;
            for (const auto& [t2, dz] : fz)
            {
                if (!(lp_reach(lp_network::road, dz) > 0.0f) || !placeable(t2, nation) || t2 == avoid)
                    continue;
                const auto st = steps.find(t2);
                if (st == steps.end())
                    continue; // not on ground that walks to the target: no step closer
                const int gap = st->second;
                if (ext == null_entity || gap < ext_gap || (gap == ext_gap && dz > ext_z)
                    || (gap == ext_gap && dz == ext_z && t2 < ext))
                {
                    ext     = t2;
                    ext_gap = gap;
                    ext_z   = dz;
                }
            }
            if (ext == null_entity || ext_gap >= last_gap)
                break; // the chain cannot get closer (sea, a frontier): stop here
            last_gap = ext_gap;
            place_hub(nation, ext, owner);
        }
        return covered || coverage_of(nation, body, tile) >= 1.0f;
    };

    // The nation's dry ground a 4-neighbour walk reaches from @p start (an island).
    const auto flood_own = [&](entity_id nation, entity_id body, entity_id start) {
        std::set<entity_id> seen;
        const auto bit = w.bodies.find(body);
        if (bit == w.bodies.end())
            return seen;
        const int gw = bit->second.grid_width, gh = bit->second.grid_height;
        const std::vector<entity_id>& grid = body_tile_grid(w, body);
        std::vector<entity_id> q{start};
        seen.insert(start);
        for (std::size_t head = 0; head < q.size(); ++head)
        {
            const tile_component& tc = w.tiles.at(q[head]);
            const int x = tc.grid_x, y = tc.grid_y;
            const int cand[4] = {y > 0 ? (y - 1) * gw + x : -1, y * gw + (x + 1) % gw,
                                 y + 1 < gh ? (y + 1) * gw + x : -1, y * gw + (x + gw - 1) % gw};
            for (const int cc : cand)
            {
                if (cc < 0 || static_cast<std::size_t>(cc) >= grid.size())
                    continue;
                const entity_id nt = grid[static_cast<std::size_t>(cc)];
                if (nt == null_entity || seen.count(nt) != 0 || host_nation(w, nt) != nation
                    || is_water(w.tiles.at(nt).substrate))
                    continue;
                seen.insert(nt);
                q.push_back(nt);
            }
        }
        return seen;
    };

    // A land tile with SEA on one of its six hex sides: where a port can stand - the
    // test placement makes (is_coastal), and the sides the sea walk lands across.
    const auto coastal = [&](entity_id t) {
        const tile_component& tc = w.tiles.at(t);
        const auto bit = w.bodies.find(tc.body);
        if (bit == w.bodies.end() || is_water(tc.substrate))
            return false;
        const int gw = bit->second.grid_width, gh = bit->second.grid_height;
        const std::vector<entity_id>& grid = body_tile_grid(w, tc.body);
        for (int side = 0; side < 6; ++side)
        {
            const hex_neighbors::coord nb = hex_neighbors::neighbour(tc.grid_x, tc.grid_y, side);
            if (nb.gy < 0 || nb.gy >= gh)
                continue;
            const std::size_t ci = static_cast<std::size_t>(nb.gy) * static_cast<std::size_t>(gw)
                                 + static_cast<std::size_t>(((nb.gx % gw) + gw) % gw);
            if (ci < grid.size() && grid[ci] != null_entity && is_sea(w.tiles.at(grid[ci]).substrate))
                return true;
        }
        return false;
    };

    // The bridge reads the sea field itself (lp_sea_walk, BL-1396): one walk, so a bridge
    // it admits is a bridge the ports connect, and no route crosses land between its ends.
    const auto water_field = [&](entity_id body, const std::vector<entity_id>& seeds) {
        return lp_sea_walk(w, body, seeds);
    };

    // THE PORT LINK (BL-1356; Ben, 2026-10-21, and 2026-10-25: "Generate port links for
    // nations"). Ground of a nation's cut off from its capital's road network by water no
    // bridge spans gets a port on each coast, joined by sea - a HOME port on a shore already
    // joined to the capital (an existing connected port, or a new one on the capital's own
    // ground or an island linked before) and an ISLAND port on the cut-off ground's shore -
    // and EACH PORT IS ROADED INTO THE NETWORK ON ITS OWN COAST, by the site pass's own-ground
    // join (`lay_own_ground_join`): the network is the roads and the rail (Ben, 2026-10-24),
    // so a port no road reaches is a port no hub's points reach, and a port no hub covers
    // joins nothing. A sea step costs the same both ways (1 a sea step, 1 a landing), so one
    // field FROM the island's coast reads every home candidate's distance. Least distance,
    // then the lower tile id: order-independent. False when the sea is too wide.
    std::set<std::tuple<entity_id, entity_id, entity_id>> bridged;
    // The ground of every island bridged so far, per (nation, body): its shore is as good
    // a home shore as the capital's for the next island out (island-hopping).
    std::map<std::pair<entity_id, entity_id>, std::set<entity_id>> bridged_ground;
    // Every tile of a linked island -> the island port's tile: the network a road on that
    // ground joins (a later consumer's, or a home port's for the next island out).
    std::map<entity_id, entity_id> ground_port;
    // The road a port link lays: @p from joined to the network holding @p anchor. A laid
    // crossing moves every road field, so the memo's fields and chains are dropped.
    const auto link_road = [&](entity_id nation, entity_id from, entity_id anchor) -> bool {
        int laid = 0;
        if (!lay_own_ground_join(w, nation, from, anchor, &laid))
            return false;
        rep.link_crossings += laid;
        if (laid > 0)
        {
            memo.fields.clear();
            memo.connected.clear();
        }
        return true;
    };
    constexpr int k_link_tries = 8; // shore candidates tried, nearest first, per port
    const auto bridge = [&](entity_id nation, entity_id body, entity_id ct,
                            const std::set<entity_id>& island, entity_id target,
                            entity_id owner) -> bool {
        std::vector<entity_id> shore;
        for (const entity_id t : island)
            if (coastal(t) && placeable(t, nation))
                shore.push_back(t);
        if (shore.empty())
            return false;
        const auto fi = water_field(body, shore);
        // Ben, 2026-10-25: two ports are laned only within FULL reach (fifty), and a port's
        // points ride the lanes - so a bridge across wider water would deliver nothing.
        const auto in_reach = [&](entity_id t) {
            const auto it = fi.find(t);
            return it != fi.end() && it->second > 0.0f && lp_full_strength(lp_network::sea, it->second)
                       ? it->second : -1.0f;
        };
        // (i) a connected port already in reach of the island's shore
        entity_id home = null_entity;
        float home_d = 0.0f;
        const std::set<entity_id>& chain = memo_chain(w, memo, nation, body, ct);
        for (const lp_source_ref& sr : memo_sources(w, memo, nation, body))
        {
            if (sr.network != lp_network::sea || chain.count(sr.building) == 0)
                continue;
            const float d = in_reach(sr.tile);
            if (d >= 0.0f && (home == null_entity || d < home_d || (d == home_d && sr.tile < home)))
            {
                home   = sr.tile;
                home_d = d;
            }
        }
        // (ii) else a new port on a shore already joined to the capital - the capital's
        //      own ground, or an island bridged before - the shore tile nearest the island
        //      by sea that a road joins to the network there. The road chain is laid out to
        //      it (a hub carrying it at full strength), so the port joins the chain - a
        //      connected hub's reach covers its tile, and a link counts either way (NR-1058).
        if (home == null_entity)
        {
            std::set<entity_id> homeland = region_of(nation, body, ct);
            if (const auto bg = bridged_ground.find({nation, body}); bg != bridged_ground.end())
                homeland.insert(bg->second.begin(), bg->second.end());
            std::vector<std::pair<float, entity_id>> cands;
            for (const entity_id t : homeland)
            {
                if (!coastal(t) || !placeable(t, nation))
                    continue;
                const float d = in_reach(t);
                if (d >= 0.0f)
                    cands.push_back({d, t});
            }
            std::sort(cands.begin(), cands.end()); // least distance, then the lower tile id
            int tries = 0;
            for (const auto& [d, t] : cands)
            {
                if (tries++ >= k_link_tries)
                    break;
                const auto gp = ground_port.find(t);
                if (link_road(nation, t, gp != ground_port.end() ? gp->second : ct))
                {
                    home   = t;
                    home_d = d;
                    break;
                }
            }
            if (home == null_entity)
                return false; // the sea is wider than a port's reach, or no road reaches a shore
            if (coverage_of(nation, body, home) < 1.0f)
                lay_chain(nation, body, ct, home, owner, /*avoid=*/home);
            // The port joins the chain only through a connected source whose reach covers
            // its tile (NR-1058). If the road chain did not get there, a port here would
            // deliver nothing, and so would the island port beyond it: place neither.
            if (!(coverage_of(nation, body, home) > 0.0f))
                return false;
            if (hub_tiles.count(home) != 0 || !place_source(nation, home, owner, building_type::port))
                return false;
        }
        // the island port: the shore tile nearest the home port by sea that a road joins to
        // the network the target stands on
        const auto fh = water_field(body, {home});
        std::vector<std::pair<float, entity_id>> lands;
        for (const entity_id t : shore)
        {
            const auto it = fh.find(t);
            if (it == fh.end() || !(it->second > 0.0f) || !lp_full_strength(lp_network::sea, it->second)
                || !placeable(t, nation))
                continue; // within fifty: the two ports are laned (Ben, 2026-10-25)
            lands.push_back({it->second, t});
        }
        std::sort(lands.begin(), lands.end());
        entity_id land = null_entity;
        int tries = 0;
        for (const auto& [d, t] : lands)
        {
            (void)d;
            if (tries++ >= k_link_tries)
                break;
            if (link_road(nation, t, target))
            {
                land = t;
                break;
            }
        }
        if (land == null_entity)
            return false;
        if (!place_source(nation, land, owner, building_type::port))
            return false;
        for (const entity_id t : island)
            ground_port.emplace(t, land);
        ++rep.islands_bridged;
        return true;
    };
    // A consumer on linked ground that the chain still misses stands on a road fragment the
    // island port's road does not reach: join it to the port's network, then lay the chain.
    const auto rejoin_linked = [&](entity_id nation, entity_id body, entity_id ct, entity_id tile,
                                   entity_id owner) -> bool {
        const auto gp = ground_port.find(tile);
        if (gp == ground_port.end() || !link_road(nation, tile, gp->second))
            return false;
        return lay_chain(nation, body, ct, tile, owner);
    };

    // 0. THE CAPITAL IS THE PEAK (BL-1367; LOGISTICS.md § Where generation puts the
    //    sources, Ben 2026-10-18). Seeded hierarchically, before any consumer: first a
    //    source ON every capital, so each chain is anchored by points of its own (rule:
    //    every capital holds a source); then the nation's other MARKET CENTRES, each
    //    covered at full strength by the chain laid out from the capital - bridged by sea
    //    where the centre's ground is cut off, as a consumer's is - the lesser sources,
    //    and the anchors a shipment leaves from (BL-1361). Consumers come after, so a
    //    chain to a centre is a chain they can extend. Ascending ids: order-independent.
    //    WHOSE: the nation's own, a minor's as a major's - a minor may hold its network
    //    nodes as the state (Ben, 2026-10-22; NATIONS.md § State industry). A capital
    //    on water, or ground the nation does not hold, gets none here, and the census
    //    counts it rather than this pass forcing it.
    {
        const auto step0_owner = [](entity_id nation) { return nation; };
        std::vector<entity_id> nids;
        for (const auto& [nid, nc] : w.nations)
        {
            (void)nc;
            nids.push_back(nid);
        }
        std::sort(nids.begin(), nids.end());
        for (const entity_id nid : nids)
        {
            const entity_id cap = w.nations.at(nid).capital_tile;
            const entity_id owner = step0_owner(nid);
            if (cap == null_entity || w.tiles.count(cap) == 0 || owner == null_entity
                || rep.hubs_placed >= k_max_hubs)
                continue;
            // A capital that already holds a running source of any network (a rail
            // station, a state port on a coastal seat) holds one: none is stacked on it.
            bool holds = hub_tiles.count(cap) != 0;
            for (const entity_id bid : in.ids)
            {
                const building_component& b = w.buildings.at(bid);
                lp_network net;
                if (b.tile == cap && lp_source_network(b.type, net) && lp_source_running(b))
                    holds = true;
            }
            if (!holds && placeable(cap, nid))
                place_source(nid, cap, owner, building_type::inland_logistics_hub);
        }
        std::vector<entity_id> mids;
        for (const auto& [mid, mc] : w.markets)
        {
            (void)mc;
            mids.push_back(mid);
        }
        std::sort(mids.begin(), mids.end());
        for (const entity_id mid : mids)
        {
            if (rep.hubs_placed >= k_max_hubs)
                break;
            const entity_id centre = w.markets.at(mid).centre_tile;
            const auto tit = w.tiles.find(centre);
            const entity_id nation = host_nation(w, centre);
            if (tit == w.tiles.end() || nation == null_entity || is_water(tit->second.substrate))
                continue;
            const entity_id owner = step0_owner(nation);
            if (owner == null_entity)
                continue;
            const entity_id body = tit->second.body;
            if (coverage_of(nation, body, centre) >= 1.0f)
                continue;
            const entity_id ct = chain_anchor(w, nation, body);
            if (ct == null_entity)
            {
                if (placeable(centre, nation))
                    place_hub(nation, centre, owner);
                continue;
            }
            if (lay_chain(nation, body, ct, centre, owner)
                || region_of(nation, body, ct).count(centre) != 0)
                continue;
            const std::set<entity_id> island = flood_own(nation, body, centre);
            const auto key = std::make_tuple(nation, body, *island.begin());
            if (bridged.count(key) == 0 && bridge(nation, body, ct, island, centre, owner))
            {
                bridged.insert(key);
                bridged_ground[{nation, body}].insert(island.begin(), island.end());
            }
            if (!lay_chain(nation, body, ct, centre, owner))
                rejoin_linked(nation, body, ct, centre, owner);
        }
    }

    // TWO PASSES (cold review): an island the first pass could not bridge - the sea
    // too wide from the capital's coast - may be in reach of an island bridged LATER in
    // the same pass, whatever order the consumers' ids put them in. The second pass
    // retries every consumer still uncovered; only it counts what is left.
    for (int pass = 0; pass < 2; ++pass)
    for (const entity_id bid : in.ids)
    {
        if (rep.hubs_placed >= k_max_hubs)
            break;
        const building_component& b = w.buildings.at(bid);
        if (!lp_consumer(b))
            continue;
        const entity_id nation = host_nation(w, b.tile);
        const auto tit = w.tiles.find(b.tile);
        if (nation == null_entity || tit == w.tiles.end())
            continue;
        const entity_id body = tit->second.body;
        const auto oit = in.owner_of.find(bid);
        if (oit == in.owner_of.end())
            continue; // unowned: never produces, claims nothing
        if (coverage_of(nation, body, b.tile) >= 1.0f)
            continue;
        const entity_id owner = oit->second;
        // The chain is anchored at the nation's capital (Ben, 2026-10-21).
        const entity_id ct = chain_anchor(w, nation, body);

        // No capital on this body: every source passes the test, so a hub on the
        // consumer's own tile covers it, exactly as before routing.
        if (ct == null_entity)
        {
            if (placeable(b.tile, nation))
                place_hub(nation, b.tile, owner);
            continue;
        }

        bool covered = lay_chain(nation, body, ct, b.tile, owner);

        // THE SEA BRIDGE (BL-1356; Ben, 2026-10-21). A consumer the road chain cannot
        // reach because its ground is cut off from the capital by WATER: land a port on
        // its coast that the chain's sea reach covers, then lay the road chain on from
        // there. An island is marked bridged only when a bridge stands, so a failed try
        // is retried (pass 2); once bridged, it serves every consumer on that ground.
        if (!covered && region_of(nation, body, ct).count(b.tile) == 0)
        {
            const std::set<entity_id> island = flood_own(nation, body, b.tile); // holds b.tile
            const auto key = std::make_tuple(nation, body, *island.begin());
            if (bridged.count(key) == 0 && bridge(nation, body, ct, island, b.tile, owner))
            {
                bridged.insert(key);
                bridged_ground[{nation, body}].insert(island.begin(), island.end());
            }
            covered = lay_chain(nation, body, ct, b.tile, owner)
                   || rejoin_linked(nation, body, ct, b.tile, owner);
        }
        if (pass == 0)
            continue; // counted after the retry
        if (!covered && coverage_of(nation, body, b.tile) < 1.0f)
        {
            ++rep.unconnectable;
            rep.unconnectable_ids.push_back(bid); // in.ids is ascending
            if (region_of(nation, body, ct).count(b.tile) == 0)
                ++rep.unconnectable_cut_off;
        }
    }

    // 2. SUFFICIENCY: each pool meets its need with margin; a further hub stands
    //    at the market centre of the first consumer (ascending id) whose centre
    //    holds none. BL-1356 (Ben, 2026-10-21, NR-1057): only a CONNECTED hub adds to
    //    the pool, so the hub must stand where it joins the chain - the centre or the
    //    consumer's tile when either would, else the free tile touching the chain
    //    nearest the capital. A hub that would not connect is never placed for pool:
    //    it would add nothing and this loop would place another, and another.
    const auto would_connect = [&](entity_id nation, entity_id body, entity_id t2,
                                   const std::unordered_map<entity_id, float>& fz) {
        (void)nation; (void)body;
        const auto it = fz.find(t2);
        return it != fz.end() && lp_reach(lp_network::road, it->second) > 0.0f;
    };
    const auto chain_field = [&](entity_id nation, entity_id body, entity_id anchor) {
        std::vector<entity_id> anchors{anchor};
        const std::set<entity_id>& chain = memo_chain(w, memo, nation, body, anchor);
        for (const lp_source_ref& sr : memo_sources(w, memo, nation, body))
            if (chain.count(sr.building) != 0)
                anchors.push_back(sr.tile);
        return lp_distance_field(w, body, lp_network::road, anchors);
    };
    for (int round = 0; round < 64 && rep.hubs_placed < k_max_hubs; ++round)
    {
        const lp_tick t = resolve_logistic_points(w);
        bool placed_any = false;
        for (const lp_nation_row& r : t.nations)
        {
            const float claim = r.claim[0] + r.claim[1] + r.claim[2];
            if (!(r.pool < claim * k_pool_margin))
                continue;
            for (const lp_building_row& row : t.buildings)
            {
                if (row.nation != r.nation)
                    continue;
                const building_component& b = w.buildings.at(row.building);
                const entity_id body = w.tiles.at(b.tile).body;
                const entity_id anchor = chain_anchor(w, r.nation, body);
                const entity_id mid = market_for_tile(w, b.tile);
                entity_id at = mid != null_entity ? w.markets.at(mid).centre_tile : null_entity;
                if (anchor == null_entity)
                {
                    // No capital on this body: every source passes, the placement stands.
                    if (at == null_entity || host_nation(w, at) != r.nation
                        || is_water(w.tiles.at(at).substrate) || hub_tiles.count(at) != 0)
                        at = b.tile;
                    if (hub_tiles.count(at) != 0 || is_water(w.tiles.at(at).substrate))
                        continue;
                }
                else
                {
                    const auto fz = chain_field(r.nation, body, anchor);
                    const auto ok = [&](entity_id t2) {
                        return t2 != null_entity && w.tiles.find(t2) != w.tiles.end()
                            && w.tiles.at(t2).body == body && placeable(t2, r.nation)
                            && would_connect(r.nation, body, t2, fz);
                    };
                    if (!ok(at))
                        at = ok(b.tile) ? b.tile : null_entity;
                    if (at == null_entity)
                    {
                        float best_z = 0.0f;
                        for (const auto& [t2, dz] : fz)
                            if (ok(t2) && (at == null_entity || dz < best_z || (dz == best_z && t2 < at)))
                            {
                                at     = t2;
                                best_z = dz;
                            }
                    }
                    if (at == null_entity)
                        continue;
                }
                if (place_hub(r.nation, at, row.owner))
                {
                    placed_any = true;
                    break;
                }
            }
        }
        if (!placed_any)
            break;
    }
    if (rep.hubs_placed > 0 || rep.ports_placed > 0 || rep.link_crossings > 0) // a port moves the sea-leg caches too
        invalidate_logistics_caches(w);

    // 3. The reading.
    lp_seed_read(w, rep);
    return rep;
}

void lp_seed_read(world& w, lp_seed_report& rep)
{
    rep.nations = 0;
    rep.consumers = rep.nobodys_ground = rep.fully_covered = rep.partly_covered = rep.uncovered = 0;
    rep.need_total = rep.pool_total = rep.granted_total = 0.0f;
    for (int& n : rep.sources_by_net)
        n = 0;
    const lp_tick t = resolve_logistic_points(w);
    rep.nations = static_cast<int>(t.nations.size());
    for (const lp_source_row& s : t.sources)
        ++rep.sources_by_net[static_cast<std::size_t>(s.network)];
    for (const lp_nation_row& r : t.nations)
    {
        rep.pool_total += r.pool;
        rep.granted_total += r.granted[0] + r.granted[1] + r.granted[2];
    }
    for (const lp_building_row& row : t.buildings)
    {
        ++rep.consumers;
        rep.need_total += row.need;
        if (row.nation == null_entity) { ++rep.nobodys_ground; continue; }
        if (row.coverage >= 1.0f) ++rep.fully_covered;
        else if (row.coverage > 0.0f) ++rep.partly_covered;
        else ++rep.uncovered;
    }
}

void lp_seed_read_at_gate(world& w, const recipe_registry& reg, lp_seed_report& rep)
{
    // A source the funding gate idles makes no points and reaches nobody
    // (`lp_source_running`), so the reading counts the world as the first tick
    // will run it: every state building the gate would idle is idled for the
    // read and restored after it. Nothing else is touched; the gate itself
    // runs at the top of the first tick and writes the flag for real.
    std::vector<entity_id> flipped;
    rep.gate_idled_sources = 0;
    for (const entity_id bid : state_gate_would_idle(w, reg))
    {
        building_component& b = w.buildings.at(bid);
        lp_network net;
        if (lp_source_network(b.type, net) && lp_source_running(b))
            ++rep.gate_idled_sources;
        if (!b.state_idle)
        {
            b.state_idle = true;
            flipped.push_back(bid);
        }
    }
    // BL-1360: rail coverage now reads the rail network (lp_distance_field), which
    // caches its stations and is rebuilt only on a cut change - so a flip made for
    // this read must clear it, as the real gate does (state_industry.cpp), both ways:
    // or the read sees idled stations still on the line, and the next reader after
    // the restore sees them still off it.
    if (!flipped.empty())
        w.rail_networks.clear();
    lp_seed_read(w, rep);
    for (const entity_id bid : flipped)
        w.buildings.at(bid).state_idle = false;
    if (!flipped.empty())
        w.rail_networks.clear();
}

void print_lp_seed_report(const lp_seed_report& r)
{
    std::printf("[lp] seeding: %d hubs placed; sources road %d rail %d sea %d (%d more the funding "
                "gate idles); %d consumers "
                "(%d full, %d partial, %d uncovered, %d on nobody's ground; %d no hub could connect, %d of them cut off; "
                "%d ports bridging %d islands, %d link crossings); "
                "need %.1f, pool %.1f, "
                "granted %.1f (%.1f%%) over %d nations\n",
                r.hubs_placed, r.sources_by_net[0], r.sources_by_net[1], r.sources_by_net[2],
                r.gate_idled_sources, r.consumers, r.fully_covered, r.partly_covered, r.uncovered, r.nobodys_ground,
                r.unconnectable, r.unconnectable_cut_off, r.ports_placed, r.islands_bridged,
                r.link_crossings, static_cast<double>(r.need_total), static_cast<double>(r.pool_total),
                static_cast<double>(r.granted_total),
                r.need_total > 0.0f ? 100.0 * r.granted_total / r.need_total : 0.0, r.nations);
    std::printf("[lp] port lanes: %d laid between ports within fifty, %d sea tiles added, "
                "%d pairs no sea joins\n",
                r.port_lanes_laid, r.port_lane_tiles, r.port_lanes_unreachable);
    std::fflush(stdout);
}
