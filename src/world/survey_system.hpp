#pragma once

#include "entity.hpp"
#include "world.hpp"

/// Survey system (BL-067) — bodies start unsurveyed; a player-dispatched survey
/// debits credits upfront and then reveals the body region-by-region over sim
/// ticks (one tick = one day). Pure and deterministic: the region partition and
/// reveal order are functions of grid dimensions and a fixed super-cell size, with
/// no RNG, no Lua, and no per-tile data exposed to Lua (standing-rules compliant).
///
/// Design authority while the item is open: backlog.json BL-067. Surfaced on the
/// Solar canvas (survey badge), the Planetary canvas (region mask) and the
/// Selection panel (the Survey section).

// ---------------------------------------------------------------------------
// Region partition (deterministic, RNG-free)
// ---------------------------------------------------------------------------

/// Super-cell edge length, in tiles. The grid is partitioned into square regions
/// of this size; the last row/column of regions may be smaller. Region count
/// therefore scales with body size.
inline constexpr int survey_super_cell = 8;

/// Number of region columns / rows for a grid of the given dimensions.
inline int survey_region_cols(int grid_width)  { return (grid_width  + survey_super_cell - 1) / survey_super_cell; }
inline int survey_region_rows(int grid_height) { return (grid_height + survey_super_cell - 1) / survey_super_cell; }

/// Total survey regions for a grid — a pure function of its dimensions.
inline int survey_region_count(int grid_width, int grid_height)
{
    return survey_region_cols(grid_width) * survey_region_rows(grid_height);
}

/// Raster (row-major) region index of tile (col, row). This *is* the reveal index:
/// regions reveal in ascending index order, so region `k` is the `k`-th revealed.
inline int survey_region_of(int grid_width, int /*grid_height*/, int col, int row)
{
    const int rc = survey_region_cols(grid_width);
    return (row / survey_super_cell) * rc + (col / survey_super_cell);
}

/// Whether tile (col, row) is currently revealed under the given survey state.
/// A fully surveyed body shows every tile; otherwise a tile shows iff its region's
/// reveal-index is below `regions_done`.
inline bool survey_tile_visible(const survey_state& s, int grid_width, int grid_height, int col, int row)
{
    if (s.phase == survey_phase::surveyed) return true;
    if (s.phase == survey_phase::hidden || s.phase == survey_phase::in_transit) return false;
    return survey_region_of(grid_width, grid_height, col, row) < s.regions_done;
}

// ---------------------------------------------------------------------------
// Cost & schedule (size × distance) — pure functions
// ---------------------------------------------------------------------------

/// Survey schedule in whole days, split so distance reads as lead time and size
/// as scan time. `total_ticks == transit_ticks + scan_ticks`.
struct survey_schedule
{
    int transit_ticks = 0;  ///< Probe en route; nothing revealed yet.
    int scan_ticks    = 0;  ///< Regions reveal evenly across this phase.
    int total_ticks   = 0;  ///< transit + scan.
    int regions_total = 0;  ///< Region count for the body's grid.
};

/// Heliocentric orbital radius of a body, in AU. A moon (a body with a `parent`)
/// uses its parent's radius — static and legible, chosen over live Euclidean
/// distance which would drift as bodies orbit.
float survey_heliocentric_radius(const world& w, entity_id body);

/// Distance term for the survey of `body`: |r_body − r_home| in AU.
float survey_distance_au(const world& w, entity_id body);

/// Survey credit cost, debited upfront at dispatch. Scales with both distance and
/// body size (tile count).
float survey_cost(const world& w, entity_id body);

/// Compute the survey schedule for `body` from its size and distance.
survey_schedule survey_compute_schedule(const world& w, entity_id body);

/// Whole days remaining until `body` is fully surveyed. For a `hidden` body this is
/// the full dispatch ETA (transit + scan); for one already surveyed it is 0. Used by
/// the Selection panel's cost/ETA preview and the canvas reads.
int survey_eta_days(const world& w, entity_id body);

// ---------------------------------------------------------------------------
// Dispatch & advance
// ---------------------------------------------------------------------------

/// Outcome of a dispatch request.
enum class survey_dispatch_result
{
    success,            ///< Survey started; cost debited from the player corp.
    insufficient_funds, ///< Player corp balance is below the survey cost.
    already_surveyed,   ///< Body is already fully surveyed.
    in_progress,        ///< A survey is already in transit / scanning.
    invalid             ///< No such body, or no player corporation.
};

/// Seed survey states at campaign start: `home_body` (and the star, which is never
/// surveyed by the player) open `surveyed`; every other body keeps the default
/// `hidden`. Idempotent — safe to call once after world construction.
void init_survey_states(world& w);

/// Dispatch a survey of `body`: validate state, debit the cost upfront from the
/// paying corporation, and arm the schedule (`phase = in_transit`). The mutation
/// is centralised here and called from app (the UI surfaces hold a const world).
/// `payer` defaults to the player corporation (null_entity → w.player_entity);
/// the corp-command seam (BL-202) passes the acting AI corp so it pays for
/// discovery like the player does. The survey *store* stays the single per-body
/// world state either way (see corp_ai.hpp § fog design call).
survey_dispatch_result dispatch_survey(world& w, entity_id body,
                                       entity_id payer = null_entity);

/// Advance every in-progress survey by `days` whole days: cross phase boundaries
/// (in_transit → scanning → surveyed) and bump `regions_done` as scan-phase region
/// boundaries pass. Deterministic; `hidden` and `surveyed` bodies are untouched.
void advance_surveys(world& w, int days);
