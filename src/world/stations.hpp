#pragma once

#include "entity.hpp"
#include "orbital_node.hpp"
#include "space_assets.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct world;

// ---------------------------------------------------------------------------
// Space stations (BL-1583) and astronauts (BL-1585) — the model and its verbs
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § Space stations, § Astronauts (Ben, 2026-11-10).
//
// A STATION REUSES THE SPACE-ASSET MACHINERY. Its core is a `station_core`
// space asset; each fitted module is a `station_module` asset carrying (station,
// grid slot, module kind); an upgrade's truss segment is a `station_module` of
// kind `truss` in no slot. So launch, transit, arrival, upkeep, decay and loss are
// `space_assets.hpp`'s, unchanged: a station's upkeep is the sum of its parts
// because each part is an asset paying its own.
//
// THE STORE (`world::stations`) is keyed by the station id, which IS its core
// asset's id (no second entity). It holds the owner, the orbital node, the
// upgrade level (grid 5x3 -> 7x5 -> 9x7) and, per slot, the asset in it — the
// core at the centre, a module (in transit or in place) or null. Slot index =
// row * width + col.
//
// THE BUDGETS are evaluated, never stored: `evaluate_station_budget` is pure and
// the quarterly step and any reader call it. A module WORKS only when it is
// active (arrived, paid), not mid-refit, not an edge-only module off the edge,
// and the station's power, crew room, life support and crew aboard all cover the
// working set; on a shortfall the newest-fitted module (highest asset id) stops
// first, repeatedly, until the rest are covered. Solar power — the core's and
// every Power module's — scales by (home world's AU / station body's AU)^2, both
// read heliocentrically (a moon reads its planet's).
//
// ASTRONAUTS are a per-nation count (`nation_component::astronauts`). Every
// completed, running, staffed Astronaut Training Centre adds `astronauts_per_centre`
// a quarter, the pool capped at `astronaut_cap_per_centre` x its running centres
// (the cap stops intake only). A crewed launch — the core and any module needing
// crew — draws its crew from the pool and is refused whole when the pool is short.
// Crew ride on the asset (`space_asset::crew`); crew on a lost asset are lost.
//
// BL-1581/1582: every station launch goes through ONE call, `station_launch` in
// stations.cpp, onto `launch_space_asset` — so the slot check (a core holds one
// slot of its node), the Rocketry/launchpad gate and the goods off the home shelf
// are the space-asset layer's. The KEYS a module kind or an upgrade needs are
// checked here, before the launch: each module kind its own key, the first
// upgrade `space_grid_7x5`, the second `space_grid_9x7` (far space: closed).
//
// WHAT IS NOT HERE: module EFFECTS (research points, rations, the factory, the
// observatory, the relay — BL-1584), signal delay (space_command.hpp queues an
// order outside coverage), any UI, any AI. The verbs are in-process; an agent seam over them must
// validate wire input as that seam's rule requires.
//
// DETERMINISM. Every store is a std::map / std::vector walked in key or index
// order; budgets are float sums in ascending asset id; no RNG, no clock.
// ---------------------------------------------------------------------------

/// The grid at each upgrade level: 5x3, 7x5, 9x7. Level 2 is the last.
inline constexpr int k_station_max_level = 2;
inline constexpr int station_grid_width(int level)  { return 5 + 2 * level; }
inline constexpr int station_grid_height(int level) { return 3 + 2 * level; }

/// Ceilings the save reader holds the stream to.
inline constexpr int32_t     k_astronaut_pool_max          = 1 << 20;
inline constexpr std::size_t k_station_templates_per_nation = 64;
inline constexpr std::size_t k_station_template_name_max    = 64;

/// One module kind's dials.
struct station_module_spec
{
    float power_draw   = 0.0f; ///< Power units it draws while working.
    float power_supply = 0.0f; ///< Power units it supplies at the home world's distance (solar).
    int   crew_needed  = 0;    ///< Astronauts it needs aboard — and draws from the pool at launch.
    int   crew_room    = 0;    ///< Crew room it adds.
    int   life_support = 0;    ///< Crew it keeps alive.
    bool  edge_only    = false;///< Must sit on the grid's outer ring to work.
};

