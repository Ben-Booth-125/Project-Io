#include "orbital_graph.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <utility>

// See orbital_graph.hpp for the contract. Every walk over bodies goes through
// sorted_body_ids, never over the unordered map directly.

namespace {

constexpr double k_pi = 3.14159265358979323846;

constexpr orbital_node_kind k_lagrange_kinds[] = {
    orbital_node_kind::l1, orbital_node_kind::l2,
    orbital_node_kind::l4, orbital_node_kind::l5,
};

const body_component* find_body(const world& w, entity_id id)
{
    if (id == null_entity) return nullptr;
    const auto it = w.bodies.find(id);
    return it == w.bodies.end() ? nullptr : &it->second;
}

bool is_node_body(const body_component* b)
{
    return b != nullptr && b->type != body_type::star;
}

/// Non-star bodies, ascending id.
std::vector<entity_id> sorted_body_ids(const world& w)
{
    std::vector<entity_id> ids;
    ids.reserve(w.bodies.size());
    for (const auto& [id, b] : w.bodies)
        if (b.type != body_type::star) ids.push_back(id);
    std::sort(ids.begin(), ids.end());
    return ids;
}

/// The LARGER member of the pair whose smaller member is @p b: its parent body
/// when that body exists and is not the star, else null_entity (the star).
entity_id pair_parent(const world& w, const body_component& b)
{
    const body_component* p = find_body(w, b.parent);
    return is_node_body(p) ? b.parent : null_entity;
}

/// The canvases' composition rule: parent's position plus own orbit; a body
/// without a (present) parent orbits the origin. Depth-capped against a
/// malformed parent cycle.
orbital_position body_position(const world& w, entity_id id, int depth = 0)
{
    const body_component* b = find_body(w, id);
    if (b == nullptr || depth > 16) return {};
    const orbital_position anchor = (find_body(w, b->parent) != nullptr)
                                  ? body_position(w, b->parent, depth + 1)
                                  : orbital_position{};
    const double a = static_cast<double>(b->orbital_angle_rad);
    const double r = static_cast<double>(b->orbital_radius_au);
    return { anchor.x_au + std::cos(a) * r, anchor.y_au + std::sin(a) * r };
}

orbital_position anchor_position(const world& w, const body_component& b)
{
    return find_body(w, b.parent) != nullptr ? body_position(w, b.parent) : orbital_position{};
}

double atmosphere_dv(const orbital_graph_params& p, atmosphere_class a)
{
    switch (a)
    {
    case atmosphere_class::none:     return p.atmosphere_none_dv;
    case atmosphere_class::thin:     return p.atmosphere_thin_dv;
    case atmosphere_class::moderate: return p.atmosphere_moderate_dv;
    case atmosphere_class::thick:    return p.atmosphere_thick_dv;
    }
    return 0.0;
}

double climb_of(const orbital_graph_params& p, const body_component& b)
{
    const double mass = b.mass_earths > 0.0f ? static_cast<double>(b.mass_earths) : 0.0;
    return p.climb_mass_coeff * std::pow(mass, p.climb_mass_exponent) + atmosphere_dv(p, b.atmosphere);
}

void add_link(std::vector<orbital_link>& out, orbital_node x, orbital_node y,
              orbital_link_kind kind, double dv, double days_per_dv)
{
    orbital_link l;
    l.a    = x < y ? x : y;
    l.b    = x < y ? y : x;
    l.kind = kind;
    l.dv   = dv;
    l.days = dv * days_per_dv;
    out.push_back(l);
}

} // namespace

bool orbital_node_valid(const world& w, orbital_node n)
{
    return is_node_body(find_body(w, n.body))
        && static_cast<int>(n.kind) < orbital_node_kind_count;
}

std::vector<orbital_node> orbital_nodes(const world& w)
{
    std::vector<orbital_node> out;
    for (const entity_id id : sorted_body_ids(w))
        for (int k = 0; k < orbital_node_kind_count; ++k)
            out.push_back({ id, static_cast<orbital_node_kind>(k) });
    return out; // already ascending (body, kind)
}

double orbital_climb_dv(const world& w, const orbital_graph_params& p, entity_id body)
{
    const body_component* b = find_body(w, body);
    return is_node_body(b) ? climb_of(p, *b) : 0.0;
}

