#pragma once

#include "entity.hpp"
#include "orbital_node.hpp"

#include <cstdint>

struct world;
struct station_params;

// ---------------------------------------------------------------------------
// Space effects (BL-1586 satellite roles, BL-1584 station modules)
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § Satellites, § Space stations (the module
// roster), § Signal delay. What an ACTIVE satellite and a WORKING module DO.
//
// EVERYTHING HERE IS DERIVED; NOTHING NEW IS SAVED. The effects either read
// state live (the reads below: a Navigation satellite's logistics discount, an
// Observation satellite's vision lift, a Comms satellite's activity-fog lift)
// or move state that is already saved (a nation's treasury and research pool,
// a market's inventory, a body's survey). The quarterly step runs once per
// econ tick, inside the nation step, right after the station step.
//
// THE STEP, in order:
//   1. Life support. A station with at least one working Life support module
//      buys `rations_per_crew` x its crew aboard of food_rations off its owner's
//      home market shelf, at the posted price, whole or nothing (the BL-1172
//      fair-price ceiling applies, as to every draw). A station that cannot buy
//      them is STARVED for the rest of this step: its life support counts as
//      zero, which in `evaluate_station_budget`'s rule stops every working
//      module that needs crew. No memory carries it to the next quarter — the
//      next quarter's purchase decides that quarter afresh.
//   2. Research. Each working (and not starved) Research module adds
//      `research_rp_per_module` RP to its owner's research pool — only while the
//      station's node is in the owner's comms coverage.
//   3. Microgravity factory. Each working, not starved factory buys an input
//      lot of silicon off the home shelf and sells an output lot of electronics
//      onto it, both at the posted price. The resource roster has no orbit-only
//      good; electronics from silicon is the stand-in (see the report).
//   4. Survey satellites. Each ACTIVE Survey satellite whose node is covered maps
//      `survey_regions_per_quarter` regions of the body it serves.
//   5. Observatory. Each working, not starved Observatory maps
//      `observatory_regions_per_quarter` regions of every body in its owner's
//      comms coverage that is not yet surveyed.
//
// Mapping a region uses the one survey store (`body_component::survey`): a
// hidden body starts scanning as a dispatched probe's would, and a scanning
// body's `regions_done` advances; the last region surveys the body. A survey in
// transit is left to its probe. The home world and the star are never touched.
//
// Defence, Dock, Fuel depot, Shipyard, Cargo hold and Market do nothing here
// (their effects belong to space fleets and space convoys, SPACE_FLEETS.md).
//
// DETERMINISM. Stations walked in ascending id (std::map), assets in ascending
// id (std::map), coverage is an ordered set; RP in integer milli-RP; no RNG, no
// clock. A world with no space assets and no stations does nothing at all.
// ---------------------------------------------------------------------------

/// The dials. First-cut, in-band calibration (see the report); not yet a Lua key.
struct space_effects_params
{
    /// Navigation: the factor on the owner's intra-body logistics haul cost on
    /// the body a Navigation satellite serves (0.9 = a 10% discount). Several
    /// satellites on one body do not stack.
    float navigation_logistics_factor = 0.9f;

    /// Survey satellite: regions mapped per quarter while its node is covered.
    int survey_regions_per_quarter = 4;

    /// Research module: RP per quarter while its station's node is covered
    /// (half a research institute's 50 — RESEARCH.md § Research points).
    float research_rp_per_module = 50.0f;

    /// Life support: food_rations per crew aboard per quarter.
    float rations_per_crew = 1.0f;

    /// Microgravity factory: silicon bought, electronics sold, per quarter.
    float factory_input_lot  = 20.0f;
    float factory_output_lot = 10.0f;

    /// Observatory: regions mapped per quarter on each body in coverage.
    int observatory_regions_per_quarter = 2;
};

/// What one quarterly effects step did — reported, never stored.
struct space_effects_report
{
    int          stations_fed      = 0; ///< Bought their rations this quarter.
    int          stations_starved  = 0; ///< Could not: life support counted zero.
    float        rations_bought    = 0.0f;
    float        rations_spent     = 0.0f; ///< Credits.
    std::int64_t research_milli    = 0;    ///< Milli-RP credited to pools.
    int          research_unsignalled = 0; ///< Research modules working out of coverage.
    int          factory_runs      = 0;
    float        factory_spent     = 0.0f; ///< Credits for input lots.
    float        factory_earned    = 0.0f; ///< Credits for output lots.
    int          survey_regions    = 0;    ///< Regions mapped by Survey satellites.
    int          observatory_regions = 0;  ///< Regions mapped by Observatories.
};

/// The body an orbital asset at @p node serves: the node's body. An L-point is
/// keyed on its pair's SMALLER body (orbital_node.hpp), so this is that body.
inline entity_id space_served_body(orbital_node node) { return node.body; }

/// @p nation's home market: the market its capital tile clears to, else the
/// lowest-id market on its home body. null when neither resolves.
entity_id space_home_market_of(const world& w, entity_id nation);

/// Navigation (READ, live): the factor on @p nation's intra-body haul cost on
/// @p body — `navigation_logistics_factor` while it has an ACTIVE Navigation
/// satellite serving @p body, else 1.
float space_navigation_factor(const world& w, entity_id nation, entity_id body,
                              const space_effects_params& p = {});

/// Observation (READ, live): does @p nation have an ACTIVE Observation
/// satellite serving @p body whose node is in its comms coverage? The vision
/// fog over that body lifts for it.
bool space_observation_lifts_vision(const world& w, entity_id nation, entity_id body);

/// Comms (READ, live): does @p nation hold the Communication Satellites
/// technology (`space_keys::role_comms`) AND an ACTIVE Comms satellite? Then
/// the activity fog lifts on every market it trades with
/// (`body_activity_visibility` carries its own header-only twin of this test,
/// because world.cpp links alone in several harness targets).
bool space_comms_lifts_activity_fog(const world& w, entity_id nation);

/// THE STEP. Once per econ tick, inside the nation step, after the station
/// step. @p reservation_mult is the price band's fair-price ceiling.
void run_space_effects(world& w, const space_effects_params& p, const station_params& stp,
                       float reservation_mult, int econ_tick,
                       space_effects_report* out = nullptr);