/// The dials. Defaults are this change's first cut (in-band; see the report).
/// Not yet a Lua key: the registry carries these defaults (`recipe_registry::stations`).
struct station_params
{
    /// Per kind, `station_module_kind` order (truss last: draws, needs and adds nothing).
    std::array<station_module_spec, station_module_kind_count> modules{{
        /* research             */ {3.0f, 0.0f, 1, 0, 0, false},
        /* defence              */ {2.0f, 0.0f, 1, 0, 0, false},
        /* power                */ {0.0f, 6.0f, 0, 0, 0, false},
        /* dock                 */ {1.0f, 0.0f, 1, 0, 0, true},
        /* habitation           */ {1.0f, 0.0f, 0, 4, 0, false},
        /* life_support         */ {2.0f, 0.0f, 0, 0, 4, false},
        /* fuel_depot           */ {1.0f, 0.0f, 0, 0, 0, false},
        /* shipyard             */ {4.0f, 0.0f, 2, 0, 0, true},
        /* microgravity_factory */ {4.0f, 0.0f, 2, 0, 0, false},
        /* observatory          */ {2.0f, 0.0f, 1, 0, 0, false},
        /* comms_relay          */ {1.0f, 0.0f, 0, 0, 0, false},
        /* cargo_hold           */ {0.0f, 0.0f, 0, 0, 0, false},
        /* market               */ {1.0f, 0.0f, 1, 0, 0, false},
        /* truss                */ {0.0f, 0.0f, 0, 0, 0, false},
    }};

    /// The core's own: "a little power, room for two crew and life support for two".
    float core_power_supply = 4.0f; ///< Solar, as a Power module's.
    int   core_crew_needed  = 1;    ///< Drawn from the pool when the core launches.
    int   core_crew_room    = 2;
    int   core_life_support = 2;

    /// A refit takes this many quarters before the module works again.
    int32_t refit_quarters = 3; // econ ticks: 3 months, the one quarter (BL-1642; name kept)

    /// BL-1585: astronauts one running, staffed training centre adds a quarter,
    /// and the pool cap each running centre contributes.
    int32_t astronauts_per_centre    = 2; // per QUARTER, spread over its monthly ticks (BL-1642)
    int32_t astronaut_cap_per_centre = 20;
};

/// One station. SERIALISED (world_save.cpp, the v81 station section).
struct space_station
{
    entity_id    id    = null_entity; ///< == its core asset's id.
    entity_id    owner = null_entity; ///< A NATION.
    entity_id    core  = null_entity; ///< The core asset (== id).
    orbital_node node{};              ///< Where it sits; never the surface.
    uint8_t      level = 0;           ///< 0..k_station_max_level.
    std::vector<entity_id> slots;     ///< width * height; the asset in each slot, or null.

    int width()  const { return station_grid_width(level); }
    int height() const { return station_grid_height(level); }
    int centre() const { return (height() / 2) * width() + width() / 2; }
};

/// A named layout: a module kind (or k_no_station_module) per slot of a grid
/// level's size, the centre always empty (the core's). SERIALISED (v81).
struct station_template
{
    uint8_t              level = 0;
    std::vector<uint8_t> kinds; ///< width * height of `level`.

    friend bool operator==(const station_template& a, const station_template& b)
    {
        return a.level == b.level && a.kinds == b.kinds;
    }
    friend bool operator!=(const station_template& a, const station_template& b) { return !(a == b); }
};

/// Is slot @p idx of a @p level grid on the outer ring?
bool station_slot_on_edge(int level, int idx);

/// Outcome of a station verb.
enum class station_result : uint8_t
{
    ok = 0,
    rejected_invalid,          ///< A kind, node, slot index or name out of range.
    rejected_unknown_nation,
    rejected_unknown_station,
    rejected_not_owner,        ///< The station is another nation's.
    rejected_slot_occupied,
    rejected_slot_empty,       ///< remove / refit: nothing (or the core) in the slot.
    rejected_edge_only,        ///< An edge-only module aimed off the outer ring.
    rejected_no_crew,          ///< The astronaut pool cannot crew the launch.
    rejected_launch,           ///< The launch quote refused it (see `launch`).
    rejected_max_level,
    rejected_not_in_place,     ///< refit: the module has not arrived, or is lost.
    rejected_unknown_template,
    rejected_template_full,    ///< save_station_template: the nation holds the most it may.
    rejected_tech_locked,      ///< BL-1582: the module kind's or the upgrade's key is not held (space_tech_keys.hpp).
};

struct station_op
{
    station_result      result  = station_result::rejected_invalid;
    space_launch_result launch  = space_launch_result::rejected_invalid; ///< When a launch was tried.
    entity_id           station = null_entity;
    entity_id           asset   = null_entity; ///< The asset launched or moved.
};

/// The pure crew/launch check a station launch makes — what `found_station` or
/// `fit_station_module` would return without touching the world.
station_op quote_found_station(const world& w, const space_asset_params& sp, const station_params& stp,
                               entity_id nation, orbital_node node);

/// THE VERBS. Each is rejected WHOLE — nothing written, not even the allocator —
/// on any result but `ok`.

/// Launch a core to @p node: one slot's asset, crew from the pool, a 5x3 grid.
station_op found_station(world& w, const space_asset_params& sp, const station_params& stp,
                         entity_id nation, orbital_node node);

