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
/// discovery like the player does. BL-1402: the payer may be a NATION, paying
/// from its uncommitted treasury (`purse_room_of`), never below zero. The survey *store* stays the single per-body
/// world state either way (see corp_ai.hpp § fog design call).
survey_dispatch_result dispatch_survey(world& w, entity_id body,
                                       entity_id payer = null_entity);

/// Advance every in-progress survey by `days` whole days: cross phase boundaries
/// (in_transit → scanning → surveyed) and bump `regions_done` as scan-phase region
/// boundaries pass. Deterministic; `hidden` and `surveyed` bodies are untouched.
void advance_surveys(world& w, int days);

// ---------------------------------------------------------------------------
// Mapping, then prospecting (BL-1600) — DISCOVERY.md § Mapping, then prospecting
// ---------------------------------------------------------------------------
// Off the home world a survey reveals in two steps, PER NATION: a region is first
// MAPPED (its tiles and terrain readable to that nation) from orbit — by a Survey
// satellite at one of the body's nodes or a working station Observatory with the
// body in its owner's comms coverage (space_effects.hpp) — and only then
// PROSPECTED (its deposit bands readable to that nation) on the ground, by an
// ACTIVE rover or a LANDED Lander at the body's surface.
//
// THE STORE is `world::survey_knowledge`, keyed (nation, body): the mapped
// regions are the raster prefix [0, regions_mapped), the prospected regions a
// byte per region, set only inside the mapped prefix. The home world and the star
// are never recorded: the home world is surveyed for every nation, the star has
// no ground.
//
// PROSPECTING ORDER. One region at a time, nearest the landing first. No landing
// TILE is modelled (a Lander lands at a body's surface NODE, space_units.hpp), so
// the landing stands at region 0 — the first region mapping reaches — and the
// nearest unprospected mapped region is the one with the least squared distance
// in region cells from it, ties to the lower raster index. Deterministic.
//
// THE PLAYER'S MIRROR. The canvases draw the player's geographic fog from the one
// per-body store, `body_component::survey`. So mapping or prospecting for the
// PLAYER'S nation also writes that store, where no dispatched probe owns it (the
// body is `hidden`, or already being mapped from orbit): `scanning` with
// `regions_done` = the mapped count and `ticks_remaining` =
// `k_survey_ticks_orbit_mapped` (no probe schedule: `advance_surveys` leaves it),
// and `surveyed` once every region is both mapped and prospected. Another
// nation's mapping never writes it.
// ---------------------------------------------------------------------------

/// `survey_state::ticks_remaining` of a body being mapped from orbit for the
/// player: no probe schedule runs, so `advance_surveys` does not step it.
inline constexpr int k_survey_ticks_orbit_mapped = -1;

/// Is @p body one survey knowledge may be recorded for? A non-star body with a
/// grid, other than the home world.
bool survey_knowledge_applies(const world& w, entity_id body);

/// @p nation's record for @p body, or null (nothing mapped yet, or the home world).
const body_survey_knowledge* survey_knowledge_of(const world& w, entity_id nation, entity_id body);

/// Has @p nation mapped region @p region of @p body? The home world: always.
bool survey_region_mapped(const world& w, entity_id nation, entity_id body, int region);

/// Has @p nation prospected region @p region of @p body? The home world: always.
bool survey_region_prospected(const world& w, entity_id nation, entity_id body, int region);

/// Is tile (col, row) of @p body mapped for @p nation — its terrain readable?
bool survey_tile_mapped(const world& w, entity_id nation, entity_id body, int col, int row);

/// Is tile (col, row) of @p body prospected for @p nation — its deposit band
/// readable? Only ever true on mapped ground.
bool survey_tile_prospected(const world& w, entity_id nation, entity_id body, int col, int row);

/// Is @p body surveyed for @p nation — every region mapped and prospected?
bool survey_body_surveyed_for(const world& w, entity_id nation, entity_id body);

/// Map up to @p k more regions of @p body for @p nation, in raster order. Returns
/// the regions mapped (0 on the home world, the star, a body with no grid, or one
/// already fully mapped by @p nation).
int survey_map_regions(world& w, entity_id nation, entity_id body, int k);

/// Why a prospecting step did or did not prospect.
enum class survey_prospect_result
{
    prospected,     ///< One region prospected.
    not_mapped,     ///< Refused: no mapped region left unprospected (nothing mapped, or all done).
    complete,       ///< Every region already mapped and prospected.
    invalid         ///< The home world, the star, a body with no grid, or no such nation.
};

/// Prospect ONE region of @p body for @p nation: the unprospected mapped region
/// nearest the landing (see above). Refused (`not_mapped`) when no mapped region
/// is left to prospect — prospecting never runs ahead of mapping.
survey_prospect_result survey_prospect_region(world& w, entity_id nation, entity_id body,
                                              int* region_out = nullptr);
