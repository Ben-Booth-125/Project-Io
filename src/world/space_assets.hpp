#pragma once

#include "entity.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

struct world;

// ---------------------------------------------------------------------------
// Space assets (BL-1327) — the data model and the one legal verb
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § Era 1 near space (Ben, 2026-10-15). A space
// asset is a NATION's owned object beyond the planetary surface: launched and
// paid for from its treasury, placed after a travel time, kept by a quarterly
// upkeep, and — unpaid — decaying and then lost. Corporations supply the goods
// through the ordinary market; they never own one in Era 1.
//
// THE ORBITAL GRAIN IS A NAMED SLOT, NOT A COORDINATE (in-band calibration
// call, BL-1327 report). A destination is (body, slot) where slot is the body's
// ORBIT or its SURFACE. Nothing finer exists: no altitude, no inclination, no
// position along an orbit. What a destination COSTS is read off its REACH tier,
// derived — never stored — from where the target sits relative to the owner's
// home body:
//
//   home_orbit    the owner's home body, orbit slot
//   moon_orbit    a body whose parent IS the home body, orbit slot
//   moon_surface  the same, surface slot
//   far_orbit     any other body, orbit slot   (+ a per-AU term)
//   far_surface   any other body, surface slot (+ a per-AU term)
//
// The home body's SURFACE is not a space destination (it is a building's
// ground), and the star has no slot of either kind; both are illegal pairs.
//
// THE LAUNCH-SITE GATE is enforced by the launch infrastructure layer, not here.
//
// WHAT IS NOT HERE, deliberately (sibling items own them): the goods a launch
// consumes (recipes), any scorer that chooses to launch (AI_OPPONENT.md § 11 —
// no grant exists for it), any UI, and the corp_command dictionary entry. The
// verb is the in-process function below; wiring it onto the agent seam is a
// separate change that must validate wire input as that seam's rule requires.
//
// DETERMINISM. `world::space_assets` is a std::map: the quarterly step walks it
// ascending id with no sort. Costs are float arithmetic over static inputs
// (authored params, a body's fixed orbital radius); no RNG, no clock.
// ---------------------------------------------------------------------------

/// The Era 1 classes (SPACE_ASSETS.md). Launch infrastructure is a BUILDING,
/// not a space asset, so it is not a class here. Append-only: the value is
/// written to the save.
enum class space_asset_class : uint8_t
{
    satellite      = 0, ///< Uncrewed orbital instrument.
    rover          = 1, ///< Uncrewed surface unit landed on another body.
    crewed_mission = 2, ///< A crew sent to a body (return is not modelled here).
    station_module = 3, ///< One launch of a crewed orbital platform.
};
inline constexpr std::size_t space_asset_class_count = 4;

/// Which slot of a body a destination names. Append-only (saved).
enum class space_slot : uint8_t
{
    orbit   = 0,
    surface = 1,
};

/// A destination: a named slot on a body. `body` is any body entity; the reach
/// tier is derived from it (`space_reach_of`).
struct space_destination
{
    entity_id  body = null_entity;
    space_slot slot = space_slot::orbit;
};

/// The reach tier — DERIVED from (owner's home body, destination), never stored.
enum class space_reach : uint8_t
{
    home_orbit   = 0,
    moon_orbit   = 1,
    moon_surface = 2,
    far_orbit    = 3,
    far_surface  = 4,
};
inline constexpr std::size_t space_reach_count = 5;

/// Lifecycle. Append-only (saved).
enum class space_asset_state : uint8_t
{
    in_transit = 0, ///< Launched; arrives at `arrival_tick`.
    active     = 1, ///< In place, upkeep paid.
    decaying   = 2, ///< In place, upkeep unpaid for `unpaid_quarters` (>= 1) quarters.
    lost       = 3, ///< Gone. Kept as a record; never steps again.
};

/// One space asset. SERIALISED (world_save.cpp, the v51 trailing section).
struct space_asset
{
    entity_id         id    = null_entity; ///< From the world's entity allocator.
    entity_id         owner = null_entity; ///< A NATION entity.
    space_asset_class cls   = space_asset_class::satellite;
    space_destination dest{};
    space_asset_state state = space_asset_state::in_transit;
    int32_t           launch_tick  = 0;    ///< Econ tick (quarter) of launch.
    int32_t           arrival_tick = 0;    ///< Econ tick it becomes active; >= launch_tick.
    float             upkeep       = 0.0f; ///< Credits per quarter, fixed at launch.
    int32_t           unpaid_quarters = 0; ///< Consecutive unpaid quarters; 0 when paid.
    float             launch_cost  = 0.0f; ///< Credits the launch debited (the quote), for the quarter's report.
};

/// The hard ceiling on `space_asset_params::quarters_to_loss`. The step clamps
/// the dial into [1, this]; the save reader, which cannot see the registry,
/// validates an asset's unpaid count against it.
inline constexpr int32_t k_space_quarters_to_loss_ceiling = 64;

/// The dials. Defaults are the BL-1327 first-cut calibration (in-band; see the
/// report). Credits are the treasury's units; the satellite's launch cost sits
/// near the BL-644 purchase lump's market value (25 spacecraft_components at
/// base 310 + 50 propellant at base 64 = ~10,950). Not yet a Lua key: the
/// registry carries these defaults (`recipe_registry::space_assets`).
struct space_asset_params
{
    /// Per class: the payload's launch cost, before reach.
    std::array<float, space_asset_class_count> launch_cost{10000.0f, 20000.0f, 60000.0f, 40000.0f};