/// Launch a module of @p kind into empty slot @p slot of @p station.
station_op fit_station_module(world& w, const space_asset_params& sp, const station_params& stp,
                              entity_id nation, entity_id station, int slot, station_module_kind kind);

/// Free slot @p slot: the module asset is removed; nothing is refunded. Its crew
/// stay aboard (they move onto the core) when it had arrived.
station_op remove_station_module(world& w, entity_id nation, entity_id station, int slot);

/// Move the in-place module in @p from to empty slot @p to: launch-free; it works
/// again `refit_quarters` quarters later.
station_op refit_station_module(world& w, const station_params& stp, entity_id nation,
                                entity_id station, int from, int to);

/// Grow the grid one ring: a truss launch, the core still central, every module
/// keeping its place relative to the core. BL-1582: the upgrade's key is held.
station_op upgrade_station(world& w, const space_asset_params& sp, entity_id nation, entity_id station);

/// Save (or overwrite) a nation's named template. Validated: a known nation, a
/// name of 1..64 bytes, a level in range, one kind (fittable or none) per slot,
/// the centre empty.
station_result save_station_template(world& w, entity_id nation, const std::string& name,
                                     const station_template& t);

/// Forget a template. `rejected_unknown_template` when there was none.
station_result delete_station_template(world& w, entity_id nation, const std::string& name);

/// What applying a template did.
struct station_template_report
{
    station_result                     result = station_result::rejected_invalid; ///< ok unless refused whole
    std::vector<int>                   fitted;  ///< station slots launched into, ascending
    std::vector<std::pair<int, station_result>> skipped; ///< station slot (or -1 - template slot when off-grid), why
};

/// Apply @p name to @p station: one fit per slot the station has not filled to
/// match it, each its own launch, ascending slot. The template is centred on the
/// station's grid. A slot holding a different module, or that cannot be filled
/// (crew, treasury, edge), is skipped and reported; refused whole only for an
/// unknown nation, station or template, or another nation's station.
station_template_report apply_station_template(world& w, const space_asset_params& sp,
                                               const station_params& stp, entity_id nation,
                                               entity_id station, const std::string& name);

/// The budgets, evaluated. Pure.
struct station_budget
{
    bool  core_in_place = false; ///< The core has arrived and is not lost.
    float solar_factor  = 1.0f;  ///< (home AU / station AU)^2; 1 when either is unknown.
    float power_supply  = 0.0f;
    float power_draw    = 0.0f;
    int   crew_room     = 0;
    int   life_support  = 0;
    int   crew_aboard   = 0;     ///< Crew on the core and every in-place module.
    int   crew_needed   = 0;     ///< The core's plus every working module's.
    std::vector<entity_id> working;   ///< Ascending id.
    std::vector<entity_id> stopped;   ///< Stopped by a shortfall, newest first as stopped.
    std::vector<entity_id> idle_edge; ///< Edge-only modules no longer on the edge.
    std::vector<entity_id> refitting;
    std::vector<entity_id> not_ready; ///< In transit, decaying or otherwise not active.
    /// BL-1605 (solar_storm.hpp): a storm strikes the station's node this quarter.
    bool storm_struck = false;
    /// BL-1605: the modules that would have been candidates, stopped by the storm. Ascending id.
    std::vector<entity_id> stormed;
};

station_budget evaluate_station_budget(const world& w, const station_params& stp,
                                       entity_id station, int econ_tick);

/// What one quarterly station step did — reported, never stored.
struct station_step_report
{
    int astronauts_trained = 0; ///< Into every pool, world-wide.
    int stations_lost      = 0; ///< Stations whose core was lost; their modules go with them.
    int modules_lost       = 0; ///< Module/truss assets lost with a core.
    int slots_freed        = 0; ///< Slots whose module was lost.
    int crew_lost          = 0; ///< Astronauts on assets lost this quarter (cores, modules).
    int refits_done        = 0;
    int modules_working    = 0;
    int modules_stopped    = 0;
    int modules_stormed    = 0; ///< BL-1605: stopped by a solar storm this quarter.
};

/// BL-1597: mark @p a lost as the quarterly step's loss does (its crew stay on the
/// record; the step frees its slot, or takes its station when it is a core).
void lose_station_asset(space_asset& a);

/// BL-1585: the training centres' quarter. Ascending nation id; integer arithmetic.
int run_astronaut_training(world& w, const station_params& stp);

/// The pool cap @p nation's running training centres give.
int astronaut_cap(const world& w, const station_params& stp, entity_id nation);

/// The quarterly step, once per econ tick inside the nation step, AFTER the
/// space-asset step: trains astronauts, sweeps losses (a lost core loses its
/// station and every part of it; a lost module frees its slot), completes refits,
/// and counts the budgets. An empty store and no centres do nothing at all.
void run_station_step(world& w, const station_params& stp, int econ_tick,
                      station_step_report* out = nullptr);
