#pragma once

#include "tongue.hpp"
#include "world.hpp"

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// Procedural nation generation
//
// Deterministic, five-pass political-layer generation for a single body.
// Runs over an already-generated tile map; reads tile compositions from `w`
// and writes nation_component entries plus tile ownership back into `w`.
// See docs/generation/NATION_GENERATION.md for the design authority and
// per-pass rules.
// ---------------------------------------------------------------------------

/// Tunable parameters for a single nation generation run.
///
/// There is deliberately **no nation-count knob**: the number of nations is a
/// *consequence* of the body's geography, not a target. Seeds scale with the
/// habitable land area (`land_tiles_per_seed`) and the merge pass absorbs every
/// nation that fails to clear a minimum viable territory (`min_nation_tiles`).
/// A bigger, drier, or less fragmented homeworld therefore ends up with a
/// different political map than a small, ocean-heavy one.
struct nation_params
{
    /// **Seed density** — one Pass-1 seed per this many habitable (non-ocean)
    /// tiles on the body. The pre-merge seed count is `land_tiles / this`
    /// (at least one on any body with land), so the seed budget grows and
    /// shrinks with the landmass. Lower = more, tighter-packed seeds. A seed is
    /// only a *candidate* core: most are absorbed by the merge pass below, so
    /// this is deliberately far denser than the expected surviving count.
    int land_tiles_per_seed = 80;

    /// Minimum grid-distance (Chebyshev, column-wrapped) between any two nation
    /// seeds placed in Pass 1. Raise to spread nations further apart. Acts as a
    /// hard ceiling on how many seeds actually fit, whatever the density asks for.
    int min_seed_separation = 6;

    /// **Minimum viable territory**, in tiles. Pass 2c absorbs any nation holding
    /// fewer tiles than this into its largest adjacent neighbour — smallest first
    /// — and stops once every survivor clears the floor. The final nation count
    /// falls out of that, rather than being counted down to a target. 0 disables
    /// the merge pass entirely (every placed seed keeps its Voronoi cell).
    ///
    /// Set equal to `land_tiles_per_seed` by default, which reads as: a nation
    /// must end up holding at least the land its own seed was budgeted. The
    /// resulting count is ladder-derived: the history ladder (BL-221) modulates
    /// seeding, so the default Kepler seed lands well above the pre-ladder figure
    /// (43 nations at the current default seed). Halve the habitable area and
    /// the count still roughly halves with it.
    int min_nation_tiles = 80;

    /// **Explicit seed tiles** (raster indices, `row * gw + col`) — when this is
    /// non-empty Pass 1's random placement is SKIPPED and these are used verbatim,
    /// in order, as the nation cores.
    ///
    /// This is the whole of BL-218's "seeding changes, expansion does not": the
    /// settlement pass (`src/world/settlement.{hpp,cpp}`) places regions on
    /// ground people would actually have settled and hands the anchors over here,
    /// so the BFS/growth machinery BL-053 tuned is reused untouched and only its
    /// starting points become historical. Empty (the default) preserves the
    /// pre-BL-218 random placement bit-for-bit for every body without a
    /// settlement pass.
    ///
    /// Entries outside the grid, on ocean, or duplicated are dropped by Pass 1;
    /// `min_seed_separation` is NOT re-applied to them (the settlement pass owns
    /// its own separation rule and re-filtering here would silently discard
    /// history).
    std::vector<int> seed_tiles;

    /// Parallel to `seed_tiles`: the tongue of the culture that settled each
    /// anchor (BL-290). Pass 5 names a nation in the speech of the region it
    /// grew from, so a realm, its gods and its cities share one sound system as
    /// a consequence of the generation chain rather than by coincidence. Short,
    /// empty, or unusable entries fall back to a tongue rolled in Pass 5.
    std::vector<tongue> seed_tongues;
};

/// Generate nations over the tile map of one body and register all results in @p w.
///
/// Runs the five-pass nation generation pipeline (seed placement, territory
/// expansion, resource profiling, political character, naming) described in
/// docs/generation/NATION_GENERATION.md. The grid wraps horizontally (columns)
/// and does not wrap vertically (rows), matching the tile layout produced by
/// generate_body_tiles.
///
/// Deterministic in @p seed: the same seed, tile map, and params always produce
/// the same political map. Does not use std::random_device or global state.
///
/// @param w        World holding the tile components; receives the new nation
///                 entities and tile_to_nation ownership entries.
/// @param body_id  Body whose tiles are being given political ownership.
/// @param tile_ids Raster-order tile IDs from generate_body_tiles (index = row * gw + col).
/// @param gw       Grid width (columns).
/// @param gh       Grid height (rows).
/// @param params   Tunable generation parameters.
/// @param seed     Per-run RNG seed for independent, reproducible results.
/// @param progress Optional progress sink (BL-305). When non-null, Pass 2
///                 publishes each tile as it is claimed and Pass 2c republishes
///                 the merged map, so a loading screen can show the carve
///                 happening. A pure TAP: write-only, consumes no randomness,
///                 changes no branch — passing null (the default, and every
///                 headless caller) produces a byte-identical political map.
///                 Defined in world/hard_coded_world.hpp.
/// @return         Nation entity IDs in seed-placement order (one per nation created).
std::vector<entity_id> generate_nations(
    world& w,
    entity_id body_id,
    const std::vector<entity_id>& tile_ids,
    int gw, int gh,
    const nation_params& params,
    uint32_t seed,
    struct generation_progress* progress = nullptr);

