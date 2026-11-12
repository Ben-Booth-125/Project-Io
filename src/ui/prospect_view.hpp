#pragma once

#include "world/survey_system.hpp"

#include <algorithm>
#include "world/world.hpp"

// ---------------------------------------------------------------------------
// The player's DEPOSIT knowledge of one body (BL-1610) — docs/ui/DISCOVERY.md
// § Mapping, then prospecting; docs/ui/PLANETARY.md § Survey mask.
// ---------------------------------------------------------------------------
// Off the home world a region is first MAPPED (its terrain drawn) and only then
// PROSPECTED (its deposit bands drawn). The canvases already draw the mapped
// half from the per-body mirror (`body_component::survey`, survey_tile_visible).
// This answers the second half: may a surface draw this tile's DEPOSITS for the
// player? Every surface that draws a deposit — the Resource lens and its key,
// the hover card, the tile summary, the province's deposit table — asks it, so
// none of them can show a band the player's nation has not prospected.
//
// Readable when ANY of:
//   - the body is the home world or the star (`survey_knowledge_applies` false),
//     or its mirror reads `surveyed` (every region mapped and prospected, or a
//     dispatched probe finished it), or the spectator god view is up;
//   - the player nation's own record prospected the tile's region
//     (`survey_tile_prospected`, read from the record directly so a per-tile
//     loop is one array read, not two map lookups);
//   - a DISPATCHED PROBE is scanning the body (`scanning` with a real tick
//     schedule, not `k_survey_ticks_orbit_mapped`) and has revealed the tile: a
//     probe's survey has always revealed terrain and deposits together, and
//     DISCOVERY.md leaves a body a probe is surveying to its probe.
// Pure reads of the live world; nothing kept between frames.
// ---------------------------------------------------------------------------

namespace ui {

struct prospect_view
{
    entity_id                    body  = null_entity;
    bool                         all   = true;    ///< Every tile's deposits are readable.
    bool                         probe = false;   ///< A dispatched probe's scan reveals deposits with terrain.
    const body_survey_knowledge* k     = nullptr; ///< The player nation's record, or null.
    const survey_state*          s     = nullptr; ///< The body's mirror.
    int                          gw = 1, gh = 1;

    /// May tile (col, row) of `body` draw its deposits for the player?
    bool readable(int col, int row) const
    {
        if (all)
            return true;
        if (col < 0 || row < 0 || col >= gw || row >= gh)
            return false;
        if (k != nullptr)
        {
            const int r = survey_region_of(gw, gh, col, row);
            if (r >= 0 && r < k->regions_mapped && static_cast<std::size_t>(r) < k->prospected.size()
                && k->prospected[static_cast<std::size_t>(r)] != 0)
                return true;
        }
        return probe && s != nullptr && survey_tile_visible(*s, gw, gh, col, row);
    }
};

/// The player's deposit knowledge of @p body. @p god_view lifts it (the spectator
/// god view, BL-408), as it lifts the survey mask: at the draw call only.
inline prospect_view prospect_view_of(const world& w, entity_id body, bool god_view = false)
{
    prospect_view v;
    v.body = body;
    const auto bit = w.bodies.find(body);
    if (god_view || bit == w.bodies.end() || !survey_knowledge_applies(w, body)
        || bit->second.survey.phase == survey_phase::surveyed)
        return v; // all
    v.all   = false;
    v.gw    = std::max(1, bit->second.grid_width);
    v.gh    = std::max(1, bit->second.grid_height);
    v.s     = &bit->second.survey;
    v.k     = survey_knowledge_of(w, player_nation(w), body);
    v.probe = bit->second.survey.phase == survey_phase::scanning
           && bit->second.survey.ticks_remaining != k_survey_ticks_orbit_mapped;
    return v;
}

/// One tile, uncached: for a surface that asks about a handful of tiles (a card,
/// a province's members). A per-tile loop builds one `prospect_view` instead.
inline bool tile_deposits_readable(const world& w, const tile_component& t, bool god_view = false)
{
    return prospect_view_of(w, t.body, god_view).readable(t.grid_x, t.grid_y);
}

} // namespace ui
