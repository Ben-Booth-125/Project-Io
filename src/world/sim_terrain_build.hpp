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
// Era -1 battle in every run was fought on default grass-on-sedimentary/plains. The whole
// terrain_combat module — defence, attrition, BL-233's modifiers — was inert.
// ---------------------------------------------------------------------------

#include "world/components.hpp"
#include "world/history_sim.hpp"

#include <vector>

/// Raster-order (row * gw + col) terrain for @p body, sized gw*gh. Cells with
/// no tile fall back to grass-on-sedimentary / plains — which is the BL-519
/// spelling of the grassland/plains the sim assumed everywhere before this
/// existed, not a new default.
struct sim_terrain_arrays
{
    std::vector<terrain_substrate> substrate;
    std::vector<terrain_cover>     cover;
    std::vector<std::uint8_t>      density;
    std::vector<terrain_landform>  landform;

    sim_terrain_view view() const
    {
        return sim_terrain_view{ &substrate, &cover, &density, &landform };
    }
};

inline sim_terrain_arrays build_sim_terrain(const world& w, entity_id body, int gw, int gh)
{
    sim_terrain_arrays out;
    if (gw <= 0 || gh <= 0) return out;

    const std::size_t n = static_cast<std::size_t>(gw) * static_cast<std::size_t>(gh);
    out.substrate.assign(n, terrain_substrate::sedimentary);
    out.cover.assign(n, terrain_cover::grass);
    out.density.assign(n, 150u);
    out.landform.assign(n, terrain_landform::plains);

    for (const auto& [id, t] : w.tiles)
    {
        if (t.body != body) continue;
        if (t.grid_x < 0 || t.grid_x >= gw || t.grid_y < 0 || t.grid_y >= gh) continue;
        const std::size_t idx = static_cast<std::size_t>(t.grid_y) * static_cast<std::size_t>(gw)
                              + static_cast<std::size_t>(t.grid_x);
        out.substrate[idx] = t.substrate;
        out.cover[idx]     = t.cover;
        out.density[idx]   = t.cover_density;
        out.landform[idx]  = t.landform;
    }
    return out;
}
