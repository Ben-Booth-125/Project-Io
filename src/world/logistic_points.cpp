#include "logistic_points.hpp"

#include "logistics.hpp"       // body_tile_grid, invalidate_logistics_caches
#include "market_clearing.hpp" // market_for_tile
#include "river_generation.hpp" // BL-1340: hex_side_for_offset — a step prices the edge it crosses

#include <algorithm>
#include <cmath>
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
    const float n   = lp_need_per_run * run;
    return std::isfinite(n) ? n : 0.0f;
}

float lp_reach(lp_network n, float d)
{
    const std::size_t i = static_cast<std::size_t>(n);
    if (!std::isfinite(d) || d < 0.0f)
        return 0.0f;
    if (d <= lp_full_reach[i])
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
            if (water)
                return -1.0f;
            const std::uint8_t lvl = road_tier_on_side(from, side);
            return (lvl >= 1 && lvl <= 3) ? lp_road_step[lvl] : lp_offnet_step;
        }
        case lp_network::rail:
            if (water)
                return -1.0f;
            return w.rail_track.count(tile) != 0 ? 1.0f : lp_offnet_step;
        case lp_network::sea:
            return water ? 1.0f : lp_offnet_step;
    }
    return -1.0f;
}

} // namespace

std::unordered_map<entity_id, float> lp_distance_field(world& w, entity_id body, lp_network n,
                                                       const std::vector<entity_id>& seeds)
{
    std::unordered_map<entity_id, float> dist;
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end() || seeds.empty())
        return dist;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    const float bound = lp_full_reach[static_cast<std::size_t>(n)]
                      + lp_fade_length[static_cast<std::size_t>(n)];

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
    const entity_id c = champion_of(w, nation);
    in.champion.emplace(nation, c);
    return c;
}

using field_key = std::tuple<entity_id, int, entity_id>; // nation, network, body

} // namespace

lp_tick resolve_logistic_points(world& w)
{
    lp_tick out;
    resolver_input in;
    read_input(w, in);

    // 1. Sources, ascending building id; seeds per (nation, network, body).
    std::map<field_key, std::vector<entity_id>> seeds;
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
        out.sources.push_back({bid, b.tile, nation, net});
        if (nation == null_entity)
            continue; // nobody's ground: adds to no pool
        lp_nation_row& r = rows[nation];
        r.nation = nation;
        const std::size_t ni = static_cast<std::size_t>(net);
        // Ben 2026-10-14: a source makes points IN PROPORTION to the upkeep
        // goods it receives (half its stone, half its points), never zeroed
        // outright — the building's upkeep supply factor (BL-641), linear.
        r.made[ni] += lp_source_points[ni] * building_supply_scalar(b);
        ++r.sources[ni];
        seeds[{nation, static_cast<int>(net), tit->second.body}].push_back(b.tile);
    }
    for (auto& [nid, r] : rows)
        r.pool = r.made[0] + r.made[1] + r.made[2];

    // 2. Consumers: need, coverage, class.
    std::map<field_key, std::unordered_map<entity_id, float>> fields;
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
            if (row.owner == row.nation)
                row.cls = lp_class::state;
            else if (row.owner != null_entity && row.owner == champion_memo(w, in, row.nation))
                row.cls = lp_class::champion;
            else
                row.cls = lp_class::other;
            const entity_id body = tit->second.body;
            float cov = 0.0f;
            for (int n = 0; n < static_cast<int>(lp_network_count); ++n)
            {
                const field_key k{row.nation, n, body};
                const auto sit = seeds.find(k);
                if (sit == seeds.end())
                    continue;
                auto fit = fields.find(k);
                if (fit == fields.end())
                    fit = fields.emplace(k, lp_distance_field(w, body, static_cast<lp_network>(n),
                                                              sit->second)).first;
                const auto dit = fit->second.find(b.tile);
                if (dit != fit->second.end())
                    cov = std::max(cov, lp_reach(static_cast<lp_network>(n), dit->second));
            }
            row.coverage = cov;
            lp_nation_row& r = rows[row.nation];
            r.nation = row.nation;
            r.claim[static_cast<std::size_t>(row.cls)] += row.need * row.coverage;
        }
        out.buildings.push_back(row);
    }

    // 3. Grants per nation, per class.
    std::map<entity_id, std::array<float, lp_class_count>> grant_frac;
    for (auto& [nid, r] : rows)
    {
        std::array<float, lp_class_count> g{};
        const auto nit = w.nations.find(nid);
        const bool rule = nit != w.nations.end() && nit->second.lp_shares.set;
        r.rule_set = rule;
        if (!rule)
        {
            const float total = r.claim[0] + r.claim[1] + r.claim[2];
            const float f = total > 0.0f ? std::min(1.0f, r.pool / total) : 0.0f;
            g.fill(f);
            for (std::size_t k = 0; k < lp_class_count; ++k)
                r.granted[k] = r.claim[k] * f;
        }
        else
        {
            const auto& sh = nit->second.lp_shares.share;
            std::array<float, lp_class_count> base{}, shortfall{};
            float surplus = 0.0f, short_total = 0.0f;
            for (std::size_t k = 0; k < lp_class_count; ++k)
            {
                const float alloc = r.pool * sh[k];
                base[k]      = std::min(alloc, r.claim[k]);
                surplus     += std::max(0.0f, alloc - r.claim[k]);
                shortfall[k] = r.claim[k] - base[k];
                short_total += shortfall[k];
            }
            for (std::size_t k = 0; k < lp_class_count; ++k)
            {
                float got = base[k];
                if (surplus > 0.0f && short_total > 0.0f)
                    got += std::min(shortfall[k], surplus * (shortfall[k] / short_total));
                r.granted[k] = got;
                g[k] = r.claim[k] > 0.0f ? std::min(1.0f, got / r.claim[k]) : 0.0f;
            }
        }
        grant_frac[nid] = g;
    }
    for (lp_building_row& row : out.buildings)
    {
        if (row.nation != null_entity && row.need > 0.0f)
        {
            const float g = grant_frac[row.nation][static_cast<std::size_t>(row.cls)];
            row.fraction = std::clamp(row.coverage * g, 0.0f, 1.0f);
            row.granted  = row.need * row.fraction;
        }
        out.fraction[row.building] = row.fraction;
    }
    out.nations.reserve(rows.size());
    for (auto& [nid, r] : rows)
        out.nations.push_back(r);
    return out;
}

