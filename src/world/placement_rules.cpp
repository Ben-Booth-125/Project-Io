#include "world/placement_rules.hpp"

namespace placement_rules {

bool is_ocean_tile(terrain_composition comp)
{
    return comp == terrain_composition::ocean;
}

bool is_extractable(resource_type r)
{
    for (resource_type e : k_extractable)
        if (e == r)
            return true;
    return false;
}

float extractable_deposit(const tile_component& tc)
{
    float sum = 0.0f;
    for (resource_type r : k_extractable)
        sum += tc.resource_deposit[static_cast<std::size_t>(r)];
    return sum;
}

resource_type richest_extractable(const tile_component& tc, bool& any)
{
    resource_type best = resource_type::iron_ore;
    float best_val = 0.0f;
    any = false;
    for (resource_type r : k_extractable)
    {
        const float v = tc.resource_deposit[static_cast<std::size_t>(r)];
        if (v > best_val)
        {
            best_val = v;
            best     = r;
            any      = true;
        }
    }
    return best;
}

bool can_place(const tile_component& tc, building_type type, resource_type target)
{
    // No building ever sits on water.
    if (is_ocean_tile(tc.composition))
        return false;

    switch (type)
    {
        case building_type::extraction_site:
            // Extraction must sit on a non-zero deposit of a prototype-extractable
            // target resource.
            return is_extractable(target)
                && tc.resource_deposit[static_cast<std::size_t>(target)] > 0.0f;

        case building_type::processing_facility:
        case building_type::port:
        case building_type::none:
        default:
            // Any non-ocean land tile is valid for non-extraction buildings.
            return true;
    }
}

} // namespace placement_rules