std::vector<orbital_link> orbital_links(const world& w, const orbital_graph_params& p)
{
    using K = orbital_node_kind;
    std::vector<orbital_link> out;
    const std::vector<entity_id> ids = sorted_body_ids(w);

    std::vector<entity_id> star_orbiting; // smaller members of star pairs, ascending
    for (const entity_id id : ids)
    {
        const body_component& b = w.bodies.at(id);
        const double climb = climb_of(p, b);

        // The body's own well.
        add_link(out, { id, K::surface }, { id, K::low_orbit }, orbital_link_kind::climb,
                 climb, p.climb_days_per_dv);
        add_link(out, { id, K::low_orbit }, { id, K::high_orbit }, orbital_link_kind::low_high,
                 p.low_high_fraction * climb, p.low_high_days_per_dv);

        // Its own pair: its high orbit reaches L1 and L2.
        const double own_hl = p.high_lagrange_fraction * climb;
        add_link(out, { id, K::high_orbit }, { id, K::l1 }, orbital_link_kind::high_lagrange,
                 own_hl, p.high_lagrange_days_per_dv);
        add_link(out, { id, K::high_orbit }, { id, K::l2 }, orbital_link_kind::high_lagrange,
                 own_hl, p.high_lagrange_days_per_dv);

        // The larger member's high orbit reaches all four, when it carries nodes.
        const entity_id parent = pair_parent(w, b);
        double pair_well = climb; // climb of the pair's largest node-bearing member
        if (parent != null_entity)
        {
            const double parent_climb = climb_of(p, w.bodies.at(parent));
            pair_well = parent_climb;
            for (const K lk : k_lagrange_kinds)
                add_link(out, { parent, K::high_orbit }, { id, lk }, orbital_link_kind::high_lagrange,
                         p.high_lagrange_fraction * parent_climb, p.high_lagrange_days_per_dv);
        }
        else
        {
            star_orbiting.push_back(id);
        }

        // L-point ↔ L-point within the pair: every pair of the four.
        for (int i = 0; i < 4; ++i)
            for (int j = i + 1; j < 4; ++j)
                add_link(out, { id, k_lagrange_kinds[i] }, { id, k_lagrange_kinds[j] },
                         orbital_link_kind::lagrange_hop, p.lagrange_hop_fraction * pair_well,
                         p.lagrange_hop_days_per_dv);
    }

    // Interplanetary: star-pair L1/L2 ↔ star-pair L1/L2 of every other star-orbiting body.
    for (std::size_t i = 0; i < star_orbiting.size(); ++i)
        for (std::size_t j = i + 1; j < star_orbiting.size(); ++j)
        {
            const entity_id a = star_orbiting[i], c = star_orbiting[j];
            const double dr = std::fabs(static_cast<double>(w.bodies.at(a).orbital_radius_au) -
                                        static_cast<double>(w.bodies.at(c).orbital_radius_au));
            const double dv = p.interplanetary_base_dv + p.interplanetary_dv_per_au * dr;
            for (const K ka : { K::l1, K::l2 })
                for (const K kc : { K::l1, K::l2 })
                    add_link(out, { a, ka }, { c, kc }, orbital_link_kind::interplanetary, dv,
                             p.interplanetary_days_per_dv);
        }

    std::sort(out.begin(), out.end(), [](const orbital_link& x, const orbital_link& y) {
        return x.a != y.a ? x.a < y.a : x.b < y.b;
    });
    return out;
}

orbital_position orbital_node_position(const world& w, orbital_node n, const orbital_graph_params& p)
{
    if (!orbital_node_valid(w, n)) return {};
    const body_component& b = w.bodies.at(n.body);
    const orbital_position self = body_position(w, n.body);
    const orbital_position anchor = anchor_position(w, b);

    switch (n.kind)
    {
    case orbital_node_kind::surface:
    case orbital_node_kind::low_orbit:
    case orbital_node_kind::high_orbit:
        return self;
    case orbital_node_kind::l1:
    case orbital_node_kind::l2:
    {
        const double f = n.kind == orbital_node_kind::l1 ? -p.lagrange_l12_offset_fraction
                                                         :  p.lagrange_l12_offset_fraction;
        return { self.x_au + f * (self.x_au - anchor.x_au), self.y_au + f * (self.y_au - anchor.y_au) };
    }
    case orbital_node_kind::l4:
    case orbital_node_kind::l5:
    {
        // L4 leads in the direction of motion; a stationary body counts as prograde.
        const double ahead = b.orbital_angular_velocity_rad_per_day < 0.0f ? -1.0 : 1.0;
        const double side  = n.kind == orbital_node_kind::l4 ? ahead : -ahead;
        const double a = static_cast<double>(b.orbital_angle_rad) + side * (k_pi / 3.0);
        const double r = static_cast<double>(b.orbital_radius_au);
        return { anchor.x_au + std::cos(a) * r, anchor.y_au + std::sin(a) * r };
    }
    }
    return {};
}

