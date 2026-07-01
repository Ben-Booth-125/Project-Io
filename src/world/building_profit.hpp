#pragma once

#include "economy_system.hpp" // economy_report
#include "entity.hpp"

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
