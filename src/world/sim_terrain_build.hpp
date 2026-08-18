#pragma once

// ---------------------------------------------------------------------------
// Build a `sim_terrain_view`'s backing arrays from a body's tiles (BL-316 S1,
// Era -1 logistics — BL-314 is the unit verb family; label fixed 2026-08-07).
//
// This lives OUTSIDE history_sim on purpose. That header states the sim's whole
// decision path is region-level and binding it to the ECS would buy nothing
// and cost headless testability — so the ECS-to-view adapter sits here, where
// callers that already hold a `world` can use it, and the sim keeps taking two
// plain const pointers.
//
// Why it matters: before this, every caller passed an empty view, so every
// Era -1 battle in every run was fought on default grassland/plains. The whole
// terrain_combat module — defence, attrition, BL-233's modifiers — was inert.
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/history_sim.hpp"

#include <vector>

/// Raster-order (row * gw + col) terrain for @p body, sized gw*gh. Cells with
/// no tile fall back to grassland/plains, which is what the sim assumed
/// everywhere before this existed.
struct sim_terrain_arrays
{
    std::vector<terrain_composition> composition;
    std::vector<terrain_landform>    landform;

    sim_terrain_view view() const { return sim_terrain_view{&composition, &landform}; }
};

inline sim_terrain_arrays build_sim_terrain(const world& w, entity_id body, int gw, int gh)
{
    sim_terrain_arrays out;
    if (gw <= 0 || gh <= 0) return out;

    out.composition.assign(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh),
                           terrain_composition::grassland);
    out.landform.assign(static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh),
                        terrain_landform::plains);

    for (const auto& [id, t] : w.tiles)
    {
        if (t.body != body) continue;
        if (t.grid_x < 0 || t.grid_x >= gw || t.grid_y < 0 || t.grid_y >= gh) continue;
        const std::size_t idx = static_cast<std::size_t>(t.grid_y) * static_cast<std::size_t>(gw)
                              + static_cast<std::size_t>(t.grid_x);
        out.composition[idx] = t.composition;
        out.landform[idx]    = t.landform;
    }
    return out;
}
