#include "space_coverage.hpp"

#include "space_assets.hpp"
#include "world.hpp"

// First implementation (BL-1587 fills the rest): the home body's low and high
// orbit only. See space_coverage.hpp for the rule.

std::set<orbital_node> space_coverage(const world& w, entity_id nation)
{
    std::set<orbital_node> out;
    const entity_id home = space_home_body_of(w, nation);
    if (home == null_entity)
        return out;
    out.insert(orbital_node{home, orbital_node_kind::low_orbit});
    out.insert(orbital_node{home, orbital_node_kind::high_orbit});
    return out;
}

bool space_node_covered(const world& w, entity_id nation, orbital_node node)
{
    const auto cov = space_coverage(w, nation);
    return cov.count(node) != 0;
}
