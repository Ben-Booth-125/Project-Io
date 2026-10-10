// wharf_census — how many shore tiles can stand a Fishing Wharf (BL-1218, wharf placed on shore).
//
// Ben's live click (2026-10-10): no Fishing Wharf offered on a coastal tile.
// is_wharf_site offers one only on coastal LAND with NO produce deposit (a
// produce deposit makes it a Farm). This counts, per body of a seeded world,
// coastal land tiles and how many of them carry produce — if nearly all do,
// the Wharf is offered almost nowhere, and that is the finding.
//
// Reports; asserts nothing. Run: build_gen\verify\wharf_census.exe

#include "world/components.hpp"
#include "world/hard_coded_world.hpp"
#include "world/placement_rules.hpp"
#include "world/world.hpp"
#include "harness_params.hpp"

#include <cstdio>
#include <map>

int main()
{
    const world w = make_hard_coded_world(no_prehistory());
    const auto food = resource_type::agricultural_produce;
    struct row { int land = 0, coast = 0, coast_food = 0, wharf = 0, placeable = 0; };
    std::map<entity_id, row> by_body;
    for (const auto& [id, tc] : w.tiles)
    {
        row& r = by_body[tc.body];
        if (is_water(tc.substrate)) continue;
        ++r.land;
        if (!placement_rules::is_coastal(w, id)) continue;
        ++r.coast;
        if (tc.resource_deposit[static_cast<std::size_t>(food)] > 0.0f) ++r.coast_food;
        if (placement_rules::is_wharf_site(w, id, food))
        {
            ++r.wharf;
            if (placement_rules::can_place_in_world(w, id, building_type::extraction_site, food).ok())
                ++r.placeable;
        }
    }
    std::printf("wharf_census (no_prehistory default seed; home body %llu)\n",
                static_cast<unsigned long long>(w.home_body));
    for (const auto& [b, r] : by_body)
        std::printf("  body %-6llu%s land %6d  coastal %5d  coastal+produce %5d (%5.1f%%)  wharf sites %5d  placeable %5d\n",
                    static_cast<unsigned long long>(b), b == w.home_body ? "*" : " ",
                    r.land, r.coast, r.coast_food,
                    r.coast ? 100.0 * r.coast_food / r.coast : 0.0, r.wharf, r.placeable);
    return 0;
}
