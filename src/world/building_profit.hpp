#pragma once

#include "economy_system.hpp" // economy_report
#include "entity.hpp"

#include <cstdint>

struct world;
class recipe_registry;

/// Estimated per-tick unit economics for a single building (BL-074). Revenue and
/// input cost are ESTIMATES: the pooled (body, resource) market commingles every
/// building's output, so an exact per-building sale is not recoverable — instead we
/// value this tick's reported output (and, for a processor, the inputs its runs
/// consumed) at the current market price. `maintenance` and `wages` come from the
/// shared budget formula (compute_building_opex), so they match the corp budget
/// exactly. All figures are last-tick (realised, not smoothed).
struct building_profit
{
    bool  has_data    = false; ///< false when no economy tick has reported this building yet.
    float revenue     = 0.0f;  ///< output this tick × current market price.
    float input_cost  = 0.0f;  ///< recipe inputs consumed × current market price (processing only).
    float maintenance = 0.0f;
    float wages       = 0.0f;

    /// Estimated net per-tick contribution: revenue less every outflow.
    float net() const { return revenue - input_cost - maintenance - wages; }
};

/// Estimate a building's per-tick unit economics from the latest economy report, its
/// recipe, and its body-market prices. Returns `has_data == false` when the building
/// or its report entry is absent (e.g. before the first tick).
building_profit estimate_building_profit(const world& w, const recipe_registry& reg,
                                         const economy_report& report, entity_id building_id);

/// Expected per-tick unit economics of a building that does NOT yet exist, on `tile_id`
/// (BL-162). Pure read of `world` + `registry` — no economy_report, no RNG, no caches.
///
/// The sibling of `estimate_building_profit`, which cannot serve a hypothetical: that one
/// requires both a live `building_component` and a matching `economy_report` row, and its
/// revenue is the REALISED `output_quantity`. This models the same `building_profit`
/// struct from world state alone, so a candidate's number reads identically to the built
/// building's Net once it exists.
///
/// Fixed, deliberate assumptions — stated in the construction ledger's caption rather
/// than hidden:
///   * `workforce_assigned` 0.5 (0.0 for port / inland_logistics_hub) and
///     `workforce_target` 100, exactly what `construct_building` authors;
///   * contention scalar 1.0 — there is no economy_report in hand and the new building
///     has not joined the labour pool yet, so assume no shortage and say so;
///   * today's local prices, flat. No supply-response elasticity, no deposit sharing with
///     existing sites (`run_extraction` gives every building the full richness, so no
///     adjustment is the correct model). Do not "improve" these silently.
///
/// The depletion taper IS applied: `resource_remaining` is the TILE's reserve, drawn down
/// by every site that ever worked it, so a tile can be spent before this candidate exists.
///
/// `has_data == false` when the tile is missing or no market resolves — a chart of zeros
/// is a lie.
///
/// @param recipe_id Processing recipe to price (ignored for other types); `no_recipe`
///                  prices nothing and yields zero revenue.
building_profit estimate_prospective_profit(const world& w, const recipe_registry& reg,
                                            entity_id tile_id, building_type type,
                                            resource_type target,
                                            std::uint16_t recipe_id = no_recipe);
