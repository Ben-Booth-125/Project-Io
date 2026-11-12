#pragma once

#include "entity.hpp"
#include "orbital_node.hpp"
#include "space_units.hpp" // space_unit_result

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

struct world;
struct space_battle_params;

// ---------------------------------------------------------------------------
// Blockade, bombardment and the orbital defence battery (BL-1599) —
// docs/military/SPACE_FLEETS.md § Blockade, § Bombardment and fire at orbit
// (Ben, 2026-11-10).
// ---------------------------------------------------------------------------
// BLOCKADE IS DERIVED, never stored. A body is blockaded FOR a nation (its holder
// of ground there) when an armed unit of a nation hostile to it (`space_hostile`,
// either direction — the battles' "at war") stands, not in transit, at the body's
// LOW ORBIT, and the nation has no armed unit there itself. Read at the moment a
// load, an unload or a launch would happen:
//   - a launch from the body (`raise_space_unit_at_pad`, a trade Hauler readied
//     over the pad, a refuel bought up through the pad, `quote_space_launch`) is
//     refused for the blockaded nation;
//   - a load or an unload through the body's pad is refused when the acting nation
//     is blockaded there, or when the nation whose ground the pad's market stands
//     on is (the blockade stops the BODY's space trade, whoever ships);
//   - a trade between bodies is not priced (no Hauler, no straight-line lane) when
//     the source body is blockaded for the trader, or either market's body for
//     that market's nation; a trade Hauler at a blockaded destination waits.
//
// BOMBARDMENT IS A STANDING ORDER (`world::space_bombard_orders`, keyed by unit).
// `order_space_bombard` sets or clears it — the space-unit seam's verb, validated
// whole. Each day (`run_space_strike_day`, after the battle day) every order,
// ascending unit id, fires once: the unit's firepower lowers the target building's
// CONDITION (`building_component::condition_permille`, the strategic-bombing
// channel: throughput scales with it and the daily repair pass mends it) by
// `bombard_condition_per_firepower` per point. A province order strikes the hostile
// building in the province in best condition, ties to the lowest id (the strategic
// mission's rule). The struck building's owner observes `force_used` toward the
// bomber, magnitude `bombard_force_per_permille` x the condition lost — the
// sentiment factor the battles emit, which carries the Alarm (RELATIONS.md § 6). Only
// a building whose condition bites (`condition_bites`) is a target, as for a bomber. An
// order whose unit, target or hostility is gone, or whose unit no longer stands in
// the target body's low orbit, is DROPPED that day.
//   THE MUNITION is the unit's orbital-missile armament (`armament.missile`): only a
//   unit carrying it may bombard. It is not consumed (stand-in: no munition stock).
//
// THE BATTERY is a building (`building_type::orbital_defence_battery`, opened by the
// `orbital_defence_battery` mechanic). Each day, ascending building id, every
// completed, standing battery fires `battery_firepower` x its condition at the
// units in its body's low orbit of nations hostile to its owner, in the battle's
// target order (escorts, capital ships, unarmed hulls; ascending id), armour first
// then hull points (the battle day's `land_fire` rule). A unit at zero is destroyed
// (its cargo, any trade riding it and any bombard order with it). Not a battle
// participant: a daily strike, as the doc says. Its munition is not drawn
// (stand-in: the building's authored maintenance carries it).
//
// DETERMINISM. Every walk is a std::map or a sorted id list; the arithmetic is
// integer (condition) or float over saved inputs in a fixed order. No RNG, no clock.
//
// SAVED: world::space_bombard_orders — world_save.hpp's BL-1599 block. Hashed when
// non-empty.
// ---------------------------------------------------------------------------

/// What a bombard order names. Append-only (saved).
enum class space_bombard_target : uint8_t
{
    building = 0, ///< `target` is a building id.
    province = 1, ///< `target` is a province id.
};

/// One standing order. SERIALISED (BL-1599).
struct space_bombard_order
{
    space_bombard_target kind   = space_bombard_target::building;
    uint32_t             target = 0;

    friend bool operator==(const space_bombard_order&, const space_bombard_order&) = default;
};

/// Ceiling the save reader holds the stream to.
inline constexpr std::size_t k_space_bombard_orders_max = 1u << 16;

/// Is @p body blockaded for @p nation (see the header)? Pure; false for a body with
/// no low orbit node or a world with no space units.
bool space_blockaded(const world& w, entity_id nation, entity_id body);

/// Is @p market's body blockaded for the nation whose ground the market's centre
/// stands on? False for an unknown market or one on no nation's ground.
bool space_market_blockaded(const world& w, entity_id market);

/// A trade's space side, refused by blockade: the source body for @p trader, or
/// either market for its own nation (`space_market_blockaded`).
bool space_trade_blockaded(const world& w, entity_id trader, entity_id src_market, entity_id dest_market);

/// May @p unit bombard at all — does it carry the orbital-missile armament? Pure.
bool space_unit_can_bombard(const space_unit& u);

/// The building a bombard order strikes today, or null: the building itself when
/// standing, completed, of a type its condition bites (`condition_bites`: the
/// strategic mission's targets) and owned by a nation hostile to @p nation; for a province,
/// the hostile building in best condition there (ties to the lowest id). Pure.
entity_id space_bombard_pick(const world& w, entity_id nation, const space_bombard_order& o);

/// The body a bombard order's target lies on, or null.
entity_id space_bombard_target_body(const world& w, const space_bombard_order& o);

/// THE VERB. Set @p unit's standing bombard order, or clear it with @p clear. Refused
/// WHOLE (nothing written): unknown unit (rejected_unknown_unit), not the nation's
/// (rejected_not_owner); clearing an order that is not there (rejected_invalid);
/// otherwise the kind out of range or a target that names no building / province
/// (rejected_invalid); the nation lacks the `orbital_bombardment` key
/// (rejected_tech_locked); the unit has no orbital-missile armament
/// (rejected_not_armed); in transit (rejected_busy); not in the target body's
/// low orbit (rejected_wrong_node); no building there it may strike — none, or none
/// hostile (rejected_target).
space_unit_result order_space_bombard(world& w, entity_id nation, entity_id unit,
                                      space_bombard_target kind, uint32_t target, bool clear = false);

/// Drop the orders of units no longer in the store.
void prune_space_bombard_orders(world& w);

/// What one strike day did — reported, never stored.
struct space_strike_day_report
{
    int   strikes = 0;           ///< Bombard orders that landed.
    int   condition_lost = 0;    ///< Per-mille, summed.
    int   orders_dropped = 0;
    int   battery_shots = 0;     ///< Batteries that had a target and fired.
    float battery_landed = 0.0f; ///< Hull points landed.
    int   units_lost = 0, crew_lost = 0;
    /// THE DEBRIS SEAM: mass destroyed by batteries per node, building id order.
    std::vector<std::pair<orbital_node, float>> mass_destroyed;
};

/// THE DAY, after `run_space_battle_day`: the batteries fire, then the bombard orders
/// land. Nothing at all when there are no space units.
void run_space_strike_day(world& w, const space_battle_params& bp, space_strike_day_report* out = nullptr);