    /// Per class: quarterly upkeep, fixed onto the asset at launch.
    std::array<float, space_asset_class_count> upkeep{500.0f, 1000.0f, 3000.0f, 2000.0f};

    /// Per reach tier: the cost to reach it (further = costlier).
    std::array<float, space_reach_count> reach_cost{0.0f, 8000.0f, 15000.0f, 30000.0f, 45000.0f};

    /// Per reach tier: travel time in quarters (further = slower).
    std::array<int32_t, space_reach_count> travel_quarters{0, 1, 1, 2, 3};

    /// Far tiers only: extra cost per AU of heliocentric separation between the
    /// home body's system position and the target's (a moon reads its planet's).
    float far_cost_per_au = 20000.0f;

    /// Far tiers only: extra quarters per AU of the same separation, floored.
    float far_quarters_per_au = 1.0f;

    /// Consecutive unpaid quarters at which a decaying asset is LOST. The first
    /// unpaid quarter makes it decaying; a paid quarter restores it to active.
    /// Clamped into [1, k_space_quarters_to_loss_ceiling] where it is read.
    int32_t quarters_to_loss = 4;

    /// Far tiers only: the AU-derived extra quarters are clamped to this before
    /// the integer cast, so no separation can overflow a tick.
    int32_t max_extra_quarters = 400;
};

/// May @p cls go to @p reach? The class/destination legality table:
///   satellite       any orbit slot (home, moon, far)
///   rover           a surface off the home body (moon, far)
///   crewed_mission  any destination off the home body (moon or far, either slot)
///   station_module  home orbit or moon orbit
bool space_launch_legal(space_asset_class cls, space_reach reach);

/// The home-independent half of the table: may @p cls ever occupy @p slot?
/// satellite and station_module are orbit-only, rover surface-only, a crewed
/// mission either. The save reader holds every stored asset to this.
bool space_slot_legal(space_asset_class cls, space_slot slot);

/// Outcome of a launch, or of a quote.
enum class space_launch_result : uint8_t
{
    launched = 0,
    rejected_invalid,              ///< Class or slot out of range; non-finite cost.
    rejected_unknown_nation,       ///< `nation` names no nation.
    rejected_unknown_body,         ///< The destination body does not exist, or the owner has no home body.
    rejected_illegal_destination,  ///< The star, the home surface, or a class/reach pair the table refuses.
    rejected_insufficient_treasury ///< The nation's treasury cannot pay the whole cost.
};

struct space_launch_quote
{
    space_launch_result result = space_launch_result::rejected_invalid;
    space_reach         reach  = space_reach::home_orbit;
    float               cost   = 0.0f;  ///< Credits, the whole launch.
    int32_t             travel_quarters = 0;
    entity_id           asset  = null_entity; ///< Set by a successful launch only.
};

/// The owner's home body: its capital tile's body, else `w.home_body`.
/// `null_entity` when neither resolves.
entity_id space_home_body_of(const world& w, entity_id nation);

/// PURE. Price @p cls to @p dest for @p nation and say whether it is legal and
/// affordable — `result` is what `launch_space_asset` would return. Reads only.
space_launch_quote quote_space_launch(const world& w, const space_asset_params& p,
                                      entity_id nation, space_asset_class cls,
                                      space_destination dest);

/// THE VERB. Launch @p cls to @p dest for @p nation, paid whole from the
/// nation's treasury. Rejected WHOLE — the world is not touched, not even the
/// entity allocator — when the quote is not `launched`.
///
/// THE TICK CONTRACT. The launch tick is `w.current_econ_tick`, read from the
/// world and never passed in: the econ tick ABOUT TO RUN, which every driver
/// mirrors onto the world before stepping it. The asset is stored `in_transit`
/// with `arrival_tick = launch_tick + travel_quarters`, and the nation step of
/// that same tick runs `run_space_asset_step` — so travel 0 is active (and pays
/// its first upkeep) in the launch quarter, travel 1 in the next, and each
/// further quarter of travel is exactly one more step.
space_launch_quote launch_space_asset(world& w, const space_asset_params& p,
                                      entity_id nation, space_asset_class cls,
                                      space_destination dest);

/// What one quarterly step did — reported, never stored.
struct space_asset_step_report
{
    int   launched = 0;  ///< Assets whose launch_tick is this quarter.
    float launch_spent = 0.0f; ///< Their launch costs: the quarter's launch debit.
    int   arrived = 0;   ///< in_transit -> active this quarter.
    int   paid    = 0;   ///< Assets whose upkeep was paid.
    int   unpaid  = 0;   ///< Assets whose upkeep was not.
    int   lost    = 0;   ///< Assets lost this quarter.
    float upkeep_paid = 0.0f; ///< Credits debited from treasuries.
};

/// The quarterly step, once per econ tick inside the nation step. Ascending
/// asset id: an in-transit asset whose arrival tick has come becomes active;
/// every active or decaying asset then pays its upkeep WHOLE from its owner's
/// treasury, or pays nothing and counts an unpaid quarter (decaying; lost at
/// `quarters_to_loss`). A paid quarter clears the count and restores active.
/// Lost assets do nothing. An empty store does nothing at all. @p out, when
/// given, also receives the quarter's launches (assets whose launch tick is
/// @p econ_tick) and their summed cost, so the report carries the launch debit
/// beside the upkeep debit.
void run_space_asset_step(world& w, const space_asset_params& p, int econ_tick,
                          space_asset_step_report* out = nullptr);
