#pragma once
// ---------------------------------------------------------------------------
// closing_rounds — the size floor and the total, inside the history (BL-1278)
// ---------------------------------------------------------------------------
//
// NATION_GENERATION.md § The nation count is a target and § Pass 2c (Ben,
// 2026-10-11): on a settled world the size floor is no longer a post-pass over
// the carve. It is CLOSING-ROUND ABSORPTIONS in Industrialisation — a realm
// below the minimum territory is absorbed by a neighbour as an event of the
// span — and, where the player set a total, the remaining gap is closed in the
// same closing rounds by the history's own verbs: absorption where there are
// too many nations, a province seceding where there are too few.
//
// THE DIFFICULTY THIS FILE SOLVES. The nation count is produced AFTER the
// history, by the carve (Pass 2/2b, water) and the polity fold (Pass 2d). The
// history works in regions and polities; the floor works in carve tiles. So
// the closing rounds read a CARVE ORACLE — the very carve `generate_nations`
// will draw, from the same region anchors and the same seeds
// (`preview_nation_carve`), mapped tile -> region — and plan against the fold
// it implies. The fold is exactly predictable from the region table: one
// nation per polity holding at least one kept anchor, plus one per kept anchor
// no polity holds (`predict_fold_count`). Pass 2c does not run after a history
// that ran these rounds (`nation_params::floor_in_history`), so the plan's
// final count IS the count the fold produces; `run_tail` asserts it (the drift
// guard: a handoff violation if they differ).
//
// PURE. The planner reads integers (tile counts, region indices, polity ids)
// and walks vectors in ascending order with fixed tie-breaks; no draw, no
// float, no container-order dependency. It returns a plan; the history sim
// applies it with its own bookkeeping (`run_history_sim`'s close).

#include "world/history_sim.hpp"
#include "world/nation_generation.hpp"
#include "world/settlement.hpp"

#include <cstdint>
#include <vector>

/// What the closing rounds need from outside the sim. Held by the caller for
/// the life of the `run_history_sim` call; the sim reads it through
/// `history_sim_params::closing_rounds` and never stores it.
struct closing_rounds_spec
{
    /// THE CARVE'S GROUND, a value snapshot (`nation_carve_ground_of` over the
    /// homeworld's tiles), and `generate_nations`' own seed. The carve oracle
    /// (`closing_carve_cells`) draws the SAME carve the tail will, from the
    /// region anchors as they stand at the close. Self-contained, so the
    /// fixture can own the spec and a replay of the span draws the same carve.
    nation_carve_ground ground;
    uint32_t nation_seed      = 0;
    int  min_nation_tiles = 0;    ///< `nation_params::min_nation_tiles`; 0 = no floor.
    bool keep_city_states = true; ///< `nation_params::keep_city_states`.
    int  total_target     = 0;    ///< `world_params::nation_total_target`; 0 = unset.
};

/// The carve oracle: per raster tile the index of the REGION whose anchor's
/// carve cell holds it (-1 = none), for @p regions as they stand. Anchors are
/// read with `settlement_seed_tiles`' filter and order.
std::vector<int> closing_carve_cells(const closing_rounds_spec& spec, const std::vector<region>& regions);

enum class closing_step_kind : uint8_t
{
    floor_absorb  = 0, ///< Below the minimum territory: absorbed by a neighbour.
    target_absorb = 1, ///< Too many nations for the total: absorbed by a neighbour.
    target_secede = 2, ///< Too few nations for the total: a province secedes.
};

/// One closing-round correction, in the order it is applied.
struct closing_step
{
    closing_step_kind kind = closing_step_kind::floor_absorb;
    /// Absorb: the absorbed realm, or -1 for ground no realm held. Secede: the parent.
    int from_polity = -1;
    /// Absorb: the absorbing realm. Secede: the id the successor will take
    /// (`polities.size()` at its turn — the sim assigns ids in push order).
    int into_polity = -1;
    /// Absorb: the absorbed realm's capital, or the ownerless region. Secede: the new seat.
    int region = -1;
    /// The regions that change hands, ascending. Absorbing a realm moves every
    /// region it holds; ownerless ground moves alone; a secession moves its
    /// seat and the unanchored hinterland already pointing at it.
    std::vector<int> moved;
    /// A floor step where the SMALL realm took the larger unheld ground beside
    /// it (ground no realm held cannot absorb): `from_polity` -1 (the ground),
    /// `into_polity` the realm that was too small. The cause is the realm's size.
    bool annex = false;
};

struct closing_plan
{
    std::vector<closing_step> steps;
    int nations_before      = 0; ///< The fold's count before any step.
    int nations_after_floor = 0; ///< After the floor's absorptions.
    int nations_after       = 0; ///< After every step: what the fold will produce.
    int floor_absorptions   = 0;
    int target_absorptions  = 0;
    int target_secessions   = 0;
    bool short_of_target    = false; ///< A target was set and no legal step could reach it (a defect: recorded as a handoff violation).
};

/// Per region, the number of carve tiles its anchor's cell holds (0 for a
/// region with no kept anchor).
std::vector<int64_t> closing_cell_tiles(const std::vector<int>& cell_region, std::size_t region_count);

/// The fold's count for a region table under a carve: one nation per polity
/// holding at least one region with a cell, plus one per such region no polity
/// holds. Exactly `generate_nations`' count when the size floor does not run there.
int predict_fold_count(const std::vector<region>& regions, const std::vector<int>& cell_region);

/// Plan the closing rounds: the floor first (always, when `min_tiles > 0`),
/// then the total (only when `total_target > 0`). `polities` gives each
/// realm's capital and liveness.
closing_plan plan_closing_rounds(const std::vector<region>& regions,
                                 const std::vector<polity>& polities,
                                 const std::vector<int>&    cell_region,
                                 int gw, int gh,
                                 int min_tiles, bool keep_city_states, int total_target);
