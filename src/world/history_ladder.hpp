#pragma once

#include "planetology.hpp"
#include "world.hpp"

#include <cstdint>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// The pre-national history ladder — docs/lore/HISTORY.md Stages 0-2 (BL-221)
//
// A deterministic sibling pass that runs AFTER the six-pass tile pipeline (it
// reads finished terrain) and INTERLEAVES with nation generation. It answers
// "how many independent peoples did this land support, and could any of them
// have conquered the rest?".
//
// THE LADDER DRIVES, IT DOES NOT NARRATE (Ben, 2026-07-30). Stage 0's cradle
// count and Stage 2's fragmentation are not captions on an already-generated
// political map — they are computed first and then SHAPE it, by deriving the
// `nation_params` the Voronoi pass is seeded with (`nation_params_from_ladder`).
// Retiring Voronoi entirely belongs to BL-218; until then this is the honest
// hook: the political map is a consequence of the ladder's numbers.
//
// The interleave, which is the part worth stating plainly:
//
//   generate_body_tiles                 terrain exists
//   run_history_ladder            ->    cradles, fragmentation, Stage 0 line
//   nation_params_from_ladder     ->    fragmentation drives the seed budget
//   generate_nations                    polities grow
//   record_institutional_history  ->    Stages 1-2, which NEED the outcome
//
// Stages 1 and 2 cannot be written in the first call: the Charter Act names a
// nation and the border accord counts them, and neither exists until the
// political pass has run. That is why this pass has two entry points rather
// than one.
//
// DETERMINISM. All scoring is INTEGER with an explicit tie-break by tile index
// — a float argmax over terrain would be a portability hazard and a tie-break
// nightmare (PLANETOLOGY.md § Determinism & cost). No transcendentals, fresh
// stage tags, and every walk is over raster order rather than a hash container.
//
// Design authority: docs/lore/HISTORY.md Stages 0-2; full design in
// backlog.json BL-221.
//
// SUBSTITUTED INPUTS, named rather than faked. BL-221 as designed reads river
// connectivity (BL-170) and BL-217's domesticable-clade output. Neither exists
// in `src/` yet — both items are designed but unbuilt — so Stage 0 scores
// cradles from arable terrain, habitability, coastal access and the biosphere's
// generated `endemics` instead. When those items land they REFINE this score;
// they do not replace it, so nothing here needs rewiring. See
// `agrarian_score` for exactly where each missing term would slot in.
// ---------------------------------------------------------------------------

/// One agricultural cradle — an independent origin of settled farming, and the
/// unit Stage 0 counts. More cradles -> deeper eventual fragmentation
/// (HISTORY.md Stage 0: "More cradles -> deeper eventual fragmentation").
struct agrarian_cradle
{
    int col = 0;            ///< Grid column of the cradle's core tile.
    int row = 0;            ///< Grid row of the cradle's core tile.
    int fertile_tiles = 0;  ///< Arable tiles in its immediate basin — how much surplus it could hold.
    int coastal = 0;        ///< 1 if the basin touches open water: the trade seat Stage 1 reads.
    std::string region;     ///< Deterministic descriptive name ("the northern reach").
};

/// What the ladder computed for one body.
struct history_ladder_state
{
    std::vector<agrarian_cradle> cradles;

    /// FRAGMENTATION, fixed-point 0-1000. How hard the land makes unification:
    /// barrier terrain on the interior, plus the number of independent cradles
    /// that had to be welded together. Fixed-point rather than float because it
    /// feeds a comparison that decides the political map.
    int fragmentation_q = 0;

    /// Conquest cost and exit cost, fixed-point 0-1000 (HISTORY.md Stage 2:
    /// geography "kept conquest expensive and exit cheap"). High conquest cost
    /// with low exit cost is the non-hegemony condition — a ruler who cannot
    /// take a neighbour cheaply, against a subject who can leave cheaply.
    int conquest_cost_q = 0;
    int exit_cost_q = 0;

    /// The polity seed budget the nation pass should grow from. This is the
    /// ladder's main DRIVING output.
    int polity_seeds = 0;

    /// Index into `cradles` of the charter's home — the best-placed trading
    /// seat, which Stage 1 attributes the first perpetual company to. -1 when
    /// no cradle formed (a world with no agrarian surplus has no charter).
    int charter_cradle = -1;

    /// Dated lines for the body's biography. Appended to
    /// planetology_state::history by the caller, not stored twice — the same
    /// convention continents_state::history follows.
    std::vector<history_event> history;
};

/// Run Stage 0 (agrarian surplus) and Stage 2's terrain half over a finished
/// tile map. Emits the Stage 0 history line; Stages 1-2 come later, from
/// `record_institutional_history`, because they need the political outcome.
///
/// Pure deterministic function of (@p pl, @p w tiles, @p tile_ids, @p gw,
/// @p gh, @p seed).
///
/// @param pl        The body's Planetology state (reads arable_share, endemics,
///                  life_stage — writes nothing back).
/// @param w         World holding the tile components. Read-only.
/// @param tile_ids  Raster-order tile IDs from generate_body_tiles (row*gw+col).
/// @param gw        Grid width (columns; wraps).
/// @param gh        Grid height (rows; does not wrap).
/// @param seed      Per-body seed, already folded with the campaign seed.
history_ladder_state run_history_ladder(const planetology_state& pl,
                                        const world& w,
                                        const std::vector<entity_id>& tile_ids,
                                        int gw, int gh, uint32_t seed);

/// Derive the nation-seeding parameters from the ladder — the point at which
/// the ladder stops describing and starts DRIVING.
///
/// A fragmented world (many cradles, expensive conquest) carries more, smaller
/// polities: seed density rises and the minimum viable territory falls, so the
/// merge pass absorbs less. A unifiable world does the opposite. @p base
/// supplies the body-specific defaults this modulates.
struct nation_params nation_params_from_ladder(const history_ladder_state& hl,
                                               const struct nation_params& base);

/// Emit Stages 1-2 once nations exist: the Charter Act (attributed to the
/// nation holding the charter cradle) and the border accord (which counts the
/// realms that actually survived the merge pass).
///
/// Appends to @p hl.history so the caller merges one list, not three.
///
/// @param hl           The ladder state from run_history_ladder; receives the lines.
/// @param w            World holding the generated nations and tile ownership.
/// @param body_id      The body whose political map was just generated.
/// @param tile_ids     Raster-order tile IDs, as above.
/// @param gw           Grid width, for locating the charter cradle's owner.
void record_institutional_history(history_ladder_state& hl,
                                  const world& w,
                                  entity_id body_id,
                                  const std::vector<entity_id>& tile_ids,
                                  int gw);
