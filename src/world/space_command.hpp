#pragma once

#include "entity.hpp"
#include "offworld_base.hpp" // BL-1601: offworld_base_result
#include "orbital_node.hpp"
#include "space_assets.hpp"
#include "stations.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct world;
class recipe_registry;

// ---------------------------------------------------------------------------
// The near-space command seam (BL-1588) and the signal-delay order queue (BL-1587)
// ---------------------------------------------------------------------------
// docs/economy/SPACE_ASSETS.md § Launching, § Satellites, § Space stations,
// § Signal delay. Every near-space verb a NATION issues, as one record:
// launch, found a station, fit / remove / refit a module, apply / save / delete
// a template, upgrade a station, retask or move a satellite.
//
// WHY ITS OWN SEAM, NOT corp_command. The nation verbs live on `corp_verb` with
// `as_nation`, a fixed-size wire record; a template verb carries a NAME and a
// whole LAYOUT, which that record cannot hold without a string field every
// command pays for. So the space verbs are a sibling record with the same
// discipline, applied by `apply_space_command`. Wiring it onto the --serve /
// MCP transport (a SPACE line beside COMMAND) is a separate change.
//
// AN UNTRUSTED INPUT BOUNDARY (.claude/rules/io-standing-rules.md). Every enum
// field is a RAW uint8 here, range-checked as the value that lands before any
// cast; every entity id is checked to exist and to be the nation's own; every
// slot index is checked against the station's grid; a name against 1..64 bytes;
// a layout against its level's grid. A violation rejects the WHOLE command and
// mutates nothing. Which nation a session may act as is the transport's call.
//
// SIGNAL DELAY. An order to a station or a satellite whose node is NOT in the
// nation's comms coverage (`space_node_covered`, space_coverage.hpp) is stored
// as a PENDING ORDER — validated now, `queued` returned, the world otherwise
// untouched — and applied at the next econ tick's nation step
// (`run_space_orders`), where it is RE-VALIDATED and dropped, reported, if no
// longer legal. Inside coverage it applies at once. The delayed verbs are fit,
// remove, refit, apply template, upgrade, retask and move. A launch or a station
// founded from the home surface is never delayed (the ground stations see the
// pad), and templates are the nation's own records on the ground.
//
// THE QUEUE IS SAVED (world_save.cpp, v82) in issue order, each order with its
// due tick. DETERMINISM: a std::vector walked in stored order; ids ascend.
// ---------------------------------------------------------------------------

/// The verbs. Append-only: the value is written to the save (a pending order).
enum class space_verb : uint8_t
{
    launch           = 0,  ///< cls (satellite / rover / crewed_mission), role (satellites), node.
    found_station    = 1,  ///< node.
    fit_module       = 2,  ///< subject = station, slot, module.
    remove_module    = 3,  ///< subject = station, slot.
    refit_module     = 4,  ///< subject = station, slot (from), slot_to.
    apply_template   = 5,  ///< subject = station, name.
    save_template    = 6,  ///< name, layout.
    delete_template  = 7,  ///< name.
    upgrade_station  = 8,  ///< subject = station.
    retask_satellite = 9,  ///< subject = satellite asset, role.
    move_satellite   = 10, ///< subject = satellite asset, node.
    // BL-1601 (NATIONS.md § Off-world bases), appended:
    found_base       = 11, ///< subject = the TILE the base is founded on (offworld_base.hpp).
    embark_colonists = 12, ///< subject = the nation's Lander, slot = colonists (thousands) to board.
};
inline constexpr uint8_t space_verb_count = static_cast<uint8_t>(space_verb::embark_colonists) + 1;

/// Does @p v wait for a signal outside coverage?
inline bool space_verb_delayed(space_verb v)
{
    switch (v)
    {
        case space_verb::fit_module:
        case space_verb::remove_module:
        case space_verb::refit_module:
        case space_verb::apply_template:
        case space_verb::upgrade_station:
        case space_verb::retask_satellite:
        case space_verb::move_satellite:
            return true;
        default:
            return false;
    }
}

