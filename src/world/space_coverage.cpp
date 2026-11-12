#include "space_coverage.hpp"

#include "orbital_graph.hpp"
#include "solar_storm.hpp" // BL-1605: a struck Comms satellite is dark
#include "space_assets.hpp"
#include "stations.hpp"
#include "world.hpp"

#include <algorithm>

// BL-1587: the whole rule (space_coverage.hpp; SPACE_ASSETS.md § Signal delay).
//
//   * the home body's low and high orbit, always;
//   * every node of the body an ACTIVE Comms satellite of the nation serves —
//     the node's body, which for an L-point is the pair's smaller body (the key
//     orbital_node.hpp gives it). "Every node" is the body's seven: its surface,
//     low and high orbit, and the four L-points keyed on it;
//   * a WORKING Comms relay module's station node, and every node one
//     `orbital_links` hop from it.
//
// "Working" is `evaluate_station_budget`'s, read with the default
// `station_params` (the registry carries those defaults; nothing authors them
// yet) at `w.current_econ_tick`. Life-support starvation (space_effects.cpp) is
// a step-local consequence and does not reach here — a relay needs no crew, so
// it would not stop one anyway.
//
// BL-1605: a solar storm at `w.current_econ_tick` darkens a struck Comms
// satellite and stops a struck station's relay (solar_storm.hpp), so neither
// covers anything for that quarter.
//
// DERIVED, NEVER STORED; deterministic: assets and stations are std::maps
// walked ascending, the links come back ordered, the result is a std::set.

namespace {

bool is_star(const world& w, entity_id body)
{
    const auto it = w.bodies.find(body);
    return it == w.bodies.end() || it->second.type == body_type::star;
}

void add_body_nodes(const world& w, entity_id body, std::set<orbital_node>& out)
{
    if (body == null_entity || is_star(w, body))
        return;
    for (int k = 0; k < orbital_node_kind_count; ++k)
    {
        const orbital_node n{body, static_cast<orbital_node_kind>(k)};
        if (orbital_node_valid(w, n))
            out.insert(n);
    }
}

bool station_relay_working(const world& w, const station_params& stp, entity_id sid)
{
    const station_budget b = evaluate_station_budget(w, stp, sid, w.current_econ_tick);
    for (const entity_id aid : b.working)
    {
        const auto it = w.space_assets.find(aid);
        if (it != w.space_assets.end()
            && it->second.module == static_cast<uint8_t>(station_module_kind::comms_relay))
            return true;
    }
    return false;
}

} // namespace

std::set<orbital_node> space_coverage(const world& w, entity_id nation)
{
    std::set<orbital_node> out;
    if (w.nations.find(nation) == w.nations.end())
        return out;
    const entity_id home = space_home_body_of(w, nation);
    if (home != null_entity && !is_star(w, home))
    {
        out.insert(orbital_node{home, orbital_node_kind::low_orbit});
        out.insert(orbital_node{home, orbital_node_kind::high_orbit});
    }

    // Comms satellites: every node of the body each serves.
    for (const auto& [id, a] : w.space_assets)
    {
        (void)id;
        if (a.owner != nation || a.cls != space_asset_class::satellite
            || a.role != space_satellite_role::comms || a.state != space_asset_state::active
            || solar_storm_satellite_dark(w, a, w.current_econ_tick))
            continue;
        add_body_nodes(w, a.node.body, out);
    }

    // Comms relays: the station's node and its one-hop neighbours.
    const station_params stp{};
    std::vector<orbital_node> relays;
    for (const auto& [sid, st] : w.stations)
        if (st.owner == nation && orbital_node_valid(w, st.node) && station_relay_working(w, stp, sid))
            relays.push_back(st.node);
    if (!relays.empty())
    {
        std::sort(relays.begin(), relays.end());
        relays.erase(std::unique(relays.begin(), relays.end()), relays.end());
        for (const orbital_node& r : relays)
            out.insert(r);
        for (const orbital_link& l : orbital_links(w))
        {
            if (std::binary_search(relays.begin(), relays.end(), l.a))
                out.insert(l.b);
            if (std::binary_search(relays.begin(), relays.end(), l.b))
                out.insert(l.a);
        }
    }
    return out;
}

bool space_node_covered(const world& w, entity_id nation, orbital_node node)
{
    const auto cov = space_coverage(w, nation);
    return cov.count(node) != 0;
}
