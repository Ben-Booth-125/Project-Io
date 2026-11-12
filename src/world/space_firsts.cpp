#include "space_firsts.hpp"

#include "nation_tech.hpp" // research_params, k_milli_per_rp
#include "world.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

// BL-1606 (ERAS.md § Firsts). See the header.

namespace {

bool in_place(space_asset_state s)
{
    return s == space_asset_state::active || s == space_asset_state::decaying;
}

bool is_moon(const world& w, entity_id body)
{
    const auto it = w.bodies.find(body);
    return it != w.bodies.end() && it->second.type == body_type::moon;
}

/// Keep the lower of @p best and @p n (a nation in the store), null meaning none.
void offer(const world& w, entity_id& best, entity_id n)
{
    if (n == null_entity || w.nations.count(n) == 0)
        return;
    if (best == null_entity || n < best)
        best = n;
}

std::string nation_name(const world& w, entity_id n)
{
    const auto it = w.nations.find(n);
    return it != w.nations.end() && !it->second.name.empty() ? it->second.name
                                                             : "Nation " + std::to_string(n);
}

} // namespace

const char* space_first_label(space_first_kind k)
{
    switch (k)
    {
    case space_first_kind::satellite:     return "the first satellite in orbit";
    case space_first_kind::crew_in_orbit: return "the first crew in orbit";
    case space_first_kind::station_core:  return "the first station core";
    case space_first_kind::crew_on_moon:  return "the first crew landed on a moon";
    case space_first_kind::offworld_base: return "the first off-world base";
    case space_first_kind::space_convoy:  return "the first space convoy between two bodies";
    }
    return "a first in space";
}

entity_id space_first_candidate(const world& w, space_first_kind k)
{
    entity_id best = null_entity;
    switch (k)
    {
    case space_first_kind::satellite:
        for (const auto& [id, a] : w.space_assets)
            if (a.cls == space_asset_class::satellite && in_place(a.state))
                offer(w, best, a.owner);
        break;
    case space_first_kind::crew_in_orbit:
        for (const auto& [id, a] : w.space_assets)
        {
            if (!in_place(a.state) || a.node.kind == orbital_node_kind::surface)
                continue;
            if (a.cls == space_asset_class::crewed_mission || a.crew > 0)
                offer(w, best, a.owner);
        }
        for (const auto& [id, u] : w.space_units)
            if (u.crew > 0 && u.state != space_unit_state::in_transit
                && u.node.kind != orbital_node_kind::surface)
                offer(w, best, u.owner);
        break;
    case space_first_kind::station_core:
        for (const auto& [id, a] : w.space_assets)
            if (a.cls == space_asset_class::station_core && in_place(a.state))
                offer(w, best, a.owner);
        break;
    case space_first_kind::crew_on_moon:
        for (const auto& [id, a] : w.space_assets)
            if (a.cls == space_asset_class::crewed_mission && in_place(a.state)
                && a.node.kind == orbital_node_kind::surface && is_moon(w, a.node.body))
                offer(w, best, a.owner);
        for (const auto& [id, u] : w.space_units)
            if (u.crew > 0 && u.state == space_unit_state::landed
                && u.node.kind == orbital_node_kind::surface && is_moon(w, u.node.body))
                offer(w, best, u.owner);
        break;
    case space_first_kind::offworld_base:
        for (const auto& [cid, b] : w.offworld_bases)
        {
            const entity_id tile = offworld_base_tile(w, cid);
            const auto      tn   = w.tile_to_nation.find(tile);
            if (tn != w.tile_to_nation.end())
                offer(w, best, tn->second);
        }
        break;
    case space_first_kind::space_convoy:
        // A RETURNING trade haul has unloaded its convoy at the destination market
        // (space_convoy.cpp's unload; a held haul has no destination). Its home is
        // the source's low orbit, so the two bodies are the home's and the market's.
        for (const auto& [id, h] : w.space_hauls)
        {
            if (h.phase != space_haul_phase::returning || h.dest_market == null_entity)
                continue;
            const entity_id dest = market_body(w, h.dest_market);
            if (dest != null_entity && h.home.body != null_entity && dest != h.home.body)
                offer(w, best, h.owner);
        }
        break;
    }
    return best;
}

void run_space_firsts(world& w, const research_params& p, space_firsts_report* out)
{
    space_firsts_report r;
    r.by.fill(null_entity);
    if (w.space_firsts.size() < space_first_kind_count)
    {
        const float rp = std::isfinite(p.space_first_grant_rp) ? std::max(0.0f, p.space_first_grant_rp) : 0.0f;
        const std::int64_t grant =
            static_cast<std::int64_t>(std::llround(static_cast<double>(rp) * static_cast<double>(k_milli_per_rp)));
        for (std::size_t k = 0; k < space_first_kind_count; ++k) // kind order
        {
            const auto kind = static_cast<space_first_kind>(k);
            if (w.space_firsts.count(static_cast<uint8_t>(k)) != 0)
                continue; // never taken twice
            const entity_id n = space_first_candidate(w, kind);
            if (n == null_entity)
                continue;
            space_first_record rec;
            rec.nation   = n;
            rec.day_tick = std::max(0, w.current_day_tick);
            w.space_firsts.emplace(static_cast<uint8_t>(k), rec);

            if (grant > 0)
            {
                std::int64_t& pool = w.nation_tech[n].pool_milli;
                pool = pool > std::numeric_limits<std::int64_t>::max() - grant
                           ? std::numeric_limits<std::int64_t>::max() : pool + grant;
            }

            world_history_entry e;
            e.timestamp   = rec.day_tick;
            e.topic       = history_topic::space_first;
            e.corp        = n;
            e.event       = nation_name(w, n) + " achieves " + space_first_label(kind);
            e.consequence = "A research grant of " + std::to_string(std::llround(rp))
                          + " RP; no other nation can claim it.";
            w.history_log.push_back(std::move(e));

            ++r.taken;
            r.grant_milli += grant;
            r.by[k] = n;
        }
    }
    if (out != nullptr)
        *out = r;
}