// ---------------------------------------------------------------------------
// BL-571 — nation garrisons (docs/military/MILITARY.md § Nation garrisons)
// ---------------------------------------------------------------------------
// Nations own units. `unit_component::owner` accepts a nation entity exactly
// as it accepts a corp's. A garrison is seeded at generation, not hired: one
// in the nation's capital province, plus one in each border province it
// shares with its single highest-grudge neighbour (`nation_grudge`,
// nation_ai.hpp). This is the FOURTH unit writer — alongside the hard-coded
// stub, `seed_starting_military` and `hire_unit` (MILITARY.md § The unit
// model) — and the only one whose owner is a nation rather than a corp.
//
// Garrisons are STATIC: they hold their seeded tile and never march. A nation
// that wants ground HIRES — that is the whole point of the mercenary
// contract, and a garrison that marched would make the nation its own
// mercenary.

/// Tunable parameters for garrison sizing, treasury-scaled against BL-543's
/// value anchor (unit equipment costs ~ 2x annual salary). PLACEHOLDER
/// NUMBERS: BL-544 (unit wage reference), the item that would give this a
/// measured figure to derive from, has not landed — see
/// `seed_nation_garrisons`'s own comment and BL-571's report for the flag to
/// Ben.
struct nation_garrison_params
{
    /// Floor a garrison never falls below, however poor the nation — "a poor
    /// one a token one" (MILITARY.md), never nothing. Also the count every
    /// nation currently gets, because `nation_component::treasury` is 0.0 at
    /// generation for every nation (NATIONS.md: "zero at generation,
    /// deliberately") — see the flag above.
    int min_count = 20;

    /// Additional garrison head per credit of `nation_component::treasury`.
    float count_per_credit = 0.05f;

    /// Ceiling so a large treasury cannot field an unbounded garrison.
    int max_count = 200;

    /// Roster row every garrison uses — row 0, the same default
    /// `seed_starting_military` gives a corp's opening unit.
    uint16_t roster_row = 0;
};

/// Seed one static garrison per nation, per `nation_garrison_params` above.
///
/// MUST run AFTER `build_province_partition` and `seed_province_holders`
/// (province.hpp) — the capital-province lookup reads `w.provinces`, and the
/// border-province test reads `province_holder_for` (BL-569), consumed here
/// exactly as this item's brief names. `w.tile_to_nation` must also already
/// be populated (`generate_nations` writes it).
///
/// DETERMINISTIC: nations walked in ascending entity id (`w.nations` is
/// unordered); a nation's candidate neighbours and its target provinces are
/// both gathered into `std::set`s (ascending id/province id) before any
/// argmax or creation, so ties break on ascending id without a second sort
/// and unit-creation order does not depend on either unordered container's
/// layout.
///
/// @param w      World whose provinces, province_holder and tile_to_nation
///               are already built; receives new nation-owned unit entities.
/// @param params Sizing and roster tuning.
void seed_nation_garrisons(world& w, const nation_garrison_params& params = {});

/// Summed `unit_strength` (BL-459, unit_roster.hpp) of every nation-owned
/// unit standing in the province with id @p province_id — the garrison-
/// strength-per-province query this item provides (alongside the units
/// themselves) for a later consumer (a contract's target-strength read,
/// a UI surface) to read without re-deriving the ownership test.
///
/// At most one nation garrisons any given province BY CONSTRUCTION —
/// `seed_nation_garrisons` places at most one — so this does not need to
/// name which nation; a province with none, or id 0 (unpartitioned ground),
/// returns 0.
///
/// ORDER-INDEPENDENT BY CONSTRUCTION: `unit_strength` is an integer, so a
/// sum over the unordered `w.units` is associative and cannot depend on the
/// map's layout (the same property `condition_subject::military_strength`
/// already relies on — MILITARY.md § The unit model).
///
/// @param w           World whose `w.units` and `w.nations` are read.
/// @param province_id Province to sum over.
/// @return            Summed strength (`unit_strength`'s own `int64_t`), or 0.
int64_t garrison_strength_in(const world& w, uint32_t province_id);