float lp_coverage_at(world& w, lp_reach_memo& memo, entity_id tile)
{
    const auto tit = w.tiles.find(tile);
    const auto nit = w.tile_to_nation.find(tile);
    if (tit == w.tiles.end() || nit == w.tile_to_nation.end())
        return 0.0f;
    const entity_id nation = nit->second, body = tit->second.body;
    float cov = 0.0f;
    for (int n = 0; n < static_cast<int>(lp_network_count); ++n)
    {
        const auto k = std::make_tuple(nation, n, body);
        auto fit = memo.fields.find(k);
        if (fit == memo.fields.end())
        {
            std::vector<entity_id> seeds;
            for (const auto& [bid, b] : w.buildings)
            {
                lp_network net;
                if (!lp_source_network(b.type, net) || static_cast<int>(net) != n || !lp_source_running(b))
                    continue;
                const auto st = w.tiles.find(b.tile);
                const auto sn = w.tile_to_nation.find(b.tile);
                if (st != w.tiles.end() && st->second.body == body && sn != w.tile_to_nation.end()
                    && sn->second == nation)
                    seeds.push_back(b.tile);
            }
            // lp_distance_field sorts its seeds, so the unordered walk cannot matter.
            fit = memo.fields.emplace(k, lp_distance_field(w, body, static_cast<lp_network>(n), seeds)).first;
        }
        const auto dit = fit->second.find(tile);
        if (dit != fit->second.end())
            cov = std::max(cov, lp_reach(static_cast<lp_network>(n), dit->second));
    }
    return cov;
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------

lp_seed_report seed_logistics_sources(world& w)
{
    lp_seed_report rep;
    constexpr float k_pool_margin = 2.0f; // the settle grows need ~1.9x (measured)
    constexpr int   k_max_hubs    = 4000; // a bound, never reached on a shipped world

    resolver_input in;
    read_input(w, in);

    // Running sources of every network, per (nation, network, body).
    std::map<field_key, std::vector<entity_id>> seeds;
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
        seeds[{nation, static_cast<int>(net), tit->second.body}].push_back(b.tile);
        if (net == lp_network::road)
            hub_tiles.insert(b.tile);
    }
    std::map<field_key, std::unordered_map<entity_id, float>> fields;
    const auto field_of = [&](const field_key& k) -> const std::unordered_map<entity_id, float>& {
        auto fit = fields.find(k);
        if (fit == fields.end())
        {
            const auto sit = seeds.find(k);
            fit = fields.emplace(k, sit != seeds.end()
                                        ? lp_distance_field(w, std::get<2>(k),
                                                            static_cast<lp_network>(std::get<1>(k)),
                                                            sit->second)
                                        : std::unordered_map<entity_id, float>{}).first;
        }
        return fit->second;
    };
    const auto coverage_of = [&](entity_id nation, entity_id body, entity_id tile) {
        float cov = 0.0f;
        for (int n = 0; n < static_cast<int>(lp_network_count); ++n)
        {
            const auto& f = field_of({nation, n, body});
            const auto it = f.find(tile);
            if (it != f.end())
                cov = std::max(cov, lp_reach(static_cast<lp_network>(n), it->second));
        }
        return cov;
    };
    const auto place_hub = [&](entity_id nation, entity_id tile, entity_id consumer_owner) {
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
        bc.type               = building_type::inland_logistics_hub;
        bc.workforce_assigned = 0.0f; // zero staff, as every network node
        bc.ticks_remaining    = 0;    // stands complete, as every generated holding
        w.buildings[bid]  = bc;
        w.stockpiles[bid] = stockpile_component{};
        if (const auto nit = w.nations.find(owner); nit != w.nations.end())
            nit->second.state_assets.push_back(bid);
        else if (const auto cit = w.corporations.find(owner); cit != w.corporations.end())
            cit->second.assets.push_back(bid);
        else
            return false;
        const entity_id body = w.tiles.at(tile).body;
        const field_key k{nation, static_cast<int>(lp_network::road), body};
        seeds[k].push_back(tile);
        fields.erase(k);
        hub_tiles.insert(tile);
        ++rep.hubs_placed;
        return true;
    };

    // 1. COVERAGE: every consumer on a nation's ground reached at full strength.
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
        // The market centre serving it, when on the nation's land and close enough
        // to reach it at full strength; else its own tile.
        entity_id at = b.tile;
        if (const entity_id mid = market_for_tile(w, b.tile); mid != null_entity)
        {
            const entity_id ct = w.markets.at(mid).centre_tile;
            const auto cit = w.tiles.find(ct);
            if (cit != w.tiles.end() && cit->second.body == body && !is_water(cit->second.substrate)
                && host_nation(w, ct) == nation)
            {
                const auto f = lp_distance_field(w, body, lp_network::road, {ct});
                const auto it = f.find(b.tile);
                if (it != f.end() && lp_reach(lp_network::road, it->second) >= 1.0f)
                    at = ct;
            }
        }
        if (hub_tiles.count(at) != 0)
            at = b.tile;
        if (hub_tiles.count(at) != 0 || is_water(w.tiles.at(at).substrate))
            continue;
        place_hub(nation, at, owner);
    }

    // 2. SUFFICIENCY: each pool meets its need with margin; a further hub stands
    //    at the market centre of the first consumer (ascending id) whose centre
    //    holds none.
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
                const entity_id mid = market_for_tile(w, b.tile);
                entity_id at = mid != null_entity ? w.markets.at(mid).centre_tile : null_entity;
                if (at == null_entity || host_nation(w, at) != r.nation
                    || is_water(w.tiles.at(at).substrate) || hub_tiles.count(at) != 0)
                    at = b.tile;
                if (hub_tiles.count(at) != 0 || is_water(w.tiles.at(at).substrate))
                    continue;
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
    if (rep.hubs_placed > 0)
        invalidate_logistics_caches(w);

    // 3. The reading.
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
    return rep;
}

void print_lp_seed_report(const lp_seed_report& r)
{
    std::printf("[lp] seeding: %d hubs placed; sources road %d rail %d sea %d; %d consumers "
                "(%d full, %d partial, %d uncovered, %d on nobody's ground); need %.1f, pool %.1f, "
                "granted %.1f (%.1f%%) over %d nations\n",
                r.hubs_placed, r.sources_by_net[0], r.sources_by_net[1], r.sources_by_net[2],
                r.consumers, r.fully_covered, r.partly_covered, r.uncovered, r.nobodys_ground,
                static_cast<double>(r.need_total), static_cast<double>(r.pool_total),
                static_cast<double>(r.granted_total),
                r.need_total > 0.0f ? 100.0 * r.granted_total / r.need_total : 0.0, r.nations);
    std::fflush(stdout);
}