/// One command. Unused fields stay at their defaults. Enums are RAW bytes
/// (validated before any cast).
struct space_command
{
    entity_id nation    = null_entity;
    uint8_t   verb      = 0;
    uint8_t   cls       = 0;                     ///< launch: a space_asset_class.
    uint8_t   role      = 0;                     ///< launch (satellites) / retask: a space_satellite_role.
    entity_id node_body = null_entity;           ///< launch / found_station / move_satellite.
    uint8_t   node_kind = 0;                     ///< an orbital_node_kind.
    entity_id subject   = null_entity;           ///< A station or a satellite asset.
    int32_t   slot      = -1;                    ///< fit / remove / refit (from).
    int32_t   slot_to   = -1;                    ///< refit (to).
    uint8_t   module    = k_no_station_module;   ///< fit: a station_module_kind.
    std::string      name;                       ///< apply / save / delete template.
    station_template layout;                     ///< save_template.
};

/// A command waiting for a signal. SERIALISED (v82).
struct space_order
{
    uint32_t      id       = 0;  ///< Ascending from `world::next_space_order_id`.
    int32_t       due_tick = 0;  ///< The econ tick whose nation step applies it.
    space_command cmd;
};

/// The ceiling on the whole queue (a runaway issuer cannot grow the save).
inline constexpr std::size_t k_space_orders_max = 4096;

/// What a command did.
enum class space_command_result : uint8_t
{
    applied = 0,
    queued,                  ///< Outside coverage: stored as a pending order (`order_id`).
    rejected_invalid,        ///< A field out of its domain (see the file comment).
    rejected_unknown_nation,
    rejected_not_owner,      ///< The station or satellite is another nation's.
    rejected_refused,        ///< The verb itself refused (see `launch` / `station` / `satellite`).
    rejected_queue_full,     ///< The pending-order queue is at `k_space_orders_max`.
};

struct space_command_outcome
{
    space_command_result   result    = space_command_result::rejected_invalid;
    space_launch_result    launch    = space_launch_result::rejected_invalid;  ///< launch / move / a station launch.
    station_result         station   = station_result::rejected_invalid;       ///< station verbs.
    satellite_order_result satellite = satellite_order_result::rejected_invalid; ///< retask / move.
    entity_id              asset     = null_entity; ///< The asset launched / moved / retasked.
    entity_id              station_id = null_entity; ///< The station acted on or founded.
    uint32_t               order_id  = 0;           ///< When queued.
    int                    fitted    = 0;           ///< apply_template: slots launched into.
    int                    skipped   = 0;           ///< apply_template: slots skipped.
    offworld_base_result   base      = offworld_base_result::rejected_invalid; ///< BL-1601: found_base / embark_colonists.
    entity_id              base_centre = null_entity; ///< BL-1601: found_base: the base's population centre.
};

/// PURE. Validate @p c field by field as the value that lands (see the file
/// comment). Returns `applied` when every field is in its domain — not whether
/// the verb itself would succeed.
space_command_result validate_space_command(const world& w, const space_command& c);

/// THE SEAM. Validate @p c, then apply it now, or queue it when it is a delayed
/// verb to a node outside the nation's coverage. Rejected whole otherwise.
space_command_outcome apply_space_command(world& w, const recipe_registry& reg, const space_command& c);

/// What one quarter's queue did — reported, never stored.
struct space_orders_report
{
    int applied = 0;
    int dropped = 0;
    std::vector<std::pair<uint32_t, space_command_outcome>> dropped_orders; ///< (order id, why), stored order.
};

/// The queue's quarter, inside the nation step BEFORE the space-asset step: every
/// pending order due at or before @p econ_tick is re-validated and applied, or
/// dropped and reported; both leave the queue. Stored order. An empty queue does
/// nothing at all.
void run_space_orders(world& w, const recipe_registry& reg, int econ_tick,
                      space_orders_report* out = nullptr);
