#pragma once

#include "world.hpp"

struct settlement_state; // settlement.hpp — the Era -1 record the campaign path reads

/// Land tiles per population centre — the divisor the FALLBACK placement pass
/// derives its target count from (BL-463: land area, not grid area).
///
/// RETIRED ON THE CAMPAIGN PATH (BL-610, centres from demography, 2026-08-25):
/// a body with an Era -1 settlement record derives its centre count and scale
/// distribution from the simulated region populations instead — see
/// `k_demography_heads_per_centre` below. This divisor survives only for the
/// fallback a body with NO settlement record takes, so a harness or a future
/// body without a history sim still gets a plausible (if history-blind) spread.
///
/// MEASURED, not chosen (the BL-463 derivation; see the note at the fallback
/// site in population_generation.cpp for how the figure was arrived at).
inline constexpr int k_land_tiles_per_centre = 410;

/// URBAN heads per population centre on the CAMPAIGN path (BL-610, centres
/// from demography). A living region's urban headcount (its simulated
/// population times the urban share, population_generation.cpp
/// § k_demography_urban_share_q) divided by this figure is how many centres
/// it contributes, floored at one — density is history's consequence, not a
/// land-area divisor.
///
/// The figure is scale 1's own headcount (`k_population_for_scale[0]` = 10
/// thousand heads): one centre per village's-worth of townsfolk, so the
/// constant is the scale table read backwards rather than a new number.
/// Against the three-seed demography baseline (~116M heads over ~1,200
/// regions on seed 0) this lands the density near the ~1 centre per 10 land
/// tiles the province-anchor ruling needs (BL-611; measured by
/// tools/verify/settlement_density.cpp — run it before moving this).
inline constexpr int k_demography_heads_per_centre = 10000;

/// Generates initial population centres for the given body and attaches them
/// to the world as entities with a `population_centre_component` on the chosen
/// tile entity.
///
/// Called from hard_coded_world.cpp after tile generation — and, since BL-610,
/// after the Era -1 settlement/history sim, whose region populations decide the
/// centre count and scale distribution (docs/economy/POPULATION.md
/// § Generation). The result is deterministic: the same seed, tile layout,
/// settlement record and body always produce identical centre placement.
///
/// @param w        World to populate; receives new population-centre entities.
/// @param body_id  Body whose tiles are candidates for population centre placement.
/// @param seed     Per-body RNG seed; the same seed always produces the same output.
/// @param settlement  The body's Era -1 settlement record. When non-null and
///        holding at least one region, the centre COUNT and SCALES are carved
///        from the regions' simulated populations (BL-610) and the divisor
///        below is ignored. Null (or empty) takes the land-area fallback.
/// @param land_tiles_per_centre  The FALLBACK density divisor. Defaults to the
///        shipped `k_land_tiles_per_centre`; the parameter exists so a harness
///        can measure what a DIFFERENT divisor would produce without a
///        recompile. Values <= 0 are treated as the default. Not a
///        world_params field on purpose: it is a measurement affordance, not a
///        save-format commitment.
void generate_population_centres(world& w, entity_id body_id, unsigned seed,
                                 const settlement_state* settlement = nullptr,
                                 int land_tiles_per_centre = k_land_tiles_per_centre);

/// Founds one population centre in every nation on @p body_id that holds none
/// (BL-463). Runs AFTER generate_nations — that is the whole point of it being a
/// separate pass: the primary placement above runs before borders exist and can
/// only derive a target from land area, so the NATION term of the target is
/// applied here, once there are nations to count.
///
/// Structural, not a tuned top-up: it founds exactly as many centres as there are
/// uncovered nations, at each nation's best habitable-and-rich unoccupied tile,
/// and skips a nation whose whole territory can host nobody. This is what makes
/// substrate_census's F1 row ("every nation holds at least one population
/// centre") true by construction.
///
/// Deterministic: sorted-id nation order, a pure argmax over a sorted tile list,
/// and one RNG stream seeded by @p seed used only for the founding's scale.
///
/// @returns the number of centres founded.
int ensure_national_population_centres(world& w, entity_id body_id, unsigned seed);

/// Urban footprint size per centre scale 1-5, in tiles including the centre's
/// own (BL-612, urban ground stamped). A village or town paves its own tile; a
/// city spills onto its best neighbour; a metropolis and a megacity take a
/// widening share of the ring (a full radius-1 disc at scale 5). Tiles here
/// are tens of kilometres across, so even 5M heads never out-paves one ring.
inline constexpr int k_urban_footprint_tiles[5] = { 1, 1, 2, 4, 7 };

/// Stamps `land_use_component::type::urban` under every population centre on
/// @p body_id (BL-612, urban ground stamped): the centre's own tile plus its
/// best neighbours up to `k_urban_footprint_tiles[scale-1]`, so city ground is
/// scarce and contested from turn one (docs/economy/POPULATION.md § Land use).
///
/// Runs AFTER both placement passes so coverage foundings are paved too, and
/// before corporations place assets. Extraction standing on ground that goes
/// urban is grandfathered (TILES.md § Urban transform) — the stamp never
/// removes anything, it only marks the ground.
///
/// Deterministic and RNG-free: centres walk in sorted-id order, neighbours are
/// ranked (habitability desc, tile id asc) over the fixed hex sides, water is
/// skipped, and stamping is idempotent — a tile two cities share is stamped
/// once. A footprint the coast cuts short stays short: an island city paves
/// what it has.
///
/// @returns the number of tiles stamped urban.
int stamp_urban_land_use(world& w, entity_id body_id);