orbital_path_result orbital_path(const world& w, const orbital_graph_params& p,
                                 orbital_node from, orbital_node to)
{
    orbital_path_result res;
    if (!orbital_node_valid(w, from) || !orbital_node_valid(w, to)) return res;
    if (from == to)
    {
        res.ok = true;
        res.nodes.push_back(from);
        return res;
    }

    const std::vector<orbital_node> nodes = orbital_nodes(w);
    const std::vector<orbital_link> links = orbital_links(w, p);
    auto index_of = [&](orbital_node n) -> int {
        const auto it = std::lower_bound(nodes.begin(), nodes.end(), n);
        return (it != nodes.end() && *it == n) ? static_cast<int>(it - nodes.begin()) : -1;
    };

    struct edge { int to; double dv; double days; };
    std::vector<std::vector<edge>> adj(nodes.size());
    for (const orbital_link& l : links)
    {
        const int ia = index_of(l.a), ib = index_of(l.b);
        if (ia < 0 || ib < 0) continue;
        adj[static_cast<std::size_t>(ia)].push_back({ ib, l.dv, l.days });
        adj[static_cast<std::size_t>(ib)].push_back({ ia, l.dv, l.days });
    }
    for (auto& e : adj)
        std::sort(e.begin(), e.end(), [](const edge& x, const edge& y) { return x.to < y.to; });

    const int src = index_of(from), dst = index_of(to);
    const double inf = std::numeric_limits<double>::infinity();
    std::vector<double> best(nodes.size(), inf), days(nodes.size(), 0.0);
    std::vector<int> prev(nodes.size(), -1);
    std::vector<char> done(nodes.size(), 0);

    using item = std::pair<double, int>; // (Δv, node index): ties pop the lower index
    std::priority_queue<item, std::vector<item>, std::greater<item>> open;
    best[static_cast<std::size_t>(src)] = 0.0;
    open.push({ 0.0, src });
    while (!open.empty())
    {
        const auto [d, u] = open.top();
        open.pop();
        const std::size_t us = static_cast<std::size_t>(u);
        if (done[us]) continue;
        done[us] = 1;
        if (u == dst) break;
        for (const edge& e : adj[us])
        {
            const std::size_t vs = static_cast<std::size_t>(e.to);
            if (done[vs]) continue;
            const double nd = d + e.dv;
            if (nd < best[vs]) // strict: the first-found predecessor holds a tie
            {
                best[vs] = nd;
                days[vs] = days[us] + e.days;
                prev[vs] = u;
                open.push({ nd, e.to });
            }
        }
    }

    const std::size_t ds = static_cast<std::size_t>(dst);
    if (best[ds] == inf) return res;
    for (int at = dst; at != -1; at = prev[static_cast<std::size_t>(at)])
        res.nodes.push_back(nodes[static_cast<std::size_t>(at)]);
    std::reverse(res.nodes.begin(), res.nodes.end());
    res.ok   = true;
    res.dv   = best[ds];
    res.days = days[ds];
    return res;
}

int orbital_slot_capacity(const world& w, orbital_node n)
{
    if (!orbital_node_valid(w, n)) return 0;
    const body_component& b = w.bodies.at(n.body);
    switch (n.kind)
    {
    case orbital_node_kind::surface:    return 0;
    case orbital_node_kind::low_orbit:
    {
        const double mass = b.mass_earths > 0.0f ? static_cast<double>(b.mass_earths) : 0.0;
        return 8 + static_cast<int>(std::floor(4.0 * mass));
    }
    case orbital_node_kind::high_orbit: return b.type == body_type::planet ? 4 : 2;
    case orbital_node_kind::l4:
    case orbital_node_kind::l5:         return 3;
    case orbital_node_kind::l1:
    case orbital_node_kind::l2:         return 2;
    }
    return 0;
}
