#include "logistics_chain.hpp"

#include "world/hex_neighbors.hpp"
#include "world/logistics.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <unordered_map>

// LENSES.md § Logistics lens, "Every link of the chain". The header says what this
// recovers and why; every walk here is in ascending id so the result is replay-exact.

namespace ui {

namespace {

using field_t = std::unordered_map<entity_id, float>;

/// The nation's running sources on the body, ascending building id - the list the
/// resolver's memo holds (`memo_sources`), taken from it when it is there.
const std::vector<lp_source_ref>& sources_of(world& w, lp_reach_memo& memo, entity_id nation,
                                             entity_id body)
{
    const auto key = std::make_pair(nation, body);
    if (const auto it = memo.sources.find(key); it != memo.sources.end())
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
    std::sort(list.begin(), list.end(),
              [](const lp_source_ref& x, const lp_source_ref& y) { return x.building < y.building; });
    return memo.sources.emplace(key, std::move(list)).first->second;
}

/// One source's own distance field: the memo's (road, rail) or the world's sea-field
/// cache (a port), both filled by the resolve's own routing; built through the public
/// `lp_distance_field` only when neither holds it.
class field_book
{
public:
    field_book(world& w, lp_reach_memo& memo, entity_id body) : m_w(w), m_memo(memo), m_body(body) {}

    const field_t& of(const lp_source_ref& s)
    {
        if (s.network == lp_network::sea)
        {
            if (const auto it = m_w.lp_sea_fields.find(s.tile); it != m_w.lp_sea_fields.end())
                return it->second;
        }
        else if (const auto it = m_memo.fields.find(s.building); it != m_memo.fields.end())
            return it->second;
        auto it = m_own.find(s.building);
        if (it == m_own.end())
            it = m_own.emplace(s.building, lp_distance_field(m_w, m_body, s.network, {s.tile})).first;
        return it->second;
    }

private:
    world&                        m_w;
    lp_reach_memo&                m_memo;
    entity_id                     m_body;
    std::map<entity_id, field_t>  m_own;
};

/// The capital tile when it stands on @p body (the chain's anchor, `chain_anchor`).
entity_id capital_on(const world& w, entity_id nation, entity_id body)
{
    const auto nit = w.nations.find(nation);
    if (nit == w.nations.end() || nit->second.capital_tile == null_entity)
        return null_entity;
    const auto tit = w.tiles.find(nit->second.capital_tile);
    if (tit == w.tiles.end() || tit->second.body != body)
        return null_entity;
    return nit->second.capital_tile;
}

/// A sea leg's water: from @p from down @p f to its seed (distance 0), each step to the
/// six-neighbour holding the least distance below the current one, the lowest tile id
/// on a tie. A Dijkstra field always holds such a neighbour (the tile it was relaxed
/// from), so the walk ends at the seed; it stops early only on a field that does not.
std::vector<entity_id> descend(world& w, entity_id body, const field_t& f, entity_id from)
{
    std::vector<entity_id> out{from};
    const auto bit = w.bodies.find(body);
    if (bit == w.bodies.end())
        return out;
    const int gw = bit->second.grid_width, gh = bit->second.grid_height;
    const std::vector<entity_id>& grid = body_tile_grid(w, body);
    entity_id cur = from;
    for (int guard = 0; guard < 4096; ++guard)
    {
        const auto cit = f.find(cur);
        if (cit == f.end() || !(cit->second > 0.0f))
            break;
        const auto tit = w.tiles.find(cur);
        if (tit == w.tiles.end())
            break;
        entity_id best = null_entity;
        float     best_d = 0.0f;
        for (int side = 0; side < 6; ++side)
        {
            const hex_neighbors::coord nb = hex_neighbors::neighbour(tit->second.grid_x, tit->second.grid_y, side);
            if (nb.gy < 0 || nb.gy >= gh)
                continue;
            const int cx = ((nb.gx % gw) + gw) % gw;
            const std::size_t ni = static_cast<std::size_t>(nb.gy) * static_cast<std::size_t>(gw)
                                 + static_cast<std::size_t>(cx);
            if (ni >= grid.size() || grid[ni] == null_entity)
                continue;
            const auto nit = f.find(grid[ni]);
            if (nit == f.end() || !(nit->second < cit->second))
                continue; // only strictly downhill
            if (best == null_entity || nit->second < best_d
                || (nit->second == best_d && grid[ni] < best))
            {
                best   = grid[ni];
                best_d = nit->second;
            }
        }
        if (best == null_entity)
            break;
        out.push_back(best);
        cur = best;
    }
    return out;
}

/// One link of the tree: the source whose FIELD it runs down (its points are what the
/// link carries) and its two ends.
struct link
{
    const lp_source_ref* walker = nullptr;
    entity_id from = null_entity; ///< the far end (an anchor tile, or the joining source's tile)
    entity_id to   = null_entity; ///< the walker's own tile
};

} // namespace

std::vector<lp_leg> lp_chain_legs(world& w, lp_reach_memo& memo, const lp_tick& t,
                                  entity_id nation, entity_id body)
{
    std::vector<lp_leg> legs;
    const entity_id centre = capital_on(w, nation, body);
    if (nation == null_entity || body == null_entity || centre == null_entity)
        return legs;
    const std::vector<lp_source_ref>& list = sources_of(w, memo, nation, body);
    field_book fields(w, memo, body);

    const auto made_of = [&](entity_id building) -> float {
        const auto sit = std::lower_bound(
            t.sources.begin(), t.sources.end(), building,
            [](const lp_source_row& row, entity_id id) { return row.building < id; });
        return (sit != t.sources.end() && sit->building == building) ? sit->made : 0.0f;
    };

    // 1. THE CHAIN AS A TREE - `lp_route_flow` step 1, test for test.
    std::map<entity_id, link>      link_of;   // joined source -> the link it joined by
    std::map<entity_id, entity_id> parent_of; // joined source -> the source it hangs from (null: the capital)
    std::vector<entity_id>               anchor_tiles{centre};
    std::vector<entity_id>               anchor_owner{null_entity};
    std::vector<const lp_source_ref*>    joined;
    std::set<entity_id>                  in;
    for (bool grew = true; grew;)
    {
        grew = false;
        for (const lp_source_ref& s : list)
        {
            if (in.count(s.building) != 0)
                continue;
            link lk;
            entity_id parent = null_entity;
            bool linked = false;
            const field_t& f = fields.of(s);
            for (std::size_t a = 0; a < anchor_tiles.size() && !linked; ++a)
            {
                const auto dit = f.find(anchor_tiles[a]);
                if (dit != f.end() && lp_reach(s.network, dit->second) > 0.0f)
                {
                    lk     = {&s, anchor_tiles[a], s.tile};
                    parent = anchor_owner[a];
                    linked = true;
                }
            }
            for (std::size_t j = 0; j < joined.size() && !linked; ++j)
            {
                const field_t& fj = fields.of(*joined[j]);
                const auto dit = fj.find(s.tile);
                if (dit != fj.end() && lp_reach(joined[j]->network, dit->second) > 0.0f)
                {
                    lk     = {joined[j], s.tile, joined[j]->tile};
                    parent = joined[j]->building;
                    linked = true;
                }
            }
            if (!linked)
                continue;
            link_of[s.building]   = lk;
            parent_of[s.building] = parent;
            in.insert(s.building);
            anchor_tiles.push_back(s.tile);
            anchor_owner.push_back(s.building);
            joined.push_back(&s);
            grew = true;
        }
    }

    // 2. THE LEGS: every tree link a rail or sea field carries, as tiles.
    std::map<entity_id, std::size_t> leg_of; // joined source -> its leg (rail / sea links only)
    for (const auto& [sb, lk] : link_of) // ascending building id
    {
        if (lk.walker == nullptr || lk.walker->network == lp_network::road || lk.from == lk.to)
            continue;
        lp_leg leg;
        leg.network = lk.walker->network;
        leg.in_tree = true;
        if (leg.network == lp_network::rail)
        {
            const logistics_path p = rail_leg_path(w, body, lk.from, lk.to);
            leg.tiles = p.reachable && !p.tiles.empty() ? p.tiles : std::vector<entity_id>{lk.from, lk.to};
        }
        else
            leg.tiles = descend(w, body, fields.of(*lk.walker), lk.from);
        if (std::find(leg.tiles.begin(), leg.tiles.end(), lk.to) == leg.tiles.end())
            leg.tiles.push_back(lk.to);
        const field_t& f = fields.of(*lk.walker);
        const auto dit = f.find(lk.from);
        leg.available = dit != f.end()
            ? made_of(lk.walker->building) * lp_reach(lk.walker->network, dit->second) : 0.0f;
        leg_of[sb] = legs.size();
        legs.push_back(std::move(leg));
    }

    // 3. THE TRAFFIC: each request routed as the resolver routes it, its shares laid
    // on every leg up its sources' paths to the capital (`lp_route_flow` step 2).
    struct cand { float norm; entity_id building; float reach; };
    std::vector<cand> cands;
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
        cands.clear();
        for (const lp_source_ref& s : list)
        {
            const field_t& f = fields.of(s);
            const auto dit = f.find(home);
            if (dit == f.end())
                continue;
            const float r = lp_reach(s.network, dit->second);
            if (!(r > 0.0f) || in.count(s.building) == 0)
                continue;
            cands.push_back({dit->second / lp_full_reach[static_cast<std::size_t>(s.network)],
                             s.building, r});
        }
        std::sort(cands.begin(), cands.end(), [](const cand& x, const cand& y) {
            return x.norm != y.norm ? x.norm < y.norm : x.building < y.building;
        });
        float cover = 0.0f;
        std::vector<std::pair<entity_id, float>> carried;
        for (const cand& k : cands)
        {
            const float take = std::min(k.reach, 1.0f - cover);
            carried.push_back({k.building, take});
            cover += take;
            if (cover >= 1.0f)
                break;
        }
        if (carried.empty() || !(cover > 0.0f))
            continue;
        for (const auto& [src, take] : carried)
        {
            const float share = take / cover;
            // Up the path: the source's own link, then its parent's, to the capital.
            entity_id at = src;
            for (int guard = 0; guard < 4096 && at != null_entity; ++guard)
            {
                if (const auto lit = leg_of.find(at); lit != leg_of.end())
                {
                    legs[lit->second].requested += br.need * share;
                    legs[lit->second].granted   += br.granted * share;
                }
                const auto pit = parent_of.find(at);
                at = pit != parent_of.end() ? pit->second : null_entity;
            }
        }
    }

    // 4. CONNECTED, IDLE: a rail line between two connected stations the tree did not
    // take still joins them - it carries nothing, and it is not cut off.
    std::set<std::pair<entity_id, entity_id>> taken; // tile pairs, lower first
    for (const auto& [sb, lk] : link_of)
        if (lk.walker != nullptr && lk.walker->network == lp_network::rail)
            taken.insert({std::min(lk.from, lk.to), std::max(lk.from, lk.to)});
    for (std::size_t i = 0; i < list.size(); ++i)
    {
        const lp_source_ref& a = list[i];
        if (a.network != lp_network::rail || in.count(a.building) == 0)
            continue;
        const field_t& fa = fields.of(a);
        for (std::size_t j = i + 1; j < list.size(); ++j)
        {
            const lp_source_ref& b = list[j];
            if (b.network != lp_network::rail || in.count(b.building) == 0 || a.tile == b.tile)
                continue;
            const auto dit = fa.find(b.tile);
            if (dit == fa.end() || !(lp_reach(lp_network::rail, dit->second) > 0.0f))
                continue;
            const std::pair<entity_id, entity_id> key{std::min(a.tile, b.tile), std::max(a.tile, b.tile)};
            if (!taken.insert(key).second)
                continue;
            const logistics_path p = rail_leg_path(w, body, a.tile, b.tile);
            lp_leg leg;
            leg.network   = lp_network::rail;
            leg.tiles     = p.reachable && !p.tiles.empty() ? p.tiles : std::vector<entity_id>{a.tile, b.tile};
            leg.available = made_of(a.building) * lp_reach(lp_network::rail, dit->second);
            legs.push_back(std::move(leg));
        }
    }
    return legs;
}

} // namespace ui
